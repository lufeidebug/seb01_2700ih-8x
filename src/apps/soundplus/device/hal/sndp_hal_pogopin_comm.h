
#ifndef __SNDP_HAL_POGOPIN_COMM_H__
#define __SNDP_HAL_POGOPIN_COMM_H__

#if defined(__SNDP_COMM_POGOPIN__)


#ifdef __cplusplus
extern "C" {
#endif



typedef enum {
	SNDP_HAL_POGOPIN_MODE_CHARGING,
	SNDP_HAL_POGOPIN_MODE_COMM_RX,
	SNDP_HAL_POGOPIN_MODE_COMM_TX,
    SNDP_HAL_POGOPIN_MODE_UNKNOWN,
} sndp_hal_pogopin_mode_e;


typedef void (*sndp_hal_pogopin_comm_data_recv_func)(uint8_t *data, uint32_t data_len);

typedef struct {
    /**
     * return: 0 initialization successed 
     */
    int32_t (* init)(void);

	/** 
     * status: sndp_hal_pogopin_mode_e
     * return: 0 no error.
     */
    int32_t (* set_mode)(sndp_hal_pogopin_mode_e mode);

	/** 
     * status: sndp_hal_pogopin_mode_e
     * return: 0 no error.
     */
    int32_t (* get_curr_mode)(sndp_hal_pogopin_mode_e *mode);

    /** 
     * callback: This pointer will be called when pogopin received data.
     * return: 0 no error.
     */
    int32_t (* set_data_recv_callback)(sndp_hal_pogopin_comm_data_recv_func callback);

	/** 
     * data:
     * data_len: 
     * return: 0 no error.
     */
    int32_t (* send_data)(uint8_t *data, uint32_t data_len);
	
} sndp_hal_pogopin_comm_s;

uint32_t sndp_hal_pogopin_comm_init(void);
uint32_t sndp_hal_pogopin_comm_set_mode(sndp_hal_pogopin_mode_e mode);
uint32_t sndp_hal_pogopin_comm_get_curr_mode(sndp_hal_pogopin_mode_e *mode);
uint32_t sndp_hal_pogopin_comm_set_data_recv_callback(sndp_hal_pogopin_comm_data_recv_func callback);
uint32_t sndp_hal_pogopin_comm_send_data(uint8_t *data, uint32_t data_len);



#ifdef __cplusplus
}
#endif


#endif  //__SNDP_COMM_POGOPIN__
#endif  //__SNDP_HAL_POGOPIN_COMM_H__





