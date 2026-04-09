/***************************************************************************
 *
 * Copyright 2015-2025 BES.
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
 ****************************************************************************/

#pragma once

#include "bt_types.h"
#include "bt_base_attributes.h"

typedef struct
{
    bool enter_pairing_on_reconnect_mobile_failed;
} bta_ux_attributes_t;

void bta_normal_init(const bt_am_attributes_t *am_attributes, const bta_ux_attributes_t *ux_attributes);

typedef enum
{
    BTA_SUBSYS_DISABLED, // UI deinitialization completed
    BTA_SUBSYS_ENABLED,  // UI initialization completed
} bta_subsys_state_t;

// Customers should use BTA_UX_USER_CUSTOMER.
// BTA_UX_USER_INTERNAL_* are reserved for internal non-bthost modules (e.g., BESUI).
typedef enum
{
    BTA_UX_USER_BTA,
    BTA_UX_USER_APP,
    BTA_UX_USER_DEPRECATED,
    BTA_UX_USER_INTERNAL_1,
    BTA_UX_USER_CUSTOMER,
    BTA_UX_USER_MAX,
} bta_ux_user_t;

typedef struct
{
    // Notifies completion of UI initialization or deinitialization
    void (*bt_subsys_state_changed)(bta_subsys_state_t state);
    // Notifies entry into or exit from pairing mode
    void (*pairing_mode_changed)(bool enabled);
    void (*page_state_changed)(const bt_bdaddr_t *addr, bool page_started);
} bta_ui_state_changed_t;

void bta_register_ui_state_changed_hook(bta_ux_user_t user, const bta_ui_state_changed_t *hooks);

/**
 ****************************************************************************************
 * @brief       Callback to determine whether to accept an incoming Bluetooth connection request.
 * @param[in]   addr: Address of the device requesting connection.
 * @param[in]   cod: Class of Device.
 * @param[out]  preempt: Pointer to a bt_bdaddr_t variable.
 *              - If accepting this request would cause another device to be removed,
 *                `preempt` will point to a valid buffer.
 *              - Fill in `preempt` to specify which existing device to remove.
 *              - If `preempt` is NULL, no removal will occur.
 * @return      true to accept the connection request, false to reject it.
 ****************************************************************************************
 */
typedef bool (*bta_accept_connection_callback_t)(const bt_bdaddr_t *addr, const uint8_t cod[3], bt_bdaddr_t *preempt);
void bta_set_accept_connection_callback(bta_accept_connection_callback_t callback);

void bta_ux_open(uint8_t num_of_loaded_addrs, uint16_t page_count_of_each_addr, uint16_t page_timeout, uint32_t lea_adv_duration);
void bta_ux_close();

void bta_connect_bt_device(const bt_bdaddr_t *addr, uint8_t page_count, uint16_t page_timeout);
void bta_connect_all_bt_devices(uint8_t page_count, uint16_t page_timeout);

void bta_connect_lea_device(const bt_bdaddr_t *addr, uint32_t duration);
void bta_connect_all_lea_device(uint32_t duration);

/**
 ****************************************************************************************
 * @brief       Master switch for access mode.
 * @param[in]   enable: true to enable, false to disable.
 * @return      None
 ****************************************************************************************
 */
void bta_enable_access_mode(bool enable);

bool bta_is_access_mode_enabled(void);

/**
 ****************************************************************************************
 * @brief       Enters or exits pairing mode.
 * @param[in]   enable: true to enter pairing mode, false to exit.
 * @return      None
 ****************************************************************************************
 */
void bta_enable_pairing_mode(bool enable);

/**
 ****************************************************************************************
 * @brief       Checks whether the earbuds is in pairing mode.
 * @return      true if in pairing mode, false otherwise.
 ****************************************************************************************
 */
bool bta_is_pairing_mode_enabled(void);

/**
 ****************************************************************************************
 * @brief       Clears (resets to zero) the page attempt count for all addresses.
 *              This effectively stops all page activities. The page policy itself
 *              (i.e., address sequence) remains unchanged.
 * @return      None
 ****************************************************************************************
 */
void bta_clear_all_page();

/**
 ****************************************************************************************
 * @brief       Enables or disables blocking of all page activities.
 *              When blocking is enabled, ongoing page activity
 *              will be immediately cancelled, and no new page activity can proceed.
 *              However, no page attempts are discarded—remain queued.
 *              When blocking is disabled, previously blocked
 *              page activities are immediately resumed.
 * @param[in]   block: true to block page, false to unblock.
 * @return      None
 ****************************************************************************************
 */
void bta_block_page(bool block);

/**
 ****************************************************************************************
 * @brief       Configures whether to block paging during streaming.
 * @param[in]   block: true to block page, false to unblock.
 * @return      None
 ****************************************************************************************
 */
void bta_block_page_when_streaming(bool block);

void bta_support_preempt_when_a2dp_streaming(bool support);

/**
 ****************************************************************************************
 * @brief Get the connected device address list.
 *
 * @param[in] p_dev_addr_l device address list
 * @return uint8_t the number of address entries
 *
 * Examples:
 *
 *
 * ```
 * bt_bdaddr_list_t dev_addr_l = { 0 };
 * uint8_t count = bta_find_all_connected_bt_device(&dev_addr_l); // * `dev_addr_l` will be filled with connected address, `count` is the number of address entries
 * count = bta_find_all_connected_bt_device(NULL); // * only the connected bt device device count will be got
 * ```
 *
 ****************************************************************************************
 */
uint8_t bta_find_all_connected_bt_device(bt_bdaddr_list_t *p_dev_addr_l);

void bta_remove_bt_device(const bt_bdaddr_t *addr);

void bta_remove_lea_device(const bt_bdaddr_t *addr);

void bta_remove_all_devices();

/**
 ****************************************************************************************
 * @brief       Gets the maximum number of devices that can be managed.
 * @return      Maximum device count.
 ****************************************************************************************
 */
uint8_t bta_get_device_num_max(void);

/**
 ****************************************************************************************
 * @brief       Sets the maximum number of devices that can be managed.
 *              If the new limit is smaller than the current number of devices,
 *              some devices may be removed.
 * @param[in]   device_num_max: New maximum device count.
 * @param[in]   reserved_hint: Array of device addresses to prioritize for retention
 *              if device removal is required.
 * @param[in]   reserved_hint_count: Number of entries in the reserved_hint array.
 * @return      None
 ****************************************************************************************
 */
void bta_set_device_num_max(uint8_t device_num_max, const bt_bdaddr_t reserved_hint[], uint8_t reserved_hint_count);
