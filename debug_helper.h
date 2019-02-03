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

extern volatile uint32_t ulRuntimeStatsCounter;
extern volatile uint32_t ulLastPortFValue;

void DebugHelperInit( void );
void vSetupTimerForRunTimeStats( void );

#endif // __TEST_HELPER_H__
