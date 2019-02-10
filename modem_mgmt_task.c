/*
 * modem_mgmt_task.c
 * FreeRTOS task that keeps track of heartbeats from the server (via Modem UART
 * task), and public functions allowing power control of the modem.
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
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/adc.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "utils/uartstdio.h"
#include "channel.h"
#include "debug_helper.h"
#include "modem_mgmt_task.h"
#include "modem_uart_task.h"
#include "priorities.h"
#include "remote_start_task.h"
#include "sample.h"
#include "stack_sizes.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"


TaskHandle_t xModemMgmtTaskHandle;


/*
 * This ISR is triggered by edge transitions on PB2, which is the modem's power
 * status (PS) pin. This allows us to know when the modem is on or off, which
 * allows confirming that our outputs are working and detecting if the modem
 * turns off due to a dead battery.
 */
void PortBIntHandler(void) {
    uint32_t ulStatus;

    debug_set_bus( 11 );

    ulStatus = GPIOIntStatus(GPIO_PORTB_BASE, true);

    GPIOIntClear(GPIO_PORTB_BASE, ulStatus);

    debug_print("PB int: %X   ", ulStatus);

    if (ulStatus & GPIO_INT_PIN_2) {
        if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {
            debug_print("powerState: ON\n");
            xModemStatus.powerState = true;
        }
        else {
            debug_print("powerState: OFF\n");
            xModemStatus.powerState = false;
        }
    }
    else {
        debug_print("unexpected Port B GPIO interrupt\n");
    }

    debug_set_bus( LAST_PORT_F_VALUE );
}

/*
 * Manually power the modem on.
 *
 * Returns true for success, false if failure.
 */
bool ModemPowerOn(void) {
    /* Check PB2 (Modem power status) to ensure that the modem is off. */
    if (!GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {

        debug_print("Attempting power on...\n");

        /* Pull PB0 (Modem Key) low for 200ms */
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, GPIO_PIN_0);

        /* Modem power on takes up to 5s. */
        vTaskDelay(pdMS_TO_TICKS(5000));

        if (!GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {
            debug_print("Error. Modem power on attempt failed.\n");
            return false;
        }

        debug_print("Success. Waiting for boot sequence.\n");

        /* Modem boot sequence takes up to 8s. */
        vTaskDelay(pdMS_TO_TICKS(8000));

        /* Modem should be accessible now. */
        return true;
    }
    else {
        debug_print("Error. Modem power on attempted but modem is already on.\n");
        return false;
    }
}

/*
 * Manually power the modem down.
 *
 * Returns true for success, false if failure.
 */
bool ModemPowerOff(void) {
    /* Check PB2 (Modem power status) to ensure that the modem is on. */
    if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {

        debug_print("Attempting power off...\n");

        /* Pull PB0 (Modem Key) low for 600ms */
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, 0);
        vTaskDelay(pdMS_TO_TICKS(600));
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, GPIO_PIN_0);

        /* Modem power off takes up to 8s. */
        vTaskDelay(pdMS_TO_TICKS(8000));

        if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {
            debug_print("Error. Modem power off attempt failed.\n");
            return false;
        }

        /* Modem should be off now. */
        return true;
    }
    else {
        debug_print("Error. Modem power off attempted but modem is already off.\n");
        return false;
    }
}

/*
 * Manually reset the modem via hardware.
 *
 * Returns true for success, false if failure.
 */
bool ModemReset(void) {
    /* Check PB2 (Modem power status) to ensure that the modem is on. */
    if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {

        debug_print("Attempting modem reset...\n");

        /* Pull PB1 (Modem RST) low for 100ms */
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_1, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_1, GPIO_PIN_1);

        if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {
            debug_print("Error. Modem reset attempted but modem did not power off.\n");
            return false;
        }

        /* Modem reset takes up to 13s. */
        vTaskDelay(pdMS_TO_TICKS(13000));

        if (!GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {
            debug_print("Error. Modem reset attempted but modem did not power on again.\n");
            return false;
        }

        /* Modem should be accessible now. */
        return true;
    }
    else {
        debug_print("Error. Modem reset attempted but modem is off.\n");
        return false;
    }
}

/*
 * This task monitors for heartbeat messages from the server (sent as
 * notifications from the Modem UART task). Lack of heartbeat results in
 * resetting the modem and telling the Remote Start task to shut down the
 * ignition if necessary.
 *
 * A future cleaner implementation of this task will consolidate all calls to
 * ModemPowerOn(), ModemPowerOff(), and ModemReset() into this task,
 * triggerable by individual notifications. Currently the functions are just
 * usable elsewhere, but their multi-second delays block the calling task.
 */
static void ModemMgmtTask(void *pvParameters) {
    /* The task's notification value */
    uint32_t ulNotificationValue;

    /* Set the powerState flag once initially. */
    if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {
        debug_print("powerState: ON\n");
        xModemStatus.powerState = true;
    }
    else {
        debug_print("powerState: OFF\n");
        xModemStatus.powerState = false;
    }

    /* Main task loop. */
    while (1) {

        /* Only expect heartbeats when there is supposed to be an active TCP
         * connection. */
        if (xModemStatus.tcpConnectionOpen) {

            /* Check for a heartbeat notification from the Modem UART task.
             * This call blocks while waiting, but only up to 5 seconds. */
            xTaskNotifyWait(MGMT_NOTIFY_NONE, MGMT_NOTIFY_ALL,
                            &ulNotificationValue, pdMS_TO_TICKS(5000));

            /* If 5 seconds elapse without a heartbeat, force the Modem UART
             * task to exit its main loop (and subsequently reset the modem). */
            if (!(ulNotificationValue &= MGMT_NOTIFY_HEARTBEAT)) {
                debug_print("heartbeat not detected\n");
                xModemStatus.tcpConnectionOpen = false;
                xModemStatus.knownState = false;

                /* Notify the Remote Start task so that it can disable the
                 * ignition. */
                xTaskNotify( xRemoteStartTaskHandle, RS_NOTIFY_IGNITION_OFF,
                             eSetBits );
            }
            /* If there was a heartbeat, delay 500ms. This is only done to
             * prevent continuous looping if the server erroneously sends
             * heartbeats too quickly. */
            else {
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
        else {
            /* Delay for 1 second before checking for an open connection. */
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

/*
 * Configures 3 GPIO pins as follows:
 * - PB0 is modem on/off (output)
 * - PB1 is modem RST (output)
 * - PB2 is modem PS (input), interrupts on rising/falling edges
 */
static void ModemGPIOConfigure(void) {

    /* Enable GPIO port B. */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

    /* Wait for port B to become ready. */
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB)) {
    }

    /* Set initial output state to high. */
    GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1,
                 GPIO_PIN_0 | GPIO_PIN_1);

    /* PB0 is the power on/off signal. PB1 is the reset signal. Both are set to
     * push-pull operation and given internal weak pullup resistors. */
    GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1,
                     GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD_WPU);

    /* PB0 and PB1 are outputs. */
    GPIODirModeSet(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_DIR_MODE_OUT);

    /* PB2 is the power status input. */
    GPIOPinTypeGPIOInput(GPIO_PORTB_BASE, GPIO_PIN_2);

    /* Set PB2 to interrupt on any edge. */
    GPIOIntTypeSet(GPIO_PORTB_BASE, GPIO_PIN_2, GPIO_BOTH_EDGES);

    /* Enable individual pin interrupts in the GPIO module. */
    GPIOIntEnable(GPIO_PORTB_BASE, GPIO_PIN_2);

}

/*
 * Initializes the Modem Management task by configuring the necessary GPIOs and
 * creating the FreeRTOS task.
 */
uint32_t ModemMgmtTaskInit(void) {

    ModemGPIOConfigure();

    if(xTaskCreate(ModemMgmtTask, (const portCHAR *)"ModemMgmt",
                   MODEMMGMTTASKSTACKSIZE, NULL,
                   tskIDLE_PRIORITY + PRIORITY_MODEM_MGMT_TASK,
                   &xModemMgmtTaskHandle)
            != pdTRUE) {
        return 1;
    }

    return 0;
}
