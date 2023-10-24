#ifndef RING_BUFFER_H
#define RING_BUFFER_H

/** @file
 @brief Single-reader single-writer lock-free ring buffer

 RingBuffer is a ring buffer used to transport samples between
 different execution contexts (threads, OS callbacks, interrupt handlers)
 without requiring the use of any locks. This only works when there is
 a single reader and a single writer (ie. one thread or callback writes
 to the ring buffer, another thread or callback reads from it).

 The RingBuffer structure manages a ring buffer containing N
 elements, where N must be a power of two. An element may be any size
 (specified in bytes).

 The memory area used to store the buffer elements must be allocated by
 the client prior to calling InitializeRingBuffer() and must outlive
 the use of the ring buffer.

 @note The ring buffer functions are not normally exposed in the PortAudio libraries.
 If you want to call them then you will need to add pa_ringbuffer.c to your application source code.
*/

#if defined(__APPLE__)
#include <sys/types.h>
typedef int32_t ring_buffer_size_t;
#elif defined(__GNUC__)
typedef long ring_buffer_size_t;
#elif (_MSC_VER >= 1400)
typedef long ring_buffer_size_t;
#elif defined(_MSC_VER) || defined(__BORLANDC__)
typedef long ring_buffer_size_t;
#else
typedef long ring_buffer_size_t;
#endif

class RingBuffer
{
    ring_buffer_size_t bufferSize_;          /**< Number of elements in FIFO. Power of 2. Set by PaUtil_InitRingBuffer. */
    volatile ring_buffer_size_t writeIndex_; /**< Index of next writable element. Set by AdvanceRingBufferWriteIndex. */
    volatile ring_buffer_size_t readIndex_;  /**< Index of next readable element. Set by PaUtil_AdvanceRingBufferReadIndex. */
    ring_buffer_size_t bigMask_;             /**< Used for wrapping indices with extra bit to distinguish full/empty. */
    ring_buffer_size_t smallMask_;           /**< Used for fitting indices to buffer. */
    ring_buffer_size_t elementSizeBytes_;    /**< Number of bytes per element. */
    char *buffer_;                           /**< Pointer to the buffer containing the actual data. */

public:
    RingBuffer();
    ~RingBuffer();
    
    /** Initialize Ring Buffer to empty state ready to have elements written to it.
     @param elementSizeBytes The size of a single data element in bytes.
     @param elementCount The number of elements in the buffer (must be a power of 2).
     @return -1 if elementCount is not a power of 2, otherwise 0.
    */
    int Initialize(ring_buffer_size_t elementSizeBytes, ring_buffer_size_t elementCount);

    /** Reset buffer to empty. Should only be called when buffer is NOT being read or written.
    */
    void Flush();

    /** Retrieve the number of elements available in the ring buffer for writing.
     @return The number of elements available for writing.
    */
    ring_buffer_size_t GetWriteAvailable();

    /** Retrieve the number of elements available in the ring buffer for reading.
     @return The number of elements available for reading.
    */
    ring_buffer_size_t GetReadAvailable();

    /** Write data to the ring buffer.
     @param data The address of new data to write to the buffer.
     @param elementCount The number of elements to be written.
     @return The number of elements written.
    */
    ring_buffer_size_t Write(const void *data, ring_buffer_size_t elementCount);

    /** Read data from the ring buffer.
     @param data The address where the data should be stored.
     @param elementCount The number of elements to be read.
     @return The number of elements read.
    */
    ring_buffer_size_t Read(void *data, ring_buffer_size_t elementCount);

private:
    /** Get address of region(s) to which we can write data.
     @param elementCount The number of elements desired.
     @param dataPtr1 The address where the first (or only) region pointer will be stored.
     @param sizePtr1 The address where the first (or only) region length will be stored.
     @param dataPtr2 The address where the second region pointer will be stored if the 
                     first region is too small to satisfy elementCount.
     @param sizePtr2 The address where the second region length will be stored if the 
                     first region is too small to satisfy elementCount.
     @return The room available to be written or elementCount, whichever is smaller.
    */
    ring_buffer_size_t GetWriteRegions(ring_buffer_size_t elementCount,
                                       void **dataPtr1, ring_buffer_size_t *sizePtr1,
                                       void **dataPtr2, ring_buffer_size_t *sizePtr2);

    /** Advance the write index to the next location to be written.
     @param elementCount The number of elements to advance.
     @return The new position.
    */
    ring_buffer_size_t AdvanceWriteIndex(ring_buffer_size_t elementCount);

    /** Get address of region(s) from which we can read data.
     @param elementCount The number of elements desired.
     @param dataPtr1 The address where the first (or only) region pointer will be stored.
     @param sizePtr1 The address where the first (or only) region length will be stored.
     @param dataPtr2 The address where the second region pointer will be stored if
                     the first region is too small to satisfy elementCount.
     @param sizePtr2 The address where the second region length will be stored if
                     the first region is too small to satisfy elementCount.
     @return The number of elements available for reading.
    */
    ring_buffer_size_t GetReadRegions(ring_buffer_size_t elementCount,
                                      void **dataPtr1, ring_buffer_size_t *sizePtr1,
                                      void **dataPtr2, ring_buffer_size_t *sizePtr2);

    /** Advance the read index to the next location to be read.
     @param elementCount The number of elements to advance.
     @return The new position.
    */
    ring_buffer_size_t AdvanceReadIndex(ring_buffer_size_t elementCount);
};

#endif /* RING_BUFFER_H */
