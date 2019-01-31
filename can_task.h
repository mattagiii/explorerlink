/*
 * can_task.h
 *
 *  Created on: May 10, 2018
 *      Author: Matt
 */


#ifndef __CAN_TASK_H__
#define __CAN_TASK_H__

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xCANTaskHandle;

extern uint32_t CANTaskInit(void);

#endif // __CAN_TASK_H__
