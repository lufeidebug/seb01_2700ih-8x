#ifndef __SNDP_COMM_BLE_H__
#define __SNDP_COMM_BLE_H__

#if defined(__SNDP_COMM_BLE__)



#ifdef __cplusplus
extern "C" {
#endif

//#define __SNDP_COMM_BLE_TRACE__

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

/* App连接类型: 区分BLE直连 / GATT-over-BR/EDR双发 / 未连接 */
typedef enum {
    SNDP_COMM_CONN_TYPE_NONE = 0,          /* 未连接 */
    SNDP_COMM_CONN_TYPE_BLE,              /* BLE直连 */
    SNDP_COMM_CONN_TYPE_GATT_OVER_BREDR,  /* GATT over BR/EDR双发 */
} sndp_comm_conn_type_e;

#if defined(__SNDP_COMM_BLE_ADV_SET__)
void sndp_ble_receive_master_public_addr(uint8_t *addr, uint16_t length);
void sndp_master_send_ble_public_addr(void);
#endif

sndp_comm_ble_conn_status_e sndp_comm_ble_get_conn_status(void);
bool sndp_comm_ble_is_connected(void);
sndp_comm_conn_type_e sndp_comm_ble_get_conn_type(void);
int32_t sndp_comm_ble_send_data(uint8_t *data, uint16_t data_len);
int32_t sndp_comm_ble_init(void);
uint32_t sndp_comm_ble_available_of_queue(void);


#ifdef __cplusplus
}
#endif

#endif /* __SNDP_COMM_BLE__ */
#endif /* __SNDP_COMM_BLE_H__ */

