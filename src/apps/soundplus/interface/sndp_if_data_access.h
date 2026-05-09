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
	SNDP_DA_FIELD_PROXIMITY_CALIB_DATA,
	SNDP_DA_FIELD_PPG_CALIB_DATA,
	SNDP_DA_FIELD_ACC_CALIB_DATA,

#if defined(__SNDP_SLEEP_APP__)   
#if defined(__SNDP_EQ_PARAM_SETTING__)	
	SNDP_DA_FIELD_EQ_DATA,
#endif
	SNDP_DA_FIELD_APP_DATA,
#endif

    SNDP_DA_FIELD_TEST_FLAG,
    SNDP_DA_FIELD_DEV_COLOR,
    
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
	uint8_t sn[20];
} sndp_da_field_sn_s;

typedef struct {
	uint32_t key;  /* This key must be defined, but it cannot be modified */
	uint8_t data[16];
} sndp_da_field_proximity_calib_data_s;


typedef struct {
	uint32_t key;  /* This key must be defined, but it cannot be modified */
	uint8_t data[64];
} sndp_da_field_ppg_calib_data_s;

typedef struct {
	uint32_t key;  /* This key must be defined, but it cannot be modified */
	uint8_t data[64];
} sndp_da_field_acc_calib_data_s;


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
	uint8_t data[16];
}sndp_da_field_sleep_app_data_s;
#endif

typedef struct {
	uint32_t key;  /* This key must be defined, but it cannot be modified */
	uint32_t test_flag;
} sndp_da_field_test_flag_s;

typedef struct {
	uint32_t key;  /* This key must be defined, but it cannot be modified */
	uint32_t dev_color;
} sndp_da_field_dev_color_s;


typedef struct {
	uint32_t offset;
	uint32_t size;
} sndp_da_field_info_s;


typedef struct {
    uint32_t struct_ver;
    uint32_t struct_checksum;
    uint32_t data_checksum;
    uint32_t data_start;

    /** Add field below this line. */
    sndp_da_field_bat_info_s field_bat_info;
#if defined(__SNDP_SLEEP_APP__) 
#if defined(__SNDP_EQ_PARAM_SETTING__)
	sndp_da_field_eq_data_s field_eq_data;
#endif
	sndp_da_field_sleep_app_data_s field_sleep_app_data;
#endif

    /** Add field above this line. */
    uint32_t data_end;

} sndp_da_running_param_s;


typedef struct {
    uint32_t struct_ver;
    uint32_t struct_checksum;
    uint32_t data_checksum;
    uint32_t data_start;

    /** Add field below this line. */
	sndp_da_field_sn_s field_sn;
    sndp_da_field_proximity_calib_data_s field_proximity_calib_data;
	sndp_da_field_ppg_calib_data_s  field_ppg_calib_data;
    sndp_da_field_acc_calib_data_s  field_acc_calib_data;
    sndp_da_field_test_flag_s field_test_flag;
    sndp_da_field_dev_color_s field_dev_color;

    /** Add field above this line. */
    uint32_t data_end;

} sndp_da_backup_param_s;



int32_t sndp_da_write_field(sndp_da_field_id_e field_id, void *field_data, uint16_t field_size, bool save_to_flash);
int32_t sndp_da_read_field(sndp_da_field_id_e field_id, void *field_data, uint16_t field_size, bool read_from_flash);
void sndp_da_flush_param_to_flash(void);

void sndp_da_init(void);


#ifdef __cplusplus
}
#endif

#endif	/* __SNDP_PROJ__ */
#endif /* __SNDP_IF_DATA_ACCESS_H__ */
