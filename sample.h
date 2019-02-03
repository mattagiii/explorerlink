/*
 * sample.h
 * Sample ring buffer typedefs and declarations.
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

#ifndef SAMPLE_H_
#define SAMPLE_H_


#include <stdbool.h>
#include <stdint.h>
#include "ring_buffer.h"
#include "FreeRTOS.h"
#include "semphr.h"


/* Size of every sample rate ring buffer in bytes. This size is meant to be
 * adequate for buffering sampled data until it is transmitted to the
 * server and may accommodate multiple samples. */
#define SAMPLE_BUFFER_SIZE              128


typedef enum {
    RATE_1HZ = 1,
    RATE_10HZ = 10,
    RATE_50HZ = 50,
    RATE_100HZ = 100,
    RATE_500HZ = 500,
    RATE_1000HZ = 1000
} SampleRateHz_t;

typedef struct {
    /* A ring buffer that can hold the most recently acquired
     * SAMPLE_BUFFER_SIZE bytes of data */
    volatile RingBuffer_t xData;
    /* The length of one sample in bytes, including the prepended frequency
     * (2 bytes), total length (2 bytes), and timestamp (6 bytes) */
    uint16_t ulSampleSize;
    /* The sample rate for this buffer */
    uint16_t usSampleRateHz;
} SampleRateBuffer_t;

extern SampleRateBuffer_t *pxSampleRateBuffers[];
extern SampleRateBuffer_t xSampleBuffer1Hz;
extern SampleRateBuffer_t xSampleBuffer10Hz;
extern SampleRateBuffer_t xSampleBuffer100Hz;

extern uint8_t ucSampleGetBufferCount(void);
extern float ulSampleGetMinPeriodMS(void);
extern void vInitSampleRateBuffers(void);


#endif /* SAMPLE_H_ */
