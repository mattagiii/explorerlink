/*
 * hibernate_rtc.c
 *
 *  Created on: May 26, 2018
 *      Author: Matt
 */


#include <stdbool.h>
#include <stdint.h>
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "inc/hw_hibernate.h"
#include "inc/hw_types.h"

/*
 * This function is a copy of the _HibernateWriteComplete() function in
 * driverlib/hibernate.h, written by TI.
 *
 * Polls until the write complete (WRC) bit in the hibernate control register
 * is set.
 *
 * The Hibernation module provides an indication when any write is completed.
 * This mechanism is used to pace writes to the module.  This function merely
 * polls this bit and returns as soon as it is set.  At this point, it is safe
 * to perform another write to the module.
 */
void
HibernateWriteComplete(void)
{
    /* Spin until the write complete bit is set. */
    while(!(HWREG(HIB_CTL) & HIB_CTL_WRC))
    {
    }
}

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
