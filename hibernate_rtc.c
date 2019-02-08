/*
 * hibernate_rtc.c
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


#include <stdbool.h>
#include <stdint.h>
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "inc/hw_hibernate.h"
#include "inc/hw_types.h"

/*
 * Polls until the write complete (WRC) bit in the hibernate control register
 * is set.
 *
 * The Hibernation module provides an indication when any write is completed.
 * This mechanism is used to pace writes to the module.  This function merely
 * polls this bit and returns as soon as it is set.  At this point, it is safe
 * to perform another write to the module.
 */
void HibernateWriteComplete(void)
{
    while( !( HWREG( HIB_CTL ) & HIB_CTL_WRC ) ) {
    }
}

/*
 * Get the seconds value from the RTC.
 */
uint32_t HibernateRTCGetS(void) {
    uint32_t ulS;

    /* Disable interrupts */
    IntMasterDisable();

    /* Read HIB_RTCC */
    do {
        ulS = HWREG(HIB_RTCC);
    } while (ulS != HWREG(HIB_RTCC));

    /* Re-enable interrupts */
    IntMasterEnable();

    return ulS;
}

/*
 * Get the subseconds value from the RTC.
 */
uint32_t HibernateRTCGetSS(void) {
    uint32_t ulSS;

    /* Disable interrupts */
    IntMasterDisable();

    /* Read HIB_RTCSS */
    do {
        ulSS = HWREG(HIB_RTCSS);
    } while (ulSS != HWREG(HIB_RTCSS));

    /* Re-enable interrupts */
    IntMasterEnable();

    return ulSS & HIB_RTCSS_RTCSSC_M;
}

/*
 * Get the subseconds match value from the RTC.
 */
uint32_t HibernateRTCGetSSMatch(void) {
    uint32_t ulSSMatch;

    /* Disable interrupts */
    IntMasterDisable();

    /* Read HIB_RTCSS */
    do {
        ulSSMatch = HWREG(HIB_RTCSS);
    } while (ulSSMatch != HWREG(HIB_RTCSS));

    /* Re-enable interrupts */
    IntMasterEnable();

    return ulSSMatch >> HIB_RTCSS_RTCSSM_S;
}

/*
 * Get the seconds and subseconds value from the RTC. This is the only
 * way to guarantee an accurate pairing between the two.
 */
void HibernateRTCGetBoth(uint32_t *ulS, uint32_t *ulSS) {
    int ulRTC1, ulRTCSS1, ulRTCSS2, ulRTC2;

    /* Disable interrupts */
    IntMasterDisable();

    do {
        ulRTC1 = HWREG(HIB_RTCC);
        ulRTCSS1 = HWREG(HIB_RTCSS);
        ulRTCSS2 = HWREG(HIB_RTCSS);
        ulRTC2 = HWREG(HIB_RTCC);
    } while ((ulRTC1 != ulRTC2) || (ulRTCSS1 != ulRTCSS2));

    *ulS = ulRTC1;
    *ulSS = ulRTCSS1 & HIB_RTCSS_RTCSSC_M;

    /* Re-enable interrupts */
    IntMasterEnable();
}
