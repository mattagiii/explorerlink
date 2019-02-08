/*
 * FreeRTOSConfig.h
 * Modified configuration header file defining constants and macros for use by
 * the FreeRTOS kernel in the ExplorerLink software. Modifications to the
 * original version of this file (as distributed by Real Time Engineers Ltd)
 * include the values of all constants and macros, and the comments, within the
 * "Application specific definitions" section below.
 *
 * Modifications copyright 2018, 2019 Matt Rounds
 *
 * This file is part of ExplorerLink, which is licensed under the GNU General
 * Public License, version 3. However it was originally distributed as part of
 * FreeRTOS, which is licensed under the GNU General Public License, version 2,
 * with a linking exception.
 *
 * The original copyright notice is therefore retained below, and the original
 * license, as distributed by Real Time Engineers Ltd with FreeRTOS, is
 * distributed with ExplorerLink in the Licenses directory.
 */


/*
    FreeRTOS V7.0.2 - Copyright (C) 2011 Real Time Engineers Ltd.


    ***************************************************************************
     *                                                                       *
     *    FreeRTOS tutorial books are available in pdf and paperback.        *
     *    Complete, revised, and edited pdf reference manuals are also       *
     *    available.                                                         *
     *                                                                       *
     *    Purchasing FreeRTOS documentation will not only help you, by       *
     *    ensuring you get running as quickly as possible and with an        *
     *    in-depth knowledge of how to use FreeRTOS, it will also help       *
     *    the FreeRTOS project to continue with its mission of providing     *
     *    professional grade, cross platform, de facto standard solutions    *
     *    for microcontrollers - completely free of charge!                  *
     *                                                                       *
     *    >>> See http://www.FreeRTOS.org/Documentation for details. <<<     *
     *                                                                       *
     *    Thank you for using FreeRTOS, and thank you for your support!      *
     *                                                                       *
    ***************************************************************************


    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
    >>>NOTE<<< The modification to the GPL is included to allow you to
    distribute a combined work that includes FreeRTOS without being obliged to
    provide the source code for proprietary components outside of the FreeRTOS
    kernel.  FreeRTOS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public
    License and the FreeRTOS license exception along with FreeRTOS; if not it
    can be viewed here: http://www.freertos.org/a00114.html and also obtained
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!

    http://www.FreeRTOS.org - Documentation, latest information, license and
    contact details.

    http://www.SafeRTOS.com - A version that is certified for use in safety
    critical systems.

    http://www.OpenRTOS.com - Commercial support, development, porting,
    licensing and training services.
*/

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE.
 *
 * See http://www.freertos.org/a00110.html.
 *----------------------------------------------------------*/

#define configUSE_PREEMPTION                1
//#define configUSE_TIME_SLICING              1
#define configUSE_IDLE_HOOK                 0
#define configUSE_TICK_HOOK                 0
#define configCPU_CLOCK_HZ                  ( ( unsigned long ) 80000000 )
#define configTICK_RATE_HZ                  ( ( portTickType ) 1000 )
#define configMINIMAL_STACK_SIZE            ( ( unsigned short ) 200 )
/* Heap size in bytes; 20,000 of 32,768 available bytes in SRAM */
#define configTOTAL_HEAP_SIZE               ( ( size_t ) ( 20000 ) )
#define configMAX_TASK_NAME_LEN             ( 12 )
#define configUSE_TRACE_FACILITY            1
#define configUSE_16_BIT_TICKS              0
#define configIDLE_SHOULD_YIELD             0
#define configUSE_CO_ROUTINES               0
#define configUSE_MUTEXES                   1
#define configUSE_RECURSIVE_MUTEXES         1
#define configCHECK_FOR_STACK_OVERFLOW      2
#define configUSE_TASK_NOTIFICATIONS        1

#define configMAX_PRIORITIES                16
#define configMAX_CO_ROUTINE_PRIORITIES     ( 2 )
#define configQUEUE_REGISTRY_SIZE           10

#ifdef DEBUG
/* Run time and task stats gathering related definitions */
#define configGENERATE_RUN_TIME_STATS           1
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1
#define configUSE_APPLICATION_TASK_TAG          1
#endif /* DEBUG */

/* Define to trap errors */
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */
#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskCleanUpResources       0
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_uxTaskGetStackHighWaterMark 1

/* Info at: http://www.freertos.org/a00110.html#kernel_priority */
/* The priority for the FreeRTOS kernel interrupt. This must be the lowest
 * priority so that ISRs can preempt the kernel. */
#define configKERNEL_INTERRUPT_PRIORITY         ( 7 << 5 )
/* The highest priority allowable for interrupts whose ISRs contain FreeRTOS
 * API calls.
 *
 * When the FreeRTOS kernel enters a critical section, it needs to ensure that
 * no API calls can be made that could corrupt kernel data. ISRs that contain
 * API calls could cause this to occur, so the kernel needs a way to ensure
 * that these interrupts are masked. configMAX_SYSCALL_INTERRUPT_PRIORITY
 * introduces a threshold value for interrupt priorities, at or below which
 * the kernel CAN mask interrupts during a critical section. Thus, by trading
 * away a bit of response time, ISRs are able to make use of API functions.
 * Interrupts whose ISRs do not need API functions and can't afford the latency
 * penalty should be given priorities above this value so that they will
 * operate with no RTOS interference.
 *
 * FreeRTOS maintains a separate interrupt-safe API to ensure interrupt entry
 * is as fast and as simple as possible. ISRs must only utilize these API
 * calls, which end with "FromISR". */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( 5 << 5 )


#ifdef DEBUG
/* Contains vSetupTimerForRunTimeStats, ulRuntimeStatsCounter, and
 * ulLastPortFValue */
#include "debug_helper.h"

/* These defines enable runtime statistic gathering for the kernel.
 * vSetupTimerForRunTimeStats and ulRuntimeStatsCounter are defined in
 * test_helper.h. */
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() vSetupTimerForRunTimeStats()
#define portGET_RUN_TIME_COUNTER_VALUE()    ulRuntimeStatsCounter

/* Define the traceTASK_SWITCHED_IN() macro to output the GPIO bus value
 * associated with the task being selected to run on port F. This also
 * updates the current task tag in ulLastPortFValue so that ISRs can place
 * their own unique values on the bus and return the value to the correct
 * (task) value when returning (back to the task the ISR interrupted). */
#define traceTASK_SWITCHED_IN()                                                \
    ulLastPortFValue = ( uint32_t ) pxCurrentTCB->pxTaskTag;                   \
    debug_set_bus( ( uint32_t ) pxCurrentTCB->pxTaskTag );
/* traceTASK_SWITCHED_OUT() returns the bus value to 0, which represents CPU
 * idle. */
#define traceTASK_SWITCHED_OUT()                                               \
    ulLastPortFValue = 0;                                                      \
    debug_set_bus( 0 );
#endif /* DEBUG */


#endif /* FREERTOS_CONFIG_H */
