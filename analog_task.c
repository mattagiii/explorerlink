/*
 * analog_task.c
 *
 *  Created on: Jun 9, 2018
 *      Author: Matt
 */


#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/adc.h"
#include "driverlib/gpio.h"
#include "driverlib/i2c.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/pwm.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "utils/uartstdio.h"
#include "analog_task.h"
#include "channel.h"
#include "hibernate_rtc.h"
#include "priorities.h"
#include "sample.h"
#include "stack_sizes.h"
#include "test_helper.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"


/* Microvolts per ADC code. e.g. ADC code 2 = 1612 uV */
#define UV_PER_ADC_CODE                 806
/* Microvolts per ADC code including downscaling op amp circuit (factor 4.6).
 * e.g. ADC code 2 = 7412 uV */
#define UV_PER_ADC_CODE_AMP             3706
/* Range of selectable temperatures for the temperature knob */
#define TEMP_SET_RANGE                  25
/* Value to add to temperature knob percentage to obtain actual set point (also
 * the minimum selectable temperature). With TEMP_SET_RANGE this yields knob
 * values from 60 to 85 degrees Fahrenheit. */
#define TEMP_SET_OFFSET                 60
/* Adjustment threshold for the temperature setting knob, in millidegrees
 * Fahrenheit. This keeps the set point from fluctuating due to measurement
 * noise. */
#define TEMP_SET_THRESHOLD              300
/* Macro to convert MCP9701A output voltage (*1000) to millidegrees Celsius
 * (degC*1000) */
#define TEMP_V_TO_C( x )                ( uint32_t )( ( float )( x - 400000 ) / 19.5 )
/* Macro to convert millidegrees Celsius to millidegrees Fahrenheit
 * (both *1000, hence 32000) */
#define TEMP_C_TO_F( x )                x * 9 / 5 + 32000
/* Limits the integral error accumulation to prevent windup */
#define INTEGRAL_ERROR_RESTRICT         100000
/* Value to add to the control value (centered on 0) to center it on a positive
 * value. This is arbitrary and does not represent a threshold between cooling
 * and heating. It only serves to align the output control value with the
 * positive integer range of DAC codes (0-4095). Tweaking this may improve
 * performance marginally but isn't necessary for stability.*/
#define CONTROL_NORMALIZE               2048
/* Proportional constant for control loop */
#define KP                              1
/* Integral constant for control loop */
#define KI                              1000
/* The 7-bit address of the MAX5815. Does not include an eighth R/W bit.
 * Determined by Table 1 in MAX5815 datasheet (ADDR0 and ADDR1 are N.C.). */
#define MAX5815_ADDR                    0x1A
/* Maximum allowable code value (12-bit DAC) */
#define MAX5815_CODE_MAX                0x00000FFF
/* Command byte for setting the internal reference to 2.5V */
#define MAX5815_CMD_REF_2V5             0x00000071
/* Command byte for simultaneously updating the CODE and DAC registers. The
 * bottom 4 bits should be ORed with DAC selection bits. */
#define MAX5815_CMD_CODEN_LOADN         0x00000030

/* DAC selection values for use in MAX5815 commands. */
typedef enum {
    MAX5815_DAC_A = 0x00000000,
    MAX5815_DAC_B = 0x00000001,
    MAX5815_DAC_C = 0x00000002,
    MAX5815_DAC_D = 0x00000003,
    MAX5815_DAC_ALL = 0x00000004
} MAX5815DACSelection_t;


TaskHandle_t xAnalogTaskHandle;

bool g_abnormalInterval0 = false;
uint32_t g_ulIntervalBetweenSamples0;
bool g_abnormalInterval1 = false;
uint32_t g_ulIntervalBetweenSamples1;

void ADC0SS0IntHandler( void ) {
    uint32_t ulStatus;
    /* Number of new samples retrieved from the FIFO by ADCSequenceDataGet() */
    uint32_t ulSampleCount;
    /* Buffer for the values read from the sequencer's FIFO. SS0 has FIFO size
     * 8. */
    uint32_t pulADC0SS0Values[8];
    static uint32_t ulTempKnobAvgSum;
    static uint32_t ulAVTEMP1AvgSum;
    static uint32_t ulAVTEMP2AvgSum;
    static uint32_t ulAVTEMP3AvgSum;
    static uint32_t ulAVTEMP4AvgSum;
    static uint32_t ul5xAvgCount = 0;
    static uint32_t ulRuntimeStatsLast = 0;

    GPIOPinWrite( GPIO_PORTF_BASE, UINT32_MAX, 2 );

    if ( ulRuntimeStatsCounter - ulRuntimeStatsLast > 501 && !g_abnormalInterval0 ) {
        g_ulIntervalBetweenSamples0 = ulRuntimeStatsCounter - ulRuntimeStatsLast;
        g_abnormalInterval0 = true;
    }
    ulRuntimeStatsLast = ulRuntimeStatsCounter;


    /* Read the masked interrupt status of the ADC module. */
    ulStatus = ADCIntStatus( ADC0_BASE, 0, true );

    /* Clear any pending status for Sequence 0. */
    ADCIntClear( ADC0_BASE, 0 );

    ulSampleCount = ADCSequenceDataGet( ADC0_BASE, 0, pulADC0SS0Values );

    if ( ulSampleCount == 6 ) {
        ulTempKnobAvgSum += pulADC0SS0Values[ 0 ];
        ulAVTEMP1AvgSum += pulADC0SS0Values[ 2 ];
        ulAVTEMP2AvgSum += pulADC0SS0Values[ 3 ];
        ulAVTEMP3AvgSum += pulADC0SS0Values[ 4 ];
        ulAVTEMP4AvgSum += pulADC0SS0Values[ 5 ];
        ul5xAvgCount++;

        if ( ul5xAvgCount >= 5 ) {
            ulTempKnobAvgSum /= ul5xAvgCount;
            ulAVTEMP1AvgSum /= ul5xAvgCount;
            ulAVTEMP2AvgSum /= ul5xAvgCount;
            ulAVTEMP3AvgSum /= ul5xAvgCount;
            ulAVTEMP4AvgSum /= ul5xAvgCount;
            vChannelStore( &chTempKnobRaw, &ulTempKnobAvgSum );
            vChannelStore( &chAVTEMP1Raw, &ulAVTEMP1AvgSum );
            vChannelStore( &chAVTEMP2Raw, &ulAVTEMP2AvgSum );
            vChannelStore( &chAVTEMP3Raw, &ulAVTEMP3AvgSum );
            vChannelStore( &chAVTEMP4Raw, &ulAVTEMP4AvgSum );

            ul5xAvgCount = 0;
            ulTempKnobAvgSum = 0;
            ulAVTEMP1AvgSum = 0;
            ulAVTEMP2AvgSum = 0;
            ulAVTEMP3AvgSum = 0;
            ulAVTEMP4AvgSum = 0;
        }

        vChannelStore( &chAVGP2Raw, &( pulADC0SS0Values[ 1 ] ) );
    }
    else {
        /* Error. Samples were not read from the FIFO in time. */
    }

    GPIOPinWrite( GPIO_PORTF_BASE, UINT32_MAX, ulLastPortFValue );
}

void ADC0SS1IntHandler( void ) {
    uint32_t ulStatus;
    /* Number of new samples retrieved from the FIFO by ADCSequenceDataGet() */
    uint32_t ulSampleCount;
    /* Buffer for the values read from the sequencer's FIFO. SS1 has FIFO size
     * 4. */
    uint32_t pulADC0SS1Values[4];
    static uint32_t ulRuntimeStatsLast = 0;

    GPIOPinWrite( GPIO_PORTF_BASE, UINT32_MAX, 2);                               //

    if ( ulRuntimeStatsCounter - ulRuntimeStatsLast > 11 && !g_abnormalInterval1 ) {
        g_ulIntervalBetweenSamples1 = ulRuntimeStatsCounter - ulRuntimeStatsLast;
        g_abnormalInterval1 = true;
    }
    ulRuntimeStatsLast = ulRuntimeStatsCounter;


    /* Read the masked interrupt status of the ADC module. */
    ulStatus = ADCIntStatus( ADC0_BASE, 1, true );

    /* Clear any pending status for Sequence 1. */
    ADCIntClear( ADC0_BASE, 1 );

    ulSampleCount = ADCSequenceDataGet( ADC0_BASE, 1, pulADC0SS1Values );

    if ( ulSampleCount == 2 ) {
        vChannelStore( &chVehicleBatt, &( pulADC0SS1Values[0] ) );
        vChannelStore( &chDeviceCurrent, &( pulADC0SS1Values[1] ) );
    }
    else {
        /* Error. Samples were not read from the FIFO in time. */
    }

    GPIOPinWrite( GPIO_PORTF_BASE, UINT32_MAX, ulLastPortFValue );
}

static void MAX5815Send( uint8_t ucCommand, uint8_t ucData1, uint8_t ucData2 ) {

    /* The MAX5815 command format includes three I2C data bytes: a command byte
     * followed by two bytes of data, which usually contain 12-bit DAC codes. */

    /* Load the command byte into I2CMDR. */
    I2CMasterDataPut( I2C2_BASE, ucCommand );
    /* Instruct the peripheral to output a START condition followed by the
     * address in I2CMSA followed by the data in I2CMDR. */
    I2CMasterControl( I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_START );
    /* Wait for the peripheral to finish the transmission before proceeding.
     * This is indeed a busy-wait, but due to the infrequency of transmissions
     * and the low priority of the Analog task, it is acceptable. */
    while( I2CMasterBusy( I2C2_BASE ) ) {}

    /* Load the first data byte into I2CMDR. */
    I2CMasterDataPut( I2C2_BASE, ucData1 );
    /* Instruct the peripheral to output the data in I2CMDR only. */
    I2CMasterControl( I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_CONT );
    while( I2CMasterBusy( I2C2_BASE ) ) {}

    /* Load the first data byte into I2CMDR. */
    I2CMasterDataPut( I2C2_BASE, ucData2 );
    /* Instruct the peripheral to output the data in I2CMDR, followed by a STOP
     * condition. */
    I2CMasterControl( I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH );
    while( I2CMasterBusy( I2C2_BASE ) ) {}
}

static void MAX5815SetREF() {

    /* Send a command to set the internal reference. Data bytes are don't
     * cares. */
    MAX5815Send( ( uint8_t ) ( MAX5815_CMD_REF_2V5 ), 0, 0 );
}

static bool MAX5815SetDAC( MAX5815DACSelection_t ulSelectedDACs,
                           uint32_t ulCode ) {

    if ( ulCode > MAX5815_CODE_MAX ) {
        return false;
    }

    /* Send a command to update the CODE and DAC registers. Note the
     * truncation of ulCode prior to the shift in the third parameter. */
    MAX5815Send( ( uint8_t ) ( MAX5815_CMD_CODEN_LOADN | ulSelectedDACs ),
                 ( uint8_t ) ( ulCode >> 4 ), ( ( uint8_t ) ulCode ) << 4 );

    return true;
}

/*
 * Get the cabin temperature based on MCP9701A sensor readings. Currently only
 * one sensor is used. The raw ADC value stored in the channel is converted
 * to a voltage, then to Celsius based on the datasheet equation (T = (Vout -
 * 400mV) / 19.5), then to Fahrenheit. The sensor is uncalibrated and has a
 * typical accuracy of +/- 1 degree Celsius. Note that TEMP_V_TO_C() utilizes
 * one floating point operation, so this function must be used infrequently.
 */
static uint32_t GetCabinTempmF( void ) {
    uint32_t ulSensor1UV = ulChannelValueGet( &chAVTEMP1Raw ) * UV_PER_ADC_CODE;
    uint32_t ulCabinTempmF = TEMP_C_TO_F( TEMP_V_TO_C( ulSensor1UV ) );

    vChannelStore( &chCabinTemp, &ulCabinTempmF );

    return ulCabinTempmF;
}

/*
 * Get the user's desired temperature setting by reading the knob position and
 * converting it to a temperature value. This is done by reading the current
 * ADC values (stored in raw channels) for both the knob and the battery
 * voltage. Both are needed because the knob is a potentiometer with battery
 * voltage at its positive end, and it's important to remove the effect of
 * battery voltage variation so that the set point doesn't change undesirably.
 * This function calculates the ratio of the knob voltage to the battery
 * voltage, and then outputs a temperature value based on the allowed range
 * [TEMP_SET_OFFSET, TEMP_SET_OFFSET + TEMP_SET_RANGE]. Small fluctuations are
 * ignored so that the set point is stable even with measurement noise. The
 * values in this function are scaled to eliminate floating point operations.
 * The return value is the set point expressed in millidegrees Fahrenheit.
 */
static uint32_t GetTempSetmF( void ) {
    /* Battery and knob voltages are computed in microvolts to avoid floating
     * point operations. */
    uint32_t ulBatteryVoltageUV = ulChannelValueGet( &chVehicleBatt ) *
                                   UV_PER_ADC_CODE_AMP;
    uint32_t ulTempKnobVoltageUV = ulChannelValueGet( &chTempKnobRaw ) *
                                   UV_PER_ADC_CODE_AMP;
    /* Knob percentage multiplied by 1000 */
    uint32_t ulTempSetPct = ulTempKnobVoltageUV / ( ulBatteryVoltageUV / 1000 );
    /* User's set value in millidegrees Fahrenheit. e.g. if knob is 21.8%
     * of battery voltage (such as 3.23V / 14.8V), ulTempSetPct will be 218 and
     * the returned value will be 65450 (65.45 degrees Fahrenheit). */
    uint32_t ulTempSetmF = ulTempSetPct * TEMP_SET_RANGE + ( TEMP_SET_OFFSET * 1000 );
    /* The previously set temperature */
    static uint32_t ulTempSetLastmF = 0;
    /* The difference between the current and previously set temperatures */
    int32_t lTempSetChangemF = ( int32_t )ulTempSetmF - ulTempSetLastmF;

    /* If the set point has changed by more than the threshold amount in either
     * direction, update the returned value. */
    if ( lTempSetChangemF > TEMP_SET_THRESHOLD ||
         lTempSetChangemF < -TEMP_SET_THRESHOLD ) {
        ulTempSetLastmF = ulTempSetmF;
        vChannelStore( &chTempKnob, &ulTempSetLastmF );
    }

    return ulTempSetLastmF;
}

static void AnalogTask( void *pvParameters ) {

    /* The user's desired temperature, in millidegrees Fahrenheit */
    uint32_t ulTempSetPointmF;
    /* The estimated cabin temperature, in millidegrees Fahrenheit */
    uint32_t ulTempActualEstimatemF;
    /* The difference between the former two, in millidegrees Fahrenheit */
    int32_t lTempErrormF;
    /* The integral (cumulative) error */
    int32_t lTempErrorIntegral;
    int32_t lPControl;
    int32_t lIControl;
    /* The proportional controller control value, unbounded */
    int32_t lControl;
    /* lControl bounded to allowable DAC code values */
    uint32_t ulControl;

    MAX5815SetREF();

    /* Main task loop. This loop runs a PI control algorithm at a
     * 1-second interval. The algorithm uses the desired and current cabin
     * temperatures to calculate a DAC output value for control of the
     * temperature blend door, which takes a 12V analog value as its input and
     * balances cold and hot air.  */
    while ( 1 ) {

        /* This task delay causes the algorithm to update every second. */
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );

        /* Get the desired and actual temperatures and compute the
         * difference. */
        ulTempSetPointmF = GetTempSetmF();
        ulTempActualEstimatemF = GetCabinTempmF();
        lTempErrormF = ( int32_t )ulTempSetPointmF -
                       ( int32_t )ulTempActualEstimatemF;

        /* Add the error from this iteration to the total error. */
        lTempErrorIntegral += lTempErrormF;

        /* Restrict the integral error to prevent windup.
         * INTEGRAL_ERROR_RESTRICT is chosen such that
         * KI * INTEGRAL_ERROR_RESTRICT + CONTROL_NORMALIZE ~=
         * MAX5815_CODE_MAX. This prevents windup past the maximum heating and
         * cooling settings, which only increases settling time. */
        if ( lTempErrorIntegral > INTEGRAL_ERROR_RESTRICT ) {
            lTempErrorIntegral = INTEGRAL_ERROR_RESTRICT;
        }
        else if ( lTempErrorIntegral < -INTEGRAL_ERROR_RESTRICT ) {
            lTempErrorIntegral = -INTEGRAL_ERROR_RESTRICT;
        }

        lPControl = lTempErrormF / KP;
        lIControl = lTempErrorIntegral / KI;

        /* Compute a control value with proportional and integral terms, and
         * normalize it to be centered at half the DAC's maximum output code
         * (which yields 1.25V, or 7.31V after the output amplifier). This is
         * arbitrary; various conditions affect the temperature of the air
         * output and the integral term is responsible for combating what
         * would otherwise be steady-state error due to output error. */
        lControl = lPControl + lIControl + CONTROL_NORMALIZE;

        /* Restrict the control value to valid DAC codes. */
        if ( lControl > MAX5815_CODE_MAX ) {
            ulControl = MAX5815_CODE_MAX;
        }
        else if ( lControl < 0 ) {
            ulControl = 0;
        }
        else {
            ulControl = lControl;
        }

        /* Send the command to update the DAC output. */
        MAX5815SetDAC( MAX5815_DAC_A, ulControl );

        if ( g_abnormalInterval0 ) {
//            UARTprintf( "abnormal interval: %d\n", g_ulIntervalBetweenSamples );
            g_abnormalInterval0 = false;
        }

        if ( g_abnormalInterval1 ) {
//            UARTprintf( "abnormal interval: %d\n", g_ulIntervalBetweenSamples );
            g_abnormalInterval1 = false;
        }

    }
}

//static void
//ADCTimerConfigure(void) {
//
//    /* Enable clocking for Timer 0. */
//    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
//
//    /* Configure Timer 0 such that its A-half will count periodically. */
//    TimerConfigure(TIMER0_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PERIODIC);
//
//    /* Configure Timer 0A to send a trigger to the ADC when it interrupts. */
//    TimerControlTrigger(TIMER0_BASE, TIMER_A, true);
//
//    /* Each timer increment will take 10 clock cycles (at 80MHz, this will be
//     * 125ns). */
//    TimerPrescaleSet(TIMER0_BASE, TIMER_A, 9);
//
//    /* Configure Timer 0A's load value to 1ms. At 80MHz, this will be a load
//     * value of 8000 (8000 * 125ns = 1ms). */
//    TimerLoadSet(TIMER0_BASE, TIMER_A, 8000);
//
//    /* Begin counting. */
//    TimerEnable(TIMER0_BASE, TIMER_A);
//}

static void
PWMADCTriggerConfigure( void ) {

    /* Enable clocking for PWM module 0. */
    SysCtlPeripheralEnable( SYSCTL_PERIPH_PWM0 );

    /* On TM4C123, the clock divisor is the same for both PWM modules and
     * is controlled by a SysCtl register instead of within the PWM
     * peripheral. 80,000,000 / 64 = 1,250,000Hz PWM clock frequency. */
    SysCtlPWMClockSet( SYSCTL_PWMDIV_64 );

    /* Configure PWM module 0, generator 0 to count down, update
     * asynchronously, and stop counting upon debug halt. */
    PWMGenConfigure( PWM0_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN |
                                           PWM_GEN_MODE_NO_SYNC |
                                           PWM_GEN_MODE_DBG_STOP );

    /* 1,250,000 / 62,500 = 20Hz PWM0 gen 0 frequency. */
    PWMGenPeriodSet( PWM0_BASE, PWM_GEN_0, 62500 );

    /* Configure PWM0 gen 0 to trigger the ADC when its countdown reaches 0. */
    PWMGenIntTrigEnable( PWM0_BASE, PWM_GEN_0, PWM_TR_CNT_LOAD );

    /* Enable PWM0 gen 0. */
    PWMGenEnable(PWM0_BASE, PWM_GEN_0);

    /* Configure PWM module 0, generator 1 to count down, update
     * asynchronously, and stop counting upon debug halt. */
    PWMGenConfigure( PWM0_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN |
                                           PWM_GEN_MODE_NO_SYNC |
                                           PWM_GEN_MODE_DBG_STOP );

    /* 1,250,000 / 1,250 = 1000Hz PWM0 gen 1 frequency. */
    PWMGenPeriodSet( PWM0_BASE, PWM_GEN_1, 1250 );

    /* Configure PWM0 gen 1 to trigger the ADC when its countdown reaches 0. */
    PWMGenIntTrigEnable( PWM0_BASE, PWM_GEN_1, PWM_TR_CNT_LOAD );

    /* Enable PWM0 gen 1. */
    PWMGenEnable(PWM0_BASE, PWM_GEN_1);
}

static void
ADC0Configure( void ) {

    /* Enable clocking for the ADC peripheral. */
    SysCtlPeripheralEnable( SYSCTL_PERIPH_ADC0 );

    /* Enable the GPIO ports with ADC pins we are using. */
    SysCtlPeripheralEnable( SYSCTL_PERIPH_GPIOD );
    SysCtlPeripheralEnable( SYSCTL_PERIPH_GPIOE );

    /* Set the required pins in the aforementioned ports to ADC pins. */
    GPIOPinTypeADC( GPIO_PORTD_BASE, GPIO_PIN_0 |  /* AVGP1_SCALED */
                                     GPIO_PIN_1 |  /* AVGP2_SCALED */
                                     GPIO_PIN_2 |  /* AVBATT */
                                     GPIO_PIN_3 ); /* AVCS */
    GPIOPinTypeADC( GPIO_PORTE_BASE, GPIO_PIN_0 |  /* AVTEMP1 */
                                     GPIO_PIN_1 |  /* AVTEMP2 */
                                     GPIO_PIN_2 |  /* AVTEMP3 */
                                     GPIO_PIN_3 ); /* AVTEMP4 */

    /* Enable hardware dithering. */
    HWREG( ADC0_BASE+ 0x038 ) |= 0x40;

    /* Use 64x hardware oversampling to average 64 readings for each trigger. */
    ADCHardwareOversampleConfigure( ADC0_BASE, 64 );

    /* Configure sample sequence 0 on ADC0 for PWM-triggered sampling from
     * module 0 generator 0. Priority 1 (second highest) is given to this
     * sequence. */
//    ADCSequenceConfigure( ADC0_BASE, 0, ADC_TRIGGER_TIMER, 0 );
    ADCSequenceConfigure( ADC0_BASE, 0,
                          ADC_TRIGGER_PWM_MOD0 | ADC_TRIGGER_PWM0, 1 );

    /* Configure sample sequence 0, step 0. CH7 is sampled (PD0). This is
     * AVGP1 on the board, which is the downscaled 12V input for the
     * temperature selector knob. */
    ADCSequenceStepConfigure( ADC0_BASE, 0, 0, ADC_CTL_CH7 );

    /* Configure sample sequence 0, step 1. CH6 is sampled (PD1). This is
     * AVGP2 on the board, which is not in use currently. */
    ADCSequenceStepConfigure( ADC0_BASE, 0, 1, ADC_CTL_CH6 );

    /* Configure sample sequence 0, step 2. CH3 is sampled (PE0). This is
     * AVTEMP1 on the board. */
    ADCSequenceStepConfigure( ADC0_BASE, 0, 2, ADC_CTL_CH3 );

    /* Configure sample sequence 0, step 3. CH2 is sampled (PE1). This is
     * AVTEMP2 on the board. */
    ADCSequenceStepConfigure( ADC0_BASE, 0, 3, ADC_CTL_CH2 );

    /* Configure sample sequence 0, step 4. CH1 is sampled (PE2). This is
     * AVTEMP3 on the board. */
    ADCSequenceStepConfigure( ADC0_BASE, 0, 4, ADC_CTL_CH1 );

    /* Configure sample sequence 0, step 5. CH0 is sampled (PE3). This is
     * AVTEMP4 on the board.
     * An interrupt will be triggered when the sample is done. This is the last
     * step in the sequence (ADC_CTL_END). */
    ADCSequenceStepConfigure( ADC0_BASE, 0, 5, ADC_CTL_CH0 | ADC_CTL_IE |
                              ADC_CTL_END );

    /* Enable sample sequence 0. */
    ADCSequenceEnable( ADC0_BASE, 0 );

    /* Enable the processor interrupt for sequence 0. */
    ADCIntEnable( ADC0_BASE, 0 );
    IntEnable( INT_ADC0SS0 );

    /* Configure sample sequence 1 on ADC0 for PWM-triggered sampling from
     * module 0 generator 1. Priority 0 (highest) is given to this sequence. */
//    ADCSequenceConfigure( ADC0_BASE, 1, ADC_TRIGGER_TIMER, 0 );
    ADCSequenceConfigure( ADC0_BASE, 1,
                          ADC_TRIGGER_PWM_MOD0 | ADC_TRIGGER_PWM1, 0 );

    /* Configure sample sequence 1, step 0. CH5 is sampled (PD2). This is
     * AVBATT on the board, which is the downscaled 12V input for the
     * vehicle battery voltage. */
    ADCSequenceStepConfigure( ADC0_BASE, 1, 0, ADC_CTL_CH5 );

    /* Configure sample sequence 1, step 1. CH4 is sampled (PD3). This is
     * AVCS on the board, which is connected to the NCS199 output.
     * An interrupt will be triggered when the sample is done. This is the last
     * step in the sequence (ADC_CTL_END). */
    ADCSequenceStepConfigure( ADC0_BASE, 1, 1, ADC_CTL_CH4 | ADC_CTL_IE |
                              ADC_CTL_END );

    /* Enable sample sequence 1. */
    ADCSequenceEnable( ADC0_BASE, 1 );

    /* Enable the processor interrupt for sequence 0. */
    ADCIntEnable( ADC0_BASE, 1 );
    IntEnable( INT_ADC0SS1 );
}

static void DACI2CConfigure( void ) {

    /* Enable clocking for the I2C peripheral. */
//    SysCtlPeripheralEnable( SYSCTL_PERIPH_I2C1 );
    SysCtlPeripheralEnable( SYSCTL_PERIPH_I2C2 );

    /* Wait for the module to become ready. */
//    while( !SysCtlPeripheralReady( SYSCTL_PERIPH_I2C1 ) ) {}
    while( !SysCtlPeripheralReady( SYSCTL_PERIPH_I2C2 ) ) {}

    /* Enable the GPIO port with I2C pins we are using. */
//    SysCtlPeripheralEnable( SYSCTL_PERIPH_GPIOA );
    SysCtlPeripheralEnable( SYSCTL_PERIPH_GPIOE );

    /* Configure pin mux for PA6 and PA7. */
//    GPIOPinConfigure( GPIO_PA6_I2C1SCL );
//    GPIOPinConfigure( GPIO_PA7_I2C1SDA );
    GPIOPinConfigure( GPIO_PE4_I2C2SCL );
    GPIOPinConfigure( GPIO_PE5_I2C2SDA );

    /* Configure GPIO pads for I2C. PA6 is push-pull, PA7 is open-drain, and
     * both have peripheral-controlled direction. */
//    GPIOPinTypeI2CSCL( GPIO_PORTA_BASE, GPIO_PIN_6 );
//    GPIOPinTypeI2C( GPIO_PORTA_BASE, GPIO_PIN_7 );
    GPIOPinTypeI2CSCL( GPIO_PORTE_BASE, GPIO_PIN_4 );
    GPIOPinTypeI2C( GPIO_PORTE_BASE, GPIO_PIN_5 );

    /* Enable I2C1 as a master, calculate the I2CMTPR value based on the system
     * clock and a 400kbps bus. */
//    I2CMasterInitExpClk( I2C1_BASE, SysCtlClockGet(), true );
    I2CMasterInitExpClk( I2C2_BASE, SysCtlClockGet(), true );

    /* We'll only be talking to one slave (MAX5815), and only sending, so we
     * set the slave address right away. false = send. */
//    I2CMasterSlaveAddrSet( I2C1_BASE, MAX5815_ADDR, false );
    I2CMasterSlaveAddrSet( I2C2_BASE, MAX5815_ADDR, false );
}

uint32_t AnalogTaskInit( void ) {

    ADC0Configure();
//    ADCTimerConfigure();
    PWMADCTriggerConfigure();

    DACI2CConfigure();

    if( xTaskCreate( AnalogTask, ( const portCHAR * )"Analog",
                     ANALOGTASKSTACKSIZE, NULL,
                     tskIDLE_PRIORITY + PRIORITY_ANALOG_TASK,
                     &xAnalogTaskHandle ) != pdTRUE ) {
        return 1;
    }

    return 0;
}
