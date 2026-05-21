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

// #define SS_PPG_INIT_PARAM_20260424
#define SS_PPG_INIT_PARAM_20260511

/**
 * @brief SSH401 initial register settings
 *
 * This array is the initialization table for the SSH401.\n
 * It may need to be optimized for the actual application.
 */
const unsigned char g_ss_ppg_init_register_ssh401[][2] = {
#if 0
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
#elif defined(SS_PPG_INIT_PARAM_20260424)
    //[1]FIFO Initialization
    {REG_SEQ_LED_SEL, 0x3F},     // LED ALL OFF
    {REG_MEASUREMENT, 0x08},     // MEASURE ON
    {REG_MEASUREMENT, 0x00},     // MEASURE OFF
    {REG_FIFO_FLUSH, 0x01},      // CLEAR FIFO

    //[2]Hidden Key enable
    {REG_HIDDEN_KEY, 0x0A},      // Test key enable
    
    //[3]LED and PD Settings Register
    {REG_SEQ_LED_SEL, 0x1B},     // SEQ0=VCSEL(prox_source), SEQ1=Green, SEQ2=Disabled
    {REG_SEQ_PD_SEL, 0x85},      // SEQ0=Internal PD, SEQ1= External PD SEQ2=GND

    //[4]Set LED Current(mA) and Integration Time (=LED On Time)
    {REG_SEQ0_LED_CUR, 0x2C},    // LED Current=15.26mA
    {REG_SEQ0_LED_RANGE, 0x02},  // LED Current Range setting (43.4mA)

    {REG_SEQ1_LED_CUR  , 0x2E},    // LED Current=11.05mA
    {REG_SEQ1_LED_RANGE , 0x01},  // LED Current Range setting (30.1mA)

    {REG_SEQ0_INTG_TIME_H, 0x00},// Integration time (=(N+1)*0.5us = 38us)
    {REG_SEQ0_INTG_TIME_L, 0x4B},

    {REG_SEQ1_INTG_TIME_H, 0x00},// Integration time (=(N+1)*0.5us = 40us)
    {REG_SEQ1_INTG_TIME_L, 0x4F},

    //[5]Set DAC Range
    {REG_SEQ0_DAC_RANGE, 0x02},  // ADAC=6.38uA, DDAC=220uA
    {REG_SEQ1_DAC_RANGE, 0x32},  // ADAC=25.5uA, DDAC=220uA
    {REG_DDAC_H_LIN_CAL, 0x02},  // DDAC linearity correction register  

    //[6]Hidden Register Settings
    {REG_H_AFE_TIME, 0xE0},       // AFE Rest time control
    {REG_SEQ0_H_TIA_GAIN, 0x56}, // TIA Gain=50Kohm (x8)
    {REG_SEQ0_H_INT_GAIN, 0x35}, // INT Gain=100Kohm/8pF

    {REG_SEQ1_H_TIA_GAIN, 0x43}, // TIA Gain=50Kohm (x16)
    {REG_SEQ1_H_INT_GAIN, 0x45}, // INT Gain=100Kohm/16pF

    //[7] Interrupt Settings
    {REG_INTREEUPT_CONF, 0x00},  // interrupt enable(A_FIFO_FULL)

    //* Note)PPG stops when earbuds are removed from the ear during PPG measurement
    //{REG_INTREEUPT_CONF, 0x82},  // interrupt enable(A_FIFO_FULL& Proximity)
    
    //{REG_INT_MODE , 0x11},      // Set interrupt after interrupt state is released
    {REG_INT_MODE_SET , 0x01},   // Mode: Threshold mode , Persistence : 2

    //[8]Proximity mode settings
    {REG_PROX_GAIN, 0x0A},          // Proximity Gain : x2
    {REG_PROX_AVG_FLT, 0x00},       // Proximity Moving Average Filter : Disable 
    {REG_PROX_THRES_HIGH_H, 0x9C},  // Threshold : 40000
    {REG_PROX_THRES_HIGH_L, 0x40},  // 
    {REG_PROX_THRES_LOW_H, 0x4E},   ///Threshold : 20000
    {REG_PROX_THRES_LOW_L, 0x20},   //

    //[9]Other recommended settings
    {REG_PPG_FILTER, 0x00},      // PPG Filter Disable
    {REG_DC_CUT_MODE, 0x80},     // DC cut

    //[10]Sensor sequence mode configuration
    //{REG_MEASUREMENT, 0x60},     // MODE setting = SEQ0:Proximity mode , SEQ1: PPG mode
    //Enable Proximity Mode (SEQ0) only start : Write 0x08 to REG_MEASUREMENT
    //Enable PPG Mode (SEQ1) only start : Write 0x38 to REG_MEASUREMENT
#elif defined(SS_PPG_INIT_PARAM_20260511)

   // NOTE:
   // This function is configured for PPG Green LED operation.
   // For PPG IR operation, register values must be reconfigured
   // (LED current, timing, and Gain settings).

     // FIFO initializatio
    {REG_SEQ_LED_SEL, 0x3F},     // LED ALL OFF
    {REG_MEASUREMENT, 0x08},     // MEASURE ON
    {REG_MEASUREMENT, 0x00},     // MEASURE OFF
    {REG_FIFO_FLUSH, 0x01},      // CLEAR FIFO

    // LED & PD configurati
    {REG_SEQ_PD_SEL, 0x85},      // SEQ0=Internal PD, SEQ1,2=External PD
    {REG_SEQ_LED_SEL, 0x1B},     // SEQ0=VCSEL, SEQ1=Green,SEQ2=Disabled

    // LED drive current configuration
    {REG_SEQ0_LED_RANGE, 0x01},  // LED Current Range setting
    {REG_SEQ0_LED_CUR, 0x2C},    // LED Current=10.58mA
    {REG_SEQ1_LED_RANGE, 0x01},  // LED Current Range setting
    {REG_SEQ1_LED_CUR, 0x2E},    // LED Current=11.05mA

    // LED on-time configuration
    {REG_SEQ0_INTG_TIME_H, 0x00},// Integration time high byte (=(N+1)*0.5us = 38us)
    {REG_SEQ0_INTG_TIME_L, 0x4B},// Integration time low byte (=(N+1)*0.5us = 38us)
    {REG_SEQ1_INTG_TIME_H, 0x00},// Integration time high byte (=(N+1)*0.5us = 40us)
    {REG_SEQ1_INTG_TIME_L, 0x4F},// Integration time low byte (=(N+1)*0.5us = 40us)

    // Other sensor setting
    {REG_PPG_TH_SEL, 0x00},      // PPG Mode DAC threshold value
    {REG_PPG_FILTER, 0x00},      // PPG Filter Disable
    {REG_DC_CUT_MODE, 0x80},     // DC cut

    // Hidden options
    {REG_HIDDEN_KEY, 0x0A},      // Test key enable

    // DAC Range setting
    {REG_SEQ0_DAC_RANGE, 0x02},  // ADAC=6.38uA, DDAC=220uA
    {REG_SEQ1_DAC_RANGE, 0x32},  // ADAC=25.5uA, DDAC=220uA
    {REG_DDAC_CO_FI_SEL, 0x02},  // ADAC=25.5uA, DDAC=220uA

    // Hidden register settings
    {REG_H_TIA_GAIN, 0x63},      // SEQ0 TIA Gain=400Kohm (x16)
    {REG_H_INT_GAIN, 0x35},      // SEQ0 INT Gain=50Kohm/16pF
    {REG_SEQ1_H_TIA_GAIN, 0x43},  // SEQ1 TIA Gain=100Kohm (x16)
    {REG_SEQ1_H_INT_GAIN, 0x45}, // SEQ1 INT Gain=100Kohm/16pF

    {REG_H_AFE_TIME, 0xE0},      // AFE Rest time control

    // Interrupt settings
    {REG_INTREEUPT_CONF, 0x00},  // A_FIFO_FULL and PROX_INT_EN interrupt enable
    {REG_PROX_STATE_MODE, 0x01}, // interrupt mode : threshold mode , Persistence : 2
    {REG_INT_MODE, 0x01},        // Interrupt clear mode , Bitp[4]=0 : Interrupt reoccurs only on a new event after clear

    // Proximity settings
    {REG_PROX_DIGITAL_GAIN, 0x0A}, // Proximity Gain = x2 
    {REG_PROX_FILTER_SET, 0x00},   // Moving average filter : Disable

    {REG_PROX_THRES_HIGH_H, 0x9C},  // Threshold : 40000
    {REG_PROX_THRES_HIGH_L, 0x40},  // 
    {REG_PROX_THRES_LOW_H, 0x75},   ///Threshold : 30000
    {REG_PROX_THRES_LOW_L, 0x30},   //
    //[10]Sensor sequence mode configuration
    //{REG_MEASUREMENT, 0x60},     // MODE setting = SEQ0:Proximity mode , SEQ1: PPG mode
    //Enable Proximity Mode (SEQ0) only start : Write 0x08 to REG_MEASUREMENT
    //Enable PPG Mode (SEQ1) only start : Write 0x38 to REG_MEASUREMENT
#endif
	
};

const unsigned char ss_ppg_init_register_seq1_ir_ssh401[][2] = {
    //[1]FIFO Initialization
    {REG_SEQ_LED_SEL, 0x3F},     // LED ALL OFF
    {REG_MEASUREMENT, 0x08},     // MEASURE ON
    {REG_MEASUREMENT, 0x00},     // MEASURE OFF
    {REG_FIFO_FLUSH, 0x01},      // CLEAR FIFO

    //[2]Set LED Current(mA) and Integration Time (=LED On Time)
    {REG_SEQ1_LED_CUR  , 0x3A},    // LED Current=20.00
    {REG_SEQ1_LED_RANGE , 0x02},   // LED Current Range setting (43.4mA)

    {REG_SEQ1_INTG_TIME_H, 0x00},// Integration time (=(N+1)*0.5us = 40us)
    {REG_SEQ1_INTG_TIME_L, 0x4F},

    //[3]Hidden Register Settings
    {REG_SEQ1_H_TIA_GAIN, 0x43}, // TIA Gain=50Kohm (x16)
    {REG_SEQ1_H_INT_GAIN, 0x56}, // INT Gain=200Kohm/32pF

    //[4]Sensor sequence mode configuration
    {REG_MEASUREMENT, 0x60},     // MODE setting = SEQ0:Proximity mode , SEQ1: PPG mode
};

const int ss_ppg_register_seq1_ir_ssh401_count = sizeof(ss_ppg_init_register_seq1_ir_ssh401) / 2;


const unsigned char ss_ppg_init_register_seq1_green_ssh401[][2] = {
    //[1]FIFO Initialization
    {REG_SEQ_LED_SEL, 0x3F},     // LED ALL OFF
    {REG_MEASUREMENT, 0x08},     // MEASURE ON
    {REG_MEASUREMENT, 0x00},     // MEASURE OFF
    {REG_FIFO_FLUSH, 0x01},      // CLEAR FIFO

    //[2]Set LED Current(mA) and Integration Time (=LED On Time)
    {REG_SEQ1_LED_CUR  , 0x2E},    // LED Current=11.05mA
    {REG_SEQ1_LED_RANGE , 0x01},  // LED Current Range setting (30.1mA)

    {REG_SEQ1_INTG_TIME_H, 0x00},// Integration time (=(N+1)*0.5us = 40us)
    {REG_SEQ1_INTG_TIME_L, 0x4F},

    //[3]Hidden Register Settings
    {REG_SEQ1_H_TIA_GAIN, 0x43}, // TIA Gain=50Kohm (x16)
    {REG_SEQ1_H_INT_GAIN, 0x45}, // INT Gain=100Kohm/16pF

    //[4]Sensor sequence mode configuration
    {REG_MEASUREMENT, 0x60},     // MODE setting = SEQ0:Proximity mode , SEQ1: PPG mode
};

const int ss_ppg_register_seq1_green_ssh401_count = sizeof(ss_ppg_init_register_seq1_green_ssh401) / 2;


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


