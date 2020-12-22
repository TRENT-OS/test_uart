/*
 *  Ring Buffer, not fully thread-safe
 *
 *  Copyright (C) 2020, Hensoldt Cyber GmbH
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>

//------------------------------------------------------------------------------
typedef struct
{
    uint8_t*  buffer;
    size_t    capacity;
    size_t    head; // position where data starts
    size_t    used;
} ringbuffer_t;

//------------------------------------------------------------------------------
static inline size_t
ringbuffer_getCappedLen(
    ringbuffer_t* const self,
    size_t pos,
    size_t len)
{
    return (pos + len <= self->capacity) ? len : self->capacity - pos;
}


//------------------------------------------------------------------------------
static inline void
ringbuffer_clear(
    ringbuffer_t* const self)
{
    assert( NULL != self );

    self->head = 0;
    self->used = 0;
}
//------------------------------------------------------------------------------
static inline size_t
ringbuffer_getCapacity(
    ringbuffer_t* const self)
{
    assert( NULL != self );

    return self->capacity;
}


//------------------------------------------------------------------------------
static inline size_t
ringbuffer_getUsed(
    ringbuffer_t* const self)
{
    assert( NULL != self );

    const size_t used = self->used; // read only once to guarantee consistency

    // sanity check
    assert( used <= self->capacity );

    return used;
}


//------------------------------------------------------------------------------
static inline bool
ringbuffer_isEmpty(
    ringbuffer_t* const self)
{
    assert( NULL != self );

    const size_t used = self->used; // read only once to guarantee consistency

    // sanity check
    assert( used <= self->capacity );

    return (0 == used);
}


//------------------------------------------------------------------------------
static inline size_t
ringbuffer_getFree(
    ringbuffer_t* const self)
{
    assert( NULL != self );

    const size_t used = self->used; // read only once to guarantee consistency

    // sanity check
    assert( used <= self->capacity );

    return self->capacity - used;
}


//------------------------------------------------------------------------------
static inline bool
ringbuffer_isFull(
    ringbuffer_t* const self)
{
    assert( NULL != self );

    const size_t used = self->used; // read only once to guarantee consistency

    // sanity check
    assert( used <= self->capacity );

    return (self->capacity == used);
}


//------------------------------------------------------------------------------
static inline void
ringbuffer_init(
    ringbuffer_t* const self,
    void* buffer,
    size_t len)
{
    assert( NULL != self );
    assert( (0 == len) || (NULL != buffer) );

    self->buffer = (uint8_t*)buffer;
    self->capacity = len;

    ringbuffer_clear(self);
}


//------------------------------------------------------------------------------
static inline size_t
ringbuffer_write(
    ringbuffer_t* const self,
    const void* src,
    size_t len)
{
    assert( NULL != self );
    assert( (0 == len) || (NULL != src) );

    const size_t used = self->used; // read only once to guarantee consistency
    assert( used <= self->capacity );

    const size_t free = self->capacity - used;
    if (len > free)
    {
        len = free;
    }

    if (len > 0)
    {
        const size_t pos_head = self->head;
        assert( pos_head < self->capacity );

        // self->capacity can't be 0 here, because len is greater 0
        assert( 0 != self->capacity );
        const size_t pos_free = (pos_head + used) % self->capacity;

        const size_t len1 = ringbuffer_getCappedLen(self, pos_free, len);
        if (len1 > 0)
        {
            memcpy(&self->buffer[pos_free], src, len1);

            const size_t len2 = len - len1;
            if (len2 > 0)
            {
                memcpy(self->buffer, &((uint8_t*)src)[len1], len2);
            }

            // this does read-copy-write and thus it's not thread-safe
            self->used += len;
            assert( self->used <= self->capacity );
        }
    }

    return len;
}


//------------------------------------------------------------------------------
// read or flush if dst is NULL
static inline size_t
ringbuffer_read(
    ringbuffer_t* const self,
    void* dst,
    size_t len)
{
    assert( NULL != self );

    const size_t avail = self->used; // read only once to guarantee consistency
    assert( avail <= self->capacity );

    if (len > avail)
    {
        len = avail;
    }

    const size_t pos_head = self->head;

    if (NULL != dst)
    {
        const size_t len1 = ringbuffer_getCappedLen(self, pos_head, len);
        if (len1 > 0)
        {
            memcpy(dst, &self->buffer[pos_head], len1);

            const size_t len2 = len - len1;
            if (len2 > 0)
            {
                memcpy(&((uint8_t*)dst)[len1], self->buffer, len2);
            }
        }
    }

    if (len > 0)
    {
        // self->capacity can't be 0 here, because len is greater 0
        assert( 0 != self->capacity );
        self->head = (pos_head + len) % self->capacity;

        // we've adjusted len before, so the subtraction is safe. Furthermore,
        // a write shall only increase the amount of data, but never reduce it,
        // so this can't get negative. However, this is still not thread safe,
        // because it does a read-modify-write.
        self->used -= len;
    }

    return len;
}


//------------------------------------------------------------------------------
// get pointer within the FIFO with data to read, which allows doing zero-copy
// operations. Call ringbuffer_read() with dst=NULL once the data has been
// processed, so this part of the buffer can is marked as free again.
static inline size_t
ringbuffer_getReadPtr(
    ringbuffer_t* const self,
    void** ptr)
{
    assert( NULL != self );
    assert( NULL != ptr );

    const size_t avail = self->used; // read only once to guarantee consistency
    assert( avail <= self->capacity );

    const size_t pos_head = self->head;
    assert( pos_head < self->capacity );

    *ptr = &self->buffer[pos_head];
    return ringbuffer_getCappedLen(self, pos_head, avail);
}
