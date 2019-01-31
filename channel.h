/**
 * @file channel.h
 * \brief Brief description is on this line and also brief description extends to the
 * next line
 *
 * this should be the detailed description I think
 *
 */

#ifndef CHANNEL_H_
#define CHANNEL_H_


#include <stdbool.h>
#include <stdint.h>
#include "ring_buffer.h"
#include "sample.h"
#include "FreeRTOS.h"
#include "semphr.h"


#define NT_RS_READY                     0x00000001


typedef enum {
    RATE_1HZ = 1,
    RATE_10HZ = 10,
    RATE_50HZ = 50,
    RATE_100HZ = 100,
    RATE_500HZ = 500,
    RATE_1000HZ = 1000
} SampleRateHz_t;

/* The Channel_t struct represents a measured value from a sensor, CAN bus, or
 * internal/onboard source. The latest value is stored (generally updated by a
 * specific task) along with various channel metadata. */
typedef struct {
    /* The latest data value for this channel */
    volatile uint8_t *xData;
    /* Number of bytes for the channel value */
    uint8_t ucByteCount;
    /* CAN ID for received CAN messages containing this channel (if
     * applicable) */
    uint16_t usCANID;
    /* Number of bytes from start of the CAN frame to this channel's first
     * byte (if applicable) */
    uint8_t ucOffset;
    /* Whether the bytes arrive reversed on the CAN bus */
    bool bReverse;
    /* Sample rate for this channel in Hz */
    SampleRateHz_t usSampleRateHz;
} Channel_t;

/* Channel declarations. These are global to the program as they are relevant
 * to many different tasks. */
extern volatile Channel_t chAVTEMP1Raw;
extern volatile Channel_t chAVTEMP2Raw;
extern volatile Channel_t chAVTEMP3Raw;
extern volatile Channel_t chAVTEMP4Raw;
extern volatile Channel_t chCabinTemp;
extern volatile Channel_t chCoolantTemp;
extern volatile Channel_t chDeviceBatt;
extern volatile Channel_t chFuelLevelMean;
extern volatile Channel_t chGearPosition;
extern volatile Channel_t chAVGP2Raw;
extern volatile Channel_t chDeviceCurrent;
extern volatile Channel_t chFuelLevelInst;
extern volatile Channel_t chNotifications;
extern volatile Channel_t chRPM;
extern volatile Channel_t chSpeed;
extern volatile Channel_t chTempKnob;
extern volatile Channel_t chTempKnobRaw;
extern volatile Channel_t chTestDist0;
extern volatile Channel_t chTestDist1;
extern volatile Channel_t chThrottlePosition;
extern volatile Channel_t chThrottlePositionROC;
extern volatile Channel_t chVehicleBatt;
extern volatile Channel_t chWheelSpeedFL;
extern volatile Channel_t chWheelSpeedFR;
extern volatile Channel_t chWheelSpeedRL;
extern volatile Channel_t chWheelSpeedRR;

uint32_t ulChannelGetByteCountForRate(SampleRateHz_t freq);
void vChannelSample(SampleRateBuffer_t *pxBuffer);
void vChannelInit(void);
void vChannelStore(volatile Channel_t *pxCh, void *pucNewValue);
uint32_t ulChannelValueGet( volatile Channel_t *pxCh );
uint16_t usChannelValueGet( volatile Channel_t *pxCh );
uint8_t ucChannelValueGet( volatile Channel_t *pxCh );
void vNotificationChannelSet(volatile Channel_t *pxCh, uint32_t ulBitsToSet);
void vNotificationChannelClear(volatile Channel_t *pxCh, uint32_t ulBitsToClear);
void vChannelStoreCANData(uint32_t ulMsgID, uint8_t *pui8MsgData);

#endif /* CHANNEL_H_ */
