/*
 * modem_mgmt_task.h
 * Handle to and public initialization function for Modem Management task, as
 * well as public modem power function declarations.
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


#ifndef __MODEM_MGMT_TASK_H__
#define __MODEM_MGMT_TASK_H__

#include "FreeRTOS.h"
#include "task.h"

#define MGMT_NOTIFY_NONE                0x00000000
#define MGMT_NOTIFY_HEARTBEAT           0x00000001
#define MGMT_NOTIFY_ALL                 0xffffffff

extern TaskHandle_t xModemMgmtTaskHandle;

extern uint32_t ModemMgmtTaskInit(void);
extern bool ModemPowerOn(void);
extern bool ModemPowerOff(void);
extern bool ModemReset(void);

#endif // __MODEM_MGMT_TASK_H__
