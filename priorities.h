

#ifndef __PRIORITIES_H__
#define __PRIORITIES_H__


/* The priorities of the various tasks. Higher numbers mean higher priorities.
 * The accepted values are in the range [0, configMAX_PRIORITIES-1]. As
 * configured, this is [0, 15]. */
#define PRIORITY_MODEM_UART_TASK        1
#define PRIORITY_CAN_TASK               4
#define PRIORITY_DATA_TASK              2
#define PRIORITY_ANALOG_TASK            1
#define PRIORITY_MODEM_MGMT_TASK        2
#define PRIORITY_JSN_TASK               2
#define PRIORITY_SRF_TASK               2
#define PRIORITY_REMOTE_START_TASK      3

/* Priorities for interrupts whose ISRs contain FreeRTOS API calls. These must
 * be >= configMAX_SYSCALL_INTERRUPT_PRIORITY. These interrupts will be
 * maskable by the kernel. 0 is the highest priority (0-7). Other interrupt
 * priorities (with ISRs not containing API calls) are left at their
 * defaults. */
#define PRIORITY_MODEM_UART_INT         6
#define PRIORITY_SRF_UART_INT           6
#define PRIORITY_DATA_SAMPLING_INT      6
#define PRIORITY_CAN0_INT               5
#define PRIORITY_IGNITION_TIMER_INT     5


#endif // __PRIORITIES_H__
