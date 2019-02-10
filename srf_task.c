/*
 * srf_task.c
 * FreeRTOS task for controlling SRF02 ultrasonic sensors via UART and
 * estimating distance to obstacles.
 *
 * TODO: support multiple sensor aggregation
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
#include <stdlib.h>
#include "inc/hw_gpio.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_uart.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "channel.h"
#include "debug_helper.h"
#include "priorities.h"
#include "stack_sizes.h"
#include "FreeRTOS.h"
#include "task.h"


#define SRF_NOTIFY_DATA_MASK            0x0000ffff
#define SRF_NOTIFY_ERROR                0x00010000
#define SRF_NOTIFY_ALL                  0xffffffff

TaskHandle_t xSRFTaskHandle;

/*
 * The UART3 ISR notifies the SRF task when data arrives.
 */
void UART3IntHandler(void) {
    uint32_t ulStatus;
    uint32_t ulRxVal = 0;
    int32_t lByteCount = 2;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    debug_set_bus( 17 );

    /* Read the (masked) interrupt status of the UART. */
    ulStatus = UARTIntStatus(UART3_BASE, 1);

    /* Clear any pending status. */
    UARTIntClear(UART3_BASE, ulStatus);

    /* The TX FIFO transitioned below its set level. This only occurs if the
     * FIFO was filled above that level first. */
    if (ulStatus == UART_INT_TX) {

    }

    /* There is data in the RX FIFO. The RX interrupt is triggered if the RX
     * FIFO is filled past its set level. The receive timeout occurs when
     * there are still characters in the FIFO but no new characters have been
     * received over a 32-bit period. Because the SRF02 sensors only transmit
     * 2-byte values, the FIFO should never contain more than 2 bytes and the
     * status should always be UART_INT_RT. */
    if (ulStatus == UART_INT_RX || ulStatus == UART_INT_RT) {

        /* Loop until the RX FIFO is empty. Data will only arrive 2 bytes at a
         * time. UARTCharGetNonBlocking() will always succeed because
         * UARTCharsAvail() is true. */
        while(UARTCharsAvail(UART3_BASE)) {
            /* During normal operation, this clause should execute twice - once
             * for each data byte. If more than two bytes are present in the
             * FIFO, the else clause clears the FIFO and exits the loop
             * immediately. */
            if (lByteCount > 0) {
                ulRxVal |= UARTCharGetNonBlocking(UART3_BASE)
                           << ((lByteCount-1)*8);
                lByteCount--;
            }
            else {
                /* Clear the FIFO. Reading is the only way to do so. */
                while(UARTCharsAvail(UART3_BASE)) {
                    UARTCharGetNonBlocking(UART3_BASE);
                }
                break;
            }
        }

        /* If the byte count value decremented to zero, the correct number of
         * bytes were received (2). Otherwise, a communication error has
         * occurred. */
        if (!lByteCount) {
            /* Notify the task with the received value. */
            // This can also be done without overwriting, as a check to ensure that previous values have been processed already <-------
            xTaskNotifyFromISR(xSRFTaskHandle, ulRxVal, eSetValueWithOverwrite,
                               &xHigherPriorityTaskWoken);
        }
        else {
            /* Notify the task with the error value. SRF_NOTIFY_ERROR is
             * distinguishable from normal data values by the presence of a one
             * in the upper two bytes. */
            xTaskNotifyFromISR(xSRFTaskHandle, SRF_NOTIFY_ERROR,
                               eSetValueWithOverwrite,
                               &xHigherPriorityTaskWoken);
        }

    }

    debug_set_bus( LAST_PORT_F_VALUE );

    /* If a notification was sent, xHigherPriorityTaskWoken may be set to
     * pdTRUE and if so, this call will tell the scheduler to switch context to
     * the SRF task. */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*
 * Sends a command to one or more SRF sensors. The task maintains timing that
 * ensures the TX FIFO is empty whenever this is called, so false is returned
 * to indicate an error if there is existing data being transmitted already.
 */
static bool UART3Send(uint32_t ulAddress, uint32_t ulCmd) {

    /* Ensure that the FIFO is currently empty. */
    if (!UARTBusy(UART3_BASE)) {
        UARTCharPutNonBlocking(UART3_BASE, (uint8_t)ulAddress);
        UARTCharPutNonBlocking(UART3_BASE, (uint8_t)ulCmd);

        return true;
    }

    return false;

}

/*
 * The SRF task operates up to 16 Devantech SRF02 ultrasonic rangefinders on a
 * single UART "bus". Each sensor is addressable individually so that all can
 * receive and transmit on UART3, so long as timing is enforced and the proper
 * commands are sent. This task effectively maintains a state machine to
 * achieve that timing.
 */
static void SRFTask(void *pvParameters) {
    uint32_t ulNotificationValue = 0;
    uint32_t ulDistUS;
    uint32_t ulDistCM;
    float ulDistFT;

    /* Task loop. */
    while (1) {

        UART3Send(0x00000000, 0x00000055);

        /* Await a notification from the UART ISR.
         * The first param clears any bits that are set already, but only
         * if there is no notification pending. The second param clears
         * all bits again on exit. */
        xTaskNotifyWait(SRF_NOTIFY_ALL, SRF_NOTIFY_ALL,
                        &ulNotificationValue, portMAX_DELAY);

        if (ulNotificationValue & SRF_NOTIFY_ERROR) {
            debug_print("UART3 error\n");
        }
        else {
            ulDistUS = ulNotificationValue & SRF_NOTIFY_DATA_MASK;
            ulDistCM = (uint32_t)(0.017 * (float)ulDistUS);
            ulDistFT = (float)ulDistCM / 30.48;
//            debug_print("SRF reading: %d cm\n", ulDistCM);
            vChannelStore(&chTestDist1, &ulDistCM);
        }
    } /* while (1) */
}

/*
 * Configures UART3 for operation on pins PC6 (RX) and PC7 (TX).
 */
static void UART3Configure(void) {

    /* Enable peripheral clocks */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);

    /* Configure the GPIO Pin Mux for PC6 for U3RX */
    GPIOPinConfigure(GPIO_PC6_U3RX);
    GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_6);

    /* Configure the GPIO Pin Mux for PC7 for U3TX */
    GPIOPinConfigure(GPIO_PC7_U3TX);
    GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_7);

    /* Enable the UART peripheral. */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART3);

    /* Configure the UART communication parameters. (8-n-2) */
    UARTConfigSetExpClk(UART3_BASE, SysCtlClockGet(), 9600,
                        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_TWO |
                        UART_CONFIG_PAR_NONE);

    IntEnable(INT_UART3);
    UARTIntEnable(UART3_BASE, UART_INT_RX | UART_INT_RT);

    /* Enable the UART for operation. */
    UARTEnable(UART3_BASE);
}

/*
 * Initializes the SRF task by configuring the hardware and creating the task
 * from its function.
 */
uint32_t SRFTaskInit(void) {
    /* Configure pins and configure UART3 for 8-n-2 operation at 9600 baud. */
    UART3Configure();

    /* Create the SRF task. */
    if(xTaskCreate(SRFTask, (const portCHAR *)"SRF Ultrasonic",
                   SRFTASKSTACKSIZE, NULL,
                   tskIDLE_PRIORITY + PRIORITY_SRF_TASK,
                   &xSRFTaskHandle) != pdTRUE) {
        return 1;
    }

    return 0;
}
