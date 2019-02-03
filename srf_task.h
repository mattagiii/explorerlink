/*
 * srf_task.h
 * Handle to and public initialization function for SRF Ultrasonic task.
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

#ifndef SRF_TASK_H_
#define SRF_TASK_H_

#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t xSRFTaskHandle;

extern uint32_t SRFTaskInit(void);

#endif /* SRF_TASK_H_ */
