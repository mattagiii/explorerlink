/*
 * hibernate_rtc.h
 * Helper functions for the TM4C hibernate module. Specifically, these
 * functions serve as alternatives to TI driverlib functions that work around
 * silicon erratum HIB#02.
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

#ifndef HIBERNATE_RTC_H_
#define HIBERNATE_RTC_H_

void HibernateWriteComplete( void );
uint32_t HibernateRTCGetS( void );
uint32_t HibernateRTCGetSS( void );
uint32_t HibernateRTCGetSSMatch( void );
void HibernateRTCGetBoth( uint32_t *ulS, uint32_t *ulSS );

#endif /* HIBERNATE_RTC_H_ */
