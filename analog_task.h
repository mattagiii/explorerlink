/*
 * analog_task.h
 *
 *  Created on: Jun 9, 2018
 *      Author: Matt
 */


#ifndef __ANALOG_TASK_H__
#define __ANALOG_TASK_H__

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xAnalogTaskHandle;

extern uint32_t AnalogTaskInit(void);

#endif // __ANALOG_TASK_H__
