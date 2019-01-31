/*
 * srf_task.h
 *
 *  Created on: Oct 7, 2018
 *      Author: Matt
 */

#ifndef SRF_TASK_H_
#define SRF_TASK_H_

#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xSRFTaskHandle;

extern uint32_t SRFTaskInit(void);

#endif /* SRF_TASK_H_ */
