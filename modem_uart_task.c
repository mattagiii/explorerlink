/*
 * modem_uart_task.c
 *
 *  Created on: Apr 23, 2018
 *      Author: Matt
 */


#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "inc/hw_gpio.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_uart.h"
#include "driverlib/gpio.h"
#include "driverlib/hibernate.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "channel.h"
#include "hibernate_rtc.h"
#include "modem_commands.h"
#include "modem_mgmt_task.h"
#include "modem_uart_task.h"
#include "priorities.h"
#include "remote_start_task.h"
#include "ring_buffer.h"
#include "sample.h"
#include "stack_sizes.h"
#include "test_helper.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"


#define TX_BUFFER_SIZE                  256
#define RX_BUFFER_SIZE                  256
#define RSP_WAIT_50_MS                  50
#define RSP_WAIT_500_MS                 500
#define RSP_WAIT_1000_MS                1000
#define RSP_WAIT_2000_MS                2000
#define RSP_WAIT_5000_MS                5000
#define RSP_WAIT_10000_MS               10000
#define EPOCH_ADJUST_S                  2208988800
#define COMMAND_MODE                    0
#define DATA_MODE                       1



TaskHandle_t xModemUARTTaskHandle;

volatile ModemStatus_t xModemStatus = { false, true, false, false, false, false, false, false };

uint8_t pucTxBufferData[TX_BUFFER_SIZE];
uint8_t pucRxBufferData[RX_BUFFER_SIZE];

volatile RingBuffer_t xTxBuffer = {
                             .pucData = pucTxBufferData,
                             .ulSize = TX_BUFFER_SIZE,
                             .ulReadIndex = 0,
                             .ulWriteIndex = 0
};

volatile RingBuffer_t xRxBuffer = {
                             .pucData = pucRxBufferData,
                             .ulSize = RX_BUFFER_SIZE,
                             .ulReadIndex = 0,
                             .ulWriteIndex = 0
};


/*
 * The UART6 ISR transfers data between the TX and RX ring buffers and the
 * peripheral data registers. When new data is available in the RX buffer, the
 * Modem UART Task is notified. ModemUARTSend is used by the task to trigger a
 * transmission. This enables the TX interrupt, and this ISR will pull data
 * from the TX ring buffer until it returns BUFFER_EMPTY. Then, the TX
 * interrupt is disabled again.
 */
void
UART6IntHandler(void) {
    uint32_t ulStatus;
    uint8_t uctxByte;

    uint8_t ucrxByte;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    GPIOPinWrite( GPIO_PORTF_BASE, UINT32_MAX, 13);

    // Read the (masked) interrupt status of the UART.
    ulStatus = UARTIntStatus(UART6_BASE, 1);

    // Clear any pending status.
    UARTIntClear(UART6_BASE, ulStatus);

    // The TX FIFO transitioned below its set level. This only occurs if the
    // FIFO was filled above that level first. If uart_prime() does not fill
    // the FIFO (quickly) enough to exceed the level, all bytes may be sent
    // without triggering the TX interrupt, and this point will not be reached.
    if (ulStatus == UART_INT_TX) {
        if (eRingBufferStatus(&xTxBuffer) == BUFFER_EMPTY) {
            UARTIntDisable(UART6_BASE, UART_INT_TX);
        }
        else {
            // Re-prime the TX FIFO
            while(UARTSpaceAvail(UART6_BASE) &&
                  eRingBufferRead(&xTxBuffer, &uctxByte) != BUFFER_EMPTY) {
                UARTCharPutNonBlocking(UART6_BASE, uctxByte);
            }
        }
    }

    // There is data in the RX FIFO. The RX interrupt is triggered if the RX
    // FIFO is filled past its set level. The receive timeout occurs when
    // there are still characters in the FIFO but no new characters have been
    // received over a 32-bit period.
    if (ulStatus == UART_INT_RX || ulStatus == UART_INT_RT) {

//        if (xModemStatus.tcpConnectionMode == DATA_MODE) {
//            UARTprintf("*\n");
//        }

        // Loop until the RX FIFO is empty. Data will not arrive fast enough
        // to keep this loop running indefinitely. UARTCharGetNonBlocking()
        // will always succeed because UARTCharsAvail() is true.
        while(UARTCharsAvail(UART6_BASE) && eRingBufferWrite(&xRxBuffer,
              UARTCharGetNonBlocking(UART6_BASE)) != BUFFER_FULL) {
        }

//        while(UARTCharsAvail(UART6_BASE)) {
//            ucrxByte = UARTCharGetNonBlocking(UART6_BASE);
//            if (xModemStatus.tcpConnectionMode == DATA_MODE) {
//                UARTprintf("[%02X]", ucrxByte);
//            }
//            eRingBufferWrite(&xRxBuffer, ucrxByte);
//        }
//
//        if (xModemStatus.tcpConnectionMode == DATA_MODE) {
//            UARTprintf("\n");
//        }

        if (xModemStatus.tcpConnectionMode == DATA_MODE) {
            /* Set the MODEM_NOTIFY_UNSOLICITED bit. Under normal operation,
             * this means the server has sent a command, but it may also
             * indicate that the connection was unexpectedly closed or be a
             * sign of some other failure. */
            xTaskNotifyFromISR(xModemUARTTaskHandle, MODEM_NOTIFY_UNSOLICITED,
                               eSetBits, &xHigherPriorityTaskWoken);
        }

        /* Set the MODEM_NOTIFY_RX bit. */
        xTaskNotifyFromISR(xModemUARTTaskHandle, MODEM_NOTIFY_RX, eSetBits,
                           &xHigherPriorityTaskWoken);
    }

    GPIOPinWrite( GPIO_PORTF_BASE, UINT32_MAX, ulLastPortFValue );

    /* If data was moved from the RX FIFO to the RX buffer,
     * xHigherPriorityTaskWoken may be set to pdTRUE and if so, this call will
     * tell the scheduler to switch context to the Modem UART task. */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*
 * Primes the UART6 transmit FIFO by filling it up while interrupts are
 * disabled. This will start a transmission sequence, as the interrupt doesn't
 * get triggered until the FIFO level passes through the threshold. If there
 * are fewer characters in the Tx ring buffer than can fill the FIFO, they will
 * be sent without triggering an interrupt.
 */
static void
UART6Prime(void) {
    uint8_t byte;

    // Do we have any data to transmit?
    if(eRingBufferStatus(&xTxBuffer) != BUFFER_EMPTY) {
        // Disable the UART interrupt.  If we don't do this there is a race
        // condition which can cause the read index to be corrupted.
        IntDisable(INT_UART6);

        // Take some characters out of the transmit buffer and feed them to
        // the UART transmit FIFO.
        while(UARTSpaceAvail(UART6_BASE) &&
              eRingBufferRead(&xTxBuffer, &byte) != BUFFER_EMPTY) {
            UARTCharPutNonBlocking(UART6_BASE, byte);
        }

        // Reenable the UART interrupt.
        IntEnable(INT_UART6);
    }
}

/*
 * Sends a sequence of characters on UART6 to the modem.
 */
static void
UART6Send(uint8_t *pucSend, uint32_t ulLength, uint32_t ulDelayMS) {
    while (ulLength--) {
        eRingBufferWrite(&xTxBuffer, *pucSend++);
    }
    UART6Prime();
    UARTIntEnable(UART6_BASE, UART_INT_TX);

    // For debugging purposes only. If a breakpoint is reached too quickly
    // after data is sent, the ISR will not be able to handle the modem's
    // response because the program is halted. This call delays execution
    // immediately after sending so that the ISR is able to handle responses.
    vTaskDelay(pdMS_TO_TICKS(ulDelayMS));
}

/*
 * Sends a command to the modem.
 */
static void
ModemSendCommand(const ModemCommand_t *pxCmd) {
    UART6Send(pxCmd->pucData, strlen((char *)pxCmd->pucData), 0);
}

/*
 * This function reads a line from the RX ring buffer into a given buffer and
 * adds a null terminator. The length of the line (including all termination)
 * is stored in pulLineLength.
 * Returns true if a full line was read. If no '\n' was detected before the
 * timeout, returns false, but the buffer and length are still valid.
 * need to check for buffer overflow (most likely?) <---------------------------------------------------------
 */
static bool
UART6RcvLine(uint8_t *pucBuffer, uint32_t ulWaitTimeMS, uint32_t *pulLineLength) {
    TickType_t xTicksToWait;
    TimeOut_t xTimeOut;
    uint8_t ucByte = 0;
    uint32_t ulNotificationValue;

    // Record the time at which this function was entered.
    vTaskSetTimeOutState(&xTimeOut);

    // xTicksToWait is the timeout value passed to this function.
    xTicksToWait = pdMS_TO_TICKS(ulWaitTimeMS);

    // Loop until a non-blank line is read or the timeout occurs.
    do {
        *pulLineLength = 0;
        // Loop until a line is read or the timeout occurs.
        do {
            // If there are already characters in the buffer, the first loop
            // iteration will read them until the buffer is empty or '\n' is
            // reached.
            while (eRingBufferRead(&xRxBuffer, &ucByte) != BUFFER_EMPTY) {
                pucBuffer[(*pulLineLength)++] = ucByte;
                if (ucByte == '\n') {
                    break;
                }
            }

            /* If a full line hasn't been read yet, check for a timeout and
             * then await notification that the ISR has updated the ring
             * buffer. */
            if (ucByte != '\n') {
                /* Because xTaskNotifyWait() will trigger on notifications
                 * other than MODEM_NOTIFY_RX, this loop re-runs the wait if
                 * the notification value doesn't have the MODEM_NOTIFY_RX bit
                 * set. */
                do {
                    // Look for a timeout, adjusting xTicksToWait to account for the
                    // time spent in this function so far.
                    if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) != pdFALSE ) {
                        /* Timed out before a non-blank line could be read.
                         * Append a null character for consistency. */
                        pucBuffer[(*pulLineLength)++] = '\0';
                        return false;
                    }

                    // Wait for a maximum of xTicksToWait ticks to be notified that
                    // the receive ISR has placed more data into the buffer.
                    xTaskNotifyWait(MODEM_NOTIFY_RX, MODEM_NOTIFY_RX,
                                    &ulNotificationValue, xTicksToWait);
                } while (!(ulNotificationValue & MODEM_NOTIFY_RX));
            }
        } while (ucByte != '\n');
        // This point is only reached if a line (ending in '\n') has been read.
        // The outermost loop only exits if the line is not "\r\n" (blank).
    } while (pucBuffer[0] == '\r' && pucBuffer[1] == '\n');

    // Append a null character to the line.
    pucBuffer[(*pulLineLength)++] = '\0';

    // A line was successfully read.
    return true;
}

/*
 * This function clears the receive buffer (useful on reboots, etc.).
 */
static void
UART6RcvBufferClear(void) {
    vRingBufferClear(&xRxBuffer);
}

/*
 * Checks a string against a known response.
 */
static bool
ModemCheckRspLine(uint8_t *pucLine, const ModemResponse_t *pxRsp) {
    /* Check for the complete response unless the response has a length limit
     * specified. */
    uint32_t ulN = pxRsp->ulCheckLength > 0 ? pxRsp->ulCheckLength :
                                              strlen((char *)pxRsp->pucData);

    if (!strncmp((char *)pucLine, (char *)pxRsp->pucData, ulN)) {
        return true;
    }

    return false;
}


static bool
ModemEchoOff(void) {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;

    ModemSendCommand(&cmdATE0);

    /* Modem might echo this command if echo wasn't already disabled, so both
     * of these responses are valid. */
    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
            ModemCheckRspLine(pucRcvdLine, &rspATE0Echo)) {
        if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
                ModemCheckRspLine(pucRcvdLine, &rspOK)) {
            xModemStatus.echoOff = true;
            return true;
        }
    }
    else if (ModemCheckRspLine(pucRcvdLine, &rspOK)) {
        xModemStatus.echoOff = true;
        return true;
    }

    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemEchoOff failed\n");
    return false;
}

/*
 * Obtains the current local time from the modem and sets the real-time clock
 * to Unix time based on the modem's response.
 */
static bool
ModemUpdateRTCTime(void) {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;
    struct tm xTime;
    int32_t lZoneOffset;
    char *pucZoneString;

    ModemSendCommand(&cmdATCCLK);

    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
            ModemCheckRspLine(pucRcvdLine, &rspATCCLK)) {
        /* The response from AT+CCLK? is parsed here and placed into a struct
         * tm for mktime() to work with. */
        xTime.tm_year = atoi(strtok((char *)pucRcvdLine, "+CLK: \"/,")) + 100;
        xTime.tm_mon = atoi(strtok(NULL, "+CLK: \"/,")) - 1;
        xTime.tm_mday = atoi(strtok(NULL, "+CLK: \"/,"));
        xTime.tm_hour = atoi(strtok(NULL, "+CLK: \"/,"));
        xTime.tm_min = atoi(strtok(NULL, "+CLK: \"/,"));
        xTime.tm_sec = atoi(pucZoneString = strtok(NULL, "+CLK: \"/,"));
        /* The last part of the response contains a positive or negative value
         * which is the offset from GMT as a count of 15-minute intervals (e.g.
         * PST corresponds to "-32" and PDT corresponds to "-28". The pointer
         * is first moved past the seconds field and then converted. */
        if ((pucZoneString = (pucZoneString + 2))[0] == '-') {
            lZoneOffset = (atoi(pucZoneString + 1) * -1) * 15 * 60;
        }
        else {
            lZoneOffset = atoi(pucZoneString + 1) * 15 * 60;
        }
        xTime.tm_isdst = -1;

        if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
                ModemCheckRspLine(pucRcvdLine, &rspOK)) {
            /* mktime() gives seconds since a 1900 epoch. Subtracting the first
             * offset gives seconds since the 1970 epoch (Unix time). The
             * second offset is subtracted to bring the local time to GMT. */
            HibernateRTCSet(mktime(&xTime) - EPOCH_ADJUST_S - lZoneOffset);

            /* In addition to setting the RTC to Unix time, we set a match in
             * the near future to kick off the RTC interrupt sampling cycle.
             * 2 seconds are added to ensure that the match time isn't in the
             * past by the time the interrupt is enabled. */
            HibernateRTCMatchSet(0, HibernateRTCGetS() + 2);
            HibernateRTCSSMatchSet(0, 0);
            /* Enable the match interrupt at the peripheral. */
            HibernateIntEnable(HIBERNATE_INT_RTC_MATCH_0);
            /* Enable the real-time clock (begin counting). */
            HibernateRTCEnable();
            return true;
        }
    }

    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemUpdateRTCTime failed\n");
    return false;
}

/*
 * Checks the battery level that the modem sees.
 */
static bool
ModemCheckBattery(void) {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;
    char pucBatteryVoltageMV[8];
    uint16_t usBatteryVoltageMV;

    ModemSendCommand(&cmdATCBC);

    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
            ModemCheckRspLine(pucRcvdLine, &rspATCBC)) {

        /* This line could be of varying length, but will always end with a
         * comma preceding the string representation of the battery voltage
         * shown below. Two other commas precede the last, so strtok is called
         * twice to discard the start of the string. */
        strtok((char *)pucRcvdLine, ",");
        strtok(NULL, ",");
        memcpy(pucBatteryVoltageMV, strtok(NULL, ","), 8); /* e.g. "3.735V\n\0" */

        /* Move the millivolt digits over by one character. */
        pucBatteryVoltageMV[1] = pucBatteryVoltageMV[2]; /* 7 */
        pucBatteryVoltageMV[2] = pucBatteryVoltageMV[3]; /* 3 */
        pucBatteryVoltageMV[3] = pucBatteryVoltageMV[4]; /* 5 */
        pucBatteryVoltageMV[4] = '\0';

        usBatteryVoltageMV = atoi(pucBatteryVoltageMV);

        vChannelStore(&chDeviceBatt, &usBatteryVoltageMV);

        if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
                ModemCheckRspLine(pucRcvdLine, &rspOK)) {
            return true;
        }
    }

    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemCheckBattery failed\n");
    return false;
}

/*
 * Checks the network signal level and sets the modem signal status flag.
 */
static bool
ModemCheckSignal(void) {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;
    uint32_t ulSignalLevel;

    ModemSendCommand(&cmdATCSQ);

    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
            ModemCheckRspLine(pucRcvdLine, &rspATCSQ)) {

        ulSignalLevel = atoi(strtok((char *)pucRcvdLine, "+CSQ: ,"));

        UARTprintf("signal level: %d\n", ulSignalLevel);

        xModemStatus.signalPresent = ulSignalLevel ? true : false;

        /* Read 'OK' */
        if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
                ModemCheckRspLine(pucRcvdLine, &rspOK)) {
            return true;
        }
    }

    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemCheckSignal failed\n");
    return false;
}

/*
 * Checks whether the modem is in non-transparent (command) mode or transparent
 * (data) mode.
 */
static bool
ModemGetNetworkMode(void) {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;

    ModemSendCommand(&cmdATCIPMODEQuery);

    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
            ModemCheckRspLine(pucRcvdLine, &rspATCIPMODECommandMode)) {
        xModemStatus.networkMode = COMMAND_MODE;
    }
    else if (ModemCheckRspLine(pucRcvdLine, &rspATCIPMODEDataMode)) {
        xModemStatus.networkMode = DATA_MODE;
    }

    /* Read 'OK' */
    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
            ModemCheckRspLine(pucRcvdLine, &rspOK)) {
        return true;
    }

    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemGetNetworkMode failed\n");
    return false;
}

/*
 * Sets the modem to non-transparent (command) mode or transparent (data) mode.
 * If the response is 'ERROR', the network is already open, so
 * ModemGetNetworkMode() is called to set the networkMode flag correctly.
 */
static bool
ModemSetNetworkMode(bool bMode) {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;

//    if (ModemGetNetworkMode()) {
//        if (xModemStatus.networkMode != bMode) {
//                                                                                //<----------------------
//        }
//        else {
//
//        }
//    }
    if (bMode == COMMAND_MODE) {
        ModemSendCommand(&cmdATCIPMODE0);
    }
    else {
        ModemSendCommand(&cmdATCIPMODE1);
    }

    /* Read 'OK' */
    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
            ModemCheckRspLine(pucRcvdLine, &rspOK)) {
        xModemStatus.networkMode = bMode;
        return true;
    }
    else if (ModemCheckRspLine(pucRcvdLine, &rspERROR)) {
        if (ModemGetNetworkMode()) {
            return true;
        }
    }

    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemSetNetworkMode failed\n");
    return false;
}

/*
 * Queries the modem to determine if the mobile network data connection is
 * established.
 */
static bool
ModemCheckNetworkStatus(void) {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;

    ModemSendCommand(&cmdATNETOPENQuery);

    /* Check for positive response */
    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
            ModemCheckRspLine(pucRcvdLine, &rspATNETOPENTrue)) {
        /* Read 'OK' */
        if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
                ModemCheckRspLine(pucRcvdLine, &rspOK)) {
            xModemStatus.networkOpen = true;
            return true;
        }
    }
    /* If network is not open, return false. */
    else if (ModemCheckRspLine(pucRcvdLine, &rspATNETOPENFalse)) {
        /* Read 'OK' */
        if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
                ModemCheckRspLine(pucRcvdLine, &rspOK)) {
            xModemStatus.networkOpen = false;
            return true;
        }
    }

    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemCheckNetworkStatus failed\n");
    return false;
}

/*
 * Opens the modem's connection to the mobile data network.
 */
static bool
ModemNetworkOpen() {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;

    ModemSendCommand(&cmdATNETOPEN);

    /* Check for 'OK' */
    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
            ModemCheckRspLine(pucRcvdLine, &rspOK)) {

        /* Wait up to 10s for network to open */
        if (UART6RcvLine(pucRcvdLine, RSP_WAIT_10000_MS, &ulByteCount) &&
                ModemCheckRspLine(pucRcvdLine, &rspATNETOPENSuccess)) {
            xModemStatus.networkOpen = true;
            return true;
        }
        /* If network was already open, handle error lines. */
        else if (ModemCheckRspLine(pucRcvdLine, &rspATNETOPENIPErr)) {
            /* Read 'ERROR' line. */
            UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount);
            xModemStatus.networkOpen = false;
            return true;
        }
    }

    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemNetworkOpen failed\n");
    return false;
}

/*
 * Opens the modem's connection to the mobile data network.
 */
static bool
ModemNetworkClose() {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;

    ModemSendCommand(&cmdATNETCLOSE);

    /* Check for 'OK' */
    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
            ModemCheckRspLine(pucRcvdLine, &rspOK)) {

        /* Wait up to 10s for network to close */
        if (UART6RcvLine(pucRcvdLine, RSP_WAIT_10000_MS, &ulByteCount) &&
                ModemCheckRspLine(pucRcvdLine, &rspATNETCLOSESuccess)) {
            xModemStatus.networkOpen = false;
            return true;
        }
        /* If network was already closed, handle error line. */
        else if (ModemCheckRspLine(pucRcvdLine, &rspERROR)) {
            xModemStatus.networkOpen = true;
            return true;
        }
    }

    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemNetworkClose failed\n");
    return false;
}

/*
 * Queries the modem to determine if the TCP connection to the server is
 * active.
 */
static bool
ModemCheckTCPConnection(void) {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;

    ModemSendCommand(&cmdATCIPOPENQuery);

    /* Check the first response line, which should correctly detail the
     * connection parameters if the connection is open. */
    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
            ModemCheckRspLine(pucRcvdLine, &rspATCIPOPENTrue)) {
        UARTprintf("[%s]\n", pucRcvdLine);
        xModemStatus.tcpConnectionOpen = true;
    }
    else {
        xModemStatus.tcpConnectionOpen = false;
    }

    /* Consume the rest of the response lines. These will be present
     * regardless. */
    while (UART6RcvLine(pucRcvdLine, RSP_WAIT_500_MS, &ulByteCount) &&
            ModemCheckRspLine(pucRcvdLine, &rspATCIPOPENRest)) {
    }

    /* Check for 'OK'. Due to the while loop it's already in pucRcvdLine. */
    if (ModemCheckRspLine(pucRcvdLine, &rspOK)) {
        return true;
    }

    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemCheckTCPConnection failed\n");
    return false;
}

/*
 * Establishes a TCP connection to the server.
 */
static bool
ModemTCPConnect(void) {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;

    ModemSendCommand(&cmdATCIPOPEN);

    if (xModemStatus.networkMode == DATA_MODE) {
        /* Wait up to 5s for the TCP connection to open. */
        if (UART6RcvLine(pucRcvdLine, RSP_WAIT_5000_MS, &ulByteCount) &&
                ModemCheckRspLine(pucRcvdLine, &rspATCIPOPENConnect)) {
            xModemStatus.tcpConnectionOpen = true;
            xModemStatus.tcpConnectionMode = DATA_MODE;
            return true;
        }
        else if (ModemCheckRspLine(pucRcvdLine, &rspATCIPOPENFail)) {
            if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
                ModemCheckRspLine(pucRcvdLine, &rspERROR)) {
                xModemStatus.tcpConnectionOpen = false;
                return true;
            }
        }
    }
    else if (xModemStatus.networkMode == COMMAND_MODE) {
        if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
                ModemCheckRspLine(pucRcvdLine, &rspOK)) {

            /* Wait up to 5s for the TCP connection to open. */
            if (UART6RcvLine(pucRcvdLine, RSP_WAIT_5000_MS, &ulByteCount) &&
                    ModemCheckRspLine(pucRcvdLine, &rspATCIPOPENSuccess)) {
                xModemStatus.tcpConnectionOpen = true;
                xModemStatus.tcpConnectionMode = COMMAND_MODE;
                return true;
            }
            else if (ModemCheckRspLine(pucRcvdLine, &rspATCIPOPENFail)) {
                xModemStatus.tcpConnectionOpen = false;
                return true;
            }
        }
    }
    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemTCPConnect failed\n");
    return false;
}

/*
 * Sends on an existing TCP connection.
 */
static bool
ModemTCPSend(SampleRateBuffer_t *pxBuffer) {
//    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
//    uint32_t ulByteCount;
//    uint8_t pucLength[7];
    uint8_t ucByte;

    /* In data mode, the modem is already ready to accept sample data for TCP
     * transmission, so we send it directly. */
    if (xModemStatus.tcpConnectionMode == DATA_MODE) {
        /* Read from the ring buffer and send bytes until it is empty. This is
         * the only place ring buffers may be read from, which keeps the
         * read thread-safe. */
        while (eRingBufferRead(&(pxBuffer->xData), &ucByte) != BUFFER_EMPTY) {
            UART6Send(&ucByte, 1, 0);
//            UARTprintf("%02X ", ucByte);
        }
        return true;
    }
//    else if (xModemStatus.tcpConnectionMode == COMMAND_MODE) {
//        ModemSendCommand(&cmdATCIPSEND);
//
//        /* if I do ever have to use this, it should be stored with the buffer
//         * because it can be calculated in advance, probably even in the init */
//        snprintf((char *)pucLength, 7, "%d\r\n",
//                 pxBuffer->ulSampleSize);
//        UART6Send(pucLength, strlen((const char *)pucLength), 1000);
//
//        /* Use a short wait to receive the ">" prompt, which is not a complete line
//         * with CRLF. */
//        if (UART6RcvLine(pucRcvdLine, RSP_WAIT_50_MS, &ulByteCount) &&
//                ModemCheckRspLine(pucRcvdLine, &rspATCIPSENDPrompt)) {
//
//            /* Check for positive response */
//            if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
//                    ModemCheckRspLine(pucRcvdLine, &rspOK)) {
//                return true;
//            }
//        }
//    }
    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemTCPSend failed\n");
    return false;
}

/*
 * Sends the '+++' sequence to return the modem to command mode when a TCP
 * connection is active. If 'test' is true, this function will not indicate
 * any errors if it gets no response.
 */
static bool
ModemSwitchToCommandMode(bool test) {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;

    /* '+++' must be preceded and followed by at least 1 second delays. */
    vTaskDelay(pdMS_TO_TICKS(1000));
    ModemSendCommand(&cmdPlus);
    vTaskDelay(pdMS_TO_TICKS(10));
    ModemSendCommand(&cmdPlus);
    vTaskDelay(pdMS_TO_TICKS(10));
    ModemSendCommand(&cmdPlus);
    //vTaskDelay(pdMS_TO_TICKS(1000));

    /* Possible need to flush buffer if data may still be arriving. Or use
     * flow control whatnot instead! */

    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_2000_MS, &ulByteCount) &&
                ModemCheckRspLine(pucRcvdLine, &rspOK)) {
        xModemStatus.tcpConnectionMode = COMMAND_MODE;
        return true;
    }
    else if (test) {
        return true;
    }
    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemSwitchToCommandMode failed\n");
    return false;
}

/*
 * Sends the ATO command to return the modem to command mode when a TCP
 * connection is active.
 */
static bool
ModemSwitchToDataMode(void) {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;

    ModemSendCommand(&cmdATO);

    /* Wait up to 5s for the TCP connection to open. */
    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_5000_MS, &ulByteCount) &&
            ModemCheckRspLine(pucRcvdLine, &rspATCIPOPENConnect)) {
        xModemStatus.tcpConnectionMode = DATA_MODE;
        return true;
    }
    /* NO CARRIER appears if there is no connection */
    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemSwitchToDataMode failed\n");
    return false;
}

/*
 * Close the active TCP connection.
 */
static bool
ModemTCPDisconnect(void) {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;

    ModemSendCommand(&cmdATCIPCLOSE);

    if (xModemStatus.networkMode == DATA_MODE) {
        /* Check for 'OK' */
        if (UART6RcvLine(pucRcvdLine, RSP_WAIT_1000_MS, &ulByteCount) &&
                ModemCheckRspLine(pucRcvdLine, &rspOK)) {

            /* Wait up to 5s for the TCP connection to close. */
            if (UART6RcvLine(pucRcvdLine, RSP_WAIT_5000_MS, &ulByteCount) &&
                    ModemCheckRspLine(pucRcvdLine, &rspATCIPCLOSESuccess)) {
                xModemStatus.tcpConnectionOpen = false;
                return true;
            }
        }
    }
    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
    xModemStatus.knownState = false;
    UARTprintf("ModemTCPDisconnect failed\n");
    return false;
}


static bool
ModemParseCommand(uint8_t *pucBuffer) {

    static uint32_t ulLastClientCount = -1;
    uint32_t ulPreviousValue;
    BaseType_t xNotifySuccessVal;

    switch(pucBuffer[3]) {

        /* action: ignition on */
        case 'a' :
            UARTprintf("notifying ignition on\n");
            xNotifySuccessVal = xTaskNotifyAndQuery(xRemoteStartTaskHandle,
                                                    RS_NOTIFY_IGNITION_ON,
                                                    eSetBits,
                                                    &ulPreviousValue);
            break;
        /* action: ignition off */
        case 'b' :
            UARTprintf("notifying ignition off\n");
            xNotifySuccessVal = xTaskNotifyAndQuery(xRemoteStartTaskHandle,
                                                    RS_NOTIFY_IGNITION_OFF,
                                                    eSetBits,
                                                    &ulPreviousValue);
            break;
        /* action: start engine */
        case 'c' :
            UARTprintf("notifying start\n");
            xNotifySuccessVal = xTaskNotifyAndQuery(xRemoteStartTaskHandle,
                                                    RS_NOTIFY_START,
                                                    eSetBits,
                                                    &ulPreviousValue);
            break;
        /* client count */
        case 'd' :
            UARTprintf("client count = %d\n", pucBuffer[4]);

            /* If no clients remain connected, notify the Remote Start task so
             * that it can disable the ignition after 1 minute. */
            if ( pucBuffer[4] < 1 ) {
                xNotifySuccessVal = xTaskNotifyAndQuery(xRemoteStartTaskHandle,
                                                        RS_NOTIFY_NO_CLIENT,
                                                        eSetBits,
                                                        &ulPreviousValue);
            }
            /* If after no clients were connected one or more connect, notify
             * the task so that it can clear the countdown to disable the
             * ignition (if needed). */
            else if ( !ulLastClientCount && pucBuffer[4] > 0 ) {
                xNotifySuccessVal = xTaskNotifyAndQuery(xRemoteStartTaskHandle,
                                                        RS_NOTIFY_CLIENT,
                                                        eSetBits,
                                                        &ulPreviousValue);
            }

            /* Store the count to allow comparing when it changes. */
            ulLastClientCount = pucBuffer[4];

            break;
        /* heartbeat */
        case 'z' :
//            UARTprintf("heartbeat received\n");
            xNotifySuccessVal = xTaskNotifyAndQuery(xModemMgmtTaskHandle,
                                                    MGMT_NOTIFY_HEARTBEAT,
                                                    eSetValueWithoutOverwrite,
                                                    &ulPreviousValue);
            break;
    }

    if (xNotifySuccessVal != pdPASS) {
        UARTprintf("Error: action notification '%c' failed because of "
                   "pending value %08X", pucBuffer[3], ulPreviousValue);
        // Possibly set status/error flags here for sending as channel data to the server
    }

    return true;
}


/*
 * Reads known unsolicited responses from the modem. Unsolicited responses
 * may include received server commands and/or unexpected modem errors.
 */
static bool
ModemReadUnsolicited(void) {
    uint8_t pucRcvdLine[RX_BUFFER_SIZE];
    uint32_t ulByteCount;
    int i;
    uint32_t ulWaitCount = 0;

    /* Receive a line. A shorter timeout is used since this function is only
     * called after the RX notification has been received. */
    if (UART6RcvLine(pucRcvdLine, RSP_WAIT_500_MS, &ulByteCount)) {

//        UARTprintf("\n[");
//        for (i = 0; i < ulByteCount; i++) {
//            UARTprintf("%02X ", pucRcvdLine[i]);
//        }
//        UARTprintf("]\n");

        if (ModemCheckRspLine(pucRcvdLine, &rspServerCommand)) {

//            UARTprintf("got a command line\n");
            return ModemParseCommand(pucRcvdLine);

        } /* if (ModemCheckRspLine(pucRcvdLine, &rspServerCommand)) */
        else if (ModemCheckRspLine(pucRcvdLine, &rspCLOSED)) {

            UARTprintf("connection was closed\n");
            /* Read the +IPCLOSE: line if it is there. */
            UART6RcvLine(pucRcvdLine, RSP_WAIT_5000_MS, &ulByteCount);

            /* Though the connection should be gone after the server closes
             * it, sometimes it may remain in the list. This section ensures
             * that the connection is completely removed before proceeding. */
            while (ModemCheckTCPConnection() &&
                   xModemStatus.tcpConnectionOpen && ulWaitCount++ <= 5) {
                UARTprintf("connection was still around\n");
                /* Usually it just takes time for the connection to go away. */
                vTaskDelay(pdMS_TO_TICKS(500));

            } /* if (ModemCheckTCPConnection()) */

            return true;

        } /* else if (ModemCheckRspLine(pucRcvdLine, &rspCLOSED)) */

    } /* if (UART6RcvLine(pucRcvdLine, RSP_WAIT_500_MS, &ulByteCount)) */

    /* If something other than the expected responses showed up, tell the main
     * loop that it should reset. */
//    UARTprintf("unsolicited read error\n");
//    xModemStatus.knownState = false;
    return false;
}


/*
 * The Modem UART task serves as a gatekeeper task for UART6, which
 * communicates with the SIM5320A modem. Ring buffers are used for TX and RX.
 * This task interprets received data and alerts other tasks as needed. If
 * another task (e.g. the CAN Processing Task) alerts this task that data is
 * ready to be transmitted, this task will set up and perform the transmission.
 */
static void
ModemUARTTask(void *pvParameters) {
    uint32_t ulNotificationValue = 0;
    uint32_t ulSignalRetryAttempts;
    uint32_t ulTCPRetryAttempts;
    bool bMode = DATA_MODE;
    uint32_t i;
    bool bFirstRun = true;

//    vTaskDelay(pdMS_TO_TICKS(UINT32_MAX));

    ModemPowerOn();

    /* This switches the modem to command mode if it was stuck in data mode
     * from an earlier run. */
    ModemSwitchToCommandMode(true);

    /* Main task loop. */
    while (1) {

        /* Ensure that the receive buffer is clear. */
        UART6RcvBufferClear();

        /* Startup and signal acquisition loop. */
        do {

            ModemEchoOff();

            if (bFirstRun) {
                if (ModemUpdateRTCTime()) {
                    bFirstRun = false;
                }
            }

            ModemCheckBattery();

            ModemCheckSignal();

            ulSignalRetryAttempts = 0;
            while (!xModemStatus.signalPresent && ulSignalRetryAttempts++ < 5) {
                vTaskDelay(pdMS_TO_TICKS(2000));
                ModemCheckSignal();

                /* Time could be out of sync if the signal was unavailable. */
                if (xModemStatus.signalPresent) {
                    ModemUpdateRTCTime();
                }
            }

            /* If a signal could not be acquired, try resetting the modem. */
            if (!xModemStatus.signalPresent) {
                UARTprintf("signal not acquired. Resetting modem...\n");
                ModemReset();
                UART6RcvBufferClear();
            }

        } while (!xModemStatus.signalPresent);

        /* A signal is present. This section attempts a data connection and
         * then a TCP connection to the server. */
        ModemCheckNetworkStatus();

        if (xModemStatus.networkOpen) {
            UARTprintf("network was open\n");
            ModemGetNetworkMode();
            if (xModemStatus.networkMode != bMode) {
                ModemNetworkClose();
                ModemSetNetworkMode(bMode);
                ModemNetworkOpen();
            }
        }
        else {
            UARTprintf("network wasn't open\n");
            ModemSetNetworkMode(bMode);
            ModemNetworkOpen();
        }

        if ( xModemStatus.knownState && xModemStatus.networkOpen ) {

            ModemCheckTCPConnection();

            if (!xModemStatus.tcpConnectionOpen) {
                UARTprintf("tcp wasn't open\n");
                ulTCPRetryAttempts = 0;
                while ( ModemTCPConnect() && !xModemStatus.tcpConnectionOpen &&
                        ulTCPRetryAttempts++ <= 4 ) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }
            else {
                UARTprintf("tcp was open\n");
                if (ModemSwitchToDataMode()) {
                    UARTprintf("now in data mode\n");
                }
            }
        }

        /* Only proceed if the TCP connection is established. */
        while ( xModemStatus.knownState && xModemStatus.networkOpen &&
                xModemStatus.tcpConnectionOpen ) {

            /* Await a notification from either the UART ISR or another task.
             * The first param clears any bits that are set already, but only
             * if there is no notification pending. The second param clears
             * all bits again on exit. */

            /* Wait for samples and/or unsolicited data on the UART. */
            xTaskNotifyWait(MODEM_NOTIFY_SAMPLE | MODEM_NOTIFY_UNSOLICITED,
                            MODEM_NOTIFY_SAMPLE | MODEM_NOTIFY_UNSOLICITED,
                            &ulNotificationValue, portMAX_DELAY);

            if (ulNotificationValue & MODEM_NOTIFY_SAMPLE) {

                /* Send data from all sample buffers. Because writes to sample
                 * buffers occur in a critical section, buffers are guaranteed
                 * to contain only complete sample chunks at all times. This,
                 * combined with the order guarantee TCP provides, ensures that
                 * sample chunks arrive at the server intact. */
                for (i = 0; i < ucSampleGetBufferCount(); i++) {
                    ModemTCPSend(pxSampleRateBuffers[i]);
                }
            }

            if (ulNotificationValue & MODEM_NOTIFY_UNSOLICITED) {
                ModemReadUnsolicited();
            }

        } /* while (knownState && networkOpen && tcpConnectionOpen) */

        /* Breaking out of the former loop means either the connection was
         * lost, or the modem entered an unknown state. If the latter, recovery
         * is attempted here. */
        if (xModemStatus.knownState == false) {
            UARTprintf("knownState false. Resetting modem...\n");
            ModemReset();
            xModemStatus.knownState = true;
        } /* if (xModemStatus.knownState == false) */
        else {
            UARTprintf("knownState true, connection likely lost.\n");
        }

        UARTprintf("\n\n");

    } /* while (1) */

}

static void
UART6Configure(void) {

    /* Enable peripheral clocks */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);

    /* Wait for port D to become ready. */
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOD)) {
    }

    /* Configure the GPIO Pin Mux for PD4 for U1RX */
    GPIOPinConfigure(GPIO_PD4_U6RX);
    GPIOPinTypeUART(GPIO_PORTD_BASE, GPIO_PIN_4);

    /* Configure the GPIO Pin Mux for PD5 for U1TX */
    GPIOPinConfigure(GPIO_PD5_U6TX);
    GPIOPinTypeUART(GPIO_PORTD_BASE, GPIO_PIN_5);

    /* Enable the UART peripheral. */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART6);

    /* Wait for UART6 to become ready. */
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_UART6)) {
    }

    /* Configure the UART communication parameters. (8-n-1) */
    UARTConfigSetExpClk(UART6_BASE, SysCtlClockGet(), 115200,
                        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                        UART_CONFIG_PAR_NONE);

    IntEnable(INT_UART6);
    UARTIntEnable(UART6_BASE, UART_INT_RX | UART_INT_RT);

    /* Enable the UART for operation. */
    UARTEnable(UART6_BASE);
}

/*
 * Initializes the Modem UART task by configuring the hardware and creating the
 * task from its function.
 */
uint32_t
ModemUARTTaskInit(void) {
    /* Configure pins and configure UART6 for 8-n-1 operation at 115200 baud. */
    UART6Configure();

    /* Create the Modem UART task. */
    if(xTaskCreate(ModemUARTTask, (const portCHAR *)"Modem UART",
                   MODEMUARTTASKSTACKSIZE, NULL,
                   tskIDLE_PRIORITY + PRIORITY_MODEM_UART_TASK,
                   &xModemUARTTaskHandle) != pdTRUE) {
        return 1;
    }

    return 0;
}
