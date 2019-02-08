/*
 * remote_start_task.h
 * Handle to and public initialization function for Remote Start task, as well
 * as public struct typedef for status of the ignition system.
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
 * Ignition status flags.
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
