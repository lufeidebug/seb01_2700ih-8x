#ifndef __SNDP_IF_DATA_ACCESS_H__
#define __SNDP_IF_DATA_ACCESS_H__

#if defined(__SNDP_PROJ__)

#ifdef __cplusplus
extern "C" {
#endif

#define SNDP_DA_PARAM_FIELD_VALID				(0xFE5AFE5A)

#define SNDP_DA_BT_NAME_LEN                    (48)    //it must be equal to CLASSIC_BTNAME_LEN


typedef enum {
	SNDP_DA_FIELD_BAT_INFO,
	SNDP_DA_FIELD_SN,
	SNDP_DA_FIELD_BT_NAME,
	SNDP_DA_FIELD_TOUCH_CALIB_DATA,
#if defined(__SNDP_ALG_MGR__)	
	SNDP_DA_FIELD_ALG_DATA,
#endif
#if defined(__SNDP_ALG_MGR__)	
	SNDP_DA_FIELD_HR_DATA,
#endif 
#if defined(__SNDP_SLEEP_APP__)   
#if defined(__SNDP_EQ_PARAM_SETTING__)	
	SNDP_DA_FIELD_EQ_DATA,
#endif
	SNDP_DA_FIELD_APP_DATA,
#endif
} sndp_da_field_id_e;



typedef struct {
	uint32_t key;  /* This key must be defined, but it cannot be modified */
	uint8_t *data;
} sndp_da_field_common_s;


typedef struct {
	uint32_t key;  /* This key must be defined, but it cannot be modified */
	uint8_t bat_per;
	uint16_t bat_volt;
} sndp_da_field_bat_info_s;

typedef struct {
	uint32_t key;  /* This key must be defined, but it cannot be modified */
    uint32_t valid;
	uint8_t sn[20];
} sndp_da_field_sn_s;

typedef struct {
	uint32_t key;  /* This key must be defined, but it cannot be modified */
    uint8_t len;
	uint8_t name[SNDP_DA_BT_NAME_LEN + 4];
} sndp_da_field_bt_name_s;


typedef struct {
	uint32_t key;  /* This key must be defined, but it cannot be modified */
	uint8_t data[256];
} sndp_da_field_touch_calib_data_s;

#if defined(__SNDP_ALG_MGR__)
typedef struct {
	uint32_t key;  /* This key must be defined, but it cannot be modified */
	uint8_t data[512];//512
} sndp_da_field_alg_data_s;
#endif


typedef struct {
	uint16_t offset;
	uint16_t size;
} sndp_da_field_info_s;

#if defined(__SNDP_SLEEP_APP__) 
#if defined(__SNDP_EQ_PARAM_SETTING__)
typedef struct {
	uint32_t key;  /* This key must be defined, but it cannot be modified */
	uint32_t data_crc;
	uint8_t data[768];
}sndp_da_field_eq_data_s;
#endif

typedef struct {
	/* data */
	uint32_t key;
	uint32_t data_crc;
	uint8_t sleep_app_flag[16];
}sndp_da_field_sleep_app_data_s;
#endif

typedef struct {
	sndp_da_field_bat_info_s field_bat_info;
	sndp_da_field_sn_s field_sn;
    sndp_da_field_bt_name_s field_bt_name;
	sndp_da_field_touch_calib_data_s  field_touch_calib_data;
#if defined(__SNDP_ALG_MGR__)	
	sndp_da_field_alg_data_s field_alg_data;
#endif
#if defined(__SNDP_SLEEP_APP__) 
#if defined(__SNDP_EQ_PARAM_SETTING__)
	sndp_da_field_eq_data_s field_eq_data;
#endif
	sndp_da_field_sleep_app_data_s field_sleep_app_data;
#endif
} sndp_da_param_s;


typedef struct {
	bool inited;
	
	uint8_t section_mod_id;
	uint32_t section_start_addr;
	uint32_t section_size;

	uint32_t running_param_start_addr;
	uint32_t running_param_size;
	sndp_da_param_s *running_param_cache;

	uint32_t backup_param_start_addr;
	uint32_t backup_param_size;
	sndp_da_param_s *backup_param_cache;
	
} sndp_da_ctx_s;


void sndp_da_flush_running_param_to_flash(void);
int32_t sndp_da_write_field_data_to_running_param(sndp_da_field_id_e field_id, void *field_data, uint16_t field_size, bool save_to_flash);
int32_t sndp_da_read_field_data_from_running_param(sndp_da_field_id_e field_id, void *field_data, uint16_t field_size, bool read_from_flash);

void sndp_da_flush_backup_param_to_flash(void);
int32_t sndp_da_write_field_data_to_backup_param(sndp_da_field_id_e field_id, void *field_data, uint16_t field_size, bool save_to_flash);
int32_t sndp_da_read_field_data_from_backup_param(sndp_da_field_id_e field_id, void *field_data, uint16_t field_size, bool read_from_flash);

void sndp_da_init(void);


#ifdef __cplusplus
}
#endif

#endif	/* __SNDP_PROJ__ */
#endif /* __SNDP_IF_DATA_ACCESS_H__ */
