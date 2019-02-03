/*
 * ring_buffer.c
 * A simple ring buffer implementation capable of multi-byte read/write and
 * peeking.
 *
 * Copyright 2018, 2019 Matt Rounds
 *
 * This file is part of ExplorerLink.
 *
 * ExplorerLink is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * ExplorerLink is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ExplorerLink. If not, see <https://www.gnu.org/licenses/>.
 */


#include <stdbool.h>
#include <stdint.h>
#include "ring_buffer.h"


/*
 * Get the status (empty, full, or partially filled) for a ring buffer.
 */
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

/*
 * Read an arbitrary number of bytes from a given buffer.
 */
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

/*
 * Clear a ring buffer. This is a reset of read/write indices and does not
 * imply erasure of garbage data in memory.
 */
void vRingBufferClear(volatile RingBuffer_t *pxBuffer) {
    pxBuffer->ulReadIndex = pxBuffer->ulWriteIndex;
}

/*
 * Get the data from the current read index without incrementing it.
 */
RingBufferStatus_t xRingBufferPeek(volatile RingBuffer_t *pxBuffer,
                                   uint8_t *pucByte) {
    uint32_t ulWriteIndexTemp = pxBuffer->ulWriteIndex;

    if (ulWriteIndexTemp == pxBuffer->ulReadIndex)
        return BUFFER_EMPTY;

    *pucByte = pxBuffer->pucData[(pxBuffer->ulSize + ulWriteIndexTemp-1)
                                   % pxBuffer->ulSize];
    return BUFFER_OK;
}
