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

#define DA_RUNNING_PARAM_STRUCT_VER         (0xFEFE0001)
#define DA_BACKUP_PARAM_STRUCT_VER          (0xFEFE0001)


/**************************************************************************************************
* Prototype
**************************************************************************************************/
extern uint32_t __sndp_param_start[];

typedef int32_t (* da_find_field_info_func)(sndp_da_field_id_e field_id, sndp_da_field_info_s *field_info);
typedef uint16_t (* da_calc_struct_checksum_func)(void);
typedef uint16_t (* da_calc_data_checksum_func)(void *param_buf);
typedef void (* da_update_data_checksum_func)(void *param_buf);


typedef struct {
	uint32_t section_offset;
    uint32_t section_size;

    uint32_t curr_param_struct_ver;
	uint32_t param_size;
	uint8_t *param_buf;
    
    da_find_field_info_func find_field_info;
    da_calc_struct_checksum_func calc_struct_checksum;
    da_calc_data_checksum_func calc_data_checksum;
    da_update_data_checksum_func update_data_checksum;
    
} sndp_da_access_info_s;


typedef struct {
	bool inited;
	
	uint8_t section_mod_id;
	uint32_t section_start_addr;
	uint32_t section_size;

	sndp_da_access_info_s running_access_info;
	sndp_da_access_info_s backup_access_info;
	
} sndp_da_ctx_s;


/**************************************************************************************************
* Extern
**************************************************************************************************/
static int32_t sndp_da_wirte_param_to_flash(sndp_da_access_info_s *info);
static int32_t sndp_da_read_param_from_flash(sndp_da_access_info_s *info);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_da_ctx_s sndp_da_ctx;
static sndp_da_running_param_s sndp_da_running_param;
static sndp_da_backup_param_s sndp_da_backup_param;


/**************************************************************************************************
* Function
**************************************************************************************************/
static uint16_t sndp_da_calc_crc16(uint16_t crc, const uint8_t *buff, uint32_t start, uint32_t end)
{
    ASSERT(buff != NULL, "%s, buff == NULL", __func__);
    ASSERT(end >= start, "%s, end(%d) < start(%d)", __func__, end, start);

    for(uint32_t i = start; i < end; i++) {
        crc = (crc >> 8) | (crc << 8);
        crc ^= buff[i];
        crc ^= ((uint8_t) crc) >> 4;
        crc ^= crc << 12;
        crc ^= (crc & 0xFF) << 5;
    }

    return crc;    
}

static uint16_t sndp_da_calc_running_struct_checksum(void)
{
    sndp_da_running_param_s *p_param = (sndp_da_running_param_s *)0;
    uint16_t checksum = 0;
    uint32_t addr;

    addr = (uint32_t)&p_param->struct_ver;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));

    addr = (uint32_t)&p_param->struct_checksum;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));

    addr = (uint32_t)&p_param->data_checksum;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));
    
    addr = (uint32_t)&p_param->field_bat_info;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));

#if defined(__SNDP_SLEEP_APP__) 
#if defined(__SNDP_EQ_PARAM_SETTING__)
    addr = (uint32_t)&p_param->field_eq_data;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));
#endif

    addr = (uint32_t)&p_param->field_sleep_app_data;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));

#endif    

    addr = (uint32_t)&p_param->data_end;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));

    return checksum;
}

static uint16_t sndp_da_calc_running_data_checksum(void *param)
{
    if(param == NULL) {
        return 0;
    }
    
    sndp_da_running_param_s *p_temp = (sndp_da_running_param_s *)0;
    sndp_da_running_param_s *running_param = (sndp_da_running_param_s *)param;
    
    return sndp_da_calc_crc16(0, (uint8_t *)running_param, (uint32_t)&p_temp->data_start, (uint32_t)&p_temp->data_end);
}

static void sndp_da_update_running_checksum(void *param)
{
    if(param == NULL) {
        return;
    }
    
    sndp_da_running_param_s *running_param = (sndp_da_running_param_s *)param;
    running_param->struct_ver = DA_RUNNING_PARAM_STRUCT_VER;
    running_param->struct_checksum = sndp_da_calc_running_struct_checksum();
    running_param->data_checksum = sndp_da_calc_running_data_checksum(param);

    SNDP_IF_TRACE(0, "%d, %08X, %08X, %08X ", __LINE__,
        running_param->struct_ver,
        running_param->struct_checksum,
        running_param->data_checksum);
}


static uint16_t sndp_da_calc_backup_struct_checksum(void)
{
    sndp_da_backup_param_s *p_param = (sndp_da_backup_param_s *)0;
    uint16_t checksum = 0;
    uint32_t addr;

    addr = (uint32_t)&p_param->struct_ver;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));

    addr = (uint32_t)&p_param->struct_checksum;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));
    
    addr = (uint32_t)&p_param->data_checksum;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));
    
    addr = (uint32_t)&p_param->data_start;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));

    addr = (uint32_t)&p_param->field_sn;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));
    
    addr = (uint32_t)&p_param->field_ppg_calib_data;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));

    addr = (uint32_t)&p_param->field_acc_calib_data;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));

    addr = (uint32_t)&p_param->field_test_flag;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));

    addr = (uint32_t)&p_param->field_dev_color;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));

    //---------------------------------------------------------------------------------
    addr = (uint32_t)&p_param->data_end;
    checksum = sndp_da_calc_crc16(checksum, (uint8_t *)&addr, 0, sizeof(uint32_t));

    return checksum;
}

static uint16_t sndp_da_calc_backup_data_checksum(void *param)
{
    if(param == NULL) {
        return 0;
    }

    sndp_da_backup_param_s *p_temp = (sndp_da_backup_param_s *)0;
    sndp_da_backup_param_s *backup_param = (sndp_da_backup_param_s *)param;
    return sndp_da_calc_crc16(0, (uint8_t *)backup_param, (uint32_t)&p_temp->data_start, (uint32_t)&p_temp->data_end);
}


static void sndp_da_update_backup_checksum(void *param)
{
    if(param == NULL) {
        return;
    }
    
    sndp_da_backup_param_s *backup_param = (sndp_da_backup_param_s *)param;
    backup_param->struct_ver = DA_RUNNING_PARAM_STRUCT_VER;
    backup_param->struct_checksum = sndp_da_calc_backup_struct_checksum();
    backup_param->data_checksum = sndp_da_calc_backup_data_checksum(param);

    SNDP_IF_TRACE(0, "%d, %08X, %08X, %08X ", __LINE__,
        backup_param->struct_ver,
        backup_param->struct_checksum,
        backup_param->data_checksum);
}

static bool sndp_da_check_running_data_validity(sndp_da_access_info_s *info)
{
    sndp_da_running_param_s *param;
    uint32_t struct_checksum;
    uint32_t data_checksum;
    
    if(info == NULL) {
        SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
        return false;
    }

    param = (sndp_da_running_param_s *)info->param_buf;
    if(param == NULL) {
        SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
        return false;
    }

    struct_checksum = info->calc_struct_checksum();
    data_checksum = info->calc_data_checksum(info->param_buf);
    
    if((param->struct_ver != info->curr_param_struct_ver)
        || (param->struct_checksum != struct_checksum)
        || (param->data_checksum != data_checksum)) {

        SNDP_IF_TRACE(0, "%d, struct_ver: %08X, %08X", __LINE__,
            param->struct_ver, info->curr_param_struct_ver);
        
        SNDP_IF_TRACE(0, "%d, struct_checksum: %08X, %08X", __LINE__,
            param->struct_checksum, struct_checksum);
        
        SNDP_IF_TRACE(0, "%d, data_checksum: %08X, %08X", __LINE__,
            param->data_checksum, data_checksum);
        
        memset(info->param_buf, 0, info->param_size);
        sndp_da_wirte_param_to_flash(info);
        
        return false;
    }

    return true;
}

POSSIBLY_UNUSED static bool sndp_da_check_backup_data_validity(sndp_da_access_info_s *info)
{
    sndp_da_backup_param_s *param;
    uint32_t struct_checksum;
    uint32_t data_checksum;
    
    if(info == NULL) {
        SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
        return false;
    }

    param = (sndp_da_backup_param_s *)info->param_buf;
    if(param == NULL) {
        SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
        return false;
    }

    struct_checksum = info->calc_struct_checksum();
    data_checksum = info->calc_data_checksum(info->param_buf);
    
    if((param->struct_ver != info->curr_param_struct_ver)
        || (param->struct_checksum != struct_checksum)
        || (param->data_checksum != data_checksum)) {

        SNDP_IF_TRACE(0, "%d, struct_ver: %08X, %08X", __LINE__,
            param->struct_ver, info->curr_param_struct_ver);
        
        SNDP_IF_TRACE(0, "%d, struct_checksum: %08X, %08X", __LINE__,
            param->struct_checksum, struct_checksum);
        
        SNDP_IF_TRACE(0, "%d, data_checksum: %08X, %08X", __LINE__,
            param->data_checksum, data_checksum);
        
        memset(info->param_buf, 0, info->param_size);
        sndp_da_wirte_param_to_flash(info);
        
        return false;
    }

    return true;
}



static int32_t sndp_da_find_running_field_info(sndp_da_field_id_e field_id, sndp_da_field_info_s *field_info)
{
	int32_t ret = 0;
    sndp_da_running_param_s *p_param = (sndp_da_running_param_s *)0;
    
	if(field_info == NULL)
		return -1;
	
	switch(field_id) {
		case SNDP_DA_FIELD_BAT_INFO:
			field_info->offset = (uint32_t)&p_param->field_bat_info;
			field_info->size = sizeof(sndp_da_field_bat_info_s);
			break;
 
#if defined(__SNDP_SLEEP_APP__)
#if defined(__SNDP_EQ_PARAM_SETTING__)
		case SNDP_DA_FIELD_EQ_DATA:
			field_info->offset = (uint32_t)&p_param->field_eq_data;
			field_info->size = sizeof(sndp_da_field_eq_data_s);
			break;
#endif
		case SNDP_DA_FIELD_APP_DATA:
			field_info->offset = (uint32_t)&p_param->field_sleep_app_data;
			field_info->size = sizeof(sndp_da_field_sleep_app_data_s);
			break;		
#endif

		default:
			ret = -1;
			break;
	}

    if(ret) {
        SNDP_IF_TRACE(0, "Invalid field_id=%d", field_id);
    } else {
        SNDP_IF_TRACE(0, "Valid field_id=%d", field_id);
    }
	return ret;
}

static int32_t sndp_da_find_backup_field_info(sndp_da_field_id_e field_id, sndp_da_field_info_s *field_info)
{
	int32_t ret = 0;
    sndp_da_backup_param_s *p_param = (sndp_da_backup_param_s *)0;
    
	if(field_info == NULL)
		return -1;
	
	switch(field_id) {
		case SNDP_DA_FIELD_SN:
			field_info->offset = (uint32_t)&p_param->field_sn;
			field_info->size = sizeof(sndp_da_field_sn_s);
			break;  
        case SNDP_DA_FIELD_PROXIMITY_CALIB_DATA:
            field_info->offset = (uint32_t)&p_param->field_proximity_calib_data;
			field_info->size = sizeof(sndp_da_field_proximity_calib_data_s);
			break;
		case SNDP_DA_FIELD_PPG_CALIB_DATA:
			field_info->offset = (uint32_t)&p_param->field_ppg_calib_data;
			field_info->size = sizeof(sndp_da_field_ppg_calib_data_s);
			break;
        case SNDP_DA_FIELD_ACC_CALIB_DATA:
			field_info->offset = (uint32_t)&p_param->field_acc_calib_data;
			field_info->size = sizeof(sndp_da_field_acc_calib_data_s);
			break;
        case SNDP_DA_FIELD_TEST_FLAG:
			field_info->offset = (uint32_t)&p_param->field_test_flag;
			field_info->size = sizeof(sndp_da_field_test_flag_s);
			break;
        case SNDP_DA_FIELD_DEV_COLOR:
			field_info->offset = (uint32_t)&p_param->field_dev_color;
			field_info->size = sizeof(sndp_da_field_dev_color_s);
			break;
		default:
			ret = -1;
			break;
	}

    if(ret) {
        SNDP_IF_TRACE(0, "Invalid field_id=%d", field_id);
    } else {
        SNDP_IF_TRACE(0, "Valid field_id=%d", field_id);
    }
	return ret;
}


static int32_t sndp_da_wirte_param_to_flash(sndp_da_access_info_s *info)
{
	if(!sndp_da_ctx.inited) {
		SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return -1;
	}

    if(info == NULL) {
        SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
        return -2;
    }

	SNDP_IF_TRACE(1, "addr=%d, size=%d", info->section_offset, info->param_size);
    
    info->update_data_checksum(info->param_buf);
    
	app_flash_erase((enum NORFLASH_API_MODULE_ID_T)sndp_da_ctx.section_mod_id, 
				info->section_offset, 
				info->section_size);
	
	app_flash_program((enum NORFLASH_API_MODULE_ID_T)sndp_da_ctx.section_mod_id, 
    			info->section_offset, 
    			(uint8_t *)info->param_buf,
    			info->param_size,
    			false);
	
    app_flash_flush_pending_op((enum NORFLASH_API_MODULE_ID_T)sndp_da_ctx.section_mod_id, NORFLASH_API_ALL);
    return 0;
}

static int32_t sndp_da_read_param_from_flash(sndp_da_access_info_s *info)
{
    if(!sndp_da_ctx.inited) {
		SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return -1;
	}

    if(info == NULL) {
        SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
        return -2;
    }

    SNDP_IF_TRACE(1, "addr=%d, size=%d", info->section_offset, info->param_size);

    app_flash_read((enum NORFLASH_API_MODULE_ID_T)sndp_da_ctx.section_mod_id,
                   info->section_offset,
                   (uint8_t *)info->param_buf,
                   info->param_size);

    return 0;
}


static int32_t sndp_da_write_field_data(sndp_da_access_info_s *access_info, sndp_da_field_info_s field_info, void *field_data, uint16_t data_size, bool save_to_flash)
{
	if(!sndp_da_ctx.inited) {
		SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return -4;
	}

    if(access_info == NULL) {
        SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return -1;
	}

	if(field_data == NULL) {
        SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return -1;
	}

	if(data_size < 4 || data_size > field_info.size) {
        SNDP_IF_TRACE(0, "data_size(%d) > field_size(%d), rtn", data_size, field_info.size);
		return -3;
	}

	SNDP_IF_TRACE(3, "offset=%d, size=%d, data_size=%d, save=%d", field_info.offset, field_info.size, data_size, save_to_flash);

	sndp_da_field_common_s *field_common = (sndp_da_field_common_s *)field_data;
	field_common->key = SNDP_DA_PARAM_FIELD_VALID;
	memcpy(&access_info->param_buf[field_info.offset], field_data, data_size);
	
	if(save_to_flash) {
		sndp_da_wirte_param_to_flash(access_info);
	}
	return 0;
}

static int32_t sndp_da_read_field_data(sndp_da_access_info_s *access_info, sndp_da_field_info_s field_info, void *field_data, uint16_t data_size, bool read_from_flash)
{
	if(!sndp_da_ctx.inited) {
		SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return -4;
	}
	
	if(field_data == NULL) {
        SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return -2;
	}

	if(data_size > field_info.size) {
        SNDP_IF_TRACE(0, "data_size(%d) > field_size(%d), rtn", data_size, field_info.size);
		return -3;
	}

    SNDP_IF_TRACE(3, "offset=%d, size=%d, data_size=%d, from_flash=%d", field_info.offset, field_info.size, data_size, read_from_flash);
    
    if(read_from_flash) {
        memset(access_info->param_buf, 0, access_info->param_size);
        sndp_da_read_param_from_flash(access_info);
        memcpy(field_data, &access_info->param_buf[field_info.offset], data_size);
    } else {
    	memcpy(field_data, &access_info->param_buf[field_info.offset], data_size);
    }
	
	return 0;
}

int32_t sndp_da_write_field(sndp_da_field_id_e field_id, void *field_data, uint16_t field_size, bool save_to_flash)
{
    sndp_da_field_info_s field_info;

    if(!sndp_da_ctx.inited) {
		SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return -4;
	}
    
    if(sndp_da_ctx.running_access_info.find_field_info(field_id, &field_info) == 0) {
	    return sndp_da_write_field_data(&sndp_da_ctx.running_access_info, 
                    field_info, field_data, field_size, save_to_flash);
    } else if(sndp_da_ctx.backup_access_info.find_field_info(field_id, &field_info) == 0) {
	    return sndp_da_write_field_data(&sndp_da_ctx.backup_access_info, 
                    field_info, field_data, field_size, save_to_flash);
    }

    return -1;
}

int32_t sndp_da_read_field(sndp_da_field_id_e field_id, void *field_data, uint16_t field_size, bool read_from_flash)
{
    sndp_da_field_info_s field_info;

    if(!sndp_da_ctx.inited) {
		SNDP_IF_TRACE(0, "%d, rtn", __LINE__);
		return -4;
	}
    
    if(sndp_da_ctx.running_access_info.find_field_info(field_id, &field_info) == 0) {
	    return sndp_da_read_field_data(&sndp_da_ctx.running_access_info, 
                    field_info, field_data, field_size, read_from_flash);
    } else if(sndp_da_ctx.backup_access_info.find_field_info(field_id, &field_info) == 0) {
	    return sndp_da_read_field_data(&sndp_da_ctx.backup_access_info, 
                    field_info, field_data, field_size, read_from_flash);
    }

    return -1;
}

void sndp_da_flush_param_to_flash(void)
{
    sndp_da_wirte_param_to_flash(&sndp_da_ctx.running_access_info);
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
	memset(&sndp_da_ctx, 0, sizeof(sndp_da_ctx));

    memset(&sndp_da_running_param, 0, sizeof(sndp_da_running_param));
	sndp_da_ctx.running_access_info.section_offset = 0;
    sndp_da_ctx.running_access_info.section_size = DA_RUNNING_PARAM_SECTION_SIZE;

    sndp_da_ctx.running_access_info.curr_param_struct_ver = DA_RUNNING_PARAM_STRUCT_VER;
    sndp_da_ctx.running_access_info.param_size = sizeof(sndp_da_running_param_s);
    sndp_da_ctx.running_access_info.param_buf = (uint8_t *)&sndp_da_running_param;
    
    sndp_da_ctx.running_access_info.find_field_info = sndp_da_find_running_field_info;
    sndp_da_ctx.running_access_info.calc_struct_checksum = sndp_da_calc_running_struct_checksum;
    sndp_da_ctx.running_access_info.calc_data_checksum = sndp_da_calc_running_data_checksum;
    sndp_da_ctx.running_access_info.update_data_checksum = sndp_da_update_running_checksum;
    
    ASSERT(sndp_da_ctx.running_access_info.param_size < DA_RUNNING_PARAM_SECTION_SIZE, 
            "param_size(%d) > RUNNING_PARAM_SECTION_SIZE(%d)", 
            sndp_da_ctx.running_access_info.param_size, 
            DA_RUNNING_PARAM_SECTION_SIZE);

	memset(&sndp_da_backup_param, 0, sizeof(sndp_da_backup_param));
    sndp_da_ctx.backup_access_info.section_offset = DA_RUNNING_PARAM_SECTION_SIZE;
    sndp_da_ctx.backup_access_info.section_size = DA_BACKUP_PARAM_SECTION_SIZE;

    sndp_da_ctx.backup_access_info.curr_param_struct_ver = DA_BACKUP_PARAM_STRUCT_VER;
    sndp_da_ctx.backup_access_info.param_size = sizeof(sndp_da_backup_param_s);
    sndp_da_ctx.backup_access_info.param_buf = (uint8_t *)&sndp_da_backup_param;
    
    sndp_da_ctx.backup_access_info.find_field_info = sndp_da_find_backup_field_info;
    sndp_da_ctx.backup_access_info.calc_struct_checksum = sndp_da_calc_backup_struct_checksum;
    sndp_da_ctx.backup_access_info.calc_data_checksum = sndp_da_calc_backup_data_checksum;
    sndp_da_ctx.backup_access_info.update_data_checksum = sndp_da_update_backup_checksum;
    
    ASSERT(sndp_da_ctx.backup_access_info.param_size < DA_BACKUP_PARAM_SECTION_SIZE, 
            "param_size(%d) > BACKUP_PARAM_SECTION_SIZE(%d)", 
            sndp_da_ctx.backup_access_info.param_size, 
            DA_BACKUP_PARAM_SECTION_SIZE);
    
	sndp_da_ctx.section_mod_id = NORFLASH_API_MODULE_ID_SNDP_PARAM;
	sndp_da_ctx.section_start_addr = (uint32_t)__sndp_param_start;
	sndp_da_ctx.section_size = DA_PARAM_SECTION_SIZE;

	SNDP_IF_TRACE(2, "__sndp_param_start: %p length: 0x%x", __sndp_param_start, DA_PARAM_SECTION_SIZE);
	app_flash_register_module((uint8_t)NORFLASH_API_MODULE_ID_SNDP_PARAM,
                              app_flash_get_dev_id_by_addr(sndp_da_ctx.section_start_addr),
                              sndp_da_ctx.section_start_addr,
                              sndp_da_ctx.section_size,
                              (uint32_t)sndp_da_flash_operate_callback);

    sndp_da_ctx.inited = true;
	
	sndp_da_read_param_from_flash(&sndp_da_ctx.running_access_info);
    sndp_da_check_running_data_validity(&sndp_da_ctx.running_access_info);
    
	sndp_da_read_param_from_flash(&sndp_da_ctx.backup_access_info);
    //sndp_da_check_backup_data_validity(&sndp_da_ctx.backup_access_info);
	
}


#endif	/* __SNDP_PROJ__ */

