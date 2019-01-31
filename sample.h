/*
 * sample.h
 *
 *  Created on: May 14, 2018
 *      Author: Matt
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
