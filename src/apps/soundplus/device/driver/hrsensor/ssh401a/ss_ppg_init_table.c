#include "ss_ppg.h"
#include "ss_ppg_register.h"

/**
 * @file ss_ppg_init_table.c
 * @brief This code defines the initial register settings for the PPG sensor.
 *
 * It should be modified to optimal settings based on the actual application.
 */

/**
 * @brief SSH202 initial register settings
 *
 * This array is the initialization table for the SSH202.\n
 * It may need to be optimized for the actual application.
 */
const unsigned char g_ss_ppg_init_register_ssh202[][2] = {
    {REG_SEQ_LED_SEL, 0x3F},     // LED ALL OFF
    {REG_MEASUREMENT, 0x08},     // MEASURE ON
    {REG_MEASUREMENT, 0x00},     // MEASURE OFF
    {REG_FIFO_FLUSH, 0x01},      // CLEAR FIFO

    {REG_SEQ_LED_SEL, 0x1F},     // SEQ0=Green, SEQ1,2=Disabled
    {REG_SEQ_PD_SEL, 0x9A},      // SEQ0=External PD, SEQ1,2=GND
    {REG_SEQ0_LED_CUR, 0x2E},    // LED Current=11.05mA
    {REG_SEQ0_LED_RANGE, 0x01},  // LED Current Range setting
    {REG_SEQ0_INTG_TIME_H, 0x00},// Integration time high byte (=(N+1)*0.5us = 16us)
    {REG_SEQ0_INTG_TIME_L, 0x1F},// Integration time low byte (=(N+1)*0.5us = 16us)
    {REG_SEQ0_DAC_RANGE, 0x01},  // ADAC=6.38uA, DDAC=210uA
    {REG_INTREEUPT_CONF, 0x80},  // A_FIFO_FULL interrupt enable
    {REG_MEASUREMENT, 0x00},     // MODE=SEQ0 Only (PPG0)
    {REG_PPG_FILTER, 0x00},      // PPG Filter Disable
    {REG_DC_CUT_MODE, 0x80},     // DC cut

    // Hidden options
    {REG_HIDDEN_KEY, 0x0A},      // Test key enable
    {REG_H_AFE_TIME, 0xE1},      // AFE Rest time control
    {REG_H_TIA_GAIN, 0x33},      // TIA Gain=50Kohm (x8)
    {REG_H_INT_GAIN, 0x44},      // INT Gain=100Kohm/8pF
};

/**
 * @brief SSH401 initial register settings
 *
 * This array is the initialization table for the SSH401.\n
 * It may need to be optimized for the actual application.
 */
const unsigned char g_ss_ppg_init_register_ssh401[][2] = {
    {REG_SEQ_LED_SEL, 0x3F},     // LED ALL OFF
    {REG_MEASUREMENT, 0x08},     // MEASURE ON
    {REG_MEASUREMENT, 0x00},     // MEASURE OFF
    {REG_FIFO_FLUSH, 0x01},      // CLEAR FIFO

    {REG_SEQ_LED_SEL, 0x2F},     // SEQ0=Green, SEQ1,2=Disabled
    {REG_SEQ_PD_SEL, 0x9A},      // SEQ0=External PD, SEQ1,2=GND
    {REG_SEQ0_LED_CUR, 0x2E},    // LED Current=11.05mA
    {REG_SEQ0_LED_RANGE, 0x01},  // LED Current Range setting
    {REG_SEQ0_INTG_TIME_H, 0x00},// Integration time high byte (=(N+1)*0.5us = 16us)
    {REG_SEQ0_INTG_TIME_L, 0x1F},// Integration time low byte (=(N+1)*0.5us = 16us)
    {REG_SEQ0_DAC_RANGE, 0x01},  // ADAC=6.38uA, DDAC=210uA
    {REG_INTREEUPT_CONF, 0x80},  // A_FIFO_FULL interrupt enable
    {REG_MEASUREMENT, 0x00},     // MODE=SEQ0 Only (PPG0)
    {REG_PPG_FILTER, 0x00},      // PPG Filter Disable
    {REG_DC_CUT_MODE, 0x80},     // DC cut

    // Hidden options
    {REG_HIDDEN_KEY, 0x0A},      // Test key enable
    {REG_H_AFE_TIME, 0xE1},      // AFE Rest time control
    {REG_H_TIA_GAIN, 0x33},      // TIA Gain=50Kohm (x8)
    {REG_H_INT_GAIN, 0x44},      // INT Gain=100Kohm/8pF
};
    
int ss_ppg_register_init_count(Sensor sensor)
{
    if (sensor == SENSOR_SSH202)
    {
        return sizeof(g_ss_ppg_init_register_ssh202) / 2;
    }
    else if (sensor == SENSOR_SSH401)
    {
        return sizeof(g_ss_ppg_init_register_ssh401) / 2;
    }
    else
    {
        return 0;
    }
}

const unsigned char (*ss_ppg_register_get_table(Sensor sensor))[2]
{
    if (sensor == SENSOR_SSH202)
    {
        return g_ss_ppg_init_register_ssh202;
    }
    else if (sensor == SENSOR_SSH401)
    {
        return g_ss_ppg_init_register_ssh401;
    }
    else
    {
        return 0;
    }
}
