/*
 * sample.c
 *
 *  Created on: May 14, 2018
 *      Author: Matt
 */


#include <stdbool.h>
#include <stdint.h>
#include "channel.h"
#include "sample.h"
#include "ring_buffer.h"


#define ARRAY_LENGTH(x)                 (sizeof(x) / sizeof(x[0]))
/* The number of bytes used for sample metadata (rate, length, timestamp) */
#define SAMPLE_METADATA_BYTES           10


SampleRateBuffer_t *pxSampleRateBuffers[] = {
                         &xSampleBuffer1Hz,
                         &xSampleBuffer10Hz,
                         &xSampleBuffer100Hz
};

volatile uint8_t puc1HzData[SAMPLE_BUFFER_SIZE];
SampleRateBuffer_t xSampleBuffer1Hz = {
                    .xData = {
                        .pucData = puc1HzData,
                        .ulSize = SAMPLE_BUFFER_SIZE,
                        .ulReadIndex = 0,
                        .ulWriteIndex = 0
                    },
                    .usSampleRateHz = RATE_1HZ,
};

volatile uint8_t puc10HzData[SAMPLE_BUFFER_SIZE];
SampleRateBuffer_t xSampleBuffer10Hz = {
                    .xData = {
                        .pucData = puc10HzData,
                        .ulSize = SAMPLE_BUFFER_SIZE,
                        .ulReadIndex = 0,
                        .ulWriteIndex = 0
                    },
                    .usSampleRateHz = RATE_10HZ,
};

volatile uint8_t puc100HzData[SAMPLE_BUFFER_SIZE];
SampleRateBuffer_t xSampleBuffer100Hz = {
                    .xData = {
                        .pucData = puc100HzData,
                        .ulSize = SAMPLE_BUFFER_SIZE,
                        .ulReadIndex = 0,
                        .ulWriteIndex = 0
                    },
                    .usSampleRateHz = RATE_100HZ,
};

uint8_t ucSampleGetBufferCount(void) {
    return ARRAY_LENGTH(pxSampleRateBuffers);
}

/*
 * Checks the sample rate buffers and returns the period of the one with the
 * highest frequency, in milliseconds.
 */
float ulSampleGetMinPeriodMS(void) {
    uint8_t ucNumBuffers = ARRAY_LENGTH(pxSampleRateBuffers);
    uint32_t i;
    uint32_t ulLast = 0;
    uint32_t ulCur;

    for (i = 0; i < ucNumBuffers; i++) {
        ulCur = (uint32_t)(pxSampleRateBuffers[i]->usSampleRateHz);
        if (ulCur > ulLast) {
            ulLast = ulCur;
        }
    }
    return (uint32_t)(1000 / ulCur);
}

/*
 * Use the channel API to store the size of each sample with its buffer.
 */
void vInitSampleRateBuffers(void) {
    uint32_t ucNumBuffers = ARRAY_LENGTH(pxSampleRateBuffers);
    uint32_t i;

    for (i = 0; i < ucNumBuffers; i++) {
        pxSampleRateBuffers[i]->ulSampleSize = ulChannelGetByteCountForRate(
            (SampleRateHz_t)(pxSampleRateBuffers[i]->usSampleRateHz)) +
            SAMPLE_METADATA_BYTES;
    }
}
