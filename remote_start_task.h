/*
 * remote_start_task.h
 *
 *  Created on: Jun 9, 2018
 *      Author: Matt
 */


#ifndef __REMOTE_START_TASK_H__
#define __REMOTE_START_TASK_H__

#include "FreeRTOS.h"
#include "task.h"

#define RS_NOTIFY_NONE                  0x00000000
#define RS_NOTIFY_IGNITION_ON           0x00000001
#define RS_NOTIFY_IGNITION_OFF          0x00000002
#define RS_NOTIFY_START                 0x00000004
#define RS_NOTIFY_NO_CLIENT             0x00000008
#define RS_NOTIFY_CLIENT                0x00000010
#define RS_NOTIFY_CHECK_PASS            0x00000020
#define RS_NOTIFY_ERROR                 0x80000000
#define RS_NOTIFY_ALL                   0xffffffff


/*
 * Ignition status flags. C99 compilation should fit these into a single byte.
 */
typedef struct IgnitionStatus_t {
    /* Whether the last attempt to turn on the ignition failed. */
    bool lastOnFailed : 1;
    /* Whether the last attempt to turn off the ignition failed. */
    bool lastOffFailed : 1;
    /* Whether the last attempt to start the engine failed. */
    bool lastStartFailed : 1;
    /* Whether the ignition is running (RUN signal asserted). */
    bool running : 1;
} IgnitionStatus_t;

extern TaskHandle_t xRemoteStartTaskHandle;
extern IgnitionStatus_t xIgnitionStatus;

extern uint32_t RemoteStartTaskInit(void);

#endif // __REMOTE_START_TASK_H__
