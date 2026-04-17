#include "ss_ppg.h"
#include "ss_ppg_errno.h"
#include "ss_os_api.h"
#include "ss_ppg_example.h"

/**
 * @file ss_ppg_example.c
 * @brief This code provides examples of calling the ss_ppg.c's API.
 *
 * It can serve as a reference to help customers apply the API appropriately in their applications.
 */
 
/* --------------------------------------------------------------
 * If the function name contains the word "example,"
 * it is intended for example purposes only.
 * Proper tuning is required in the actual assembly environment.
 * -------------------------------------------------------------- */

/** @private */
extern unsigned char is_ppg_interrupt;

int ss_ppg_example_main(ExampleMode exam_mode)
{
    if(ss_ppg_verify() != SS_SUCCESS) 
    {
        os_api_print_log("chip id not match.");
        return SS_ERROR_BASE;
    }
    
    if (ss_ppg_init(SENSOR_SSH401, SPS_64, 32) != SS_SUCCESS)
    {
        os_api_print_log("ss_ppg_init failed");
        return SS_ERROR_BASE;
    }

    if (exam_mode == EXAM_PROX_GREEN)
    {
        if (ss_ppg_example_use_proximity() != SS_SUCCESS)
        {
            os_api_print_log("proximity mode set failed");
            return SS_ERROR_BASE;
        }
    }
#if 0
    if (ss_ppg_start_measurement() != SS_SUCCESS)
    {
        os_api_print_log("start_measurement failed");
        return SS_ERROR_BASE;
    }
#endif

    /*
    The measured PPG data is callback as a function pointer of the 'callback_ppg_data' member of the SS_OS_API structure.
    */

#if 0    
    for(;;)
    {

        int data_count = ss_ppg_mem_get_fifo_data_count();
        if (data_count > 0)
        {
            ss_ppg_interrupt_clear();

            for (int idx=0; idx < data_count; idx++)
            {
                SS_PPG* ppg_data = ss_ppg_mem_fifo_data_pop();

                //TODO
                //You can perform tasks such as calculating heart rate or oxygen saturation,
                //or controlling the LED current here.

                os_api_print_log("[%d/%d] seq0 %d  seq1 %d  seq2 %d",
                    idx+1,
                    data_count,
                    ppg_data->seq0,
                    ppg_data->seq1,
                    ppg_data->seq2);
            }
        }
        else
        {
            os_api_delay_ms(1);
            /*
            unsigned short prox;
            ss_ppg_read_proximity_value(&prox);
            os_api_print_log("prox %d\n", prox);
            */
        }

    }
#endif

  
    return SS_SUCCESS;
}

int ss_ppg_example_use_proximity(void)
{
    int ret = 0;

    ret = ss_ppg_operation_mode(PROX_PPG_0); //SEO0=PROX, SEQ1=PPG
    if (ret != SS_SUCCESS)
        return ret;

    ret = ss_ppg_led_config(SEQ0, LED_IR);
    if (ret != SS_SUCCESS)
        return ret;

    ret = ss_ppg_led_config(SEQ1, LED_OFF); //It will turn on when a proximity high interrupt is detected.
    if (ret != SS_SUCCESS)
        return ret;
    
    ret = ss_ppg_led_config(SEQ2, LED_OFF);
    if (ret != SS_SUCCESS)
        return ret;

    ret = ss_ppg_pd_select(SEQ0, PD_EXT);
    if (ret != SS_SUCCESS)
        return ret;

    ret = ss_ppg_pd_select(SEQ1, PD_EXT);
    if (ret != SS_SUCCESS)
        return ret;

    ret = ss_ppg_pd_select(SEQ2, PD_DISABLE);
    if (ret != SS_SUCCESS)
        return ret;

    ret = ss_ppg_set_led_current(SEQ0, RANGE_30_1mA, 2.0f); //IR 2mA
    if (ret != SS_SUCCESS)
        return ret;

    ret = ss_ppg_set_led_current(SEQ1, RANGE_30_1mA, 20.0f); //Green 20mA
    if (ret != SS_SUCCESS)
        return ret;

    ret = ss_ppg_led_integrate_time(SEQ0, 1); //IR 1us
    if (ret != SS_SUCCESS)
        return ret;

    ret = ss_ppg_led_integrate_time(SEQ1, 16); //Green 16us
    if (ret != SS_SUCCESS)
        return ret;

    ret = ss_ppg_proximity_threshold(23000, 18000); //[High] DC>=20000, [Low] DC<=10000
    if (ret != SS_SUCCESS)
        return ret;

    ret = ss_ppg_interrupt_setting(A_FIFO_FULL_EN, 0); //It will enable when a proximity high interrupt is detectd.
    if (ret != SS_SUCCESS)
        return ret;
    
    ret = ss_ppg_interrupt_setting(PROX_INT_EN, 1);
    if (ret != SS_SUCCESS)
        return ret;

    return SS_SUCCESS;
}