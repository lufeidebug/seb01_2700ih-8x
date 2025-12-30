#if defined(__SNDP_PROJ__)
#include "stdio.h"
#include "cmsis_os.h"
#include "string.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "norflash_api.h"
#include "app_flash_api.h"

#include "sndp_if_common.h"
#include "sndp_if_data_access.h"



/**************************************************************************************************
* Constant
**************************************************************************************************/
#define DA_PARAM_SECTION_SIZE				(SNDP_PARAM_SECTION_SIZE)

#define DA_RUNNING_PARAM_SECTION_SIZE		(DA_PARAM_SECTION_SIZE/2)
#define DA_BACKUP_PARAM_SECTION_SIZE		(DA_PARAM_SECTION_SIZE/2)


/**************************************************************************************************
* Prototype
**************************************************************************************************/
extern uint32_t __sndp_param_start[];


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_da_ctx_s sndp_da_ctx;
static sndp_da_param_s sndp_da_running_param;
static sndp_da_param_s sndp_da_backup_param;


/**************************************************************************************************
* Function
**************************************************************************************************/
static int32_t sndp_da_find_field_info(sndp_da_field_id_e field_id, sndp_da_field_info_s *field_info)
{
	int32_t ret = 0;
	
	if(field_info == NULL)
		return -1;
	
	switch(field_id) {
		case SNDP_DA_FIELD_BAT_INFO:
			field_info->offset = (uint32_t)(&((sndp_da_param_s *)0)->field_bat_info);
			field_info->size = sizeof(sndp_da_field_bat_info_s);
			break;
		case SNDP_DA_FIELD_SN:
			field_info->offset = (uint32_t)(&((sndp_da_param_s *)0)->field_sn);
			field_info->size = sizeof(sndp_da_field_sn_s);
			break;
        case SNDP_DA_FIELD_BT_NAME:
			field_info->offset = (uint32_t)(&((sndp_da_param_s *)0)->field_bt_name);
			field_info->size = sizeof(sndp_da_field_bt_name_s);
			break;    
		case SNDP_DA_FIELD_TOUCH_CALIB_DATA:
			field_info->offset = (uint32_t)(&((sndp_da_param_s *)0)->field_touch_calib_data);
			field_info->size = sizeof(sndp_da_field_touch_calib_data_s);
			break;
#if defined(__SNDP_ALG_MGR__)		
		case SNDP_DA_FIELD_ALG_DATA:
			field_info->offset = (uint32_t)(&((sndp_da_param_s *)0)->field_alg_data);
			field_info->size = sizeof(sndp_da_field_alg_data_s);
			break;
#endif			
		default:
			ret = -1;
			ASSERT(0, "Invalid field_id=%d", field_id);
			break;
	}
	
	return ret;
}


void sndp_da_flush_running_param_to_flash(void)
{
	if(!sndp_da_ctx.inited) {
		SNDP_IF_TRACE(0, "uninitialized, return");
		return;
	}

	SNDP_IF_TRACE(1, "start_addr=%08x", sndp_da_ctx.running_param_start_addr);
	app_flash_erase((enum NORFLASH_API_MODULE_ID_T)sndp_da_ctx.section_mod_id, 
				sndp_da_ctx.running_param_start_addr, DA_RUNNING_PARAM_SECTION_SIZE);
	
	app_flash_program((enum NORFLASH_API_MODULE_ID_T)sndp_da_ctx.section_mod_id, 
			sndp_da_ctx.running_param_start_addr, 
			(uint8_t *)sndp_da_ctx.running_param_cache,
			sndp_da_ctx.running_param_size,
			false);
	
    app_flash_flush_pending_op((enum NORFLASH_API_MODULE_ID_T)sndp_da_ctx.section_mod_id, NORFLASH_API_ALL);
}

static void sndp_da_read_running_param_from_flash(uint8_t *data_buf, uint32_t length)
{
    SNDP_IF_TRACE(1, "read data, length=%d", length);

	if(length > sndp_da_ctx.running_param_size) {
		return;
	}

    app_flash_read((enum NORFLASH_API_MODULE_ID_T)sndp_da_ctx.section_mod_id,
                   sndp_da_ctx.running_param_start_addr,
                   data_buf,
                   length);
}

void sndp_da_flush_backup_param_to_flash(void)
{
	if(!sndp_da_ctx.inited) {
		SNDP_IF_TRACE(0, "uninitialized, return");
		return;
	}
	
    SNDP_IF_TRACE(1, "start_addr=%08x", sndp_da_ctx.backup_param_start_addr);

	app_flash_erase((enum NORFLASH_API_MODULE_ID_T)sndp_da_ctx.section_mod_id, 
				sndp_da_ctx.backup_param_start_addr, DA_BACKUP_PARAM_SECTION_SIZE);
	
	app_flash_program((enum NORFLASH_API_MODULE_ID_T)sndp_da_ctx.section_mod_id, 
			sndp_da_ctx.backup_param_start_addr, 
			(uint8_t *)sndp_da_ctx.backup_param_cache,
			sndp_da_ctx.backup_param_size,
			false);
	
    app_flash_flush_pending_op((enum NORFLASH_API_MODULE_ID_T)sndp_da_ctx.section_mod_id, NORFLASH_API_ALL);
}

static void sndp_da_read_backup_param_from_flash(uint8_t *data_buf, uint32_t length)
{
	SNDP_IF_TRACE(1, "read data, length=%d", length);

	if(length > sndp_da_ctx.backup_param_size){
		return;
	}

	app_flash_read((enum NORFLASH_API_MODULE_ID_T)sndp_da_ctx.section_mod_id,
				   sndp_da_ctx.backup_param_start_addr,
				   data_buf,
				   length);
}


int32_t sndp_da_write_field_data_to_running_param(sndp_da_field_id_e field_id, void *field_data, uint16_t field_size, bool save_to_flash)
{
	uint8_t *buff = (uint8_t *)sndp_da_ctx.running_param_cache;
	sndp_da_field_info_s field_info;

	if(!sndp_da_ctx.inited) {
		SNDP_IF_TRACE(0, "uninitialized, return");
		return -4;
	}

	if(field_data == NULL) {
         SNDP_IF_TRACE(0, "field_data == NULL, return");
		return -1;
	}

	if(sndp_da_find_field_info(field_id, &field_info) != 0) {
        SNDP_IF_TRACE(0, "find_field_info error, return");
		return -2;
	}

	if(field_size > field_info.size) {
        SNDP_IF_TRACE(0, "field_size(%d) != field_info.size(%d), return", field_size, field_info.size);
		return -3;
	}

	SNDP_IF_TRACE(3, "field_id=%d, field_size=%d, save_to_flash=%d", field_id, field_size, save_to_flash);

	sndp_da_field_common_s *field_common = (sndp_da_field_common_s *)field_data;
	field_common->key = SNDP_DA_PARAM_FIELD_VALID;
	memcpy(buff + field_info.offset, field_data, field_size);
	
	if(save_to_flash)
		sndp_da_flush_running_param_to_flash();
	return 0;
}


int32_t sndp_da_read_field_data_from_running_param(sndp_da_field_id_e field_id, void *field_data, uint16_t field_size, bool read_from_flash)
{
	uint8_t *buff = (uint8_t *)sndp_da_ctx.running_param_cache;
	sndp_da_field_info_s field_info;

    
	if(!sndp_da_ctx.inited) {
		SNDP_IF_TRACE(0, "uninitialized, return");
		return -4;
	}
	
	if(sndp_da_find_field_info(field_id, &field_info) != 0) {
        SNDP_IF_TRACE(0, "find_field_info error, return");
		return -1;
	}
	
	if(field_data == NULL) {
        SNDP_IF_TRACE(0, "field_data == NULL, return");
		return -2;
	}

	if(field_size > field_info.size) {
        SNDP_IF_TRACE(0, "field_size(%d) != field_info.size(%d), return", field_size, field_info.size);
		return -3;
	}

	SNDP_IF_TRACE(2, "field_id=%d, field_size=%d", field_id, field_size);
    if(read_from_flash) {
        memset(buff, 0, sndp_da_ctx.running_param_size);
        sndp_da_read_running_param_from_flash(buff, sndp_da_ctx.running_param_size);
        memcpy(field_data, buff + field_info.offset, field_size);
    } else {
    	memcpy(field_data, buff + field_info.offset, field_size);
    }
	
	return 0;
}


int32_t sndp_da_write_field_data_to_backup_param(sndp_da_field_id_e field_id, void *field_data, uint16_t field_size, bool save_to_flash)
{
	uint8_t *buff = (uint8_t *)sndp_da_ctx.backup_param_cache;
	sndp_da_field_info_s field_info;

	if(!sndp_da_ctx.inited) {
		SNDP_IF_TRACE(0, "uninitialized, return");
		return -4;
	}

	if(field_data == NULL) {
        SNDP_IF_TRACE(0, "field_data == NULL, return");
		return -1;
	}

	if(sndp_da_find_field_info(field_id, &field_info) != 0) {
        SNDP_IF_TRACE(0, "find_field_info error, return");
		return -2;
	}

	if(field_size > field_info.size) {
        SNDP_IF_TRACE(0, "field_size(%d) != field_info.size(%d), return", field_size, field_info.size);
		return -3;
	}

	SNDP_IF_TRACE(3, "field_id=%d, field_size=%d, save_to_flash=%d", field_id, field_size, save_to_flash);
	
	sndp_da_field_common_s *field_common = (sndp_da_field_common_s *)field_data;
	field_common->key = SNDP_DA_PARAM_FIELD_VALID;
	memcpy(buff + field_info.offset, field_data, field_size);
	
	if(save_to_flash)
		sndp_da_flush_backup_param_to_flash();
	return 0;
}


int32_t sndp_da_read_field_data_from_backup_param(sndp_da_field_id_e field_id, void *field_data, uint16_t field_size, bool read_from_flash)
{
	uint8_t *buff = (uint8_t *)sndp_da_ctx.backup_param_cache;
	sndp_da_field_info_s field_info;

	if(!sndp_da_ctx.inited) {
		SNDP_IF_TRACE(0, "uninitialized, return");
		return -4;
	}
	
	if(sndp_da_find_field_info(field_id, &field_info) != 0) {
        SNDP_IF_TRACE(0, "find_field_info error, return");
		return -1;
	}
	
	if(field_data == NULL) {
        SNDP_IF_TRACE(0, "field_data == NULL, return");
		return -2;
	}

	if(field_size > field_info.size) {
        SNDP_IF_TRACE(0, "field_size(%d) != field_info.size(%d), return", field_size, field_info.size);
		return -3;
	}

	SNDP_IF_TRACE(2, "field_id=%d, field_size=%d", field_id, field_size);
    if(read_from_flash) {
        memset(buff, 0, sndp_da_ctx.backup_param_size);
        sndp_da_read_backup_param_from_flash(buff, sndp_da_ctx.backup_param_size);
        memcpy(field_data, buff + field_info.offset, field_size);
    } else {
    	memcpy(field_data, buff + field_info.offset, field_size);
    }
    
	return 0;
}

static void sndp_da_flash_operate_callback(void *param)
{
    NORFLASH_API_OPERA_RESULT *opera_result = (NORFLASH_API_OPERA_RESULT*)param;

    SNDP_IF_TRACE(2, "type:%d, addr:0x%x",
          opera_result->type,
          opera_result->addr);
    SNDP_IF_TRACE(2, "len:0x%x, result:%d.",
          opera_result->len,
          opera_result->result);
}

void sndp_da_init(void)
{

	ASSERT(sizeof(sndp_da_param_s) < DA_RUNNING_PARAM_SECTION_SIZE, 
			"sizeof(sndp_da_param_s)(%d) > RUNNING_PARAM_SECTION_SIZE(%d)", 
			sizeof(sndp_da_param_s), DA_RUNNING_PARAM_SECTION_SIZE);

	ASSERT(sizeof(sndp_da_param_s) < DA_BACKUP_PARAM_SECTION_SIZE, 
				"sizeof(sndp_da_param_s)(%d) > BACKUP_PARAM_SECTION_SIZE(%d)", 
				sizeof(sndp_da_param_s), DA_BACKUP_PARAM_SECTION_SIZE);


	
	memset(&sndp_da_ctx, 0, sizeof(sndp_da_ctx));
	sndp_da_ctx.section_mod_id = NORFLASH_API_MODULE_ID_SNDP_PARAM;
	sndp_da_ctx.section_start_addr = (uint32_t)__sndp_param_start;
	sndp_da_ctx.section_size = DA_PARAM_SECTION_SIZE;

	SNDP_IF_TRACE(2, "__sndp_param_start: %p length: 0x%x", __sndp_param_start, DA_PARAM_SECTION_SIZE);
	app_flash_register_module((uint8_t)NORFLASH_API_MODULE_ID_SNDP_PARAM,
                              app_flash_get_dev_id_by_addr(sndp_da_ctx.section_start_addr),
                              sndp_da_ctx.section_start_addr,
                              sndp_da_ctx.section_size,
                              (uint32_t)sndp_da_flash_operate_callback);

	
	memset(&sndp_da_running_param, 0, sizeof(sndp_da_running_param));
	sndp_da_ctx.running_param_start_addr = 0;
	sndp_da_ctx.running_param_size = sizeof(sndp_da_param_s);
	sndp_da_ctx.running_param_cache = (sndp_da_param_s *)&sndp_da_running_param;

	memset(&sndp_da_backup_param, 0, sizeof(sndp_da_backup_param));
	sndp_da_ctx.backup_param_start_addr = sndp_da_ctx.running_param_start_addr + DA_RUNNING_PARAM_SECTION_SIZE;
	sndp_da_ctx.backup_param_size = sizeof(sndp_da_param_s);
	sndp_da_ctx.backup_param_cache = (sndp_da_param_s *)&sndp_da_backup_param;
	
	sndp_da_read_running_param_from_flash((uint8_t *)sndp_da_ctx.running_param_cache, sndp_da_ctx.running_param_size);
	sndp_da_read_backup_param_from_flash((uint8_t *)sndp_da_ctx.backup_param_cache, sndp_da_ctx.backup_param_size);

	sndp_da_ctx.inited = true;
}


#endif	/* __SNDP_PROJ__ */


