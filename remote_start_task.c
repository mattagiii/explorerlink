/*
 * remote_start_task.c
 * FreeRTOS task for controlling ignition switch outputs, and a timer ISR
 * for ensuring robustness and safety.
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
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_timer.h"
#include "inc/hw_types.h"
#include "driverlib/adc.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "utils/uartstdio.h"
#include "can_task.h"
#include "channel.h"
#include "debug_helper.h"
#include "hibernate_rtc.h"
#include "remote_start_task.h"
#include "priorities.h"
#include "sample.h"
#include "stack_sizes.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define RUN_PIN                         GPIO_PIN_6
#define START_PIN                       GPIO_PIN_7

TaskHandle_t xRemoteStartTaskHandle;

IgnitionStatus_t xIgnitionStatus = { false, false, false, false };

/* Enumeration for "type" of timeout currently in effect. Wide Timer 1A is used
 * for safety checks including:
 * - Automatically disabling ignition after 10 minutes
 * - Disabling ignition after 1 minute if no clients are connected
 * - Verifying success of the two former functions 1 second later
 *
 * eTimeoutType gets set as needed (before beginning a countdown) to let the
 * timer ISR know how to act when the timeout occurs. The values in the enum
 * are also the required timer load values. With the prescaler, these values
 * are in microseconds. */
enum timeoutType {
    TIMEOUT_START = 7000000,            /* 7 seconds */
    TIMEOUT_10MIN = 600000000,          /* 10 minutes */
    TIMEOUT_NO_CLIENT = 60000000,       /* 1 minute */
    TIMEOUT_CHECK_OFF = 1000000         /* 1 second */
} eTimeoutType = TIMEOUT_10MIN;

typedef enum timeoutType tTimeoutType;


/*
 * ISR for the "safety backup" timer.
 */
void
WTimer1AIntHandler( void ) {

    uint32_t ulStatus;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Read the masked interrupt status of the timer module. */
    ulStatus = TimerIntStatus( WTIMER1_BASE, true );

    /* Clear any pending status. */
    TimerIntClear( WTIMER1_BASE, ulStatus );

    if ( ulStatus & TIMER_TIMA_TIMEOUT ) {
        /* This was a timeout interrupt. Check which kind of timeout we are
         * currently expecting. Note that xIgnitionStatus.running isn't
         * significant to any of these timeout checks because we can only be
         * concerned with whether the output is correct. Ignition control is
         * wired-OR with the signal from the ignition lock cylinder, so if the
         * driver has turned the ignition on manually, it is of course normal
         * for xIgnitionStatus.running to be true despite the GPIO output
         * being switched low/off. For safety, all we need to be concerned with
         * is the output. */

        /* This call yields the currently programmed load value (the value the
         * countdown started at), which corresponds to a timeoutType. In other
         * words, because each possible countdown has a unique starting value,
         * we use that value to determine what action to take on timeout. */
        eTimeoutType = ( tTimeoutType ) TimerLoadGet( WTIMER1_BASE, TIMER_A );

        switch ( eTimeoutType ) {
            case TIMEOUT_START :
                /* The START output has timed out. This can only occur if the
                 * task hangs after beginning cranking. This timeout serves as
                 * a fail-safe to guarantee that the output goes low. */

                if ( GPIOPinRead( GPIO_PORTB_BASE, START_PIN ) ) {
                    /* The START output was still high after 7 seconds. The
                     * task should only have attempted cranking for 5
                     * seconds. Disable both outputs. */
                    GPIOPinWrite( GPIO_PORTB_BASE, RUN_PIN, 0 );
                    GPIOPinWrite( GPIO_PORTB_BASE, START_PIN, 0 );

                    /* Disable further interrupts. */
                    TimerIntDisable( WTIMER1_BASE, TIMER_TIMA_TIMEOUT );

                    /* Send an error notification to the remote start task.
                     * This notification means that the system is in an error
                     * state and remote start functionality is disabled. */
                    xTaskNotifyFromISR( xRemoteStartTaskHandle,
                                        RS_NOTIFY_ERROR, eSetBits,
                                        &xHigherPriorityTaskWoken );
                }
                break;

            case TIMEOUT_CHECK_OFF :
                /* Final check to ensure that a previous attempt to turn the
                 * ignition off was successful. Here we first check if the
                 * output is high. If it is, a serious error has occurred and
                 * we proceed to disable remote start by putting the task in an
                 * error state. */

                if ( GPIOPinRead( GPIO_PORTB_BASE, RUN_PIN ) ) {
                    /* Attempting to turn the ignition off normally was
                     * unsuccessful. Bring the output low to turn it off
                     * manually. */
                    GPIOPinWrite( GPIO_PORTB_BASE, START_PIN, 0 );
                    GPIOPinWrite( GPIO_PORTB_BASE, RUN_PIN, 0 );

                    /* Disable further interrupts. */
                    TimerIntDisable( WTIMER1_BASE, TIMER_TIMA_TIMEOUT );

                    /* Send an error notification to the remote start task.
                     * This notification means that the system is in an error
                     * state and remote start functionality is disabled. */
                    xTaskNotifyFromISR( xRemoteStartTaskHandle,
                                        RS_NOTIFY_ERROR, eSetBits,
                                        &xHigherPriorityTaskWoken );
                }
                else {
                    /* Send a "pass" notification to the remote start task.
                     * This notification allows the task to proceed after this
                     * verification check is complete. */
                    xTaskNotifyFromISR( xRemoteStartTaskHandle,
                                        RS_NOTIFY_CHECK_PASS, eSetBits,
                                        &xHigherPriorityTaskWoken );
                }
                break;

            default :
                /* The ignition has timed out (either after 10 minutes or 1
                 * with no clients connected). We notify the main task to turn
                 * it off, and just in case, set a new timeout which will cause
                 * this ISR to verify that the ignition is off in 1 second. */

                /* Set the starting value to 1 second. */
                TimerLoadSet( WTIMER1_BASE, TIMER_A, TIMEOUT_CHECK_OFF );

                /* Begin counting. */
                TimerEnable( WTIMER1_BASE, TIMER_A );

                xTaskNotifyFromISR( xRemoteStartTaskHandle,
                                    RS_NOTIFY_IGNITION_OFF, eSetBits,
                                    &xHigherPriorityTaskWoken );

                break;

        } /* switch ( eTimeoutType ) */

    } /* if (ulStatus & TIMER_TIMA_TIMEOUT) */
    else {
        /* This was an unknown interrupt. */
    }

    /* If a notification was sent, xHigherPriorityTaskWoken may be set to
     * pdTRUE and if so, this call will tell the scheduler to switch context to
     * the Remote Start task. */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

/*
 * Turns the ignition on. Returns true only if the output is high and the CAN
 * task confirms that the ignition is actually on. Returns false otherwise.
 * If an error condition is detected, this function also self-notifies the
 * remote start task with RS_NOTIFY_ERROR.
 */
static bool
IgnitionOn( void ) {

    uint32_t ulIgnitionErrorCount = 0;

    /* Verify that the ignition output is currently off.
     *
     * If the CAN task tells us the ignition isn't running, this is the normal
     * use case and we perform a normal on sequence.
     *
     * If the CAN task tells us the ignition is running, the driver has
     * manually turned it on. Though it's not a likely use case, the driver
     * could want the ignition to stay on after removing the key. Thus, we
     * perform the same sequence. The CAN message confirmation will happen
     * immediately. */
    if ( !GPIOPinRead( GPIO_PORTB_BASE, RUN_PIN ) ) {
        /* Bring the output high to turn on the ignition. */
        GPIOPinWrite( GPIO_PORTB_BASE, RUN_PIN, RUN_PIN );

        /* Use CAN messages to confirm that the ignition is on. Loop
         * until messages have been received or 200ms has elapsed. */
        do {
            vTaskDelay( pdMS_TO_TICKS( 10 ) );
        } while ( !xIgnitionStatus.running &&
                  ulIgnitionErrorCount++ < 20 );

        /* Update the status flag. */
        xIgnitionStatus.lastOnFailed = ulIgnitionErrorCount >= 20 ?
                                       true : false;

        /* Bring the output low if the attempt failed. Currently we don't
         * consider this an error condition. */
        if ( xIgnitionStatus.lastOnFailed ) {
            GPIOPinWrite( GPIO_PORTB_BASE, RUN_PIN, 0 );
            debug_print( "Ignition ON failed\n" );
        }
        else {
            debug_print( "Ignition ON succeeded\n" );
        }

        return !xIgnitionStatus.lastOnFailed;
    }
    /* If both indicators say the ignition is on, return true. This means
     * the command was simply sent multiple times. This shouldn't happen
     * but isn't an error. */
    else if ( GPIOPinRead( GPIO_PORTB_BASE, RUN_PIN ) &&
              xIgnitionStatus.running ) {
        debug_print( "Ignition ON unnecessary\n" );
        return true;
    }
    /* If RUN_PIN is high already but the CAN task tells us the ignition isn't
     * running, there's a problem. Could be a (very) dead battery or problem
     * with the CAN task. Here we self-notify the task of an error so that it
     * will enter the error state before acting on any new commands. */
    else {
        debug_print( "\nError: Ignition ON commanded but current state cannot be determined\n" );
        GPIOPinWrite( GPIO_PORTB_BASE, RUN_PIN, 0 );
        xTaskNotify( xRemoteStartTaskHandle, RS_NOTIFY_ERROR, eSetBits );
        return false;
    }
}

/*
 * Turns the ignition off. Returns true only if the output is low and the CAN
 * task confirms that the ignition is actually off. Returns false otherwise.
 * If an error condition is detected, this function also self-notifies the
 * remote start task with RS_NOTIFY_ERROR.
 */
static bool
IgnitionOff( void ) {

    uint32_t ulIgnitionErrorCount = 0;

    /* Verify that the ignition is currently on. The current output
     * state must be high and the status (controlled by the CAN task)
     * must indicate that the ignition is in the RUN position. */
    if ( GPIOPinRead( GPIO_PORTB_BASE, RUN_PIN ) &&
         xIgnitionStatus.running ) {
        /* Bring the output low to turn off the ignition. */
        GPIOPinWrite( GPIO_PORTB_BASE, RUN_PIN, 0 );

        /* Use CAN messages to confirm that the ignition is off. Loop
         * until no messages have been received or 300ms has elapsed. */
        do {
            vTaskDelay( pdMS_TO_TICKS( 10 ) );
        } while ( xIgnitionStatus.running &&
                  ulIgnitionErrorCount++ < 30 );

        /* Update the status flag. */
        xIgnitionStatus.lastOffFailed = ulIgnitionErrorCount >= 30 ?
                                        true : false;

        if ( xIgnitionStatus.lastOffFailed ) {
            debug_print( "Ignition OFF failed\n" );
        }
        else {
            debug_print( "Ignition OFF succeeded\n" );
        }

        return xIgnitionStatus.lastOffFailed;
    }
    /* If both indicators say the ignition is off, return true. This means
     * the command was simply sent multiple times. This shouldn't happen
     * but isn't an error. */
    else if ( !GPIOPinRead( GPIO_PORTB_BASE, RUN_PIN ) &&
              !xIgnitionStatus.running ) {
        debug_print( "Ignition OFF unnecessary\n" );
        return true;
    }
    /* If the CAN task tells us the ignition isn't running, but the output was
     * switched high, there's a problem. Could be a (very) dead battery or
     * problem with the CAN task. Here we switch it low and self-notify the
     * task of an error so that it will enter the error state before having a
     * chance to act on any new commands. */
    else if ( !xIgnitionStatus.running ) {
        GPIOPinWrite( GPIO_PORTB_BASE, RUN_PIN, 0 );
        xTaskNotify( xRemoteStartTaskHandle, RS_NOTIFY_ERROR, eSetBits );
        debug_print( "\nError: Ignition OFF commanded but current state cannot be determined\n" );
        return false;
    }
    /* If RUN_PIN is low already but the CAN task tells us the ignition is
     * running, the driver must have the key switched to ON. This is normal if
     * the OFF command was due to a timeout or if the driver decided to send
     * an OFF command with the key ON. Still, we return false because the
     * ignition has not been confirmed OFF. */
    else {
        debug_print( "\nIgnition OFF commanded but key ON\n" );
        return false;
    }
}

/*
 * Start the vehicle. Turns on the ignition first if needed. Only returns true
 * if the starting sequence completes successfully and RPM confirms that the
 * engine is running.
 */
static bool
IgnitionStart( void ) {

    uint32_t ulIgnitionErrorCount = 0;
    uint16_t usInitialRPM = *( uint16_t * )( chRPM.xData );

    if ( !IgnitionOn() ) {
        debug_print( "Ignition START attempted but ON failed\n" );
        return false;
    }

    vTaskDelay( pdMS_TO_TICKS( 5000 ) );

    /* Verify that the START output is low and the RPM is 0. */
    if ( !GPIOPinRead( GPIO_PORTB_BASE, START_PIN ) && usInitialRPM == 0 ) {

        /* Set the starting value to 7 seconds. */
        TimerLoadSet( WTIMER1_BASE, TIMER_A, TIMEOUT_START );

        /* Begin counting. */
        TimerEnable( WTIMER1_BASE, TIMER_A );

        /* Bring the START output high to begin cranking. */
        GPIOPinWrite( GPIO_PORTB_BASE, START_PIN, START_PIN );

        /* Use the RPM to confirm when the engine has started. If cranking is
         * unsuccessful for 5 seconds, abort. */
        do {
            vTaskDelay( pdMS_TO_TICKS( 10 ) );
        } while ( ulIgnitionErrorCount++ < 500 &&
                  *( uint16_t * )( chRPM.xData ) <= 1000 );

        GPIOPinWrite( GPIO_PORTB_BASE, START_PIN, 0 );

        /* Update the status flag. */
        xIgnitionStatus.lastStartFailed = ulIgnitionErrorCount >= 500 ?
                                          true : false;

        /* If the start was successful, set the 10-minute timeout. */
        if ( !xIgnitionStatus.lastStartFailed ) {
            /* Set the starting value to 10 minutes. This immediately
             * overwrites the 7-second countdown that was started before
             * cranking. That countdown will only time out if the task hangs.
             * This countdown will always time out unless overridden by the
             * TIMEOUT_NO_CLIENT timeout. */
            TimerLoadSet( WTIMER1_BASE, TIMER_A, TIMEOUT_10MIN );

            /* Begin counting. */
            TimerEnable( WTIMER1_BASE, TIMER_A );

            debug_print( "Ignition START succeeded\n" );
        }
        else {
            debug_print( "Ignition START failed\n" );
            return false;
        }

        return true;
    }
    else {
        /* Guarantee that the output is low. */
        GPIOPinWrite( GPIO_PORTB_BASE, START_PIN, 0 );

        /* Print an error message. */
        if ( GPIOPinRead( GPIO_PORTB_BASE, START_PIN ) && usInitialRPM != 0 ) {
            debug_print( "\nError: Ignition START attempted but engine appears to be starting already\n" );
        }
        else if ( GPIOPinRead( GPIO_PORTB_BASE, START_PIN ) ) {
            debug_print( "\nError: Ignition START attempted but START output was already high\n" );
        }
        else if ( usInitialRPM != 0 ) {
            debug_print( "\nError: Ignition START attempted with RPM nonzero\n" );
        }
        else {
            debug_print( "\nError: Ignition START attempted with unknown failure cause\n" );
        }
    }

    return false;
}

static void
RemoteStartTask( void *pvParameters ) {

    uint32_t ulNotificationValue;

    IntEnable(INT_WTIMER1A);

    /* Main task loop. */
    while ( 1 ) {

        /* Before awaiting notifications, we set the RS_READY flag in the
         * notification data channel. This tells clients to enable remote
         * start controls. */
        vNotificationChannelSet(&chNotifications, NT_RS_READY);

        /* This conditional is like a single "more complicated"
         * xTaskNotifyWait() call. First we check for notifications with a
         * delay of 0 (return immediately). If there was a pending
         * notification, we move along as normal. Usually notifications arrive
         * slowly, however, and most of the time one is not already pending.
         *
         * If there was not a pending notification, we now have accessed the
         * notification value, and can use it to skip over the "real" call to
         * xTaskNotifyWait(). In other words, this sequence allows waiting for
         * the notification VALUE to have set bits, instead of waiting for the
         * notification STATE to be set to pending.
         *
         * The benefit of doing this is that it allows using xTaskNotifyWait()
         * elsewhere in this task for a specific notification while not
         * delaying any notifications that arrive while that call to
         * xTaskNotifyWait() blocks the task. See below in the
         * RS_NOTIFY_IGNITION_OFF section. */
        if ( xTaskNotifyWait( RS_NOTIFY_NONE, RS_NOTIFY_ALL,
                              &ulNotificationValue, 0 ) == pdFALSE ) {
            /* Timed out. No notification was pending. */

            /* We know there was no pending notification, but there may have
             * been bits set in the notification value. If there are we move on
             * to the rest of the task. If there aren't, we perform a "normal"
             * blocking xTaskNotifyWait(). */
            if ( !ulNotificationValue ) {
                xTaskNotifyWait( RS_NOTIFY_NONE, RS_NOTIFY_ALL,
                                 &ulNotificationValue, portMAX_DELAY );
            }
        }

        /* Now that a notification has been received, we clear the ready bit to
         * tell clients that remote start is busy. This *helps* avoid unwanted
         * commands being sent. */
        vNotificationChannelClear(&chNotifications, NT_RS_READY);

        /* Check the notification type. */
        if ( ulNotificationValue & RS_NOTIFY_ERROR ) {
            /* A critical error has occurred. Manually disable remote start
             * outputs and suspend this task. Currently there is no recovery
             * from this state. */

            debug_print("rs notified: RS_NOTIFY_ERROR\n");

            /* Disable further interrupts. */
            TimerIntDisable( WTIMER1_BASE, TIMER_TIMA_TIMEOUT );

            GPIOPinWrite( GPIO_PORTB_BASE, RUN_PIN, 0 );
            GPIOPinWrite( GPIO_PORTB_BASE, START_PIN, 0 );

            TimerDisable( WTIMER1_BASE, TIMER_A );

            vTaskSuspend( NULL );
        }
        else if ( ulNotificationValue & RS_NOTIFY_IGNITION_OFF ) {
            /* This notification is sent in two locations:
             * - Modem Management task, when server heartbeat is lost
             * - WTimer1IntHandler() ISR, in the default case
             * It's important to ensure that turning the ignition off is
             * successful, because failing to do so could result in
             * consequences ranging from inconvenience to moderately
             * compromised safety. Because of this, the timer ISR is used as a
             * redundancy to ensure that the ignition is switched off, even in
             * the event of task malfunctions. */

            debug_print("rs notified: RS_NOTIFY_IGNITION_OFF\n");

            /* Set the starting value to 1 second. */
            TimerLoadSet( WTIMER1_BASE, TIMER_A, TIMEOUT_CHECK_OFF );

            /* Begin counting. */
            TimerEnable( WTIMER1_BASE, TIMER_A );

            IgnitionOff();

            /* Await a notification from the ISR. This call doesn't touch
             * any other bits in the task's notification value, but it has to
             * be in a loop because xTaskNotifyWait() will return on any
             * notification - not just RS_NOTIFY_CHECK_PASS. To ensure that
             * other notifications that arrive during this loop are still
             * received as quickly as possible, the task's main call to
             * xTaskNotifyWait() is augmented above.
             *
             * This wait is necessary to ensure that the task can't proceed
             * while still awaiting confirmation from the ISR that the
             * ignition is off. */
            do {
                debug_print("ignition off check in progress\n");
                xTaskNotifyWait( RS_NOTIFY_NONE, RS_NOTIFY_CHECK_PASS,
                                 &ulNotificationValue, portMAX_DELAY );
            } while ( !( ulNotificationValue & RS_NOTIFY_CHECK_PASS ) );

            debug_print("ignition off check passed\n");
        }
        else if ( ulNotificationValue & RS_NOTIFY_NO_CLIENT ) {
            /* Modem UART task is signaling that no clients are connected.
             * If this is the case, we set a shorter 1-minute timeout before
             * the ignition will be disabled. */

            debug_print("rs notified: RS_NOTIFY_NO_CLIENT\n");

            /* If the timer is enabled (counting) already, we need to
             * check if it would already time out within 1 minute. */
            if ( HWREG( WTIMER1_BASE + TIMER_O_CTL ) & TIMER_CTL_TAEN &&
                 TimerLoadGet( WTIMER1_BASE, TIMER_A ) == TIMEOUT_10MIN &&
                 TimerValueGet( WTIMER1_BASE, TIMER_A ) > TIMEOUT_NO_CLIENT ) {

                /* The timer was counting down from 10 minutes and had more
                 * than 1 minute to go. Count down from 1 minute. */
                TimerLoadSet( WTIMER1_BASE, TIMER_A, TIMEOUT_NO_CLIENT );
            }
        }
        else if ( ulNotificationValue & RS_NOTIFY_CLIENT ) {
            /* One or more clients reconnected after all were disconnected.
             * If there is a 1-minute timeout running, cancel it. */

            debug_print("rs notified: RS_NOTIFY_CLIENT\n");

            if ( HWREG( WTIMER1_BASE + TIMER_O_CTL ) & TIMER_CTL_TAEN &&
                 TimerLoadGet( WTIMER1_BASE, TIMER_A ) == TIMEOUT_NO_CLIENT ) {

                /* Reinstate a 10-minute timeout. Really we should store the
                 * timer value when the client connection was lost and do quick
                 * math to reinstate the value as if nothing happened. To do. */
                TimerLoadSet( WTIMER1_BASE, TIMER_A, TIMEOUT_NO_CLIENT );
            }
        }
        else if ( ulNotificationValue == RS_NOTIFY_IGNITION_ON ) {

            debug_print("rs notified: RS_NOTIFY_IGNITION_ON\n");

            IgnitionOn();
        }
        else if ( ulNotificationValue == RS_NOTIFY_START ) {

            debug_print("rs notified: RS_NOTIFY_START\n");

            IgnitionStart();

        }
        /* Either an undefined notification or more than one notification was
         * detected. */
        else {
            debug_print("%08X\n", ulNotificationValue);
            debug_print("Error: Remote Start Task received an unexpected notification\n");
        }

    } /* while ( 1 ) */
}

static void
RemoteStartOutputConfigure( void ) {

    /* Enable clocking for the GPIO ports used for the output signals. */
    SysCtlPeripheralEnable( SYSCTL_PERIPH_GPIOB );

    /* Wait for port B to become ready. */
    while ( !SysCtlPeripheralReady( SYSCTL_PERIPH_GPIOB ) ) {
    }

    /* Set initial output state to low. */
    GPIOPinWrite( GPIO_PORTB_BASE, RUN_PIN | START_PIN, 0 );

    /* PB6 and PB7 are the outputs. Both are set to push-pull operation and
     * given internal weak pulldown resistors (in addition, there are stronger
     * external pulldowns). */
    GPIOPadConfigSet( GPIO_PORTB_BASE, RUN_PIN | START_PIN,
                      GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD_WPD );
    GPIODirModeSet( GPIO_PORTB_BASE, RUN_PIN | START_PIN,
                    GPIO_DIR_MODE_OUT );
}

/*
 * Configures a timer that serves multiple purposes for remote start safety.
 * These include:
 * - Automatically disabling ignition after 10 minutes
 * - Disabling ignition after 1 minute if no clients are connected
 * - Verifying success of the two former functions 5 seconds later
 */
static void
RemoteStartTimerConfigure(void) {

    /* Enable clocking for Wide Timer 1. */
    SysCtlPeripheralEnable( SYSCTL_PERIPH_WTIMER1 );

    /* Wait for port D to become ready. */
    while ( !SysCtlPeripheralReady( SYSCTL_PERIPH_WTIMER1 ) ) {
    }

    /* Configure Wide Timer 1 such that its A-half will count down in One-Shot
     * mode. */
    TimerConfigure( WTIMER1_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_ONE_SHOT );

    /* Prescaler value of 80 yields a 80/80,000,000 = 1us "tick". The max
     * timeout needed is 10 minutes = 600,000,000us. */
    TimerPrescaleSet( WTIMER1_BASE, TIMER_A, 80 );

    /* This should not be needed, but can't hurt. */
    TimerIntClear( WTIMER1_BASE, TIMER_TIMA_TIMEOUT );

    /* Enable interrupts on timeout. */
    TimerIntEnable( WTIMER1_BASE, TIMER_TIMA_TIMEOUT );
}

/*
 * Initialize the remote start task by configuring the timers and GPIOs
 * necessary for the control outputs. Then create the task itself for the
 * scheduler.
 */
uint32_t
RemoteStartTaskInit( void ) {

    RemoteStartOutputConfigure();
    RemoteStartTimerConfigure();

    if (xTaskCreate(RemoteStartTask, (const portCHAR *)"Remote Start",
                    REMOTESTARTTASKSTACKSIZE, NULL,
                    tskIDLE_PRIORITY + PRIORITY_REMOTE_START_TASK,
                    &xRemoteStartTaskHandle)
            != pdTRUE) {
        return 1;
    }

    return 0;
}
