/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "string.h"
#include "bluetooth_ble_api.h"
#include "ai_thread.h"
#include "ai_control.h"
#include "app_ai_ble.h"
#include "app_ai_if_ble.h"
#include "app_ai_tws.h"
#include "ai_transport.h"

#ifdef __AMA_VOICE__
extern void app_ama_gatt_svc_event_report(bes_ble_ai_event_param_t *param);
#endif
#ifdef __DMA_VOICE__
extern void app_dma_gatt_svc_event_report(bes_ble_ai_event_param_t *param);
#endif

#ifdef __GMA_VOICE__
extern void app_gma_gatt_svc_event_report(bes_ble_ai_event_param_t *param);
#endif

#ifdef __SMART_VOICE__
extern void app_smart_gatt_svc_event_report(bes_ble_ai_event_param_t *param);
#endif

#ifdef DUAL_MIC_RECORDING
extern void app_recording_gatt_svc_event_report(bes_ble_ai_event_param_t *param);
#endif

#ifdef __TENCENT_VOICE__
extern void app_tencent_gatt_svc_event_report(bes_ble_ai_event_param_t *param);
#endif

#ifdef __CUSTOMIZE_VOICE__
extern void app_voice_common_gatt_svc_event_report(bes_ble_ai_event_param_t *param);
#endif

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
struct app_ai_env_tag app_ai_env[2];

/*
 * GLOBAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void app_ai_connected_evt_handler(uint8_t conidx, uint8_t connect_index, uint16_t connhdl)
{
    AI_VOICE_TRACE(1,"%s", __func__);
    if (app_ai_ble_is_connected(connect_index) && (conidx != app_ai_env[connect_index].connectionIndex))
    {
        AI_VOICE_TRACE(0, "ai ble has connected");
        return;
    }

    app_ai_env[connect_index].connectionIndex = conidx;
    app_ai_env[connect_index].connhdl = connhdl;
}

void app_ai_disconnected_evt_handler(uint8_t conidx)
{
    for(uint8_t connect_index = 0; connect_index < AI_CONNECT_NUM_MAX; connect_index++)
    {
        AI_VOICE_TRACE(0, "%s index %d conidx 0x%x 0x%x", __func__, connect_index, conidx, app_ai_env[connect_index].connectionIndex);
        if (conidx == app_ai_env[connect_index].connectionIndex)
        {
            app_ai_env[connect_index].connectionIndex = BLE_INVALID_CONNECTION_INDEX;
            app_ai_env[connect_index].isCmdNotificationEnabled = false;
            app_ai_env[connect_index].isDataNotificationEnabled = false;
            app_ai_env[connect_index].mtu = 0;

            AI_connect_info *ai_info = app_ai_get_connect_info(connect_index);
            if(NULL == ai_info) {
                return;
            }
            uint8_t dest_id = app_ai_get_dest_id(ai_info);
            app_ai_update_connect_state(AI_IS_DISCONNECTING, connect_index);
            AI_VOICE_TRACE(3,"%s %d %d", __func__, app_ai_get_transport_type(ai_info->ai_spec, connect_index), dest_id);
            if (app_ai_get_transport_type(ai_info->ai_spec, connect_index) == AI_TRANSPORT_BLE)
            {
                AI_VOICE_TRACE(1,"%s", __func__);
                ai_function_handle(CALLBACK_AI_DISCONNECT, NULL, AI_TRANSPORT_BLE, ai_info->ai_spec, dest_id);
            }
        }
    }
}

bool app_ai_ble_is_connected(uint8_t connect_index)
{
    return (BLE_INVALID_CONNECTION_INDEX != app_ai_env[connect_index].connectionIndex);
}

uint8_t app_ai_ble_get_connection_index(uint8_t connect_index)
{
    return app_ai_env[connect_index].connectionIndex;
}

void app_ai_disconnect_ble(uint8_t connect_index)
{
    if (app_ai_ble_is_connected(connect_index))
    {
        app_ai_if_ble_disconnect_ble(app_ai_env[connect_index].connectionIndex);
    }
}

uint16_t app_ai_ble_get_conhdl(uint8_t connect_index)
{
    return app_ai_env[connect_index].connhdl;
}

bool app_ai_ble_notification_processing_is_busy(uint8_t ai_index)
{
    return app_ai_if_ble_check_if_notification_processing_is_busy(app_ai_env[ai_index].connectionIndex);
}

void app_ai_ble_update_conn_param_mode(bool isEnabled, uint8_t ai_index, uint8_t connect_index)
{
    AI_VOICE_TRACE(2,"%s isEnabled %d", __func__, isEnabled);

    if (AI_TRANSPORT_BLE == app_ai_get_transport_type(ai_index, connect_index))
    {
        app_ai_if_ble_update_conn_param_mode(isEnabled);
    }
}

static bool app_ai_is_ble_adv_uuid_enabled(void)
{
    bool adv_enable = false;
#if defined(SLAVE_ADV_BLE)
    adv_enable = true;
#endif

    if (app_ai_is_in_tws_mode(0) && !app_ai_tws_init_done())
    {
        AI_VOICE_TRACE(1,"%s ibrt don't init", __func__);
    }
    else if (app_ai_is_in_tws_mode(0) && app_ai_tws_get_local_role() == APP_AI_TWS_SLAVE)
    {
        AI_VOICE_TRACE(2,"%s role %d isn't MASTER or UNKNOW", __func__, app_ai_tws_get_local_role());
    }
    else
    {
        //uint8_t ai_spec = app_ai_if_get_ai_spec();
        //if (app_is_ai_voice_connected(ai_spec))
        //{
        //    AI_VOICE_TRACE(1,"%s ai has connected", __func__);
        //}
        //else if (app_ai_is_role_switching(ai_spec))
        //{
        //    AI_VOICE_TRACE(1,"%s ai is role switching", __func__);
        //}
        //else
        //{
            adv_enable = true;
        //}
    }

    return adv_enable;
}

/****************************function defination****************************/
static bool app_ai_ble_data_fill_handler(void *param)
{
    bool adv_enable = app_ai_is_ble_adv_uuid_enabled();

    if (adv_enable && param)
    {
        uint32_t ai_spec = app_ai_if_get_ai_spec();
#ifdef IS_MULTI_AI_ENABLED
        if(ai_spec & 0xFF)
        {
            adv_enable = ai_function_handle(API_START_ADV, param, 0, (uint8_t)(ai_spec & 0xFF), 0);
        }
        if(ai_spec >> 8)
        {
            adv_enable = ai_function_handle(API_START_ADV, param, 0, (uint8_t)(ai_spec >> 8), 0)?true:adv_enable;
        }
#else
        adv_enable = ai_function_handle(API_START_ADV, param, 0, (uint8_t)ai_spec, 0);
#endif
    }

    return adv_enable;
}

void app_ai_mtu_exchanged_handler(uint8_t conidx, uint16_t mtu)
{
    for(uint8_t connect_index=0; connect_index < AI_CONNECT_NUM_MAX; connect_index++)
    {
        AI_VOICE_TRACE(3,"%s conidx %d index %d", __func__, conidx, app_ai_env[connect_index].connectionIndex);

        if (conidx == app_ai_env[connect_index].connectionIndex) {
            AI_connect_info *ai_info = app_ai_get_connect_info(connect_index);
            uint8_t dest_id = app_ai_get_dest_id(ai_info);
            ai_function_handle(CALLBACK_UPDATE_MTU, NULL, mtu, ai_info->ai_spec, dest_id);
        }
        else {
            app_ai_env[connect_index].mtu = mtu;
        }
    }
}

AI_BLE_HANDLER_INSTANCE_T *ai_ble_handler_get_entry_pointer_from_ai_code(uint32_t ai_code)
{
    for (uint32_t index = 0;
         index < ((uint32_t)__ai_ble_handler_table_end - (uint32_t)__ai_ble_handler_table_start) / sizeof(AI_BLE_HANDLER_INSTANCE_T);
         index++)
    {
        if (AI_BLE_HANDLER_PTR_FROM_ENTRY_INDEX(index)->ai_code == ai_code)
        {
            AI_VOICE_TRACE(3, "%s ai_code %d index %d", __func__, ai_code, index);
            return AI_BLE_HANDLER_PTR_FROM_ENTRY_INDEX(index);
        }
    }

    //ASSERT(0, "%s fail ai_code %d", __func__, ai_code);
    AI_VOICE_TRACE(0, "%s fail ai_code %d", __func__, ai_code);
    return NULL;
}

uint32_t ai_ble_handler_get_entry_index_from_ai_code(uint32_t ai_code)
{
    for (uint32_t index = 0;
         index < ((uint32_t)__ai_ble_handler_table_end - (uint32_t)__ai_ble_handler_table_start) / sizeof(AI_BLE_HANDLER_INSTANCE_T);
         index++)
    {
        if (AI_BLE_HANDLER_PTR_FROM_ENTRY_INDEX(index)->ai_code == ai_code)
        {
            AI_VOICE_TRACE(3, "%s ai_code %d index %d", __func__, ai_code, index);
            return index;
        }
    }

    //ASSERT(0, "%s fail ai_code %d", __func__, ai_code);
    AI_VOICE_TRACE(0, "%s fail ai_code %d", __func__, ai_code);
    return INVALID_AI_BLE_HANDLER_ENTRY_INDEX;
}

/*
 * LOCAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
//const ai_ble_ke_state_handler_t *app_ai_table_handler[MUTLI_AI_NUM];
//const ai_ble_ke_state_handler_t *app_ai_table_handler;

void ai_ble_handler_init(uint32_t ai_spec)
{
#if 0
    AI_BLE_HANDLER_INSTANCE_T *pInstance = NULL;

    pInstance = ai_ble_handler_get_entry_pointer_from_ai_code(ai_spec);
    if (pInstance)
    {
        app_ai_table_handler[ai_spec] = pInstance->ai_table_handler;
    }
    else
    {
        app_ai_table_handler[ai_spec] = NULL;
    }
#endif
}

void ble_ai_gatt_event_cb(bes_ble_ai_event_param_t *param)
{
    if(param->event_type == BES_BLE_AI_MTU_CHANGE)
    {
        app_ai_mtu_exchanged_handler(param->conidx, param->data.mtu);
        return;
    }
    if(param->event_type == BES_BLE_AI_DISCONN)
    {
        app_ai_disconnected_evt_handler(param->conidx);
        return;
    }
    switch(param->ai_type){
    #ifdef __AMA_VOICE__
    case BES_BLE_AI_SPEC_AMA:
        app_ama_gatt_svc_event_report(param);
        break;
    #endif
    #ifdef __DMA_VOICE__
    case BES_BLE_AI_SPEC_DMA:
        app_dma_gatt_svc_event_report(param);
        break;
    #endif
    #ifdef __GMA_VOICE__
    case BES_BLE_AI_SPEC_GMA:
        app_gma_gatt_svc_event_report(param);
        break;
    #endif
    #ifdef __SMART_VOICE__
    case BES_BLE_AI_SPEC_SMART:
        app_smart_gatt_svc_event_report(param);
        break;
    #endif
    #ifdef __TENCENT_VOICE__
    case BES_BLE_AI_SPEC_TENCENT:
        app_tencent_gatt_svc_event_report(param);
        break;
    #endif
    #ifdef DUAL_MIC_RECORDING
    case BES_BLE_AI_SPEC_RECORDING:
        app_recording_gatt_svc_event_report(param);
        break;
    #endif
    #ifdef __CUSTOMIZE_VOICE__
    case BES_BLE_AI_SPEC_COMMON:
        app_voice_common_gatt_svc_event_report(param);
        break;
    #endif
    default:
        break;
    }
}

void app_ai_ble_init(uint32_t ai_spec)
{
    // Reset the environment
    for(uint8_t ai_connect_index = 0; ai_connect_index < AI_CONNECT_NUM_MAX; ai_connect_index++)
    {
        app_ai_env[ai_connect_index].connectionIndex =  APP_AI_IF_BLE_INVALID_CONNECTION_INDEX;
        app_ai_env[ai_connect_index].isCmdNotificationEnabled = false;
        app_ai_env[ai_connect_index].isDataNotificationEnabled = false;
        app_ai_env[ai_connect_index].mtu = 0;
    }

    app_ai_if_ble_register_data_fill_handle((void *)app_ai_ble_data_fill_handler, false);

    bes_ble_ai_gatt_event_reg(ble_ai_gatt_event_cb);
}

/// @} APP

