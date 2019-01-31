/*
 * data_task.h
 *
 *  Created on: May 10, 2018
 *      Author: Matt
 */


#ifndef __DATA_TASK_H__
#define __DATA_TASK_H__

#include "FreeRTOS.h"

extern TaskHandle_t xDataTaskHandle;

extern uint32_t DataTaskInit(void);

#endif // __DATA_TASK_H__
