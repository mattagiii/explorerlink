/*
 * data_task.c
 * A somewhat dispensable task for monitoring data sampling, and an
 * indispensable ISR that performs data sampling.
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


#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h> /* Needed for hibernate.h. */
#include "inc/hw_ints.h"
#include "inc/hw_hibernate.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/hibernate.h"
#include "driverlib/interrupt.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "utils/uartstdio.h"
#include "channel.h"
#include "debug_helper.h"
#include "hibernate_rtc.h"
#include "modem_uart_task.h"
#include "priorities.h"
#include "sample.h"
#include "stack_sizes.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "srf_task.h"
#include "jsn_task.h"
#include "modem_mgmt_task.h"
#include "analog_task.h"
#include "can_task.h"
#include "remote_start_task.h"


TaskHandle_t xDataTaskHandle;

uint32_t debugCount = 0;


/*
 * The hibernate module's real-time clock (RTC) is used to sample data at
 * regular intervals (defined in channel.c). This ISR is triggered whenever the
 * RTC reaches its next match value. The initial match is set in modem_uart.c
 * when the RTC is synchronized with network time. Thereafter, this ISR updates
 * the match value on every call so that it triggers indefinitely. The interval
 * used is the fastest interval (sample rate) defined among the list of data
 * channels. Channels are only sampled on intervals corresponding to their
 * sample rates. Floating-point operations are used to provide a combination
 * of temporal accuracy and flexibility in defining sample rates when new
 * channels are added, but these could be replaced with match and sample lookup
 * tables for better efficiency.
 */
void
HibernateIntHandler(void) {
    /* The hibernate interrupt status */
    uint32_t ulStatus;
    /* The current seconds match value for the RTC */
    uint32_t ulMatchS;
    /* The current subseconds match value for the RTC */
    uint32_t ulMatchSS;
    /* The next subseconds match value for the RTC, i.e. floor(fNextMatchSS) */
    uint32_t ulNextMatchSS;
    /* The minimum sampling period in seconds (e.g. if the fastest channel is
     * 100Hz, ulMinPeriodMS = 10). Also the increment for ulCurrentMS */
    static uint32_t ulMinPeriodMS = 0;
    /* The fractional part of the current match subseconds */
    static uint32_t ulCurrentMS = 0;
    /* The value to increment the RTC match by. This is a count of 1/32768ths
     * of a second. */
    static float fIncrementSS = 0.0;
    /* The next subseconds match value for the RTC (ulMatchSSNext is used) */
    static float fNextMatchSS = 0.0;
    uint16_t usSampleRateHz;
    uint32_t i;
    UBaseType_t uxSavedInterruptStatus;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    GPIOPinWrite( GPIO_PORTF_BASE, UINT32_MAX, 6);

    /* Ensure write completion before accessing hibernate registers. */
    HibernateWriteComplete();

    /* Read and clear the (masked) interrupt status. */
    ulStatus = HibernateIntStatus(true);


    /* Equivalent to HibernateIntClear(ulStatus). */
    HWREG(HIB_IC) |= ulStatus;


    /* Verify that the interrupt was the RTC match interrupt. */
    if (ulStatus == HIBERNATE_INT_RTC_MATCH_0) {
        
        /* Set these only on the first interrupt. */
        if (!fIncrementSS) {
            ulMinPeriodMS = ulSampleGetMinPeriodMS();
            fIncrementSS = 32.768 * (float)ulMinPeriodMS;
        }

        /* Wait for write completion after HWREG(HIB_IC) |= ulStatus */
        HibernateWriteComplete();

        /* Get the current match values (the exact sample time that triggered
         * this interrupt). */
        ulMatchS = HibernateRTCMatchGet(0);
        ulMatchSS = HibernateRTCGetSSMatch();

        /* Iterate through the sample buffers, only sampling for them if 
         * needed. */
        for (i = 0; i < ucSampleGetBufferCount(); i++) {
            usSampleRateHz = pxSampleRateBuffers[i]->usSampleRateHz;

            /* Only sample this buffer if the current time is divisible by the
             * buffer's sample frequency. */
            if ( !( ulCurrentMS % (1000/usSampleRateHz) ) ) {
                /* Ensure an uninterrupted write to the buffer. No other part
                 * of the application will ever write to a sample buffer, but
                 * this is needed to ensure that the buffer can't be read when
                 * a sample has been only partially written. */
                uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();

                /* Write the frequency to the buffer (2 bytes). */
                eRingBufferWriteN(&(pxSampleRateBuffers[i]->xData),
                                  (uint8_t *)(&usSampleRateHz),
                                  sizeof(usSampleRateHz));

                /* Write the sample byte count to the buffer (2 bytes). */
                eRingBufferWriteN(&(pxSampleRateBuffers[i]->xData),
                                  (uint8_t *)(&(pxSampleRateBuffers[i]->ulSampleSize)),
                                  sizeof(pxSampleRateBuffers[i]->ulSampleSize));

                /* Write the timestamp to the buffer (6 bytes). */
                eRingBufferWriteN(&(pxSampleRateBuffers[i]->xData),
                                  (uint8_t *)(&ulMatchS), sizeof(ulMatchS));
                eRingBufferWriteN(&(pxSampleRateBuffers[i]->xData),
                                  (uint8_t *)(&ulMatchSS), sizeof(uint16_t));

                /* Sample the channel values themselves. */
                vChannelSample(pxSampleRateBuffers[i]);

                taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);

            } /* if (!(ulMatchSS % (uint32_t)(32768 / usSampleRateHz))) */
        }

        /* Set up the next match. The match subseconds value is computed and
         * stored as a float so that it does not appreciably lose accuracy. */
        fNextMatchSS = fNextMatchSS + fIncrementSS;
        ulNextMatchSS = (uint32_t)fNextMatchSS;
        /* Check if we are within one increment of the next second and update
         * the RTC match values accordingly. The fractional part of the
         * subseconds is also updated because it will be needed on the next
         * interrupt to determine which sample buffers to sample. */
        if ( ulNextMatchSS > 32768 - (uint32_t)fIncrementSS ) {
            ulCurrentMS = 0;
            fNextMatchSS = 0.0;
            HibernateRTCMatchSet(0, ++ulMatchS);
            HibernateRTCSSMatchSet(0, 0);
        }
        else {
            ulCurrentMS += ulMinPeriodMS;
            HibernateRTCSSMatchSet(0, ulNextMatchSS);
        }

        /* Set the MODEM_NOTIFY_SAMPLE bit. */
        xTaskNotifyFromISR(xModemUARTTaskHandle, MODEM_NOTIFY_SAMPLE, eSetBits,
                           &xHigherPriorityTaskWoken);

    } /* if (ulStatus == HIBERNATE_INT_RTC_MATCH_0) */
    else {
        /* No other interrupts should occur, so this should never be reached. */
        UARTprintf("unexpected HIB interrupt: %d\n", ulStatus);
    }

    GPIOPinWrite( GPIO_PORTF_BASE, UINT32_MAX, ulLastPortFValue );

    /* If the notification brought the Modem UART task to the ready state,
     * xHigherPriorityTaskWoken will be set to pdTRUE and this call will tell
     * the scheduler to switch context to the Modem UART task. */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*
 * This task doesn't do a lot in its current state; it merely verifies that
 * sampling is always occurring, which is marginally useful outside the context
 * of debugging. A future implementation might optimize the hibernate ISR by
 * deferring some of the sampling work to this task, but currently it isn't
 * known if the memory penalty that would cause is worth the speed advantage
 * for the ISR.
 */
static void DataTask(void *pvParameters) {
    uint32_t ulS;
    uint32_t ulMatchS;

    // Debug definitions
//    UBaseType_t uxDataTaskWatermark;
    volatile UBaseType_t uxArraySize;
    TaskStatus_t pxTaskStatusArray[ 8 ];
    uint32_t ulTotalRunTime;

    /* Main task loop. The hibernate interrupt does all the sampling work, so
     * the task's only job is to periodically check that sampling is still
     * running. */
    while (1) {

        /* This if statement acts as a "watchdog" for the RTC sampling
         * interrupts. If the program ever hangs and the interrupt fails to
         * trigger, this will reset the match to the next second. This only
         * needs to happen if the RTC interrupt is enabled in the first place,
         * so we verify that here. */
        if (HWREG(HIB_IM) & HIBERNATE_INT_RTC_MATCH_0) {

            /* Disable hibernate interrupts at the NVIC to prevent a normal
             * match interrupt from occurring while this check is running.
             * Normally it would be suitable to disable the RTC match interrupt
             * only, but because the hibernate module is in a separate clock
             * domain (32768Hz), its calls to HibernateIntDisable() and
             * HibernateIntEnable() take a long time as they must wait for
             * register writes to complete (up to ~100us). IntDisable() is much
             * faster and equally effective for this purpose. */
            IntDisable(INT_HIBERNATE);
            ulMatchS = HibernateRTCMatchGet(0);
            ulS = HibernateRTCGetS();

            if (ulS > ulMatchS) {

                UARTprintf("RTC interrupts fell out of sync.\n");
                UARTprintf("adjusting match: %d to %d\n", ulMatchS, ulS + 2);

                /* The hibernate module is a bit buggy on the TM4C (see the
                 * errata document), so to be extra safe we disable the RTC
                 * while reloading the matches. This will rarely need to occur
                 * anyway, so the time penalty in waiting for hibernate
                 * register writes is inconsequential.  */
                HibernateRTCDisable();
                HibernateRTCMatchSet(0, ulS + 2);
                HibernateRTCSSMatchSet(0, 0);
                HibernateRTCEnable();
            }

            /* Re-enable hibernate interrupts at the NVIC. */
            IntEnable(INT_HIBERNATE);
        }

//        UARTprintf("task | min free / total (words)\n");
//        UARTprintf("analog:      %d / %d\n", uxTaskGetStackHighWaterMark(xAnalogTaskHandle), ANALOGTASKSTACKSIZE);
//        UARTprintf("CAN:         %d / %d\n", uxTaskGetStackHighWaterMark(xCANTaskHandle), CANTASKSTACKSIZE);
//        UARTprintf("data:        %d / %d\n", uxTaskGetStackHighWaterMark(NULL), DATATASKSTACKSIZE);
//        UARTprintf("jsn:         %d / %d\n", uxTaskGetStackHighWaterMark(xJSNTaskHandle), JSNTASKSTACKSIZE);
//        UARTprintf("modem mgmt:  %d / %d\n", uxTaskGetStackHighWaterMark(xModemMgmtTaskHandle), MODEMMGMTTASKSTACKSIZE);
//        UARTprintf("modem uart:  %d / %d\n", uxTaskGetStackHighWaterMark(xModemUARTTaskHandle), MODEMUARTTASKSTACKSIZE);
//        UARTprintf("remote:      %d / %d\n", uxTaskGetStackHighWaterMark(xRemoteStartTaskHandle), REMOTESTARTTASKSTACKSIZE);
//        UARTprintf("srf:         %d / %d\n", uxTaskGetStackHighWaterMark(xSRFTaskHandle), SRFTASKSTACKSIZE);
//        UARTprintf("free heap:   %d bytes / %d total\n\n", xPortGetFreeHeapSize(), configTOTAL_HEAP_SIZE);

//        UARTprintf("srf: %d \n", *((uint32_t *)(chTestDist1.xData)));

//        UARTprintf("device: %u \n", HibernateRTCGetS());

//        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray,
//                                            uxArraySize,
//                                            &ulTotalRunTime );

//        UARTprintf("%s\n", pcRuntimeStats);

        /* Run this check every second. */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void
RTCConfigure(void) {

    /* Disable the hibernate peripheral's clock source. This is only done in
     * case of a reset, because the hibernate module will stay enabled unless
     * VBAT is removed. We want to be certain that the peripheral is fully
     * reset. */
    HibernateDisable();

    /* Disable the hibernate peripheral. Same reason as above. */
    SysCtlPeripheralDisable(SYSCTL_PERIPH_HIBERNATE);

    /* Enable the hibernate peripheral. */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_HIBERNATE);

    /* Wait for the hibernate peripheral to become ready. */
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_HIBERNATE)) {
    }

    /* Perform a software reset to ensure registers are clear. */
    SysCtlPeripheralReset(SYSCTL_PERIPH_HIBERNATE);

    /* Wait for the hibernate peripheral to become ready. */
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_HIBERNATE)) {
    }

    /* Enable clocking for the hibernate module. */
    HibernateEnableExpClk(80000000);

    /* Wait for the hibernate peripheral to become ready. */
//    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_HIBERNATE)) {
//    }

    /* The following two calls are a workaround for silicon erratum HIB#01.
     * These are the default values for the HIBRTCT and HIBIM registers, and
     * because they may be erroneously changed when the hibernation oscillator
     * is enabled in the previous call, they must be explicitly re-initialized
     * to their defaults here. */
    HibernateRTCTrimSet(0x7FFF);
    HibernateIntDisable(HIBERNATE_INT_PIN_WAKE | HIBERNATE_INT_LOW_BAT |
                        HIBERNATE_INT_RTC_MATCH_0 | HIBERNATE_INT_WR_COMPLETE);

    /* Hibernate interrupts are enabled at the NVIC, but we wait to
     * enable them at the peripheral until the Modem UART task sets the RTC
     * properly. */
    IntEnable(INT_HIBERNATE);
}

uint32_t
DataTaskInit(void) {

    /* Allocate memory for latest channel values. */
    vChannelInit();

    /* Create mutexes for each sample buffer. */
    vInitSampleRateBuffers();

    /* Enable the hibernate module and the real-time clock. */
    RTCConfigure();

    if(xTaskCreate(DataTask, (const portCHAR *)"Data", DATATASKSTACKSIZE, NULL,
                   tskIDLE_PRIORITY + PRIORITY_DATA_TASK, &xDataTaskHandle) != pdTRUE) {
        return 1;
    }

    return 0;
}
