/***************************************************************************
 *
 * Copyright 2015-2024 BES.
 * All rights reserved. All unpublished rights reserved.
 *
 * No part of this work may be used or reproduced in any form or by any
 * means, or stored in a database or retrieval system, without prior written
 * permission of BES.
 *
 * Use of this work is governed by a license granted by BES.
 * This work contains confidential and proprietary information of
 * BES. which is protected by copyright, trade secret,
 * trademark and other intellectual property rights.
 *
 * @brief dongle application.
 *
 ****************************************************************************/

/****************************** header include ********************************/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hal_trace.h"
#include "app_trace_rx.h"
#include "app_dongle.h"
#include "bt_service.h"

#include "apps.h"

/***************************** external declaration *****************************/

/***************************** macro defination *******************************/
#define APP_DONGLE_NAME     "APP_DONGLE_BES"

/*****************************  type defination ********************************/
typedef struct
{
    const char *string;
    void (*function)(char* BufPtr, uint32_t BufLen);
} app_dongle_cmd_handle_t;

/*****************************  variable defination *****************************/

/*****************************  function declaration ****************************/
static void app_dongle_open_cmd_handler(char* BufPtr, uint32_t BufLen)
{
    int input_type  = 0;
    int output_type = 0;

    if((BufPtr) && (BufLen))
    {
        TRACE(0, "[%s][%d]para: %s", __func__, __LINE__, BufPtr);
        sscanf(BufPtr, "%d|%d", &input_type, &output_type);
    }
    else
    {
        TRACE(0, "[%s][%d]paraeter error: %p, %d", __func__, __LINE__, BufPtr, BufLen);
        return;
    }
    TRACE(0, "[%s][%d]input=%d, output=%d", __func__, __LINE__,
        input_type, output_type);

    app_dongle_open(input_type, output_type);
}

static void app_dongle_close_cmd_handler(char* BufPtr, uint32_t BufLen)
{
    app_dongle_close();
}

static void app_dongle_scan_start_cmd_handler(char* BufPtr, uint32_t BufLen)
{
    app_dongle_start_scan();
}

static void app_dongle_scan_stop_cmd_handler(char* BufPtr, uint32_t BufLen)
{
    app_dongle_stop_scan();
}

static void app_dongle_conn_cmd_handler(char* BufPtr, uint32_t BufLen)
{
    int read_addr[6] = {0};
    uint8_t* bt_addr = (uint8_t*)read_addr;

    if((BufPtr) && (BufLen))
    {
        TRACE(0, "[%s][%d]para: %s", __func__, __LINE__, BufPtr);
        sscanf(BufPtr, "%x:%x:%x:%x:%x:%x", 
            &read_addr[0], &read_addr[1], &read_addr[2], &read_addr[3], &read_addr[4], &read_addr[5]);
    }
    else
    {
        TRACE(0, "[%s][%d]paraeter error: %p, %d", __func__, __LINE__, BufPtr, BufLen);
        return;
    }
    for (int i = 0; i < 6; i++)
    {
        bt_addr[i] = read_addr[i];
    }
    DUMP8("%02x ", bt_addr, 6);

    app_dongle_dev_conn(bt_addr);
}

static void app_dongle_disconn_cmd_handler(char* BufPtr, uint32_t BufLen)
{
    app_dongle_dev_disconn(NULL);
}

static void app_dongle_set_mode_handler(char* BufPtr, uint32_t BufLen)
{
    int boot_mode  = 0;

    if((BufPtr) && (BufLen))
    {
        TRACE(0, "[%s][%d]para: %s", __func__, __LINE__, BufPtr);
        sscanf(BufPtr, "%d", &boot_mode);
    }
    else
    {
        TRACE(0, "[%s][%d]paraeter error: %p, %d", __func__, __LINE__, BufPtr, BufLen);
        return;
    }
    TRACE(0, "[%s][%d]boo_mode=%d", __func__, __LINE__, boot_mode);

    app_switch_mode(boot_mode, true);
}

extern "C" void pmu_reboot(void);
static void app_dongle_reboot_handler(char* BufPtr, uint32_t BufLen)
{
    TRACE(1,"soft_reset_test");
    pmu_reboot();
}

static const app_dongle_cmd_handle_t app_dongle_cmd_table[] =
{
    {"dongle_open",       app_dongle_open_cmd_handler},
    {"dongle_close",      app_dongle_close_cmd_handler},
    {"dongle_scan_start", app_dongle_scan_start_cmd_handler},
    {"dongle_scan_stop",  app_dongle_scan_stop_cmd_handler},
    {"dongle_conn",       app_dongle_conn_cmd_handler},
    {"dongle_disconn",    app_dongle_disconn_cmd_handler},
    {"dongle_set_mode",   app_dongle_set_mode_handler},
    {"reboot",     app_dongle_reboot_handler},
};

uint32_t app_dongle_cmd_handler(unsigned char *buf, unsigned int length)
{
    char *para_addr = NULL;
    unsigned int para_len = 0;

    TRACE(1, "cmd: %s", buf);
    for (uint32_t i = 0; i < ARRAY_SIZE(app_dongle_cmd_table); i++)
    {
        if ((strncmp((char *)buf, app_dongle_cmd_table[i].string, strlen(app_dongle_cmd_table[i].string)) == 0) ||
                strstr(app_dongle_cmd_table[i].string, (char *)buf))
        {
            para_addr = strstr((char *)buf, "|");
            if (para_addr != NULL)
            {
                para_addr++;
                para_len = length - (para_addr - (char *)buf);
            }

            app_dongle_cmd_table[i].function(para_addr, para_len);
            break;
        }

        if (i == ARRAY_SIZE(app_dongle_cmd_table))
        {
            TRACE(0, "ERROR:can not find handle function");
        }
    }

    return 0;
}

void app_dongle_test_init(void)
{
#ifdef APP_TRACE_RX_ENABLE
    app_trace_rx_register("APP_DONGLE", app_dongle_cmd_handler);
#endif
}

void app_dongle_test_deinit(void)
{
#ifdef APP_TRACE_RX_ENABLE
    app_trace_rx_deregister("APP_DONGLE");
#endif
}

