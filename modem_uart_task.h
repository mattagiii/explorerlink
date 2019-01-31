/*
 * modem_uart_task.h
 *
 *  Created on: Apr 23, 2018
 *      Author: Matt
 */

#ifndef MODEM_UART_TASK_H_
#define MODEM_UART_TASK_H_

#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

#define MODEM_NOTIFY_NONE               0x00000000
#define MODEM_NOTIFY_RX                 0x00000001
#define MODEM_NOTIFY_SAMPLE             0x00000002
#define MODEM_NOTIFY_UNSOLICITED        0x00000004
#define MODEM_NOTIFY_ALL                0xffffffff

/*
 * Modem status flags. C99 compilation should fit these into a single byte.
 */
typedef struct ModemStatus_t {
    /* Whether the modem is running (i.e., the PS pin is high). */
    bool powerState : 1;
    /* Whether the modem is in a known state. If received characters can't be
     * parsed as expected responses, this flag is set so that the task can
     * reinitialize. */
    bool knownState : 1;
    /* Whether serial echoing has been turned off. */
    bool echoOff : 1;
    /* Whether a network signal is present. */
    bool signalPresent : 1;
    /* Whether the network is configured for data mode (if not, this indicates
     * command mode). */
    bool networkMode : 1;
    /* Whether the 3G network connection is open. */
    bool networkOpen : 1;
    /* Whether the TCP connection to the server is open. */
    bool tcpConnectionOpen : 1;
    /* Whether the TCP connection to the server is in data mode. Otherwise,
     * command mode (value 0/false). */
    bool tcpConnectionMode : 1;
} ModemStatus_t;

extern TaskHandle_t xModemUARTTaskHandle;
extern volatile ModemStatus_t xModemStatus;

extern uint32_t ModemUARTTaskInit(void);

#endif /* MODEM_UART_TASK_H_ */
