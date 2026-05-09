
#ifndef __SNDP_SSH401A_ADAPTER_H__
#define __SNDP_SSH401A_ADAPTER_H__
#if defined(__SNDP_HRSENSOR_SSH401A__)

#ifdef __cplusplus
extern "C" {
#endif


#define __SSH401A_TRACE__

#ifdef __SSH401A_TRACE__
#define SSH401A_TRACE(num, str, ...)    SNDP_TRACE(1 + num, "[SSH401A] %s, " str, __func__, ##__VA_ARGS__)
#else
#define SSH401A_TRACE(num, str, ...)
#endif

typedef struct {
	uint16_t high_threshold;
    uint16_t low_threshold;
} ssh401a_proximity_calib_data_s;


typedef struct {
	uint8_t data[64];
} ssh401a_ppg_calib_data_s;


int32_t ssh401a_proximity_read_calib_data(ssh401a_proximity_calib_data_s *proximity_calib_data);
int32_t ssh401a_proximity_write_calib_data(ssh401a_proximity_calib_data_s *proximity_calib_data);
int32_t ssh401a_ppg_read_calib_data(ssh401a_ppg_calib_data_s *ppg_calib_data);
int32_t ssh401a_ppg_write_calib_data(ssh401a_ppg_calib_data_s *ppg_calib_data);



#ifdef __cplusplus
}
#endif

#endif  //__SNDP_HRSENSOR_SSH401A__
#endif  //__SNDP_SSH401A_ADAPTER_H__


