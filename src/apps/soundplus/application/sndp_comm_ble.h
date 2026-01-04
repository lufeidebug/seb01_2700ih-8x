#ifndef __SNDP_COMM_BLE_H__
#define __SNDP_COMM_BLE_H__

#if defined(__SNDP_COMM_BLE__)



#ifdef __cplusplus
extern "C" {
#endif

#define __SNDP_COMM_BLE_TRACE__

#if defined(__SNDP_COMM_BLE_TRACE__)
#define COMM_BLE_ENTER()                    SNDP_TRACE(2, 		"[COMM_BLE] %s, line=%d, enter", __func__, __LINE__)
#define COMM_BLE_TRACE(num, str, ...)       SNDP_TRACE(1 + num,	"[COMM_BLE] %s, " str, __func__, ##__VA_ARGS__)
#define COMM_BLE_EXIT()                     SNDP_TRACE(2, 		"[COMM_BLE] %s, line=%d, exit", __func__, __LINE__)

#else
#define COMM_BLE_ENTER() 
#define COMM_BLE_TRACE(num, str, ...)
#define COMM_BLE_EXIT()
#endif

typedef enum {
	SNDP_COMM_BLE_DISCONNECTED,
	SNDP_COMM_BLE_CONNECTING,
	SNDP_COMM_BLE_CONNECTED,
	SNDP_COMM_BLE_DISCONNECTING,
		
} sndp_comm_ble_conn_status_e;


sndp_comm_ble_conn_status_e sndp_comm_ble_get_conn_status(void);
bool sndp_comm_ble_is_connected(void);
int32_t sndp_comm_ble_send_data(uint8_t *data, uint16_t data_len);
int32_t sndp_comm_ble_init(void);



#ifdef __cplusplus
}
#endif

#endif /* __SNDP_COMM_BLE__ */
#endif /* __SNDP_COMM_BLE_H__ */

