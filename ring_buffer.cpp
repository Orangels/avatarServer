#include "ring_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#if defined(__APPLE__)
/* Support for the atomic library was added in C11.
 */
#if (__STDC_VERSION__ < 201112L) || defined(__STDC_NO_ATOMICS__)
#include <libkern/OSAtomic.h>
/* Here are the memory barrier functions. Mac OS X only provides
   full memory barriers, so the three types of barriers are the same,
   however, these barriers are superior to compiler-based ones.
   These were deprecated in MacOS 10.12. */
#define FullMemoryBarrier() OSMemoryBarrier()
#define ReadMemoryBarrier() OSMemoryBarrier()
#define WriteMemoryBarrier() OSMemoryBarrier()
#else
#include <stdatomic.h>
#define FullMemoryBarrier() atomic_thread_fence(memory_order_seq_cst)
#define ReadMemoryBarrier() atomic_thread_fence(memory_order_acquire)
#define WriteMemoryBarrier() atomic_thread_fence(memory_order_release)
#endif
#elif defined(__GNUC__)
/* GCC >= 4.1 has built-in intrinsics. We'll use those */
#if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#define FullMemoryBarrier() __sync_synchronize()
#define ReadMemoryBarrier() __sync_synchronize()
#define WriteMemoryBarrier() __sync_synchronize()
/* as a fallback, GCC understands volatile asm and "memory" to mean it
 * should not reorder memory read/writes */
/* Note that it is not clear that any compiler actually defines __PPC__,
 * it can probably removed safely. */
#elif defined(__ppc__) || defined(__powerpc__) || defined(__PPC__)
#define FullMemoryBarrier() asm volatile("sync" :: \
                                             : "memory")
#define ReadMemoryBarrier() asm volatile("sync" :: \
                                             : "memory")
#define WriteMemoryBarrier() asm volatile("sync" :: \
                                              : "memory")
#elif defined(__i386__) || defined(__i486__) || defined(__i586__) || \
    defined(__i686__) || defined(__x86_64__)
#define FullMemoryBarrier() asm volatile("mfence" :: \
                                             : "memory")
#define ReadMemoryBarrier() asm volatile("lfence" :: \
                                             : "memory")
#define WriteMemoryBarrier() asm volatile("sfence" :: \
                                              : "memory")
#else
#ifdef ALLOW_SMP_DANGERS
#warning Memory barriers not defined on this system or system unknown
#warning For SMP safety, you should fix this.
#define FullMemoryBarrier()
#define ReadMemoryBarrier()
#define WriteMemoryBarrier()
#else
#error Memory barriers are not defined on this system. You can still compile by defining ALLOW_SMP_DANGERS, but SMP safety will not be guaranteed.
#endif
#endif
#elif (_MSC_VER >= 1400) && !defined(_WIN32_WCE)
#include <intrin.h>
#pragma intrinsic(_ReadWriteBarrier)
#pragma intrinsic(_ReadBarrier)
#pragma intrinsic(_WriteBarrier)
/* note that MSVC intrinsics _ReadWriteBarrier(), _ReadBarrier(), _WriteBarrier() are just compiler barriers *not* memory barriers */
#define FullMemoryBarrier() _ReadWriteBarrier()
#define ReadMemoryBarrier() _ReadBarrier()
#define WriteMemoryBarrier() _WriteBarrier()
#elif defined(_WIN32_WCE)
#define FullMemoryBarrier()
#define ReadMemoryBarrier()
#define WriteMemoryBarrier()
#elif defined(_MSC_VER) || defined(__BORLANDC__)
#define FullMemoryBarrier() _asm { lock add    [esp], 0}
#define ReadMemoryBarrier() _asm { lock add    [esp], 0}
#define WriteMemoryBarrier() _asm { lock add    [esp], 0}
#else
#ifdef ALLOW_SMP_DANGERS
#warning Memory barriers not defined on this system or system unknown
#warning For SMP safety, you should fix this.
#define FullMemoryBarrier()
#define ReadMemoryBarrier()
#define WriteMemoryBarrier()
#else
#error Memory barriers are not defined on this system. You can still compile by defining ALLOW_SMP_DANGERS, but SMP safety will not be guaranteed.
#endif
#endif

RingBuffer::RingBuffer()
{
    bufferSize_ = 0;
    writeIndex_ = 0;
    readIndex_ = 0;
    bigMask_ = 0;
    smallMask_ = 0;
    elementSizeBytes_ = 0;
    buffer_ = nullptr;
}

RingBuffer::~RingBuffer()
{
    if (buffer_)
    {
        free(buffer_);
        buffer_ = nullptr;
    }
}

/***************************************************************************
 * Initialize FIFO.
 * elementCount must be power of 2, returns -1 if not.
 */
int RingBuffer::Initialize(ring_buffer_size_t elementSizeBytes, ring_buffer_size_t elementCount)
{
    if (buffer_)
    {
        free(buffer_);
        buffer_ = nullptr;
    }

    if (((elementCount - 1) & elementCount) != 0)
        return -1; /* Not Power of two. */
    bufferSize_ = elementCount;
    buffer_ = (char *)malloc(elementCount * elementSizeBytes);
    Flush();
    bigMask_ = (elementCount * 2) - 1;
    smallMask_ = elementCount - 1;
    elementSizeBytes_ = elementSizeBytes;
    return 0;
}

/***************************************************************************
 * Return number of elements available for reading.
 */
ring_buffer_size_t RingBuffer::GetReadAvailable()
{
    return ((writeIndex_ - readIndex_) & bigMask_);
}

/***************************************************************************
 * Return number of elements available for writing.
 */
ring_buffer_size_t RingBuffer::GetWriteAvailable()
{
    return (bufferSize_ - GetReadAvailable());
}

/***************************************************************************
 * Clear buffer. Should only be called when buffer is NOT being read or written.
 */
void RingBuffer::Flush()
{
    writeIndex_ = readIndex_ = 0;
}

/***************************************************************************
** Get address of region(s) to which we can write data.
** If the region is contiguous, size2 will be zero.
** If non-contiguous, size2 will be the size of second region.
** Returns room available to be written or elementCount, whichever is smaller.
*/
ring_buffer_size_t RingBuffer::GetWriteRegions(ring_buffer_size_t elementCount,
                                               void **dataPtr1, ring_buffer_size_t *sizePtr1,
                                               void **dataPtr2, ring_buffer_size_t *sizePtr2)
{
    ring_buffer_size_t index;
    ring_buffer_size_t available = GetWriteAvailable();
    if (elementCount > available)
        elementCount = available;
    /* Check to see if write is not contiguous. */
    index = writeIndex_ & smallMask_;
    if ((index + elementCount) > bufferSize_)
    {
        /* Write data in two blocks that wrap the buffer. */
        ring_buffer_size_t firstHalf = bufferSize_ - index;
        *dataPtr1 = &buffer_[index * elementSizeBytes_];
        *sizePtr1 = firstHalf;
        *dataPtr2 = &buffer_[0];
        *sizePtr2 = elementCount - firstHalf;
    }
    else
    {
        *dataPtr1 = &buffer_[index * elementSizeBytes_];
        *sizePtr1 = elementCount;
        *dataPtr2 = NULL;
        *sizePtr2 = 0;
    }

    if (available)
        FullMemoryBarrier(); /* (write-after-read) => full barrier */

    return elementCount;
}

/***************************************************************************
 */
ring_buffer_size_t RingBuffer::AdvanceWriteIndex(ring_buffer_size_t elementCount)
{
    /* ensure that previous writes are seen before we update the write index
       (write after write)
    */
    WriteMemoryBarrier();
    return writeIndex_ = (writeIndex_ + elementCount) & bigMask_;
}

/***************************************************************************
** Get address of region(s) from which we can read data.
** If the region is contiguous, size2 will be zero.
** If non-contiguous, size2 will be the size of second region.
** Returns room available to be read or elementCount, whichever is smaller.
*/
ring_buffer_size_t RingBuffer::GetReadRegions(ring_buffer_size_t elementCount,
                                              void **dataPtr1, ring_buffer_size_t *sizePtr1,
                                              void **dataPtr2, ring_buffer_size_t *sizePtr2)
{
    ring_buffer_size_t index;
    ring_buffer_size_t available = GetReadAvailable(); /* doesn't use memory barrier */
    if (elementCount > available)
        elementCount = available;
    /* Check to see if read is not contiguous. */
    index = readIndex_ & smallMask_;
    if ((index + elementCount) > bufferSize_)
    {
        /* Write data in two blocks that wrap the buffer. */
        ring_buffer_size_t firstHalf = bufferSize_ - index;
        *dataPtr1 = &buffer_[index * elementSizeBytes_];
        *sizePtr1 = firstHalf;
        *dataPtr2 = &buffer_[0];
        *sizePtr2 = elementCount - firstHalf;
    }
    else
    {
        *dataPtr1 = &buffer_[index * elementSizeBytes_];
        *sizePtr1 = elementCount;
        *dataPtr2 = NULL;
        *sizePtr2 = 0;
    }

    if (available)
        ReadMemoryBarrier(); /* (read-after-read) => read barrier */

    return elementCount;
}
/***************************************************************************
 */
ring_buffer_size_t RingBuffer::AdvanceReadIndex(ring_buffer_size_t elementCount)
{
    /* ensure that previous reads (copies out of the ring buffer) are always completed before updating (writing) the read index.
       (write-after-read) => full barrier
    */
    FullMemoryBarrier();
    return readIndex_ = (readIndex_ + elementCount) & bigMask_;
}

/***************************************************************************
** Return elements written. */
ring_buffer_size_t RingBuffer::Write(const void *data, ring_buffer_size_t elementCount)
{
    ring_buffer_size_t size1, size2, numWritten;
    void *data1, *data2;
    numWritten = GetWriteRegions(elementCount, &data1, &size1, &data2, &size2);
    if (size2 > 0)
    {

        memcpy(data1, data, size1 * elementSizeBytes_);
        data = ((char *)data) + size1 * elementSizeBytes_;
        memcpy(data2, data, size2 * elementSizeBytes_);
    }
    else
    {
        memcpy(data1, data, size1 * elementSizeBytes_);
    }
    AdvanceWriteIndex(numWritten);
    return numWritten;
}

/***************************************************************************
** Return elements read. */
ring_buffer_size_t RingBuffer::Read(void *data, ring_buffer_size_t elementCount)
{
    ring_buffer_size_t size1, size2, numRead;
    void *data1, *data2;
    numRead = GetReadRegions(elementCount, &data1, &size1, &data2, &size2);
    if (size2 > 0)
    {
        memcpy(data, data1, size1 * elementSizeBytes_);
        data = ((char *)data) + size1 * elementSizeBytes_;
        memcpy(data, data2, size2 * elementSizeBytes_);
    }
    else
    {
        memcpy(data, data1, size1 * elementSizeBytes_);
    }
    AdvanceReadIndex(numRead);
    return numRead;
}
