/*
 * modem_mgmt_task.c
 *
 *  Created on: Jun 26, 2018
 *      Author: Matt
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
#include "modem_mgmt_task.h"
#include "modem_uart_task.h"
#include "priorities.h"
#include "remote_start_task.h"
#include "sample.h"
#include "stack_sizes.h"
#include "test_helper.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"


TaskHandle_t xModemMgmtTaskHandle;


void
PortBIntHandler(void) {
    uint32_t ulStatus;

    GPIOPinWrite( GPIO_PORTF_BASE, UINT32_MAX, 11);

    ulStatus = GPIOIntStatus(GPIO_PORTB_BASE, true);

    GPIOIntClear(GPIO_PORTB_BASE, ulStatus);

    UARTprintf("PB int: %X   ", ulStatus);

    if (ulStatus & GPIO_INT_PIN_2) {
        if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {
            UARTprintf("powerState: ON\n");
            xModemStatus.powerState = true;
        }
        else {
            UARTprintf("powerState: OFF\n");
            xModemStatus.powerState = false;
        }
    }
    else {
        UARTprintf("unexpected Port B GPIO interrupt\n");
    }

    GPIOPinWrite( GPIO_PORTF_BASE, UINT32_MAX, ulLastPortFValue );
}

bool
ModemPowerOn(void) {
    /* Check PB2 (Modem power status) to ensure that the modem is off. */
    if (!GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {

        UARTprintf("Attempting power on...\n");

        /* Pull PB0 (Modem Key) low for 200ms */
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, GPIO_PIN_0);

        /* Modem power on takes up to 5s. */
        vTaskDelay(pdMS_TO_TICKS(5000));

        if (!GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {
            UARTprintf("Error. Modem power on attempt failed.\n");
            return false;
        }

        UARTprintf("Success. Waiting for boot sequence.\n");

        /* Modem boot sequence takes up to 8s. */
        vTaskDelay(pdMS_TO_TICKS(8000));

        /* Modem should be accessible now. */
        return true;
    }
    else {
        UARTprintf("Error. Modem power on attempted but modem is already on.\n");
        return false;
    }
}

bool
ModemPowerOff(void) {
    /* Check PB2 (Modem power status) to ensure that the modem is on. */
    if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {

        UARTprintf("Attempting power off...\n");

        /* Pull PB0 (Modem Key) low for 600ms */
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, 0);
        vTaskDelay(pdMS_TO_TICKS(600));
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, GPIO_PIN_0);

        /* Modem power off takes up to 8s. */
        vTaskDelay(pdMS_TO_TICKS(8000));

        if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {
            UARTprintf("Error. Modem power off attempt failed.\n");
            return false;
        }

        /* Modem should be off now. */
        return true;
    }
    else {
        UARTprintf("Error. Modem power off attempted but modem is already off.\n");
        return false;
    }
}

bool
ModemReset(void) {
    /* Check PB2 (Modem power status) to ensure that the modem is on. */
    if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {

        UARTprintf("Attempting modem reset...\n");

        /* Pull PB1 (Modem RST) low for 100ms */
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_1, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_1, GPIO_PIN_1);

        if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {
            UARTprintf("Error. Modem reset attempted but modem did not power off.\n");
            return false;
        }

        /* Modem reset takes up to 13s. */
        vTaskDelay(pdMS_TO_TICKS(13000));

        if (!GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {
            UARTprintf("Error. Modem reset attempted but modem did not power on again.\n");
            return false;
        }

        /* Modem should be accessible now. */
        return true;
    }
    else {
        UARTprintf("Error. Modem reset attempted but modem is off.\n");
        return false;
    }
}

static void
ModemMgmtTask(void *pvParameters) {
    uint32_t ulNotificationValue;
    bool bPulseGood = false;

    /* Set the powerState flag once initially. */
    if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_2)) {
        UARTprintf("powerState: ON\n");
        xModemStatus.powerState = true;
    }
    else {
        UARTprintf("powerState: OFF\n");
        xModemStatus.powerState = false;
    }

    /* Main task loop. */
    while (1) {

        if (xModemStatus.tcpConnectionOpen) {

            /* Check for a heartbeat notification from the Modem UART task.
             * This call blocks while waiting, but only up to 5 seconds. */
            xTaskNotifyWait(MGMT_NOTIFY_NONE, MGMT_NOTIFY_ALL,
                            &ulNotificationValue, pdMS_TO_TICKS(5000));

            /* If 5 seconds elapse without a heartbeat, force the Modem UART
             * task to exit its main loop (and subsequently reset the modem). */
            if (!(ulNotificationValue &= MGMT_NOTIFY_HEARTBEAT)) {
                UARTprintf("heartbeat not detected\n");
                xModemStatus.tcpConnectionOpen = false;
                xModemStatus.knownState = false;

                /* Notify the Remote Start task so that it can disable the
                 * ignition. */
                xTaskNotify( xRemoteStartTaskHandle, RS_NOTIFY_IGNITION_OFF,
                             eSetBits );

                bPulseGood = false;
            }
            /* If there was a heartbeat, delay 500ms. This is only done to
             * prevent continuous looping if the server erroneously sends
             * heartbeats too quickly. */
            else {

                bPulseGood = true;

                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
        /* Delay for 1 second before checking for an open connection. */
        else {

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

static void
ModemGPIOConfigure(void) {

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

    // THIS SHOULD NOT BE NEEDED, AS THIS FUNCTION IS ONLY FOR RUNTIME INTERRUPT REGISTRATION
    /* Register interrupt handler. This also enables the interrupt at the
     * NVIC. */
    GPIOIntRegister(GPIO_PORTB_BASE, PortBIntHandler);

    /* Set PB2 to interrupt on any edge. */
    GPIOIntTypeSet(GPIO_PORTB_BASE, GPIO_PIN_2, GPIO_BOTH_EDGES);

    /* Enable individual pin interrupts in the GPIO module. */
    GPIOIntEnable(GPIO_PORTB_BASE, GPIO_PIN_2);

}

uint32_t
ModemMgmtTaskInit(void) {

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
