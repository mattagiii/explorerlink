/*
 * modem_mgmt_task.h
 *
 *  Created on: Jun 26, 2018
 *      Author: Matt
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
