/*
 * debug_helper.h
 * Tools for debugging ExplorerLink.
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


#ifndef __DEBUG_HELPER_H__
#define __DEBUG_HELPER_H__


#include <stdbool.h>
#include <stdarg.h>
#include "inc/hw_gpio.h"
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "utils/uartstdio.h"

/* This section defines three macros that evaluate to nothing unless build
 * variable DEBUG is defined. If DEBUG is NOT defined, the macros are the only
 * things present when debug_helper.h is #included in another file. None of the
 * other debug code makes it past the preprocessor.
 *
 * Credit to Jonathan Leffler (https://stackoverflow.com/a/1644898) for this
 * robust method of including debug code based on a build variable (DEBUG). */
#ifdef DEBUG
#define DEBUG_TEST                      1
#define LAST_PORT_F_VALUE               ulLastPortFValue
#define debug_init()                    do { DebugHelperInit(); } while (0)
#else
#define DEBUG_TEST                      0
#define LAST_PORT_F_VALUE               0
#define debug_init()                    do { } while (0)
#endif /* DEBUG */
/* Wrapper for uartstdio.h's implementation of printf */
#define debug_print( ... ) \
            do { if ( DEBUG_TEST ) UARTprintf( __VA_ARGS__ ); } while (0)
/* Wrapper for GPIOPinWrite that fills in port F and selects all pins to be
 * updated. Note that bits 5-7 are don't care because the TM4C123GH6PMI only
 * has PF0-4. */
#define debug_set_bus( busValue ) \
            do { if ( DEBUG_TEST ) GPIOPinWrite( GPIO_PORTF_BASE, UINT8_MAX, \
                                                 busValue ); } while (0)

#define NUM_TASKS                       8


#ifdef DEBUG
extern volatile uint32_t ulRuntimeStatsCounter;
extern volatile uint32_t ulLastPortFValue;

void DebugHelperInit( void );
void vSetupTimerForRunTimeStats( void );
void vPrintStackWatermarks( void );
void vGetTaskRunTimes( void );
#endif /* DEBUG */

#endif /* __DEBUG_HELPER_H__ */
