/*
 *  UART test
 *
 *  Copyright (C) 2020, Hensoldt Cyber GmbH
 */

#include "OS_Error.h"
#include "OS_Dataport.h"
#include "LibIO/FifoDataport.h"
#include "ringbuffer.h"
#include "LibDebug/Debug.h"

#include <camkes.h>

#include <string.h>

// 2048 byte worked well during tests, with 1024 byte we see a lot of messages
// from the eager fetching strategy, so UART log out if effectively slowing
// down things
#define INTERNAL_FIFO_SIZE  2048


typedef struct {
    FifoDataport*  uart_fifo;
    ringbuffer_t   rb;
    size_t         processing_boost;
    size_t         bytes_processed;
    uint8_t        byte_processor;
    uint8_t        expecting_byte;
    uint8_t        data_window[6];
} test_ctx_t;


//---------------------------------------------------------------------------
static void
data_processor(
    test_ctx_t* ctx,
    const uint8_t data_byte)
{
    // dummy processing, volatile ensure the compiler can optimize this out

    volatile uint8_t x = data_byte;

    x = (x >> 1) | ((uint8_t)(x << 7));
    x = ~x;
    x |= ctx->byte_processor;

    ctx->byte_processor = x;

}


//---------------------------------------------------------------------------
static OS_Error_t
do_process(
    test_ctx_t* ctx,
    const uint8_t data_byte)
{
    memmove(
        ctx->data_window,
        &(ctx->data_window[1]),
        sizeof(ctx->data_window)-1 );

    bool err = (data_byte != ctx->expecting_byte);
    if (err)
    {
        Debug_LOG_ERROR(
            "bytes processed: 0x%zx, expected 0x%02x, read 0x%02x, window:",
            ctx->bytes_processed,
            ctx->expecting_byte,
            data_byte);
        Debug_DUMP_ERROR(ctx->data_window, sizeof(ctx->data_window));

        // re-sync with the data stream, in case caller wants to continue.
        ctx->expecting_byte = data_byte + 1;
    }
    else
    {
        ctx->expecting_byte++;
    }


    // call a dummy processing function that creates some load
    data_processor(ctx, data_byte);

    ctx->bytes_processed++;
    if (0 == (ctx->bytes_processed % (64 * 1024)))
    {
        Debug_LOG_INFO("bytes processed: 0x%zx", ctx->bytes_processed);
    }

    return err ? OS_ERROR_INVALID_STATE : OS_SUCCESS;
}


//---------------------------------------------------------------------------
static OS_Error_t
process_data(
    test_ctx_t*  ctx)
{
    // we get the amount of contiguous data from the ring buffer and pass it on
    // for processing. We loop here, because what we get per call may not be
    // all data that is in the fing buffer, as the used part of the buffer can
    // wrap around.

    size_t processing_boost = ctx->processing_boost;

    for (;;)
    {
        void* p = NULL;
        const size_t len = ringbuffer_getReadPtr(&(ctx->rb), &p);
        if (0 == len)
        {
            return OS_SUCCESS;
        }

        const uint8_t* data = (const uint8_t*)p;

        for (size_t cnt = 0; cnt < len; cnt++)
        {
            OS_Error_t ret = do_process(ctx, data[cnt]);
            if (OS_SUCCESS != ret)
            {
                Debug_LOG_ERROR("do_process() failed, code %d ", ret);
                if (cnt > 0)
                {
                    ringbuffer_read(&(ctx->rb), NULL, cnt); // flush
                }
                return OS_ERROR_GENERIC;
            }

            // note that we actually process "processing_boost + 1" bytes here.
            // That is acceptable for this test case, since we want to process
            // at least one byte in any case.
            if (0 == processing_boost--)
            {
                ringbuffer_read(&(ctx->rb), NULL, cnt+1);
                return OS_SUCCESS;
            }
        }

        // flush processed data from the internal FIFO. There must be data,
        // otherwise we would never be here
        ringbuffer_read(&(ctx->rb), NULL, len);
    }

    return OS_SUCCESS;
}


//---------------------------------------------------------------------------
static void
read_data(
    test_ctx_t*  ctx)
{       // try to read new data to drain the dataport FIFO
    size_t avail = FifoDataport_getAmountConsecutives(ctx->uart_fifo);
    if (0 == avail)
    {
        return;
    }

    char const* buf_port = FifoDataport_getFirst(ctx->uart_fifo);
    assert( NULL != buf_port );
    const size_t copied = ringbuffer_write(&(ctx->rb), buf_port, avail);
    FifoDataport_remove(ctx->uart_fifo, copied);

    // process up to 16 bytes in a row if there was new data, This is an
    // arbitrary value picked for the test.
    ctx->processing_boost = 16;

    // if our internal FIFO is more than 75% filled, give processing
    // of data a real boost and drain the FIFO until 75% are available
    // again
    const size_t capacity = ringbuffer_getCapacity(&(ctx->rb));
    const size_t used = ringbuffer_getUsed(&(ctx->rb));
    const size_t watermark = (capacity / 2) + (capacity / 4);
    if (used <= watermark)
    {
        ctx->processing_boost = used - watermark;

        // if we are above the watermark level and we could copy less
        // data then there is available, there is the danger of running
        // into an overflow condition. So we better log a message, that
        // helps when debugging a real overflow issue.
        if (copied < avail)
        {
            Debug_LOG_INFO("avail %zu, copied %zu, boost %zu",
                            avail, copied, ctx->processing_boost);
        }
    }
}


//---------------------------------------------------------------------------
static OS_Error_t
do_run_test(void)
{
    OS_Dataport_t in_port = OS_DATAPORT_ASSIGN(uart_input_port);
    void* buf_port = OS_Dataport_getBuf(in_port);

    test_ctx_t ctx = { 0U };

    ctx.uart_fifo = (FifoDataport*)buf_port;

    // the last byte of the dataport holds an overflow flag
    volatile char* isFifoOverflow = (volatile char*)(
                                        (uintptr_t)buf_port
                                        + OS_Dataport_getSize(in_port) - 1 );

    // don't allocate the buffer on the stack. Aligning with not specification
    // will use the largest alignment which is ever used for any data type on
    // the target machine.
    static uint8_t fifo_buffer[INTERNAL_FIFO_SIZE] __attribute__ ((aligned));
    ringbuffer_init(&(ctx.rb), fifo_buffer, sizeof(fifo_buffer));

    // test runner check for this string
    Debug_LOG_DEBUG("UART tester loop running");

    for (;;)
    {
        // if there is no data in the FIFO then wait for new data
        if (ringbuffer_isEmpty(&(ctx.rb))
            && FifoDataport_isEmpty(ctx.uart_fifo))
        {
            // no new data will arrive if there was an overflow
            if (0 != *isFifoOverflow)
            {
                Debug_LOG_ERROR("dataport FIFO overflow detected");

                // in a real application we should handle the overflow, but for
                // the test here it is considered fatal, as we expect things to
                // be good enough to never run into overflows.
                return OS_ERROR_OVERFLOW_DETECTED;
            }

            // block waiting for an event. Such an event indicates either
            // new data or a state change that needs attention.
            uart_event_wait();
        }


        // We have to balance things between reading data from the dataport
        // FIFO and processing it. Options are:
        //
        // * prefer reading from the dataport into out internal FIFO to ensure
        //   the FIFO in the dataport never fills up
        // * process all available data as soon as it becomes available, so we
        //   the higher layers can consume them
        // * something in the middle.
        //
        // The variable "processing_boost" controls how much data is processed
        // in one iteration. In general, we prefer reading data from the
        // dataport FIFO over processing the data. This ensures the dataport
        // FIFO has space for new data. However, when our internal FIFO is
        // full, we prefer processing data over reading more data from the
        // underlying FIFO.

        // process up to 8 bytes in a row by default. This is an arbitrary
        // value picked for the test.
        ctx.processing_boost = 8;

        read_data(&ctx);

        OS_Error_t ret = process_data(&ctx);
        if (OS_SUCCESS != ret)
        {
            Debug_LOG_ERROR("process_data() failed, code %d ", ret);
            return OS_ERROR_GENERIC;
        }

    } // end for (;;)
}


//---------------------------------------------------------------------------
void pre_init(void)
{
    Debug_LOG_DEBUG("pre_init");
}


//---------------------------------------------------------------------------
void post_init(void)
{
    Debug_LOG_DEBUG("post_init");
}


//------------------------------------------------------------------------------
int run()
{
    Debug_LOG_DEBUG("run");

    OS_Error_t ret = do_run_test();
    Debug_LOG_DEBUG("do_run() returned %d", ret);

    return 0;
}
