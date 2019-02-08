/*
 * can_task.c
 * A FreeRTOS task that processes CAN message data, and the CAN ISR.
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
#include <string.h>
#include "inc/hw_can.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/can.h"
#include "driverlib/gpio.h"
#include "driverlib/hibernate.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "utils/uartstdio.h"
#include "can_task.h"
#include "channel.h"
#include "debug_helper.h"
#include "hibernate_rtc.h"
#include "priorities.h"
#include "remote_start_task.h"
#include "stack_sizes.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"


/* The number of the final controller message object that will be used for RX.
 * 12 objects are used in this application - one for each CAN ID. */
#define LAST_OBJ                        12

/* A mask for the message objects that are in use. The CAN ISR uses the CAN
 * task's notification value to tell the task which message objects contain new
 * data. With 12 objects (1-12, out of 32), this is 0b1111 1111 1111. There is
 * no object 0. */
#define OBJS_IN_USE                     0x00000FFF

#define DEFAULT_RX_TIMEOUT_MS           100

#define CAN_NOTIFY_NONE                 0x00000000
#define CAN_NOTIFY_RX                   0x00000001
#define CAN_NOTIFY_ALL                  0xFFFFFFFF


TaskHandle_t xCANTaskHandle;

/* CAN message object structure used to initialize the RX object and when
 * getting messages from it */
tCANMsgObject xCAN0RxMessage;

/* Holds the data received */
uint64_t ullRxMsgData;

/* This array contains the CAN IDs that message objects will filter by. The
 * array index corresponds with the message object number (1-32), and the value
 * at that index will be used to set up the ID filter during InitCAN0(). The
 * approximate frequency of messages with a given ID is shown in the comments.
 *
 * The IDs are spread across message objects to reduce the chance that
 * consecutive message receptions might overwrite a message object before the
 * application retrieves and processes the frame.
 *
 * Prioritization of processing does compound the time it can take for the
 * application to be ready to process subsequent frames on the same ID. For
 * example, if frames from 0x230, 0x212, 0x211, 0x201, 0x200, and 0x080
 * arrived in quick succession, all of those frames would have to be processed
 * before the next 0x230 frame if we use the ID ordering in this array (i.e.
 * prioritize processing IDs earlier in the array). As it stands, even that
 * unlikely sequence would not result in an overwrite given worst case
 * processing time (~200us plus untimely interruptions from other ISRs). The
 * processing order could be tweaked to optimize for higher-frequency IDs if
 * necessary, but typically the most important data is at lower IDs anyway,
 * because CAN arbitration prioritizes those.  */
uint32_t pulObj2ID[] = {
                        0x000, /* Empty. There is no message object 0. */
                        0x080, /* Message object 1, 35 Hz */
                        0x200, /* Message object 2, 122 Hz */
                        0x201, /* Message object 3, 61 Hz */
                        0x211, /* Message object 4, 71 Hz */
                        0x212, /* Message object 5, 35 Hz */
                        0x230, /* Message object 6, 122 Hz */
                        0x420, /* Message object 7, 10 Hz */
                        0x430, /* Message object 8, 51 Hz */
                        0x4B0, /* Message object 9, 71 Hz */
                        0x4B8, /* Message object 10, 71 Hz */
                        0x4E0, /* Message object 11, 1 Hz */
                        0x4FF  /* Message object 12, 2 Hz */
};

/* A global to keep track of the error flags that have been thrown so they may
 * be processed. */
volatile uint32_t ulErrFlag = 0;


/*
 * The CAN0 interrupt handler notifies the CAN task when a message is received
 * and sets error flags if errors occur.
 */
void
CAN0IntHandler( void ) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    uint32_t ulStatus;

    debug_set_bus( 4 );

    /* Read the CAN interrupt status to find the cause of the interrupt.
     * CAN_INT_STS_CAUSE register values:
     * 0x0000        = No Interrupt Pending
     * 0x0001-0x0020 = Number of message object that caused the interrupt (1-32)
     * 0x8000        = Status interrupt
     * A status interrupt occurs for every message, regardless of filtering.
     * Once it is cleared (and any errors stored), this ISR will be triggered
     * again if there is still a pending interrupt for a specific message
     * object (ID filtering taken into account). On that call, CANIntStatus
     * will return the message object's number so that it can be handled
     * below. */
    ulStatus = CANIntStatus( CAN0_BASE, CAN_INT_STS_CAUSE );

    /* If this was a status interrupt, acknowledge it by reading the CAN
     * controller status register. When receiving a message, a status interrupt
     * will trigger first, with a subsequent interrupt for the specific message
     * object. */
    if ( ulStatus == CAN_INT_INTID_STATUS ) {
        /* Read the controller status. This call clears the status interrupt,
         * so a separate clear is not needed. */
        ulStatus = CANStatusGet( CAN0_BASE, CAN_STS_CONTROL );

        /* Add ERROR flags to list of current errors. */
        ulErrFlag |= ulStatus;
    }

    /* Check if the cause is one of the message objects we are using for
     * receiving messages. This will occur after an RX status interrupt. */
    else if ( ulStatus > 0 && ulStatus <= LAST_OBJ ) {
        /* Getting to this point means that the RX interrupt occurred on
         * a message object, and the message reception is complete.
         * Clear the message object interrupt. CANMessageGet() would also clear
         * this interrupt, but as we defer processing to the CAN task we must
         * separately clear it here. */
        CANIntClear( CAN0_BASE, ulStatus );

        /* Set the object's bit so that the message will be handled in
         * the CAN task. The bits in the CAN task's notification value map to
         * the 32 message objects. Multiple bits can be set, instructing the
         * task that it has multiple messages to retrieve. We need to bit
         * shift because the interrupt value (ulStatus) is the integer value
         * of the message object. */
        xTaskNotifyFromISR( xCANTaskHandle, 1 << ( ulStatus - 1 ), eSetBits,
                            &xHigherPriorityTaskWoken );

        /* Since a message was received, clear any error flags.
         * This is done because before the message is received it triggers
         * a status interrupt for RX complete (RXOK). By clearing the flag
         * here, we prevent unnecessary error handling from happening. */
        ulErrFlag = 0;
    }
    else {
        // Placeholder for unexpected interrupt handling code
    }

    debug_set_bus( LAST_PORT_F_VALUE );

    /* If data was received, xHigherPriorityTaskWoken may be set to pdTRUE and
     * if so, this call will tell the scheduler to switch context to the
     * CAN task. */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

//*****************************************************************************
//
// Can ERROR handling. When a message is received if there is an error it is
// saved to ulErrFlag, the Error Flag Set.
//
//*****************************************************************************
void
CANErrorHandler(void) {
    // CAN controller has entered a Bus Off state.
    if(ulErrFlag & CAN_STATUS_BUS_OFF) {
        // Handle Error Condition here
        debug_print("    ERROR: CAN_STATUS_BUS_OFF \n");

        // Clear CAN_STATUS_BUS_OFF Flag
        ulErrFlag &= ~(CAN_STATUS_BUS_OFF);

    }

    // CAN controller error level has reached warning level.
    if(ulErrFlag & CAN_STATUS_EWARN) {
        // Handle Error Condition here
        debug_print("    ERROR: CAN_STATUS_EWARN \n");

        // Clear CAN_STATUS_EWARN Flag
        ulErrFlag &= ~(CAN_STATUS_EWARN);
    }

    // CAN controller error level has reached error passive level.
    if(ulErrFlag & CAN_STATUS_EPASS) {
        // Handle Error Condition here

        // Clear CAN_STATUS_EPASS Flag
        ulErrFlag &= ~(CAN_STATUS_EPASS);
    }

    // A message was received successfully since the last read of this status.
    if(ulErrFlag & CAN_STATUS_RXOK) {
        // Handle Error Condition here

        // Clear CAN_STATUS_RXOK Flag
        ulErrFlag &= ~(CAN_STATUS_RXOK);
    }

    // A message was transmitted successfully since the last read of this
    // status.
    if(ulErrFlag & CAN_STATUS_TXOK) {
        // Handle Error Condition here

        // Clear CAN_STATUS_TXOK Flag
        ulErrFlag &= ~(CAN_STATUS_TXOK);
    }

    // This is the mask for the last error code field.
    if(ulErrFlag & CAN_STATUS_LEC_MSK) {
        // Handle Error Condition here

        // Clear CAN_STATUS_LEC_MSK Flag
        ulErrFlag &= ~(CAN_STATUS_LEC_MSK);
    }

    // A bit stuffing error has occurred.
    if(ulErrFlag & CAN_STATUS_LEC_STUFF) {
        // Handle Error Condition here

        // Clear CAN_STATUS_LEC_STUFF Flag
        ulErrFlag &= ~(CAN_STATUS_LEC_STUFF);
    }

    // A formatting error has occurred.
    if(ulErrFlag & CAN_STATUS_LEC_FORM) {
        // Handle Error Condition here

        // Clear CAN_STATUS_LEC_FORM Flag
        ulErrFlag &= ~(CAN_STATUS_LEC_FORM);
    }

    // An acknowledge error has occurred.
    if(ulErrFlag & CAN_STATUS_LEC_ACK) {
        // Handle Error Condition here

        // Clear CAN_STATUS_LEC_ACK Flag
        ulErrFlag &= ~(CAN_STATUS_LEC_ACK);
    }

    // The bus remained a bit level of 1 for longer than is allowed.
    if(ulErrFlag & CAN_STATUS_LEC_BIT1) {
        // Handle Error Condition here

        // Clear CAN_STATUS_LEC_BIT1 Flag
        ulErrFlag &= ~(CAN_STATUS_LEC_BIT1);
    }

    // The bus remained a bit level of 0 for longer than is allowed.
    if(ulErrFlag & CAN_STATUS_LEC_BIT0) {
        // Handle Error Condition here

        // Clear CAN_STATUS_LEC_BIT0 Flag
        ulErrFlag &= ~(CAN_STATUS_LEC_BIT0);
    }

    // A CRC error has occurred.
    if(ulErrFlag & CAN_STATUS_LEC_CRC) {
        // Handle Error Condition here

        // Clear CAN_STATUS_LEC_CRC Flag
        ulErrFlag &= ~(CAN_STATUS_LEC_CRC);
    }

    // This is the mask for the CAN Last Error Code (LEC).
    if(ulErrFlag & CAN_STATUS_LEC_MASK) {
        // Handle Error Condition here

        // Clear CAN_STATUS_LEC_MASK Flag
        ulErrFlag &= ~(CAN_STATUS_LEC_MASK);
    }

    // If there are any bits still set in ulErrFlag then something unhandled
    // has happened. Print the value of ulErrFlag.
    if(ulErrFlag !=0) {
        debug_print("    Unhandled ERROR: %x \n", ulErrFlag);
    }
}

/*
 * This task performs CAN message processing deferred from the CAN ISR.
 */
static void CANTask( void *pvParameters ) {

    /* Task notification value. The bits in this value map to the 32 message
     * objects. */
    uint32_t ulNotificationValue = 0;
    /* Loop variable for checking which objects have new data. */
    uint32_t ulObjNum;
    /* Initially only wait 100ms for messages to arrive. */
    uint32_t ulCANRXTimeout = pdMS_TO_TICKS( DEFAULT_RX_TIMEOUT_MS );
    /* Count of message lost events, which occur if an object is overwritten
     * before the task retrieves a new message. */
    uint32_t ulCANMsgLossCount = 0;
    /* Count of old data reads, which should only occur as a potential side
     * effect of a message loss event. */
    uint32_t ulCANOldDataCount = 0;

    /* Main task loop. */
    while ( 1 ) {

        /* Wait for a CAN message to arrive. */
        if ( xTaskNotifyWait( CAN_NOTIFY_NONE, CAN_NOTIFY_ALL,
                              &ulNotificationValue, ulCANRXTimeout ) ) {

            /* When a notification arrives (not a timeout), (re)set the timeout
             * to 100ms so that if messages stop arriving, the next call will
             * quickly time out. */
            ulCANRXTimeout = pdMS_TO_TICKS( DEFAULT_RX_TIMEOUT_MS );

            /* xTaskNotifyWait received a notification. */
            if ( ulNotificationValue & OBJS_IN_USE ) {

                if ( xIgnitionStatus.running == false ) {
                    debug_print( "\nCAN messages started arriving\n" );
                }

                /* If a CAN message was received, the ignition is on. */
                xIgnitionStatus.running = true;

                /* Reuse the same message object that was used earlier to
                 * configure the CAN for receiving messages. A buffer for
                 * storing the received data must also be provided, so set the
                 * buffer pointer within the message object. */
                xCAN0RxMessage.pui8MsgData = ( uint8_t * ) &ullRxMsgData;

                /* Read the message objects that ulNotificationValue indicates
                 * have new data. Numerically low objects are read first. */
                for ( ulObjNum = 1; ulObjNum <= LAST_OBJ; ulObjNum++ ) {
                    if ( ( 1 << ( ulObjNum - 1 ) ) & ulNotificationValue ) {

                        /* Read the message from the message object. The
                         * interrupt clearing flag is not set because this
                         * interrupt was already cleared in the ISR. */
                        CANMessageGet( CAN0_BASE, ulObjNum, &xCAN0RxMessage,
                                       0 );

                        /* Check to see if there is an indication that some
                         * messages were lost. For this to occur, this task
                         * must be blocked for long enough that two messages
                         * arrive before the first is read. */
                        if ( xCAN0RxMessage.ui32Flags & MSG_OBJ_DATA_LOST ) {
                            debug_print( "\nCAN message loss detected\n" );
                            ulCANMsgLossCount++;
                            /* This flag is not cleared by CANMessageGet(), so
                             * clear it. */
                            xCAN0RxMessage.ui32Flags &= ~MSG_OBJ_DATA_LOST;
                            CANMessageSet( CAN0_BASE, ulObjNum,
                                           &xCAN0RxMessage,
                                           MSG_OBJ_TYPE_RX );
                        }

                        /* Ensure that new data has been read (which should
                         * always be the case because the CAN ISR has just
                         * notified this task). This flag may be set after
                         * losing a message though, if the ISR re-notifies this
                         * task to execute before it completes processing a
                         * prior message. */
                        if ( !( xCAN0RxMessage.ui32Flags & MSG_OBJ_NEW_DATA ) ) {
                            debug_print( "\nError: Old data was read from a CAN message object\n" );
                            ulCANOldDataCount++;
                        }
                        else {
                            vChannelStoreCANData( xCAN0RxMessage.ui32MsgID,
                                                  xCAN0RxMessage.pui8MsgData );
                        }

                    } /* if ( ( 1 << ulObjNum ) & ulNotificationValue ) */

                } /* for ( ulObjNum = 1; ulObjNum <= LAST_OBJ; ulObjNum++ ) */

            } /* if ( ulNotificationValue & OBJS_IN_USE ) */
            else {
                debug_print( "\nError: Unexpected CAN task notification\n" );
            }

        } /* if ( xTaskNotifyWait( ... ) */
        else {
            /* xTaskNotifyWait timed out. No CAN messages have been received
             * for 100ms. */
            xIgnitionStatus.running = false;
            debug_print( "\nCAN messages stopped arriving\n" );

            /* After a timeout, set the timeout to the maximum value so that
             * the next xTaskNotifyWait call will block indefinitely until CAN
             * messages begin arriving again. */
            ulCANRXTimeout = portMAX_DELAY;
        }

    } /* while ( 1 ) */

}

/*
 * Set up CAN0 to transmit at 500kbit/s. The first 12 message objects are used,
 * and each is assigned a CAN ID in the array pulObj2ID. This array is used to
 * initialize the objects.
 */
void
InitCAN0(void) {

    /* Loop variable for assigning CAN IDs to message objects. */
    uint32_t ulObjNum;

    /* GPIO pins B4 and B5 will be used, so enable the peripheral. */
    SysCtlPeripheralEnable( SYSCTL_PERIPH_GPIOB );

    /* Configure the GPIO pin muxing to select CAN0 functions for these pins. */
    GPIOPinConfigure( GPIO_PB4_CAN0RX );
    GPIOPinConfigure( GPIO_PB5_CAN0TX );

    /* Use the default direction and pad configuration for CAN pins (inputs
     * configured for push-pull operation with 8mA drive strength). */
    GPIOPinTypeCAN( GPIO_PORTB_BASE, GPIO_PIN_4 | GPIO_PIN_5 );

    /* Enable the CAN peripheral. */
    SysCtlPeripheralEnable( SYSCTL_PERIPH_CAN0 );

    /* Wait for CAN0 to become ready. */
    while( !SysCtlPeripheralReady( SYSCTL_PERIPH_CAN0 ) ) {
    }

    /* Initialize the CAN controller. This erases any garbage data in the
     * message object memory after reset and allows setting the bit rate. */
    CANInit( CAN0_BASE );

    /* Set up the bit rate for the CAN bus to 500kbit/s */
    CANBitRateSet( CAN0_BASE, SysCtlClockGet(), 500000 );

    /* Enable interrupts on the CAN peripheral. */
    CANIntEnable( CAN0_BASE, CAN_INT_MASTER | CAN_INT_ERROR | CAN_INT_STATUS );

    /* Enable the CAN interrupt at the NVIC. */
    IntEnable( INT_CAN0 );

    /* Enable the CAN module (clear the init bit). This enables bus
     * communication, but no messages will be received or sent until
     * CANMessageSet() sets the MSGVAL flag for a message object. */
    CANEnable( CAN0_BASE );

    /* Initialize a message object to be used for receiving CAN messages. All
     * fields of xCAN0RxMessage except ui32MsgID are set here. The ID will be
     * set when we loop through the message objects. */
    xCAN0RxMessage.ui32MsgIDMask = UINT32_MAX;
    xCAN0RxMessage.ui32Flags = MSG_OBJ_RX_INT_ENABLE | MSG_OBJ_USE_ID_FILTER;
    xCAN0RxMessage.ui32MsgLen = sizeof( ullRxMsgData );

    /* Now load message objects into the CAN peripheral. Once loaded the
     * module will receive any message on the bus, and an interrupt will occur.
     * ulObjNum will identify the message object that the messages are placed
     * into. */
    for ( ulObjNum = 1; ulObjNum <= LAST_OBJ; ulObjNum++ ) {

        /* pulObj2ID maps message object numbers (array indices) to their
         * assigned CAN IDs for filtering. */
        xCAN0RxMessage.ui32MsgID = pulObj2ID[ ulObjNum ];

        /* Instruct the controller to populate the message object. */
        CANMessageSet( CAN0_BASE, ulObjNum, &xCAN0RxMessage, MSG_OBJ_TYPE_RX );
    }
}

/*
 * Initialize the CAN task by setting up the CAN controller and creating the
 * task itself.
 */
uint32_t
CANTaskInit(void) {

    InitCAN0();

    if ( xTaskCreate( CANTask, ( const portCHAR * ) "CAN", CANTASKSTACKSIZE,
                      NULL, tskIDLE_PRIORITY + PRIORITY_CAN_TASK,
                      &xCANTaskHandle ) != pdTRUE ) {
        return 1;
    }

    return 0;
}
