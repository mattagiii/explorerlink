/*
 * explorerlink_main.c
 * Starting point for ExplorerLink - a main() function that initializes all
 * FreeRTOS tasks.
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
#include "inc/hw_types.h"
#include "inc/hw_uart.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/hibernate.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "utils/uartstdio.h"
#include "analog_task.h"
#include "can_task.h"
#include "channel.h"
#include "data_task.h"
#include "debug_helper.h"
#include "jsn_task.h"
#include "modem_mgmt_task.h"
#include "modem_uart_task.h"
#include "priorities.h"
#include "remote_start_task.h"
#include "srf_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/*
 * The error routine that is called if the driver library encounters an error.
 */
#ifdef DEBUG
void __error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

/*
 * This hook is called by FreeRTOS when an stack overflow error is detected.
 */
void vApplicationStackOverflowHook(xTaskHandle *pxTask, char *pcTaskName) {

    /* This function cannot return, so loop forever.  Interrupts are disabled
     * on entry to this function, so no processor interrupts will interrupt
     * this loop. */
    while(1) {
    }

}

/*
 * Initialize FreeRTOS and start the initial set of tasks.
 */
int
main(void) {

    /* Enable lazy stacking for interrupt handlers.  This allows floating-point
     * instructions to be used within interrupt handlers, but at the expense of
     * extra stack usage. */
    FPUEnable();
    FPULazyStackingEnable();

    /* Set the clocking to run at 80 MHz from the PLL. The output of the PLL is
     * always predivided by 2, so even though it is 400MHz, it can be
     * considered to be 200MHz. SYSCTL_SYSDIV_2_5 divides the output of the PLL
     * by 2.5, yielding 80MHz. SYSCTL_XTAL_16MHZ indicates that the main
     * oscillator is a 16MHz crystal. SYSCTL_OSC_MAIN selects this main
     * oscillator as the source (input to the PLL). */
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ |
                       SYSCTL_OSC_MAIN);

    /* Disable all interrupts while preparing tasks. Calls to xTaskCreate()
     * should also disable interrupts globally, but we want to be able to
     * call IntEnable() as needed even before that. */
    IntMasterDisable();

    /* Create the Modem UART task (modem_uart_task.h). */
    if(ModemUARTTaskInit() != 0) { while(1) {} }

    /* Create the CAN processing task (can_task.h). */
    if(CANTaskInit() != 0) { while(1) {} }

    /* Create the ADC task (analog_task.h). */
    if(AnalogTaskInit() != 0) { while(1) {} }

    /* Create the Modem Management task (modem_mgmt_task.h). */
    if(ModemMgmtTaskInit() != 0) { while(1) {} }

    /* Create the SRF ultrasonic sensing task (srf_task.h). */
    if(SRFTaskInit() != 0) { while(1) {} }

    /* Create the Remote Start task (remote_start_task.h). */
    if(RemoteStartTaskInit() != 0) { while(1) {} }

    /* Create the data collection task (data_task.h). */
    if(DataTaskInit() != 0) { while(1) {} }

    /* Set the priorities of interrupts whose ISRs contain FreeRTOS API calls.
     * The priorities are defined alongside task priorities in priorities.h.
     * Only the high 3 bits are used in the priority registers, so shift by 5
     * to move the priority value to those bits. */
    IntPrioritySet( INT_UART6, PRIORITY_MODEM_UART_INT << 5 );
    IntPrioritySet( INT_UART3, PRIORITY_SRF_UART_INT << 5 );
    IntPrioritySet( INT_HIBERNATE, PRIORITY_DATA_SAMPLING_INT << 5 );
    IntPrioritySet( INT_CAN0, PRIORITY_CAN0_INT << 5 );
    IntPrioritySet( INT_WTIMER1A, PRIORITY_IGNITION_TIMER_INT << 5 );

    /* The xTaskCreate() calls have globally masked interrupts using PRIMASK,
     * so these will not trigger until vTaskStartScheduler() unmasks them
     * before launching the first task. This prevents any FreeRTOS API calls
     * from occurring before the scheduler has started. */

    /* Initialize the debug helper. */
    debug_init();

    /* Start the scheduler. This should not return. */
    vTaskStartScheduler();

    /* In case the scheduler returns for some reason, loop forever. */
    while(1) {
    }
}
