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
 * @brief xxx.
 *
 ****************************************************************************/
#ifndef __APP_DONGLE_H__
#define __APP_DONGLE_H__

/****************************** header include ********************************/
#include <stdint.h>

/***************************** external declaration *****************************/

/***************************** macro defination *******************************/

/*****************************  type defination ********************************/

/*****************************  variable defination *****************************/

/*****************************  function declaration ****************************/
/**
 ***************************************************************************
 * @brief application dongle initialization
 * @param[in]  : application mode, see@nvrec_appmode_e
 * @param[out] : none
 * @return: none
 ***************************************************************************
 */
void app_dongle_init(uint8_t mode);

/**
 ***************************************************************************
 * @brief application dongle deinitialization
 * @param[in]  : none
 * @param[out] : none
 * @return: none
 ***************************************************************************
 */
void app_dongle_deinit(void);

/**
 ***************************************************************************
 * @brief application dongle open
 * @param[in]  : input path type, see@BT_SVC_AUDIO_PATH_TYPE_E
 * @param[in]  : output path type, see@BT_SVC_AUDIO_PATH_TYPE_E
 * @param[out] : none
 * @return: none
 ***************************************************************************
 */
void app_dongle_open(uint8_t input_type, uint8_t output_type);

/**
 ***************************************************************************
 * @brief application dongle close
 * @param[in]  : none
 * @param[out] : none
 * @return: none
 ***************************************************************************
 */
void app_dongle_close(void);

/**
 ***************************************************************************
 * @brief application dongle stop scan device
 * @param[in]  : none
 * @param[out] : none
 * @return: none
 ***************************************************************************
 */
void app_dongle_start_scan(void);

/**
 ***************************************************************************
 * @brief application dongle start scan device
 * @param[in]  : none
 * @param[out] : none
 * @return: none
 ***************************************************************************
 */
void app_dongle_stop_scan(void);

/**
 ***************************************************************************
 * @brief application dongle connect device
 * @param[in]  : Address of the connected target device
 * @param[out] : none
 * @return: none
 ***************************************************************************
 */
void app_dongle_dev_conn(uint8_t* addr);

/**
 ***************************************************************************
 * @brief application dongle disconnect device
 * @param[in]  : Address of the connected target device
 * @param[out] : none
 * @return: none
 ***************************************************************************
 */
void app_dongle_dev_disconn(uint8_t* addr);

#endif //__APP_DONGLE_H__