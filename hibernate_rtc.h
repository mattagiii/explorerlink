/*
 * hibernate_rtc.h
 *
 *  Created on: May 26, 2018
 *      Author: Matt
 */

#ifndef HIBERNATE_RTC_H_
#define HIBERNATE_RTC_H_

void HibernateWriteComplete( void );
uint32_t HibernateRTCGetS( void );
uint32_t HibernateRTCGetSS( void );
uint32_t HibernateRTCGetSSMatch( void );
void HibernateRTCGetBoth( uint32_t *ulS, uint32_t *ulSS );

#endif /* HIBERNATE_RTC_H_ */
