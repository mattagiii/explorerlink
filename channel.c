/*
 * channel.c
 * Channel definitions and channel function definitions.
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
#include "driverlib/gpio.h"
#include "inc/hw_memmap.h"
#include "utils/uartstdio.h"
#include "channel.h"
#include "ring_buffer.h"
#include "sample.h"
#include "task.h"


#define ARRAY_LENGTH(x)                 (sizeof(x) / sizeof(x[0]))



volatile Channel_t chAVTEMP1Raw = { .ucByteCount = sizeof(uint32_t),
                              .usSampleRateHz = RATE_1HZ
};

volatile Channel_t chAVTEMP2Raw = { .ucByteCount = sizeof(uint32_t),
                              .usSampleRateHz = RATE_1HZ
};

volatile Channel_t chAVTEMP3Raw = { .ucByteCount = sizeof(uint32_t),
                              .usSampleRateHz = RATE_1HZ
};

volatile Channel_t chAVTEMP4Raw = { .ucByteCount = sizeof(uint32_t),
                              .usSampleRateHz = RATE_1HZ
};

volatile Channel_t chCabinTemp = { .ucByteCount = sizeof(uint32_t),
                              .usSampleRateHz = RATE_1HZ
};

volatile Channel_t chCoolantTemp = { .ucByteCount = sizeof(uint8_t),
                              .usSampleRateHz = RATE_1HZ,
                              .usCANID = 0x420,
                              .ucOffset = 0
};

volatile Channel_t chDeviceBatt = { .ucByteCount = sizeof(uint16_t),
                              .usSampleRateHz = RATE_1HZ
};

volatile Channel_t chFuelLevelMean = { .ucByteCount = sizeof(uint8_t),
                              .usSampleRateHz = RATE_1HZ,
                              .usCANID = 0x430,
                              .ucOffset = 0
};

volatile Channel_t chGearPosition = { .ucByteCount = sizeof(uint8_t),
                              .usSampleRateHz = RATE_1HZ,
                              .usCANID = 0x230,
                              .ucOffset = 0
};

volatile Channel_t chAVGP2Raw = { .ucByteCount = sizeof(uint32_t),
                              .usSampleRateHz = RATE_10HZ
};

volatile Channel_t chDeviceCurrent = { .ucByteCount = sizeof(uint32_t),
                              .usSampleRateHz = RATE_10HZ
};

volatile Channel_t chFuelLevelInst = { .ucByteCount = sizeof(uint8_t),
                              .usSampleRateHz = RATE_10HZ,
                              .usCANID = 0x430,
                              .ucOffset = 2
};

volatile Channel_t chNotifications = { .ucByteCount = sizeof(uint32_t),
                              .usSampleRateHz = RATE_10HZ
};

volatile Channel_t chRPM = { .ucByteCount = sizeof(uint16_t),
                              .usSampleRateHz = RATE_10HZ,
                              .usCANID = 0x201,
                              .ucOffset = 0,
                              .bReverse = true
};

volatile Channel_t chSpeed = { .ucByteCount = sizeof(uint16_t),
                              .usSampleRateHz = RATE_10HZ,
                              .usCANID = 0x201,
                              .ucOffset = 4,
                              .bReverse = true
};

volatile Channel_t chTempKnob = { .ucByteCount = sizeof(uint32_t),
                              .usSampleRateHz = RATE_10HZ
};

volatile Channel_t chTempKnobRaw = { .ucByteCount = sizeof(uint32_t),
                              .usSampleRateHz = RATE_10HZ
};

volatile Channel_t chTestDist0 = { .ucByteCount = sizeof(uint32_t),
                              .usSampleRateHz = RATE_10HZ
};

volatile Channel_t chTestDist1 = { .ucByteCount = sizeof(uint32_t),
                              .usSampleRateHz = RATE_10HZ
};

volatile Channel_t chThrottlePosition = { .ucByteCount = sizeof(uint8_t),
                              .usSampleRateHz = RATE_10HZ,
                              .usCANID = 0x201,
                              .ucOffset = 6
};

volatile Channel_t chThrottlePositionROC = { .ucByteCount = sizeof(uint8_t),
                              .usSampleRateHz = RATE_10HZ,
                              .usCANID = 0x201,
                              .ucOffset = 7
};

volatile Channel_t chVehicleBatt = { .ucByteCount = sizeof(uint32_t),
                              .usSampleRateHz = RATE_10HZ
};

volatile Channel_t chWheelSpeedFL = { .ucByteCount = sizeof(uint16_t),
                              .usSampleRateHz = RATE_10HZ,
                              .usCANID = 0x4B0,
                              .ucOffset = 0,
                              .bReverse = true
};

volatile Channel_t chWheelSpeedFR = { .ucByteCount = sizeof(uint16_t),
                              .usSampleRateHz = RATE_10HZ,
                              .usCANID = 0x4B0,
                              .ucOffset = 2,
                              .bReverse = true
};

volatile Channel_t chWheelSpeedRL = { .ucByteCount = sizeof(uint16_t),
                              .usSampleRateHz = RATE_10HZ,
                              .usCANID = 0x4B0,
                              .ucOffset = 4,
                              .bReverse = true
};

volatile Channel_t chWheelSpeedRR = { .ucByteCount = sizeof(uint16_t),
                              .usSampleRateHz = RATE_10HZ,
                              .usCANID = 0x4B0,
                              .ucOffset = 6,
                              .bReverse = true
};

/* An array of pointers to each channel, allowing for iteration. The order of
 * these pointers defines the order that the channels values are sampled and
 * transmitted. */
volatile Channel_t *xChannels[] = {
                         &chAVTEMP1Raw,
                         &chAVTEMP2Raw,
                         &chAVTEMP3Raw,
                         &chAVTEMP4Raw,
                         &chCabinTemp,
                         &chCoolantTemp,
                         &chDeviceBatt,
                         &chFuelLevelMean,
                         &chGearPosition,
                         &chAVGP2Raw,
                         &chDeviceCurrent,
                         &chFuelLevelInst,
                         &chNotifications,
                         &chRPM,
                         &chSpeed,
                         &chTempKnob,
                         &chTempKnobRaw,
                         &chTestDist0,
                         &chTestDist1,
                         &chThrottlePosition,
                         &chThrottlePositionROC,
                         &chVehicleBatt,
                         &chWheelSpeedFL,
                         &chWheelSpeedFR,
                         &chWheelSpeedRL,
                         &chWheelSpeedRR
};


/*
 * Counts the number of bytes of channel data for a given sample rate. Data is
 * transmitted in sequences that group all channels with a given rate, and this
 * count is used to calculate the length of the sequence.
 */
uint32_t ulChannelGetByteCountForRate(SampleRateHz_t freq) {
    uint8_t ucChannelCount = ARRAY_LENGTH(xChannels);
    uint32_t i;
    uint32_t byteCount = 0;

    for (i = 0; i < ucChannelCount; i++) {
        if (xChannels[i]->usSampleRateHz == freq) {
            byteCount += xChannels[i]->ucByteCount;
        }
    }

    return byteCount;
}

/*
 * This function iterates through all channels, writing their current values to
 * the ring buffer of the passed SampleRateBuffer_t if they match the buffer's
 * sample rate. The buffer should have already been written with the sample
 * frequency as described in sample.h, and this call should occur within a
 * critical section so that a complete sample snapshot is always written.
 */
void vChannelSample(SampleRateBuffer_t *pxBuffer) {
    uint32_t ucChannelCount = ARRAY_LENGTH(xChannels);
    uint32_t i;

    for (i = 0; i < ucChannelCount; i++) {
        if (xChannels[i]->usSampleRateHz == pxBuffer->usSampleRateHz) {
            eRingBufferWriteN(&(pxBuffer->xData),
                              (uint8_t *)(xChannels[i]->xData),
                              xChannels[i]->ucByteCount);
        }
    }
}

/*
 * Allocate memory for all channels' data. This function will only be called
 * once, and the memory is needed until the device resets, so it is never freed.
 * This could be accomplished with plain static allocation and is only done for
 * convenience while the number and size of channels is in flux.
 */
void vChannelInit(void) {
    uint32_t ucChannelCount = ARRAY_LENGTH(xChannels);
    uint32_t i;

    for (i = 0; i < ucChannelCount; i++) {
        xChannels[i]->xData = pvPortMalloc(xChannels[i]->ucByteCount);
    }
}

/*
 * Get a 32-bit channel's current value.
 */
uint32_t ulChannelValueGet( volatile Channel_t *pxCh ) {

    configASSERT( pxCh->ucByteCount == sizeof( uint32_t ) );

    return *( ( uint32_t * )pxCh->xData );
}

/*
 * Get a 16-bit channel's current value.
 */
uint16_t usChannelValueGet( volatile Channel_t *pxCh ) {

    configASSERT( pxCh->ucByteCount == sizeof( uint16_t ) );

    return *( ( uint16_t * )pxCh->xData );
}

/*
 * Get an 8-bit channel's current value.
 */
uint8_t ucChannelValueGet( volatile Channel_t *pxCh ) {

    configASSERT( pxCh->ucByteCount == sizeof( uint8_t ) );

    return *( ( uint8_t * )pxCh->xData );
}

/*
 * Store a new value into the channel referenced by pointer pxCh. Again, the
 * pointer/memcpy approach here is really just for convenience working with
 * arbitrary channels during development. In a production system the channels
 * would just be statically allocated and not require byte-by-byte handling.
 */
void vChannelStore(volatile Channel_t *pxCh, void *pucNewValue) {
    memcpy((void *)(pxCh->xData), pucNewValue, pxCh->ucByteCount);
}

/*
 * A notification channel is a 32-bit channel whose bits represent notification
 * flags. The purpose of these notifications is to alert the server of
 * something in a compact form, avoiding the need for individual channels and
 * allowing 32 notifications per channel to operate independently. Some bits
 * serve as status indicators while others are true notifications that require
 * a response from the server to confirm and clear.
 */
void vNotificationChannelSet(volatile Channel_t *pxCh, uint32_t ulBitsToSet) {

    if (pxCh->ucByteCount == sizeof(uint32_t)) {
        *(uint32_t *)(pxCh->xData) |= ulBitsToSet;
    }
    else {
        /* The channel is of incorrect size. */
    }
}

void vNotificationChannelClear(volatile Channel_t *pxCh, uint32_t ulBitsToClear) {

    if (pxCh->ucByteCount == sizeof(uint32_t)) {
        *(uint32_t *)(pxCh->xData) &= !ulBitsToClear;
    }
    else {
        /* The channel is of incorrect size. */
    }
}

/*
 * Store the data from a single CAN message in the applicable channels. Each
 * channel with an ID that matches the message's ID is updated.
 */
void vChannelStoreCANData(uint32_t ulMsgID, uint8_t *pui8MsgData) {
    uint32_t ucChannelCount = ARRAY_LENGTH(xChannels);
    uint32_t i, j;

    for (i = 0; i < ucChannelCount; i++) {
        if (xChannels[i]->usCANID == ulMsgID) {
            if (xChannels[i]->bReverse) {
                for (j = 0; j < xChannels[i]->ucByteCount; j++) {
                    memcpy((void *)(xChannels[i]->xData + j), pui8MsgData + xChannels[i]->ucOffset + xChannels[i]->ucByteCount - j - 1, 1);
                }
            }
            else {
                memcpy((void *)(xChannels[i]->xData), pui8MsgData + xChannels[i]->ucOffset, xChannels[i]->ucByteCount);
            }
        }
    }
}


