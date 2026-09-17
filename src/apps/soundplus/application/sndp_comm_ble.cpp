#if defined(__SNDP_COMM_BLE__)
#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_timer.h"
#include "hal_trace.h"
#include "hal_sleep.h"
#include "cqueue.h"
#include "bta_ble_api.h"
#include "bts_ble_api.h"
#include "factory_section.h"
#include "nvrecord_extension.h"
#include "nvrecord_env.h"
#if defined(__SNDP_CTKD_OVER_LE__)
#include "nvrecord_bt.h"
#endif
#include "ble_core_common.h"
#include "app_custom.h"
#include "gap_service.h"
#include "bes_gap_api.h"
#include "app_ibrt_customif_cmd.h"
#include "bts_core_if.h"
#include "app_ble_adv.h"
#if defined(__GATT_OVER_BR_EDR__)
#include "app_btgatt.h"
#endif

#include "sndp_if_device.h"
#include "sndp_if_common.h"
#include "sndp_if_platform.h"

#include "sndp_comm_main.h"
#include "sndp_comm_ble.h"
#include "sndp_heart_rate.h"
#ifndef CFG_APP_DATAPATH_SERVER
#define CFG_APP_DATAPATH_SERVER
#endif
#include "app_datapaths.h"

#if defined(__SNDP_CTKD_OVER_LE__)
#ifdef __cplusplus
extern "C" {
#endif
/* Avoid including app_ble.h here: it pulls ble_dp_common.h and conflicts with app_datapaths.h. */
void app_ble_smp_get_specifc_irk_ia_callback_register(void (*ble_get_specific_irk_ia)(uint16_t, uint8_t **,
                                                                                      bt_bdaddr_t **));
void app_ble_gap_add_record_modify_callback_register(void (*ble_add_record_modify)(uint16_t, BleDevicePairingInfo *));
void bta_tws_register_ui_secure_request_callback(bool (*cb)(void *bdaddr, uint8_t local_initiate, bool is_bt_acl, bool conn_req));
#ifdef IBRT
bool bts_tws_if_is_tws_addr(const uint8_t *pBdAddr);
#endif
#ifdef __cplusplus
}
#endif
#endif /* __SNDP_CTKD_OVER_LE__ */


/**************************************************************************************************
* Constant
**************************************************************************************************/
#define SNDP_COMM_BLE_SEND_BUF_SIZE			(256)

/* 功耗优化: BLE广播间隔(ms). 待机省电调大(默认160ms); 若手机搜不到再调小*/
#define SNDP_COMM_BLE_ADV_INTERVAL_MS          (160)

/**************************************************************************************************
* Prototype
**************************************************************************************************/
typedef struct {
    bool inited;

    uint8_t conidx;
    sndp_comm_ble_conn_status_e conn_status;
    bool sending;
    uint16_t mtu;

} sndp_comm_ble_context_s;


/**************************************************************************************************
* Extern
**************************************************************************************************/
static void sndp_comm_ble_send_timeout_timer_handler(void const *param);
static void sndp_comm_ble_send_data_handle(void);


/**************************************************************************************************
* Variable
**************************************************************************************************/
static sndp_comm_ble_context_s sndp_comm_ble_ctx;

static CQueue sndp_comm_ble_send_queue;	
static osMutexId sndp_comm_ble_send_queue_mutex_id = NULL;
osMutexDef(sndp_comm_ble_send_queue_mutex);

static uint8_t sndp_comm_ble_send_queue_buf[SNDP_COMM_BLE_SEND_BUF_SIZE*3];
static uint8_t sndp_comm_ble_send_pop_buf[SNDP_COMM_BLE_SEND_BUF_SIZE+2];

osTimerDef(BLE_SEND_TIMEOUT_TIMER, sndp_comm_ble_send_timeout_timer_handler);
static osTimerId ble_send_timeout_timer = NULL;

#if defined(__SNDP_CTKD_OVER_LE__)
/* CTKD: Host also supports simultaneous LE+BR/EDR. gap_dt_add_flags() only sets Controller bit 0x08. */
#ifndef GAP_FLAGS_SIMU_LE_BREDR_HOST
#define GAP_FLAGS_SIMU_LE_BREDR_HOST            (0x10)
#endif
#endif /* __SNDP_CTKD_OVER_LE__ */

#if defined(__SNDP_COMM_BLE_ADV_SET__)
typedef struct {
    uint8_t public_addr[6];  // 广播地址
    bool    synced;          // 是否已同步
} sndp_ble_addr_ctx_t;
static sndp_ble_addr_ctx_t g_ble_addr_ctx = {0};
#endif

#if defined(__SNDP_CTKD_OVER_LE__)
static uint8_t s_sndp_ctkd_local_irk[16];
static bt_bdaddr_t s_sndp_ctkd_local_ia;

/* 排除未组队时 NV 里残留的全 0 / 全 0xFF */
static bool sndp_ctkd_bt_addr_is_valid(const uint8_t *addr)
{
    uint8_t all_zero = 1;
    uint8_t all_ff = 1;
    uint8_t i;

    if (addr == NULL) {
        return false;
    }

    for (i = 0; i < 6; i++) {
        if (addr[i] != 0x00) {
            all_zero = 0;
        }
        if (addr[i] != 0xFF) {
            all_ff = 0;
        }
    }

    return ((all_zero == 0) && (all_ff == 0));
}

/* bt_tws_types.h: BT_IBRT_MASTER=0, BT_IBRT_SLAVE=1, BT_IBRT_UNKNOWN=0xff */
#ifndef BT_IBRT_MASTER
#define BT_IBRT_MASTER 0
#endif
#ifndef BT_IBRT_SLAVE
#define BT_IBRT_SLAVE 1
#endif

/* 已组队判定，必须同时满足两个条件：
 * 1) NV mode 是 MASTER/SLAVE。只判 mode != BT_IBRT_UNKNOWN 不够，
 *    未组队时 NV 里 mode 也可能是 0(MASTER) 的残留值。
 * 2) 配对地址在经典 DDB 里有记录佐证 —— 组队成功时 TWS link key 会以配对地址
 *    为 key 落库，两耳都有；查不到记录说明该地址只是无效残留
 *    (实测两只耳机都广播过 00:11:02:86:00:19，谁的都不是)。 */
static bool sndp_ctkd_is_paired(uint8_t *pair_addr)
{
    struct nvrecord_env_t *nvrecord_env = NULL;
    btif_device_record_t rec;

    if (!sndp_ctkd_bt_addr_is_valid(pair_addr)) {
        return false;
    }

    if (nv_record_env_get(&nvrecord_env) != 0 || nvrecord_env == NULL) {
        return false;
    }

    if ((nvrecord_env->ibrt_mode.mode != BT_IBRT_MASTER) &&
        (nvrecord_env->ibrt_mode.mode != BT_IBRT_SLAVE)) {
        return false;
    }

    memset(&rec, 0, sizeof(rec));
    return (nv_record_ddbrec_find((const bt_bdaddr_t *)pair_addr, &rec) == BT_STS_SUCCESS);
}

/* 本机对外的 BT 地址，CTKD 链路三处必须同源（否则手机按 BLE 连接用到的地址
 * 去派生经典 LinkKey 会拿到错的 key）：
 *   sndp_ble_advertising_init()      -> BLE public address
 *   sndp_comm_ble_activity_prepare() -> 广播内 BT MAC
 *   sndp_ctkd_get_specific_irk_ia()  -> SMP Identity Address
 * - 未组队：本机出厂 BT 地址 sndp_dev_get_bt_addr(false)
 * - 已组队：本机组唯一地址 sndp_get_pair_addr()，即【右耳 BT 地址】
 *   sndp_tws_pairing_config() 中右耳(master)把 peer 写成自身地址、左耳(slave)写入
 *   右耳地址，故两耳 NV 内 ibrt_mode.record.bdAddr 相同，读出来都是右耳 BT 地址。 */
static uint8_t *sndp_ctkd_get_local_bt_addr(void)
{
    uint8_t *pair_addr = sndp_get_pair_addr();
    uint8_t *factory_addr = sndp_dev_get_bt_addr(false);

    if (sndp_ctkd_is_paired(pair_addr)) {
        return pair_addr;
    }

    /* 未组队：用出厂地址快照。比 bt_get_local_address()(bt_global_addr) 稳定，
     * 后者运行期会被"统一地址"机制改写(实测被写过 0B:00:00:00:4A:5D)。
     * sndp_dev_get_bt_addr(false) 由 sndp_dev_init_device_info() 填充，
     * 在 apps.cpp 的 sndp_comm_main_init(SNDP_COMM_INIT_ALL) 之前完成，
     * 而 BLE 初始化在该 INIT_ALL 里，故此处的值一定有效；仅做兜底校验。 */
    if (sndp_ctkd_bt_addr_is_valid(factory_addr)) {
        return factory_addr;
    }

    return (uint8_t *)bt_get_local_address();
}

/* Phone pairing req was SC+CT2+LinkKey; local rsp was Bonding+CT2 only. Force SC+LinkKey so CTKD can derive Classic LK. */
static void sndp_ctkd_smp_require_modify(uint16_t connhdl, bt_pairing_reqiure_t *p_req)
{
    if (p_req == NULL) {
        return;
    }

    p_req->auth_req = (uint8_t)(SMP_AUTH_BONDING | SMP_AUTH_SC_SUPPORT | SMP_AUTH_CT2_SUPPORT);
    p_req->init_key_dist = (uint8_t)(p_req->init_key_dist | GAP_KDIST_ENCKEY | GAP_KDIST_IDKEY | GAP_KDIST_LINKKEY);
    p_req->resp_key_dist = (uint8_t)(p_req->resp_key_dist | GAP_KDIST_ENCKEY | GAP_KDIST_IDKEY | GAP_KDIST_LINKKEY);

    COMM_BLE_TRACE(0, "CTKD smp auth=0x%02x ik=0x%02x rk=0x%02x connhdl=0x%04x",
                   p_req->auth_req, p_req->init_key_dist, p_req->resp_key_dist, connhdl);
}

/* SMP Identity Address 必须等于广告出去的本机地址（未组队=本机 BT，已组队=右耳 BT）。
 * 之前直接用 bt_get_local_address()，从耳(本机 BT != 右耳 BT)会与 BLE 广播地址不一致。 */
static void sndp_ctkd_get_specific_irk_ia(uint16_t connhdl, uint8_t **irk, bt_bdaddr_t **ia)
{
    uint8_t *local_bt_addr = sndp_ctkd_get_local_bt_addr();

    bt_ble_gap_irk_t local_irk = {{0}};
    bts_ble_gap_get_local_irk(&local_irk);
    memcpy(s_sndp_ctkd_local_irk, local_irk.data, sizeof(s_sndp_ctkd_local_irk));
    if (local_bt_addr) {
        memcpy(s_sndp_ctkd_local_ia.address, local_bt_addr, 6);
    } else {
        memset(s_sndp_ctkd_local_ia.address, 0, 6);
    }

    if (irk) {
        *irk = s_sndp_ctkd_local_irk;
    }
    if (ia) {
        *ia = &s_sndp_ctkd_local_ia;
    }

    COMM_BLE_TRACE(0, "CTKD identity %02X:%02X:%02X:%02X:%02X:%02X connhdl=0x%04x",
                   s_sndp_ctkd_local_ia.address[0], s_sndp_ctkd_local_ia.address[1],
                   s_sndp_ctkd_local_ia.address[2], s_sndp_ctkd_local_ia.address[3],
                   s_sndp_ctkd_local_ia.address[4], s_sndp_ctkd_local_ia.address[5], connhdl);
}

/* Phone pages Classic with identity, but CTKD first stores the BLE RPA. Keep both. */
static bool s_sndp_ctkd_peer_valid;
static bt_bdaddr_t s_sndp_ctkd_peer_rpa;
static bt_bdaddr_t s_sndp_ctkd_peer_ia;

static void sndp_ctkd_cache_peer(const uint8_t *rpa, const uint8_t *ia)
{
    if ((rpa == NULL) || (ia == NULL)) {
        return;
    }
    memcpy(s_sndp_ctkd_peer_rpa.address, rpa, 6);
    memcpy(s_sndp_ctkd_peer_ia.address, ia, 6);
    s_sndp_ctkd_peer_valid = true;
}

static bool sndp_ctkd_is_tws_addr(const uint8_t *addr)
{
#ifdef IBRT
    return (addr != NULL) && bts_tws_if_is_tws_addr(addr);
#else
    (void)addr;
    return false;
#endif
}

static bool sndp_ctkd_lookup_solved_ia(const uint8_t *addr, ble_bdaddr_t *solved)
{
    ble_bdaddr_t look = {{0}};
    uint8_t conidx;
    uint8_t i;
    uint8_t conn_cnt = 0;
    uint8_t found_conidx = BTS_BLE_INVALID_CONIDX;

    if ((addr == NULL) || (solved == NULL)) {
        return false;
    }

    memcpy(look.addr, addr, 6);
    look.addr_type = BT_ADDR_TYPE_RANDOM;
    conidx = bts_ble_get_conidx_by_addr(&look);
    if (conidx == BTS_BLE_INVALID_CONIDX) {
        look.addr_type = BT_ADDR_TYPE_PUBLIC;
        conidx = bts_ble_get_conidx_by_addr(&look);
    }
    if ((conidx != BTS_BLE_INVALID_CONIDX) && bts_ble_gap_get_peer_solved_addr(conidx, solved)) {
        return true;
    }

    for (i = 0; i < 8; i++) {
        if (bts_ble_gap_is_connection_on(i)) {
            conn_cnt++;
            found_conidx = i;
        }
    }
    if ((conn_cnt == 1) && bts_ble_gap_get_peer_solved_addr(found_conidx, solved)) {
        return true;
    }

    return false;
}

static void sndp_ctkd_add_record_modify(uint16_t connhdl, BleDevicePairingInfo *p_record_add)
{
    ble_bdaddr_t solved = {{0}};
    uint8_t conidx;

    if (p_record_add == NULL) {
        return;
    }

    conidx = bts_ble_gap_get_conidx_by_conhdl(connhdl);
    if (conidx == BTS_BLE_INVALID_CONIDX) {
        conidx = sndp_comm_ble_ctx.conidx;
    }

    if (!bts_ble_gap_get_peer_solved_addr(conidx, &solved)) {
        if (!sndp_ctkd_lookup_solved_ia(p_record_add->peer_addr.addr, &solved)) {
            COMM_BLE_TRACE(0, "CTKD BLE record keep %02X:%02X:%02X:%02X:%02X:%02X type=%d",
                           p_record_add->peer_addr.addr[0], p_record_add->peer_addr.addr[1],
                           p_record_add->peer_addr.addr[2], p_record_add->peer_addr.addr[3],
                           p_record_add->peer_addr.addr[4], p_record_add->peer_addr.addr[5],
                           p_record_add->peer_addr.addr_type);
            return;
        }
    }

    sndp_ctkd_cache_peer(p_record_add->peer_addr.addr, solved.addr);
    if (memcmp(p_record_add->peer_addr.addr, solved.addr, 6) == 0) {
        return;
    }

    COMM_BLE_TRACE(0, "CTKD BLE addr rewrite %02X:%02X:%02X:%02X:%02X:%02X -> %02X:%02X:%02X:%02X:%02X:%02X type=%d",
                   p_record_add->peer_addr.addr[0], p_record_add->peer_addr.addr[1],
                   p_record_add->peer_addr.addr[2], p_record_add->peer_addr.addr[3],
                   p_record_add->peer_addr.addr[4], p_record_add->peer_addr.addr[5],
                   solved.addr[0], solved.addr[1], solved.addr[2],
                   solved.addr[3], solved.addr[4], solved.addr[5], solved.addr_type);
    memcpy(p_record_add->peer_addr.addr, solved.addr, 6);
    p_record_add->peer_addr.addr_type = solved.addr_type;
}

extern "C" bool sndp_ctkd_remap_classic_bdaddr(bt_bdaddr_t *bdAddr)
{
    ble_bdaddr_t solved = {{0}};

    if (bdAddr == NULL) {
        return false;
    }
    if (sndp_ctkd_is_tws_addr(bdAddr->address)) {
        return false;
    }

    if (s_sndp_ctkd_peer_valid &&
        (memcmp(bdAddr->address, s_sndp_ctkd_peer_rpa.address, 6) == 0) &&
        (memcmp(bdAddr->address, s_sndp_ctkd_peer_ia.address, 6) != 0)) {
        COMM_BLE_TRACE(0, "CTKD classic addr remapped cache %02X:%02X:%02X:%02X:%02X:%02X -> %02X:%02X:%02X:%02X:%02X:%02X",
                       bdAddr->address[0], bdAddr->address[1], bdAddr->address[2],
                       bdAddr->address[3], bdAddr->address[4], bdAddr->address[5],
                       s_sndp_ctkd_peer_ia.address[0], s_sndp_ctkd_peer_ia.address[1],
                       s_sndp_ctkd_peer_ia.address[2], s_sndp_ctkd_peer_ia.address[3],
                       s_sndp_ctkd_peer_ia.address[4], s_sndp_ctkd_peer_ia.address[5]);
        memcpy(bdAddr->address, s_sndp_ctkd_peer_ia.address, 6);
        return true;
    }

    if (!sndp_ctkd_lookup_solved_ia(bdAddr->address, &solved)) {
        return false;
    }
    if (memcmp(solved.addr, bdAddr->address, 6) == 0) {
        sndp_ctkd_cache_peer(bdAddr->address, solved.addr);
        return false;
    }

    sndp_ctkd_cache_peer(bdAddr->address, solved.addr);
    COMM_BLE_TRACE(0, "CTKD classic addr remapped %02X:%02X:%02X:%02X:%02X:%02X -> %02X:%02X:%02X:%02X:%02X:%02X",
                   bdAddr->address[0], bdAddr->address[1], bdAddr->address[2],
                   bdAddr->address[3], bdAddr->address[4], bdAddr->address[5],
                   solved.addr[0], solved.addr[1], solved.addr[2],
                   solved.addr[3], solved.addr[4], solved.addr[5]);
    memcpy(bdAddr->address, solved.addr, 6);
    return true;
}

static bool sndp_ctkd_ui_secure_request(void *bdaddr, uint8_t local_initiate, bool is_bt_acl, bool conn_req)
{
    bt_bdaddr_t *addr = (bt_bdaddr_t *)bdaddr;
    btif_device_record_t rec;

    if (addr == NULL) {
        return false;
    }

    COMM_BLE_TRACE(0, "CTKD ui_secure local=%d acl=%d req=%d %02X:%02X:%02X:%02X:%02X:%02X",
                   local_initiate, is_bt_acl, conn_req,
                   addr->address[0], addr->address[1], addr->address[2],
                   addr->address[3], addr->address[4], addr->address[5]);

    if (sndp_ctkd_is_tws_addr(addr->address) || !is_bt_acl) {
        return false;
    }

    if (s_sndp_ctkd_peer_valid &&
        ((memcmp(addr->address, s_sndp_ctkd_peer_ia.address, 6) == 0) ||
         (memcmp(addr->address, s_sndp_ctkd_peer_rpa.address, 6) == 0))) {
        COMM_BLE_TRACE(0, "CTKD allow classic inbound cached peer");
        return true;
    }

    memset(&rec, 0, sizeof(rec));
    if (nv_record_ddbrec_find(addr, &rec) == BT_STS_SUCCESS) {
        COMM_BLE_TRACE(0, "CTKD allow classic inbound nv record");
        return true;
    }

    return false;
}
#endif /* __SNDP_CTKD_OVER_LE__ */

/**************************************************************************************************
* Function
**************************************************************************************************/

int32_t sndp_comm_ble_send_queue_push_data(const uint8_t *data, uint32_t data_len)
{
    int32_t ret = 0;
    
    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
    ret = EnCQueue(&sndp_comm_ble_send_queue, (CQItemType *)data, data_len);
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
    return ret;
}

int32_t sndp_comm_ble_send_queue_push_data_push_len(const uint8_t *data, uint16_t data_len)
{
    int32_t ret = 0;
    uint8_t per_frame_len[2] = {0};
    per_frame_len[0] = data_len & 0xFF;
    per_frame_len[1] = (data_len >> 8) & 0xFF;
    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
    ret = EnCQueue(&sndp_comm_ble_send_queue, (CQItemType *)&per_frame_len, sizeof(uint16_t));
    if(ret != 0) {
        osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
        return ret;
    }
    ret = EnCQueue(&sndp_comm_ble_send_queue, (CQItemType *)data, data_len);
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
    return ret;
}

POSSIBLY_UNUSED static int32_t sndp_comm_ble_send_queue_pop_data(uint8_t *buf, uint32_t len)
{
    int32_t ret = 0;
    
    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
    ret = DeCQueue(&sndp_comm_ble_send_queue, (CQItemType *)buf, len);
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id); 
    return ret;
}

POSSIBLY_UNUSED static int32_t sndp_comm_ble_send_queue_pop_data_pop_len(uint8_t *buf, uint16_t* len)
{
    int32_t ret = 0;
    uint8_t per_frame_len[2] = {0};
    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
    ret = DeCQueue(&sndp_comm_ble_send_queue, (CQItemType *)per_frame_len, sizeof(uint16_t));
    if(ret != 0) {
        osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
        return ret;
    }
    uint16_t data_len = per_frame_len[0] | (per_frame_len[1] << 8);
    if(data_len > SNDP_COMM_BLE_SEND_BUF_SIZE) {
        // 数据长度超过buf长度，丢弃这条数据
        DeCQueue(&sndp_comm_ble_send_queue, NULL, data_len);
        osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
        return -2;
    }
    *len = data_len;
    ret = DeCQueue(&sndp_comm_ble_send_queue, (CQItemType *)buf, data_len);
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id); 
    return ret;
}

uint32_t sndp_comm_ble_available_of_queue(void)
{
    int32_t available_len = 0;
    
    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
    available_len = AvailableOfCQueue(&sndp_comm_ble_send_queue);
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id); 
    return available_len;
}

int32_t sndp_comm_ble_send_queue_get_len(void)
{
    int32_t len = 0;
    
    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
    len = LengthOfCQueue(&sndp_comm_ble_send_queue);
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
    return len;
}


static void sndp_comm_ble_send_timeout_timer_handler(void const *param)
{
	sndp_comm_ble_send_data_handle();
}

static void sndp_comm_ble_send_data_handle(void)
{
    uint16_t send_len = 0;
    int32_t ret;
    bool has_data = false;

    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
    if (LengthOfCQueue(&sndp_comm_ble_send_queue) == 0) {
        sndp_comm_ble_ctx.sending = false;
        osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
        return;
    }

    ret = DeCQueue(&sndp_comm_ble_send_queue, (CQItemType *)sndp_comm_ble_send_pop_buf, sizeof(uint16_t));
    if (ret != 0) {
        COMM_BLE_TRACE(0, "pop len from queue failed, ret=%d", ret);
        sndp_comm_ble_ctx.sending = false;
        osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
        return;
    }

    send_len = sndp_comm_ble_send_pop_buf[0] | (sndp_comm_ble_send_pop_buf[1] << 8);
    if (send_len > SNDP_COMM_BLE_SEND_BUF_SIZE) {
        // 数据长度超过buf长度，丢弃这条数据
        DeCQueue(&sndp_comm_ble_send_queue, NULL, send_len);
        COMM_BLE_TRACE(0, "invalid frame len=%d, discard", send_len);
        sndp_comm_ble_ctx.sending = false;
        osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
        return;
    }

    ret = DeCQueue(&sndp_comm_ble_send_queue, (CQItemType *)sndp_comm_ble_send_pop_buf, send_len);
    if (ret != 0) {
        COMM_BLE_TRACE(0, "pop data from queue failed, ret=%d", ret);
        sndp_comm_ble_ctx.sending = false;
        osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
        return;
    }

    sndp_comm_ble_ctx.sending = true;
    has_data = true;
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id);

    if (has_data) {
        osTimerStart(ble_send_timeout_timer, 100);
#ifdef CFG_APP_DATAPATH_SERVER   
        app_datapath_server_send_data_via_notification(
                bta_ble_get_conhdl_by_conidx(sndp_comm_ble_ctx.conidx), 
                sndp_comm_ble_send_pop_buf, 
                send_len);
#endif    
    }
}

int32_t sndp_comm_ble_send_data(uint8_t *data, uint16_t data_len)
{
    if(data == NULL) {
        COMM_BLE_TRACE(0, "data == null, return");
        return -1;
    }
    
    if(!sndp_comm_ble_ctx.inited) {
        COMM_BLE_TRACE(0, "not inited, return");
        return -2;
    }
    
    if(!sndp_comm_ble_is_connected()) {
        COMM_BLE_TRACE(0, "not connected, return");
        return -3;
    }

    int32_t ret = 0;
    bool need_start = false;
    uint8_t per_frame_len[2];

    per_frame_len[0] = data_len & 0xFF;
    per_frame_len[1] = (data_len >> 8) & 0xFF;

    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
    if (AvailableOfCQueue(&sndp_comm_ble_send_queue) < (int32_t)(sizeof(uint16_t) + data_len)) {
        COMM_BLE_TRACE(0, "no enough space in queue, return");
        ret = -4;
    } else {
        ret = EnCQueue(&sndp_comm_ble_send_queue, (CQItemType *)per_frame_len, sizeof(uint16_t));
        if (ret != 0) {
            COMM_BLE_TRACE(0, "push length to queue failed, return");
            ret = -6;
        } else {
            ret = EnCQueue(&sndp_comm_ble_send_queue, (CQItemType *)data, data_len);
            if (ret != 0) {
                COMM_BLE_TRACE(0, "push data to queue failed, return");
                ret = -6;
            } else if (!sndp_comm_ble_ctx.sending) {
                need_start = true;
                sndp_comm_ble_ctx.sending = true;
                ret = 0;
            }
        }
    }
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id);

    if (need_start) {
        sndp_call_func_in_app_thread((uint32_t)sndp_comm_ble_send_data_handle, 0, 0, 0);
    }
    return ret;
}

POSSIBLY_UNUSED static void sndp_comm_ble_tx_done(void)
{
	COMM_BLE_ENTER();
    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
	sndp_comm_ble_ctx.sending = false;
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
	// osTimerStop(ble_send_timeout_timer);
	// sndp_comm_ble_send_data_handle();
}

POSSIBLY_UNUSED static void sndp_comm_ble_recv_data_callback(uint8_t *p_buff, uint16_t bufLength)
{
	COMM_BLE_TRACE(1, "bufLength=%d", bufLength);
    
    sndp_comm_main_recv_queue_push_data(SNDP_COMM_PATH_BLE, p_buff, bufLength);
}

POSSIBLY_UNUSED static void sndp_comm_ble_disconnected_done(uint8_t conidx)
{
	COMM_BLE_ENTER();
	sndp_comm_ble_ctx.conn_status = SNDP_COMM_BLE_DISCONNECTED;
    /* 上报BLE断开事件给UI (sndp_ui) */
    sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_BLE_DISCONNECTED, conidx);
#if defined(__SNDP_HEART_RATE_MGR__)
    sndp_hr_ble_disconnected_delay10s_start();
#endif
}

POSSIBLY_UNUSED static void sndp_comm_ble_connected_done(uint8_t conidx)
{
	COMM_BLE_TRACE(0,"conidx:%d",conidx);
	sndp_comm_ble_ctx.conn_status = SNDP_COMM_BLE_CONNECTED;
    sndp_comm_ble_ctx.conidx = conidx;
    /* 上报BLE连接事件给UI (sndp_ui). 注: LE栈连接建立即触发(BT_GATTS_EVENT_CONN_OPENED);
     * GATT-over-BR/EDR走app_btgatt独立路径, 不会进入本回调 */
    sndp_bt_conn_status_changed(SNDP_BT_CONN_STATUS_BLE_CONNECTED, conidx);
    if(sndp_comm_ble_ctx.conidx == 0x01) {
#if defined(__SNDP_HEART_RATE_MGR__)
        sndp_hr_ble_connected_delay10s_stop();
#endif
#ifdef CFG_APP_DATAPATH_SERVER
        app_datapath_server_register_tx_done(sndp_comm_ble_tx_done);
#endif
    }
}

POSSIBLY_UNUSED static void sndp_comm_ble_mtuexchanged_done(uint8_t conidx, uint16_t mtu)
{
	COMM_BLE_TRACE(1, "mute=%d", mtu);
    if(mtu > SNDP_COMM_BLE_SEND_BUF_SIZE){
        sndp_comm_ble_ctx.mtu = SNDP_COMM_BLE_SEND_BUF_SIZE;
    } else {
        sndp_comm_ble_ctx.mtu = mtu;
    }
	
    sndp_comm_ble_ctx.conidx = conidx;
    sndp_comm_ble_ctx.conn_status = SNDP_COMM_BLE_CONNECTED;
#ifdef CFG_APP_DATAPATH_SERVER    
    app_datapath_server_register_tx_done(sndp_comm_ble_tx_done);
#endif    
}

sndp_comm_ble_conn_status_e sndp_comm_ble_get_conn_status(void)
{
	return sndp_comm_ble_ctx.conn_status;
}

bool sndp_comm_ble_is_connected(void)
{
    if(bts_ble_gap_is_connection_on(bta_ble_get_conhdl_by_conidx(sndp_comm_ble_ctx.conidx))) {
        return true;
    }
	return (sndp_comm_ble_ctx.conn_status == SNDP_COMM_BLE_CONNECTED) ? (true) : (false);
}

sndp_comm_conn_type_e sndp_comm_ble_get_conn_type(void)
{
    COMM_BLE_TRACE(0, "btgatt_is_connected:%d", app_btgatt_is_connected_by_conidx(sndp_comm_ble_ctx.conidx));
#if defined(__GATT_OVER_BR_EDR__)
    /* 参考ota_ble_get_path_type: 用当前conidx查GATT-over-BREDR连接状态.
     * GATT-over-BREDR与BLE共享conidx空间, 同一conidx在两条路径上都可见 */
    if (app_btgatt_is_connected_by_conidx(sndp_comm_ble_ctx.conidx)) {
        return SNDP_COMM_CONN_TYPE_GATT_OVER_BREDR;
    }
#endif
    if (sndp_comm_ble_is_connected()) {
        return SNDP_COMM_CONN_TYPE_BLE;
    }
    return SNDP_COMM_CONN_TYPE_NONE;
}

#if defined(__SNDP_COMM_BLE_ADV_SET__)

/* 获取地址 */
sndp_ble_addr_ctx_t* sndp_ble_addr_get_ctx(void)
{
    return &g_ble_addr_ctx;
}

bool sndp_comm_ble_activity_prepare(ble_adv_activity_t *adv);

/* 初始化 */
void sndp_ble_advertising_init(void)
{   
    COMM_BLE_TRACE(0, ".");
#if defined(__SNDP_CTKD_OVER_LE__)
    /* CTKD: BLE public address 必须与广播里上报的 BT 地址一致
     * （未组队=本机 BT 地址，已组队=右耳 BT 地址） */
    uint8_t *adv_bt_addr = sndp_ctkd_get_local_bt_addr();
    if (adv_bt_addr) {
        memcpy(g_ble_addr_ctx.public_addr, adv_bt_addr, 6);
        bes_ble_gap_set_public_address((const bt_bdaddr_t *)adv_bt_addr);
    } else {
        ble_bdaddr_t addr = {{0}};
        addr = bes_ble_gap_get_current_ble_addr();
        memcpy(g_ble_addr_ctx.public_addr, addr.addr, 6);
    }
#else
    ble_bdaddr_t addr = {{0}};
    addr = bes_ble_gap_get_current_ble_addr();

    memcpy(g_ble_addr_ctx.public_addr, addr.addr, 6);
#endif /* __SNDP_CTKD_OVER_LE__ */
    g_ble_addr_ctx.synced = false;

    ble_adv_activity_t *ble_create_adv;
    ble_create_adv=app_ble_register_advertising(BLE_BASIC_ADV_HANDLE, BLE_ADV_USAGE_DEFAULT, sndp_comm_ble_activity_prepare);
    if (ble_create_adv == NULL) {
        COMM_BLE_TRACE(0, "SNDP ADV Register FAIL!!!");
    }    

}

/* 获取当前广播地址 */
uint8_t* sndp_ble_get_public_addr(void)
{
    return g_ble_addr_ctx.public_addr;
}
/* 主耳：发送public地址 */
void sndp_master_send_ble_public_addr(void)
{
    if (sndp_is_tws_link_connected() && sndp_is_tws_slave_mode())
    {
        COMM_BLE_TRACE(0, "[TWS] slave mode, dont send public addr");
        return;
    }
    COMM_BLE_TRACE(0, "done.");

    g_ble_addr_ctx.synced = true;
	tws_ctrl_send_cmd(APP_TWS_CMD_SNDP_BLE_PUBLIC_ADDR_SYNC, g_ble_addr_ctx.public_addr, 6);
}

/* 从耳：接收public地址 */
void sndp_ble_receive_master_public_addr(uint8_t *addr, uint16_t length)
{
    COMM_BLE_TRACE(0, "done.");
    if(addr == NULL || length != 6) {
        COMM_BLE_TRACE(0, "addr == null or length != 6, return");
        return;
    }

    memcpy(g_ble_addr_ctx.public_addr, addr, 6);
    g_ble_addr_ctx.synced = true;
}

void sndp_ble_set_public_addr(void)
{
    COMM_BLE_TRACE(0, ".");
    uint8_t *addr = sndp_ble_get_public_addr();
    COMM_BLE_TRACE(0, "public_addr=%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    bes_ble_gap_set_public_address((const bt_bdaddr_t *)addr);//api-set public addr!!!
}


bool sndp_comm_ble_activity_prepare(ble_adv_activity_t *adv)
{

    sndp_ble_set_public_addr();//ble addr set

    if (sndp_is_tws_link_connected() && sndp_is_tws_slave_mode())
    {
        COMM_BLE_TRACE(0, "[TWS] slave mode, disable advertising");
        return false;
    }


    gap_adv_param_t *adv_param = &adv->adv_param;

    adv->adv_handle = BLE_BASIC_ADV_HANDLE;
    adv->user = USER_SNDP_BLE;
    adv_param->connectable = true;
    adv_param->scannable = true;
    adv_param->use_legacy_pdu = true;

    /* 功耗优化: 调大产测BLE广播间隔降低待机功耗
     * 间隔 可调 */
    bes_ble_gap_param_set_adv_interval(BLE_ADV_INTERVALREQ_USER_TWS_STM,
                                       USER_SNDP_BLE,
                                       SNDP_COMM_BLE_ADV_INTERVAL_MS);

    uint8_t* ble_name = (uint8_t *)bt_get_ble_local_name();
    int ble_name_len = strlen((char *)ble_name);

    uint8_t local_ble_addr[6] = {0};
    uint8_t local_bt_addr[6] = {0};

    uint8_t adv_data[31];
    uint8_t adv_data_size = 0;
    uint8_t scan_rsp_data[31];
    uint8_t scan_rsp_data_size = 0;

    bt_bdaddr_t *local_bdaddr = NULL;
#if defined(__SNDP_CTKD_OVER_LE__)
    /* SNDP CTKD: 广播里上报的 "BT MAC" 必须与 BLE 广播地址同源：
     * 未组队 → 本机 BT 地址；已组队 → 本机组唯一地址(右耳 BT 地址)。 */
    local_bdaddr = (bt_bdaddr_t *)sndp_ctkd_get_local_bt_addr();
#else
    local_bdaddr = (bt_bdaddr_t *)sndp_get_pair_addr();
#endif /* __SNDP_CTKD_OVER_LE__ */
    bt_bdaddr_t *ble_bdaddr = (bt_bdaddr_t *)bt_get_ble_local_address();
    if (local_bdaddr) {
        memcpy(local_bt_addr, local_bdaddr->address, 6);
    }
    if (ble_bdaddr) {
        memcpy(local_ble_addr, ble_bdaddr->address, 6);
    } else {
        memset(local_ble_addr, 0, 6);
    }
#if defined(__SNDP_CTKD_OVER_LE__)
    /* 开机核对：adv 上报的 BT MAC 必须等于 BLE 广播出去的本机地址。
     * 未组队: adv MAC == local BT addr == local BLE addr(产测需同值)
     * 已组队: adv MAC == 右耳 BT 地址(sndp_get_pair_addr) */
    {
        uint8_t *hw_bt_addr = (uint8_t *)bt_get_local_address();

        COMM_BLE_TRACE(0, "ADV adv BT MAC   : %02X %02X %02X %02X %02X %02X",
                       local_bt_addr[0], local_bt_addr[1], local_bt_addr[2],
                       local_bt_addr[3], local_bt_addr[4], local_bt_addr[5]);
        COMM_BLE_TRACE(0, "ADV local BT addr: %02X %02X %02X %02X %02X %02X",
                       hw_bt_addr[0], hw_bt_addr[1], hw_bt_addr[2],
                       hw_bt_addr[3], hw_bt_addr[4], hw_bt_addr[5]);
        COMM_BLE_TRACE(0, "ADV local BLE adr: %02X %02X %02X %02X %02X %02X",
                       local_ble_addr[0], local_ble_addr[1], local_ble_addr[2],
                       local_ble_addr[3], local_ble_addr[4], local_ble_addr[5]);
    }
#endif

    /* ---------------- Flags ---------------- */
#if defined(__SNDP_CTKD_OVER_LE__)
    /* CTKD 文档要求 0x18/0x1A（bit3 Controller + bit4 Host）。
     * gap_dt_add_flags(true) 只会打出 0x0A，nRF 显示 Controller only。 */

    // uint8_t ad_flags = (uint8_t)(GAP_FLAGS_LE_GENERAL_DISCOVERABLE_MODE |
    //                              GAP_FLAGS_SIMU_LE_BREDR_TO_SAME_DEVICE |
    //                              GAP_FLAGS_SIMU_LE_BREDR_HOST);
    // gap_dt_add_data_type(&adv_param->adv_data, GAP_DT_FLAGS, &ad_flags, 1);
    // COMM_BLE_TRACE(0, "CTKD adv flags=0x%02x", ad_flags);

    adv_data[adv_data_size++] = 0x02; // length
    adv_data[adv_data_size++] = 0x01; // AD Type: Flags
    adv_data[adv_data_size++] = 0x1A; // CTKD 文档要求 0x18/0x1A（bit3 Controller + bit4 Host）。

#else
    adv_data[adv_data_size++] = 0x02; // length
    adv_data[adv_data_size++] = 0x01; // AD Type: Flags
    adv_data[adv_data_size++] = 0x06; // General Discoverable | BR/EDR Not Supported 
#endif /* __SNDP_CTKD_OVER_LE__ */

    /* ---------------- Manufacturer Specific Data ---------------- */
    adv_data[adv_data_size++] = 0x0E;
    adv_data[adv_data_size++] = 0xFF; // Manufacturer Type

    /* Company ID */
    adv_data[adv_data_size++] = 0x00;
    adv_data[adv_data_size++] = 0x00;

    /* Product Key */
    adv_data[adv_data_size++] = 0x00;
    adv_data[adv_data_size++] = 0x00;

    /* BT MAC */
    for (int i = 0; i < 6; i++) {
        adv_data[adv_data_size++] = local_bt_addr[5-i];
    }
    COMM_BLE_TRACE(0, "mobileconnected=%d", sndp_is_master_mobile_link_connected());
    /* Mobile connect status */
    adv_data[adv_data_size++] = sndp_is_master_mobile_link_connected();

    /* TWS connect status */
    adv_data[adv_data_size++] = sndp_is_tws_link_connected();

    /* Reserved */
    adv_data[adv_data_size++] = 0x00;

    gap_dt_add_raw_data(&adv_param->adv_data,
                        adv_data,
                        adv_data_size);

    /* ==================== Scan Response ==================== */

    /* ---------------- Local Name ---------------- */
    if (ble_name_len > 31 - 2) {
        ble_name_len = 31 - 2;
    }

    if (ble_name_len > 0) {
        scan_rsp_data[scan_rsp_data_size++] = 1 + ble_name_len; // length
        scan_rsp_data[scan_rsp_data_size++] = 0x09;             // Complete Local Name
        memcpy(&scan_rsp_data[scan_rsp_data_size], ble_name, ble_name_len);
        scan_rsp_data_size += ble_name_len;
    }

    gap_dt_add_raw_data(&adv_param->scan_rsp_data,
                        scan_rsp_data,
                        scan_rsp_data_size);

    COMM_BLE_TRACE(0, "SUCCESS!!!");
    return true;
 }
#endif

int32_t sndp_comm_ble_init(void)
{
	if(ble_send_timeout_timer == NULL)
		ble_send_timeout_timer = osTimerCreate (osTimer(BLE_SEND_TIMEOUT_TIMER), osTimerOnce, NULL);

    sndp_comm_ble_send_queue_mutex_id = osMutexCreate(osMutex(sndp_comm_ble_send_queue_mutex));
    ASSERT(sndp_comm_ble_send_queue_mutex_id != NULL, "%s, sndp_comm_ble_send_queue_mutex_id == NULL", __func__);

    osMutexWait(sndp_comm_ble_send_queue_mutex_id, osWaitForever);
    InitCQueue(&sndp_comm_ble_send_queue, sizeof(sndp_comm_ble_send_queue_buf), sndp_comm_ble_send_queue_buf);
    osMutexRelease(sndp_comm_ble_send_queue_mutex_id);
	
	memset(&sndp_comm_ble_ctx, 0, sizeof(sndp_comm_ble_ctx));
	sndp_comm_ble_ctx.conn_status = SNDP_COMM_BLE_DISCONNECTED;
	sndp_comm_ble_ctx.sending = false;
	sndp_comm_ble_ctx.mtu = 20;
    
#ifdef CFG_APP_DATAPATH_SERVER
	app_datapath_server_register_tx_done(sndp_comm_ble_tx_done);
	app_datapath_server_register_rx_done(sndp_comm_ble_recv_data_callback);
	app_datapath_server_register_disconnected_done(sndp_comm_ble_disconnected_done);
	app_datapath_server_register_connected_done(sndp_comm_ble_connected_done);
	app_datapath_server_register_mtu_exchanged_done(sndp_comm_ble_mtuexchanged_done);
#endif

    sndp_comm_ble_ctx.inited = true;
    sndp_comm_ble_ctx.conidx = 0;

#if defined(__SNDP_CTKD_OVER_LE__)
    bts_ble_gap_smp_require_modify_cb_register(sndp_ctkd_smp_require_modify);
    app_ble_smp_get_specifc_irk_ia_callback_register(sndp_ctkd_get_specific_irk_ia);
    app_ble_gap_add_record_modify_callback_register(sndp_ctkd_add_record_modify);
    bta_tws_register_ui_secure_request_callback(sndp_ctkd_ui_secure_request);
#endif /* __SNDP_CTKD_OVER_LE__ */
	COMM_BLE_TRACE(0, "done.");

    
#if defined(__SNDP_COMM_BLE_ADV_SET__)
    sndp_ble_advertising_init();
    sndp_delay_exec_start(200, (uint32_t)app_ble_refresh_adv_state_generic, 0, 0, 0);
    COMM_BLE_TRACE(0, "SNDP ADV INIT SUCC!!!");
#endif
    
	return 0;
}


#endif	/* __SNDP_COMM_BLE__ */


