/*
 * ring_buffer.c
 *
 *  Created on: Apr 27, 2018
 *      Author: Matt
 */

#include <stdbool.h>
#include <stdint.h>
#include "ring_buffer.h"

RingBufferStatus_t eRingBufferStatus(volatile RingBuffer_t *pxBuffer) {

    if (pxBuffer->ulReadIndex == pxBuffer->ulWriteIndex) {
        return BUFFER_EMPTY;
    }
    else if ((pxBuffer->ulWriteIndex+1) % pxBuffer->ulSize ==
             pxBuffer->ulReadIndex) {
        return BUFFER_FULL;
    }

    return BUFFER_OK;
}

/*
 * Read a single byte from the ring buffer. This function is not reentrant
 * when called for the same RingBuffer_t. However, writes to the buffer are
 * generally thread-safe as long as it can be tolerated that BUFFER_EMPTY may
 * be returned despite a write adding to the buffer before the return. If this
 * can't be tolerated, use critical sections or mutexes.
 */
RingBufferStatus_t eRingBufferRead(volatile RingBuffer_t *pxBuffer,
                                   uint8_t *pucByte) {
    /* Check if the buffer is empty first. If a write occurs just after this
     * check, the return value may be incorrect. */
    if (pxBuffer->ulReadIndex == pxBuffer->ulWriteIndex) {
        return BUFFER_EMPTY;
    }
    
    /* Read the byte out. */
    *pucByte = pxBuffer->pucData[pxBuffer->ulReadIndex];

    /* Increment the read index, wrapping if needed. */
    pxBuffer->ulReadIndex = (pxBuffer->ulReadIndex+1) % pxBuffer->ulSize;
    
    return BUFFER_OK;
}

RingBufferStatus_t eRingBufferReadN(volatile RingBuffer_t *pxBuffer,
                                    uint8_t *pucBytes, uint32_t ulNBytes) {
    while (ulNBytes--) {
        if (eRingBufferRead(pxBuffer, pucBytes++) == BUFFER_EMPTY) {
            return BUFFER_EMPTY;
        }
    }
    return BUFFER_OK;
}

/*
 * Write a single byte to a given buffer. This function is not reentrant when
 * called for the same RingBuffer_t. Reading the buffer during a write call
 * can be thread-safe if it is tolerable that BUFFER_FULL is returned
 * incorrectly when execution returns to the write call. Use critical sections
 * or mutexes otherwise.
 */
RingBufferStatus_t eRingBufferWrite(volatile RingBuffer_t *pxBuffer,
                                    uint8_t ucByte) {
    /* Get the next write index after this byte is written, wrapping if
     * needed. */
    uint32_t ulNextIndex = (pxBuffer->ulWriteIndex+1) % pxBuffer->ulSize;

    /* Check that there is more than one space remaining. If the current write
     * index is the only space remaining, we consider the buffer already full.
     * This prevents confusion between the empty and full states, at the
     * expense of a single byte of capacity. If a read occurs after this check
     * but before returning, the return value may be incorrect. */
    if (ulNextIndex == pxBuffer->ulReadIndex) {
        return BUFFER_FULL;
    }
    
    /* Write the byte to the buffer and update the write index. */
    pxBuffer->pucData[pxBuffer->ulWriteIndex] = ucByte;
    pxBuffer->ulWriteIndex = ulNextIndex;
    
    return BUFFER_OK;
}

/*
 * Write an arbitrary number of bytes to a given buffer.
 */
RingBufferStatus_t eRingBufferWriteN(volatile RingBuffer_t *pxBuffer,
                                    uint8_t *pucBytes, uint32_t ulNBytes) {
    while (ulNBytes--) {
        if (eRingBufferWrite(pxBuffer, *(pucBytes++)) == BUFFER_FULL) {
            return BUFFER_FULL;
        }
    }
    return BUFFER_OK;
}

void vRingBufferClear(volatile RingBuffer_t *pxBuffer) {
    pxBuffer->ulReadIndex = pxBuffer->ulWriteIndex;
}

RingBufferStatus_t xRingBufferPeek(volatile RingBuffer_t *pxBuffer,
                                   uint8_t *pucByte) {
    uint32_t ulWriteIndexTemp = pxBuffer->ulWriteIndex;

    if (ulWriteIndexTemp == pxBuffer->ulReadIndex)
        return BUFFER_EMPTY;

    *pucByte = pxBuffer->pucData[(pxBuffer->ulSize + ulWriteIndexTemp-1)
                                   % pxBuffer->ulSize];
    return BUFFER_OK;
}
