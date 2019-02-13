/*
 * priorities.h
 * This file contains all task and ISR priority defines, grouped into this
 * single file for easy comparison and adjustment.
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


#ifndef __PRIORITIES_H__
#define __PRIORITIES_H__


/* The priorities of the various tasks. Higher numbers mean higher priorities.
 * The accepted values are in the range [0, configMAX_PRIORITIES-1]. As
 * configured, this is [0, 15]. */
#define PRIORITY_MODEM_UART_TASK        1
#define PRIORITY_CAN_TASK               4
#define PRIORITY_DATA_TASK              2
#define PRIORITY_ANALOG_TASK            1
#define PRIORITY_MODEM_MGMT_TASK        2
#define PRIORITY_JSN_TASK               2
#define PRIORITY_SRF_TASK               2
#define PRIORITY_REMOTE_START_TASK      3

/* Priorities for interrupts whose ISRs contain FreeRTOS API calls. These must
 * be >= configMAX_SYSCALL_INTERRUPT_PRIORITY. These interrupts will be
 * maskable by the kernel. 0 is the highest priority (0-7). Other interrupt
 * priorities (with ISRs not containing API calls) are left at their
 * defaults. */
#define PRIORITY_MODEM_UART_INT         7
#define PRIORITY_SRF_UART_INT           7
#define PRIORITY_DATA_SAMPLING_INT      7
#define PRIORITY_CAN0_INT               6
#define PRIORITY_IGNITION_TIMER_INT     5


#endif /* __PRIORITIES_H__ */
