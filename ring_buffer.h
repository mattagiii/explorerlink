/*
 * ring_buffer.h
 *
 *  Created on: Apr 26, 2018
 *      Author: Matt
 */

#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

#include <stdbool.h>
#include <stdint.h>


typedef struct {
    volatile uint8_t *pucData;
    volatile uint32_t ulSize;
    volatile uint32_t ulReadIndex;
    volatile uint32_t ulWriteIndex;
} RingBuffer_t;

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
