/*
 * UART test
 *
 * Copyright (C) 2020-2024, HENSOLDT Cyber GmbH
 * 
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * For commercial licensing, contact: info.cyber@hensoldt.net
 */

#include "OS_Error.h"
#include "OS_Dataport.h"
#include "lib_io/FifoDataport.h"
#include "ringbuffer.h"
#include "lib_debug/Debug.h"

#include <camkes.h>

#include <string.h>

//#define FIFO_PROFILING

typedef struct {
    FifoDataport*  uart_fifo; // FIFO in dataport shared with the UART driver
    ringbuffer_t   rb; // internal FIFO
    size_t         bytes_processed;
    uint8_t        byte_processor;
    uint8_t        expecting_byte;
    uint8_t        data_window[6];
    // seems we need this internal FIFO on QEMU, as UART baudtrates are not
    // guaranteed there. And besides throttling, data sometimes still comes
    // faster than we can process it.
    uint8_t        fifo_buffer[4096];
#ifdef FIFO_PROFILING
    size_t         fifo_read_cnt;
    size_t         fifo_reads[128];
#endif // FIFO_PROFILING
} test_ctx_t;


//---------------------------------------------------------------------------
static void
data_processor(
    test_ctx_t* ctx,
    const uint8_t data_byte)
{
    // Dummy processing, volatile ensures the compiler can't optimize this out.
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

    ctx->data_window[sizeof(ctx->data_window)-1] = data_byte;

    bool err = (data_byte != ctx->expecting_byte);
    if (err)
    {
        Debug_LOG_ERROR(
            "bytes processed: 0x%zx (%zu), expected 0x%02x, read 0x%02x, window:",
            ctx->bytes_processed,
            ctx->bytes_processed,
            ctx->expecting_byte,
            data_byte);
        Debug_DUMP_ERROR(ctx->data_window, sizeof(ctx->data_window));

        // Re-sync with the data stream, in case caller wants to continue.
        ctx->expecting_byte = data_byte + 1;
    }
    else
    {
        ctx->expecting_byte++;
    }


    // Call a dummy processing function that creates some load.
    data_processor(ctx, data_byte);

    ctx->bytes_processed++;
    if (0 == (ctx->bytes_processed % (64 * 1024)))
    {
        Debug_LOG_INFO("bytes processed: 0x%zx", ctx->bytes_processed);
#ifdef FIFO_PROFILING
        char buf[100] = { 0 };
        size_t idx = 0;
        while (idx < ctx->fifo_read_cnt)
        {
            int l = sprintf(buf, "avail[%3zu]:", idx);
            while (idx < ctx->fifo_read_cnt)
            {
                l += sprintf(&buf[l], " %4zd", ctx->fifo_reads[idx++]);
                if (0 == (idx & 0x7)) { break; }
            }
            Debug_LOG_INFO("%s", buf);
        }
        ctx->fifo_read_cnt = 0;
#endif // FIFO_PROFILING
    }

    return err ? OS_ERROR_INVALID_STATE : OS_SUCCESS;
}


//---------------------------------------------------------------------------
static OS_Error_t
process_data(
    test_ctx_t*  ctx)
{
    ringbuffer_t* rb = &(ctx->rb);

    // Get a contiguous buffer from the internal FIFO that we can pass on
    // for processing. Since the FIFO can wrap around, there is no guarantee
    // that we can get all available data in one contiguous buffer.
    for(;;)
    {
        uint8_t* buffer = NULL;
        size_t len = ringbuffer_getReadPtr(rb, (void**)&buffer);
        if (0 == len)
        {
            return OS_SUCCESS;
        }

        assert(buffer); // We have data in the FIFO, so this can't be NULL.
        for(size_t cnt_processed = 0; cnt_processed < len; cnt_processed++)
        {

            uint8_t data_byte = buffer[cnt_processed];
            // call a dummy function to simulate processing load
            OS_Error_t ret = do_process(ctx, data_byte);
            if (OS_SUCCESS != ret)
            {
                Debug_LOG_ERROR("do_process() failed, code %d", ret);

                Debug_LOG_ERROR(
                    "buffer %p, processed %zu (0x%zx) of %zu",
                    buffer, cnt_processed, cnt_processed, len);
                Debug_DUMP_ERROR(
                    buffer,
                    MIN(cnt_processed+3, len));
                Debug_LOG_ERROR(
                    "rb: used %zu (0x%zx) of %zu, head %zu (0x%zx)",
                    rb->used, rb->used, rb->capacity, rb->head, rb->head);
                Debug_DUMP_ERROR(rb->buffer, rb->capacity);
                Debug_LOG_ERROR(
                    "FIFO: used %zu (0x%zx) of %zu, head %zu (0x%zx)",
                    FifoDataport_getSize(ctx->uart_fifo),
                    FifoDataport_getSize(ctx->uart_fifo),
                    FifoDataport_getCapacity(ctx->uart_fifo),
                    ctx->uart_fifo->dataStruct.first,
                    ctx->uart_fifo->dataStruct.first);
                Debug_DUMP_ERROR(
                    ctx->uart_fifo->data,
                    FifoDataport_getCapacity(ctx->uart_fifo));
                return OS_ERROR_GENERIC;
            }
        }

        ringbuffer_flush(rb, len);

    } // for(;;)

    UNREACHABLE();
}


//---------------------------------------------------------------------------
static bool
is_fifo_overflow(
    test_ctx_t*  ctx)
{
    // Currently, the overflow "flag" is defined as byte and not as bit.
    uint8_t isFifoOverflow = *((volatile uint8_t*)(
                                    (uintptr_t)ctx->uart_fifo
                                    + (Uart_INPUT_FIFO_DATAPORT_SIZE - 1) ));

    return (0 != isFifoOverflow);
}


//---------------------------------------------------------------------------
static OS_Error_t
blocking_read(
    test_ctx_t*  ctx)
{
    FifoDataport* fifo = ctx->uart_fifo;
    ringbuffer_t* rb = &(ctx->rb);
    bool is_overflow = false;

    for (;;)
    {
        // Check if there is an overflow. Print a warning message only once and
        // set an internal flag that we check later
        if (!is_overflow && is_fifo_overflow(ctx))
        {
            is_overflow = true;
            Debug_LOG_ERROR("dataport FIFO overflow detected, %zu left to be read",
                            FifoDataport_getSize(fifo));
        }

        // Try to read new data to drain the dataport FIFO.
        void* buffer = NULL;
        size_t avail = FifoDataport_getContiguous(fifo, &buffer);
        if (avail > 0)
        {
            // put the new data in our internal buffer
            assert(buffer);
            size_t copied = ringbuffer_write(rb, buffer, avail);
            assert(copied <= avail);
            if (0 == copied)
            {
                Debug_LOG_ERROR("ringbuffer full, avail %zu", avail);
                return OS_SUCCESS;
            }

            FifoDataport_remove(fifo, copied);
#ifdef FIFO_PROFILING
            ctx->fifo_reads[ctx->fifo_read_cnt++] = avail;
#endif // FIFO_PROFILING
            return OS_SUCCESS;
        }

        // There was no new data in the FIFO. Check if there was an overflow,
        // in this case the driver still not add new data to the buffer until
        // the overflow is resolved.
        if (is_overflow)
        {
            // In a real application we should handle the overflow, but for the
            // test here it is considered fatal, as we expect things to be good
            // enough to never run into overflows.
            return OS_ERROR_OVERFLOW_DETECTED;
        }

        // There was no new data in the FIFO. However, we can't block if there
        // is still data in the internal FIFO buffer.
        if (!ringbuffer_isEmpty(rb))
        {
            return OS_SUCCESS;
        }

        // Block waiting for an event that reports there is new data in the
        // dataport FIFO. We can never end in a deadlock here, even if the
        // driver update the dataport FIFO in parallel. The worst thing that
        // can happen is that we get an event and there is no new data, because
        // we have processed this data above already.

        uart_event_wait();

        // We got the event, simply repeat the loop. Note that getting an event
        // does not guarantee there is really new data in the dataport FIFO.

    } // end for (;;)
}


//---------------------------------------------------------------------------
static OS_Error_t
do_run_test(void)
{
    OS_Dataport_t in_port = OS_DATAPORT_ASSIGN(uart_input_port);
    void* buf_port = OS_Dataport_getBuf(in_port);

    static test_ctx_t ctx = { 0 }; // don't use the stack

    ctx.uart_fifo = (FifoDataport*)buf_port;

    ringbuffer_t* rb = &(ctx.rb);
    ringbuffer_init(rb, ctx.fifo_buffer, sizeof(ctx.fifo_buffer));

    // test runner check for this string
    Debug_LOG_DEBUG("UART tester loop running");

    for(;;)
    {
        OS_Error_t ret;

        // Read as much data as possible from the dataport FIFO into the
        // internal FIFO. If both the internal FIFO and the dataport FIFO are
        // empty, this will block until data is available.
        ret = blocking_read(&ctx);
        if (OS_SUCCESS != ret)
        {
            Debug_LOG_ERROR("blocking_read() failed, code %d", ret);
            return OS_ERROR_GENERIC;
        }

        // If we arrive here, there is data in the internal FIFO available for
        // processing.
        assert( !ringbuffer_isEmpty(rb) );
        ret = process_data(&ctx);
        if (OS_SUCCESS != ret)
        {
            Debug_LOG_ERROR("process_data() failed, code %d", ret);
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
    if (OS_SUCCESS != ret)
    {
        Debug_LOG_ERROR("do_run_test() failed, code %d", ret);
        return -1;
    }

    return 0;
}
