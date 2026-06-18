/**
 * @file ss_ppg.c
 * @brief Release notes and copyright information for SoluM Sensor PPG API.
 */
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_trace.h"

#include "ss_os_api.h"
#include "ss_ppg.h"
#include "ss_ppg_errno.h"
#include "ss_ppg_init_table.h"
#include "ss_ppg_example.h"

/** @private */
#include "ss_util_ring_buffer.h"

/** @private */
static unsigned char _g_interrupt_clear_mode = 1;
/** @private */
static int _g_fifo_first_seq_num = -1;

static int start_measurement(void);
static int stop_measurement(void);
static int fifo_init(void);
static int clear_fifo(void);

static unsigned int g_pre_adc_data[4] = {0,0,0,0};
static unsigned short g_selected_sps = 32;
static Sensor g_target_sensor;
static unsigned char g_fifo_onoff = 0;
static unsigned char g_proximity_sta = 0;
static SS_PPG ppg_buf[64];
static unsigned char g_ppg_test_mode = 0; //0:disable, 1:enable
static unsigned int g_ppg_samples_count = 0;

static const float g_led_range_list[] = {
    CURRENT_RANGE_16_7,
    CURRENT_RANGE_30_1,
    CURRENT_RANGE_43_4,
    CURRENT_RANGE_56_7,
    CURRENT_RANGE_70_0
};

int ss_ppg_init(Sensor target_sensor, SamplingRate sample_rate, unsigned char fifo_full_int_cnt)
{
    int ret;
    unsigned char val;
    const unsigned char (*register_init_table)[2] = ss_ppg_register_get_table(target_sensor);

    if (fifo_full_int_cnt <= 0 || fifo_full_int_cnt > 255)
    {
        //out of range
        return SS_ERROR_INVALID_ARGUMENT;
    }

    // +++ Applies the predefined sensor configuration values +++
    int reg_size = ss_ppg_register_init_count(target_sensor);
    for (unsigned char idx = 0; idx < reg_size; idx++)
    {
        ret = os_api_i2c_write_byte(register_init_table[idx][0], register_init_table[idx][1]); 
        if (ret != SS_SUCCESS)
        {
            return ret;
        }
    }

    g_target_sensor = target_sensor;

    if (sample_rate == SPS_25)
        g_selected_sps = 25;
    else if (sample_rate == SPS_32)
        g_selected_sps = 32;
    else if (sample_rate == SPS_50)
        g_selected_sps = 50;
    else if (sample_rate == SPS_64)
        g_selected_sps = 64;
    else if (sample_rate == SPS_100)
        g_selected_sps = 100;
    else if (sample_rate == SPS_128)
        g_selected_sps = 128;
    else if (sample_rate == SPS_192)
        g_selected_sps = 192;
    else if (sample_rate == SPS_200)
        g_selected_sps = 200;
    else if (sample_rate == SPS_256)
        g_selected_sps = 256;
    else if (sample_rate == SPS_400)
        g_selected_sps = 400;
    else if (sample_rate == SPS_500)
        g_selected_sps = 500;
    else if (sample_rate == SPS_512)
        g_selected_sps = 512;
    else if (sample_rate == SPS_4096)
        g_selected_sps = 4096;

    ret = os_api_i2c_write_byte(REG_SAMPLE_RATE, (unsigned char)sample_rate);
    if (ret != SS_SUCCESS)
        return ret;
    
    // +++ Set FIFO Full Count for interrupt +++
    ret = os_api_i2c_write_byte(REG_FIFO_FULL_CNT, 256 - fifo_full_int_cnt); 
    if (ret != SS_SUCCESS)
        return ret;
    
    // read interrupt clear mode setting
    ret = os_api_i2c_read_byte(REG_INT_MODE, &val); 
    if (ret != SS_SUCCESS)
        return ret;

    if ((val & 0x01) == 0x01) // 'INT_CLR_MODE' bit is 1
    {
        //Writing a 1 to a specific bit at address 0x05 clears the corresponding interrupt.
        _g_interrupt_clear_mode = 1;
    }
    else if ((val & 0x01) == 0x00) // 'INT_CLR_MODE' bit is 0
    {
        //Reading address 0x05 clears the interrupt.
        _g_interrupt_clear_mode = 0;
    }

    return SS_SUCCESS;
}

int ss_ppg_interrupt_fifo_full_count(unsigned char fifo_full_int_count)
{
    return os_api_i2c_write_byte(REG_FIFO_FULL_CNT, 256 - fifo_full_int_count);
}

int ss_ppg_interrupt_clear_mode(unsigned char is_write_clear)
{
    int ret;
    unsigned char val;

    if (is_write_clear > 1)
        return SS_ERROR_INVALID_ARGUMENT;

    ret = os_api_i2c_read_byte(REG_INT_MODE, &val); 
    if (ret != SS_SUCCESS)
        return ret;

    val = val & ~0x01;
    val |= is_write_clear;

    ret = os_api_i2c_write_byte(REG_INT_MODE, val); 
    if (ret != SS_SUCCESS)
        return ret;
    
    _g_interrupt_clear_mode = is_write_clear;

    return SS_SUCCESS;
}

int ss_ppg_verify(void)
{
    int ret = 0;
    unsigned char chip_id = 0;

    ret = ss_ppg_read_chip_id(&chip_id);
    if (ret != SS_SUCCESS)
    {
        return ret;
    }

    if (chip_id == SS_PPG_CHIP_ID)
    {
        return SS_SUCCESS;
    }
    else
    {
        return SS_ERROR_NOT_MATCH_ID;
    }
}


//If communication with the sensor is successful,
//the value of the chip_id pointer is 0xA0~A2.
int ss_ppg_read_chip_id(unsigned char* chip_id)
{
    return os_api_i2c_read_byte(REG_CHIP_ID, chip_id);
}

int ss_ppg_read_fifo(int read_length, unsigned char* fifo_data)
{   
    return os_api_i2c_read_burst(REG_FIFO_DATA, fifo_data, read_length);
}

int ss_ppg_clear_fifo(void)
{
    _g_fifo_first_seq_num = -1;
    return os_api_i2c_write_byte(REG_FIFO_FLUSH, 0x01);
}

int ss_ppg_read_fifo_count(unsigned char* fifo_count)
{
    return os_api_i2c_read_byte(REG_FIFO_DATA_CNT, fifo_count);
}

int ss_ppg_run_seq1_green(void)
{
    int ret = SS_SUCCESS;

    for (unsigned char idx = 0; idx < ss_ppg_register_seq1_green_ssh401_count; idx++)
    {
        ret = os_api_i2c_write_byte(ss_ppg_init_register_seq1_green_ssh401[idx][0], ss_ppg_init_register_seq1_green_ssh401[idx][1]); 
        if (ret != SS_SUCCESS)
        {
            return ret;
        }
    }

    return SS_SUCCESS;
}

int ss_ppg_run_seq1_ir(void)
{
    int ret = SS_SUCCESS;

    for (unsigned char idx = 0; idx < ss_ppg_register_seq1_ir_ssh401_count; idx++)
    {
        ret = os_api_i2c_write_byte(ss_ppg_init_register_seq1_ir_ssh401[idx][0], ss_ppg_init_register_seq1_ir_ssh401[idx][1]); 
        if (ret != SS_SUCCESS)
        {
            return ret;
        }
    }

    return SS_SUCCESS;
}

int ss_ppg_start_measurement(void)
{
    int ret;

    ret = clear_fifo();
    if (ret != SS_SUCCESS)
        return ret;

    ret = ss_ppg_interrupt_clear();
    if (ret != SS_SUCCESS)
        return ret;

    return start_measurement();
}

int ss_ppg_stop_measurement(void)
{
    int ret;
    
    ret = stop_measurement();
    if (ret != SS_SUCCESS)
        return ret;

    ret = ss_ppg_interrupt_clear();
    if (ret != SS_SUCCESS)
        return ret;

    ret = clear_fifo();
    if (ret != SS_SUCCESS)
        return ret;

    return SS_SUCCESS;
}

int ss_ppg_interrupt_clear(void)
{
    unsigned char val;

    if (_g_interrupt_clear_mode == 1)
        return os_api_i2c_write_byte(REG_INTERRUPT_STAT, 0xFF);
    else
        return os_api_i2c_read_byte(REG_INTERRUPT_STAT, &val);
}

int ss_ppg_pd_select(Seq seq, PD pd)
{
    int ret;
    unsigned char val;

    ret = os_api_i2c_read_byte(REG_SEQ_PD_SEL, &val);
    if (ret != SS_SUCCESS)
        return ret;

    if (seq == SEQ0)
    {
        val = val & ~0x30;
        val |= (unsigned char)pd << 4;
    }
    else if (seq == SEQ1)
    {
        val = val & ~0x0C;
        val |= (unsigned char)pd << 2;
    }
    else if (seq == SEQ2)
    {
        val = val & ~0x03;
        val |= (unsigned char)pd;
    }

    return os_api_i2c_write_byte(REG_SEQ_PD_SEL, val);
}

int ss_ppg_led_integrate_time(Seq seq, float time_us)
{
    int ret;
    unsigned short targetValue;
    unsigned char msb;
    unsigned char lsb;

    if (time_us < 0.5f || time_us > 256.0f)
        return SS_ERROR_INVALID_ARGUMENT; //out of range
    
    targetValue = (unsigned short)(2*time_us-1.0f+0.5f);

    msb = targetValue >> 8;
    lsb = targetValue;
    
    if (seq == SEQ0)
    {
        ret = os_api_i2c_write_byte(REG_SEQ0_INTG_TIME_H, msb);
        if (ret != SS_SUCCESS)
            return ret;
        ret = os_api_i2c_write_byte(REG_SEQ0_INTG_TIME_L, lsb);
        if (ret != SS_SUCCESS)
            return ret;
    }
    else if (seq == SEQ1)
    {
        ret = os_api_i2c_write_byte(REG_SEQ1_INTG_TIME_H, msb);
        if (ret != SS_SUCCESS)
            return ret;
        ret = os_api_i2c_write_byte(REG_SEQ1_INTG_TIME_L, lsb);
        if (ret != SS_SUCCESS)
            return ret;
    }
    else if (seq == SEQ2)
    {
        ret = os_api_i2c_write_byte(REG_SEQ2_INTG_TIME_H, msb);
        if (ret != SS_SUCCESS)
            return ret;
        ret = os_api_i2c_write_byte(REG_SEQ2_INTG_TIME_L, lsb);
        if (ret != SS_SUCCESS)
            return ret;
    }
    else
    {
        return SS_ERROR_INVALID_ARGUMENT;
    }

    return SS_SUCCESS;
}

int ss_ppg_proximity_interrupt_mode(ProxIntMode mode)
{
    int ret = 0;
    unsigned char val = 0;
    
    ret = os_api_i2c_read_byte(REG_INT_MODE, &val);
    if (ret != SS_SUCCESS)
        return ret;

    val = val & ~0x10; //INT_MODE bit set to 0
    val |= ((unsigned char)mode) << 4;

    return os_api_i2c_write_byte(REG_INT_MODE, val);
}

int ss_ppg_read_interrupt_status(unsigned char* status)
{
    return os_api_i2c_read_byte(REG_INTERRUPT_STAT, status);
}

int ss_ppg_operation_mode(OperationMode op_mode)
{
    int ret = 0;
    unsigned char val = 0;

    ret = os_api_i2c_read_byte(REG_MEASUREMENT, &val);
    if (ret != SS_SUCCESS)
        return ret;
    
    val = val & ~0x70; //MODE bit mask
    val |= (unsigned char)op_mode << 4;

    return os_api_i2c_write_byte(REG_MEASUREMENT, val);
}

int ss_ppg_led_config(Seq seq, Led led)
{
    unsigned char val = 0;
    unsigned char mask = 0;
    int ret = 0;

    ret = os_api_i2c_read_byte(REG_SEQ_LED_SEL, &val);
    if (ret < 0)
        return ret;

    if (led == LED_OFF)
    {
        mask = 0x03;
    }
    else if (g_target_sensor == SENSOR_SSH202)
    {
        if (led == LED_RED)
            mask = 0x00;
        else if (led == LED_GREEN)
            mask = 0x01;
        else if (led == LED_IR)
            mask = 0x02;
    }
    else if (g_target_sensor == SENSOR_SSH401)
    {
        if (led == LED_IR)
            mask = 0x00;
        else if (led == LED_VCSEL)
            mask = 0x01;
        else if (led == LED_GREEN)
            mask = 0x02;
    }
    else
        return -1;
    

    if (seq == SEQ0)
    {
        val = val & ~0x30;
        val |= (unsigned char)mask << 4;
    }
    else if (seq == SEQ1)
    {
        val = val & ~0x0C;
        val |= (unsigned char)mask << 2;
    }
    else if (seq == SEQ2)
    {
        val = val & ~0x03;
        val |= (unsigned char)mask;
    }
    
    return os_api_i2c_write_byte(REG_SEQ_LED_SEL, val);
}

void ss_ppg_fifo_parse(unsigned char* fifo_data, unsigned char fifo_length)
{
    SS_PPG* ppg_data;
    unsigned short idx = 0;
    POSSIBLY_UNUSED unsigned char fifo_page_offset = 0;
    int fifo_header = 0;
    POSSIBLY_UNUSED int pre_fifo_header = -1;
    POSSIBLY_UNUSED unsigned char fifo_flag = 0;
    unsigned int adc_data = 0;
    unsigned char start_collect = 0;
    

    for (idx = 0; idx < fifo_length; idx+=3)
    {
        fifo_header = (fifo_data[idx + 0] & 0xF0) >> 4;
        fifo_flag = (fifo_data[idx + 0] & 0x08) >> 3;

        if (_g_fifo_first_seq_num == fifo_header)
        {
            RingBuffer_Data_Push();
            start_collect = 0;
        }
        else if (_g_fifo_first_seq_num == -1)
        {
            _g_fifo_first_seq_num = fifo_header;
        }

        if (fifo_header <= 3) //Handles only 'Proximity,SEQ0,SEQ1,SEQ2'
        {
            if (start_collect == 0)
            {
                start_collect = 1;

                ppg_data = (SS_PPG*)RingBuffer_Get_SaveBuffer();
                os_api_memset(ppg_data, 0, sizeof(SS_PPG));
            }
            
            adc_data = (fifo_data[idx + 0] & 0x07) << 16;
            adc_data |= fifo_data[idx + 1] << 8;
            adc_data |= fifo_data[idx + 2];

            if (fifo_flag == 1) //Saturation Occurs
            {
                //Saturated data is ignored and replaced with the value just before saturation.
                adc_data = g_pre_adc_data[fifo_header];
            }
            else //No Saturated
            {
                g_pre_adc_data[fifo_header] = adc_data;
            }

            if (fifo_header == 0 || fifo_header == 1)
                ppg_data->seq0 = adc_data;
            else if (fifo_header == 2)
                ppg_data->seq1 = adc_data;
            else if (fifo_header == 3)
                ppg_data->seq2 = adc_data;
        }
    }

    if (start_collect == 1)
    {
        //Pushing the remaining unprocessed data.
        RingBuffer_Data_Push();
        _g_fifo_first_seq_num = -1;
    }
}

SS_PPG* ss_ppg_mem_fifo_data_pop(void)
{
    return (SS_PPG*)RingBuffer_Data_Pop();
}

int ss_ppg_mem_get_fifo_data_count(void)
{
    return RingBuffer_Get_DataCount();
}

int ss_ppg_interrupt_setting(SensorInterrupt sensor_int, unsigned char is_enable)
{
    int ret;
    unsigned char val;

    ret = os_api_i2c_read_byte(REG_INTREEUPT_CONF, &val);
    if (ret != SS_SUCCESS)
        return ret;

    val = val & ~((unsigned char)sensor_int);
    if (is_enable)
        val |= (unsigned char)sensor_int;
    
    return os_api_i2c_write_byte(REG_INTREEUPT_CONF, val);
}

unsigned int ss_ppg_get_ppg_samples_count(void)
{
    return g_ppg_samples_count;
}

void ss_ppg_clear_ppg_samples_count(void)
{
    g_ppg_samples_count = 0;
}

void ss_ppg_interrupt_handler(void)
{
    unsigned char fifo_count = 0;
    unsigned char read_len = 0;
    unsigned char* fifo_data = (void*)0;
    unsigned char int_status = 0;
    //POSSIBLY_UNUSED SS_PPG* ppg_buf;
    
    ss_ppg_read_interrupt_status(&int_status);
   
    if ((int_status & INT_STAT_PROX_HIGH) == INT_STAT_PROX_HIGH)
    {
        //Wearing Earbuds
        g_proximity_sta = 1;
        os_api_callback_proximity(1);

        if(g_fifo_onoff) {
            ss_ppg_clear_fifo();
            ss_ppg_led_config(SEQ1, LED_GREEN);
            ss_ppg_interrupt_setting(A_FIFO_FULL_EN, 1);
        }
    }
    else if ((int_status & INT_STAT_PROX_LOW) == INT_STAT_PROX_LOW)
    {
        //Removing Earbuds
        g_proximity_sta = 0;
        os_api_callback_proximity(0);
        
        ss_ppg_led_config(SEQ1, LED_OFF);
        ss_ppg_interrupt_setting(A_FIFO_FULL_EN, 0);
    }

    if ((int_status & INT_STAT_FIFO_FULL) != INT_STAT_FIFO_FULL)
    {
        ss_ppg_interrupt_clear();
        return;
    }

    ss_ppg_read_fifo_count(&fifo_count);

    if (fifo_count == 0)
    {
        ss_ppg_interrupt_clear();
        return;
    }

    read_len = sizeof(unsigned char) * fifo_count * 3;
    fifo_data = (unsigned char*)os_api_malloc(read_len);

    ss_ppg_read_fifo(read_len, fifo_data);

    ss_ppg_fifo_parse(fifo_data, read_len);


    //notify PPG data
    int data_count = ss_ppg_mem_get_fifo_data_count();
    g_ppg_samples_count += data_count;
    if (data_count > 0)
    {
        ss_ppg_interrupt_clear();

        if(data_count > 64) {
            data_count = 64;
        }

        for (int idx=0; idx < data_count; idx++)
        {
            SS_PPG* ppg_data = ss_ppg_mem_fifo_data_pop();
            memcpy(&ppg_buf[idx], ppg_data, sizeof(SS_PPG));
        }


        if (g_ppg_test_mode)
        {
            os_api_callback_ppg_test_data(fifo_data, read_len);
        }
        else
        {
            os_api_callback_ppg_data(ppg_buf, data_count);
        }
    }

    os_api_free(fifo_data);
}

int ss_ppg_set_led_current(Seq seq, LEDCurrentRange range, float current)
{
    int ret;
    unsigned char targetRegValue = 0;

    //Curent validation.
    //The current cannot exceed the range.
    if (g_led_range_list[(int)range] < current)
    {
        return SS_ERROR_INVALID_ARGUMENT; 
    }
    
    //Converting mA(milliampere) value to register value.
    targetRegValue = (unsigned char)(((128 * current) / g_led_range_list[(int)range]) - 1);

    POSSIBLY_UNUSED unsigned char val = 0;
    if (seq == SEQ0)
    {
        ret = os_api_i2c_write_byte(REG_SEQ0_LED_CUR, targetRegValue);
        if (ret != SS_SUCCESS)
            return ret;
        //ret = os_api_i2c_write_byte(REG_SEQ0_LED_RANGE, (unsigned char)g_led_range_list[(int)range]);
        ret = os_api_i2c_write_byte(REG_SEQ0_LED_RANGE, (unsigned char)range); //[260423] fixed by SoluM
        if (ret != SS_SUCCESS)
            return ret;
    }
    else if (seq == SEQ1)
    {
        ret = os_api_i2c_write_byte(REG_SEQ1_LED_CUR, targetRegValue);
        if (ret != SS_SUCCESS)
            return ret;

        //ret = os_api_i2c_write_byte(REG_SEQ1_LED_RANGE, (unsigned char)g_led_range_list[(int)range]);
        ret = os_api_i2c_write_byte(REG_SEQ1_LED_RANGE, (unsigned char)range); //[260423] fixed by SoluM
        if (ret != SS_SUCCESS)
            return ret;
    }
    else if (seq == SEQ2)
    {
        ret = os_api_i2c_write_byte(REG_SEQ2_LED_CUR, targetRegValue);
        if (ret != SS_SUCCESS)
            return ret;
        
        //ret = os_api_i2c_write_byte(REG_SEQ2_LED_RANGE, (unsigned char)g_led_range_list[(int)range]);
        ret = os_api_i2c_write_byte(REG_SEQ2_LED_RANGE, (unsigned char)range); //[260423] fixed by SoluM
        if (ret != SS_SUCCESS)
            return ret;
    }

    return SS_SUCCESS;
}

int ss_ppg_proximity_threshold(unsigned short high_threshold, unsigned short low_threshold)
{
    int ret;
    unsigned char msb;
    unsigned char lsb;

    msb = high_threshold >> 8;
    lsb = high_threshold & 0x00FF;

    ret = os_api_i2c_write_byte(REG_PROX_THRES_HIGH_H, msb);
    if (ret != SS_SUCCESS)
        return ret;
    ret = os_api_i2c_write_byte(REG_PROX_THRES_HIGH_L, lsb);
    if (ret != SS_SUCCESS)
        return ret;

    msb = low_threshold >> 8;
    lsb = low_threshold & 0x00FF;

    ret = os_api_i2c_write_byte(REG_PROX_THRES_LOW_H, msb);
    if (ret != SS_SUCCESS)
        return ret;
    ret = os_api_i2c_write_byte(REG_PROX_THRES_LOW_L, lsb);
    if (ret != SS_SUCCESS)
        return ret;

    return SS_SUCCESS;
}

int ss_ppg_read_proximity_value(unsigned short* value)
{
    int ret;
    unsigned char buffer[2];
    
    ret = os_api_i2c_read_burst(REG_PROX_DATA_H, buffer, 2);
    if (ret != SS_SUCCESS)
        return ret;
    
    *value = buffer[0] << 8;
    *value |= buffer[1];

    return SS_SUCCESS;
}

static int start_measurement(void)
{
    int ret = SS_SUCCESS;
    
    unsigned char read_val = 0;
    
    ret = os_api_memset(g_pre_adc_data, 0, sizeof(g_pre_adc_data));
    if (ret != SS_SUCCESS)
        return ret;

    ret = os_api_i2c_read_byte(REG_MEASUREMENT, &read_val);
    if (ret != SS_SUCCESS)
        return ret;

    read_val |= 0x08; //REG_MEASUREMENT[3] MEAS_ON bit set 1

    ret = fifo_init();
    if (ret != SS_SUCCESS)
        return ret;

    ret = os_api_delay_ms((unsigned int)(1000/g_selected_sps));
    if (ret != SS_SUCCESS)
        return ret;

    return os_api_i2c_write_byte(REG_MEASUREMENT, read_val);
}

static int stop_measurement(void)
{
    int ret;
    unsigned char read_val = 0;

    ret = os_api_i2c_read_byte(REG_MEASUREMENT, &read_val);
    if (ret != SS_SUCCESS)
        return -1;

    read_val &= ~0x08; //REG_MEASUREMENT[3] MEAS_ON bit set 0

    return os_api_i2c_write_byte(REG_MEASUREMENT, read_val);
}

int ss_ppg_open_fifo(void)
{
    g_fifo_onoff = 1;

    if(g_proximity_sta) {
        ss_ppg_clear_fifo();
        ss_ppg_led_config(SEQ1, LED_GREEN);
        ss_ppg_interrupt_setting(A_FIFO_FULL_EN, 1);
    }
    return SS_SUCCESS;
}

int ss_ppg_close_fifo(void)
{
    g_fifo_onoff = 0;
    
    ss_ppg_led_config(SEQ1, LED_OFF);
    ss_ppg_interrupt_setting(A_FIFO_FULL_EN, 0);

    return SS_SUCCESS;
}

static int fifo_init(void)
{
    return RingBuffer_Initialization();
}

static int clear_fifo(void)
{
    int ret;
    ret = fifo_init();
    if (ret != SS_SUCCESS)
        return ret;
    
    return ss_ppg_clear_fifo();
}

int ss_ppg_test_mode_switch(unsigned char en)
{
    g_ppg_test_mode = en;
    return 0;
}

