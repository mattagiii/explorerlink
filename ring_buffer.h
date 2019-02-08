/*
 * ring_buffer.h
 * API for a simple ring buffer implementation.
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

#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

#include <stdbool.h>
#include <stdint.h>


/* RingBuffer_t represents a fixed-size buffer that functions as a queue
 * (FIFO). */
typedef struct {
    volatile uint8_t *pucData;
    volatile uint32_t ulSize;
    volatile uint32_t ulReadIndex;
    volatile uint32_t ulWriteIndex;
} RingBuffer_t;

/* All ring buffer functions return the status of the buffer - either empty,
 * partially filled (OK), or full. */
typedef enum {
    BUFFER_OK,
    BUFFER_EMPTY,
    BUFFER_FULL
} RingBufferStatus_t;


extern RingBufferStatus_t eRingBufferStatus(volatile RingBuffer_t *pxBuffer);

extern RingBufferStatus_t eRingBufferRead(volatile RingBuffer_t *pxBuffer,
                                        uint8_t *pucByte);

extern RingBufferStatus_t eRingBufferReadN(volatile RingBuffer_t *pxBuffer,
                                        uint8_t *pucBytes, uint32_t ulNBytes);

extern RingBufferStatus_t eRingBufferWrite(volatile RingBuffer_t *pxBuffer,
                                         uint8_t ucByte);

extern RingBufferStatus_t eRingBufferWriteN(volatile RingBuffer_t *pxBuffer,
                                    uint8_t *pucBytes, uint32_t ulNBytes);

extern void vRingBufferClear(volatile RingBuffer_t *pxBuffer);

extern RingBufferStatus_t xRingBufferPeek(volatile RingBuffer_t *pxBuffer,
                                  uint8_t *ucByte);

#endif /* RING_BUFFER_H_ */
