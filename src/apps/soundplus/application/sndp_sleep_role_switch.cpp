#if defined(__SNDP_SLEEP_APP__) && defined(__SNDP_HEART_RATE_MGR__)

#include "stdio.h"
#include "string.h"
#include "cmsis_os.h"
#include "hal_trace.h"

#include "sndp_if_common.h"
#include "sndp_if_device.h"
#include "sndp_if_platform.h"

#include "sndp_sleep_role_switch.h"
#include "sndp_heart_rate.h"
#include "sndp_comm_cmd.h"

#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
#include "sleepsense.h"
#endif

/**************************************************************************************************
 * Trace
 **************************************************************************************************/
#define ROLE_TRACE(num, str, ...)   SNDP_TRACE(num, "[SLEEP_ROLE_LOG] %s, " str, __func__, ##__VA_ARGS__)

/**************************************************************************************************
 * Types & Constants
 **************************************************************************************************/
typedef struct {
    Device_Role_t       role;               /* ��ǰ��ɫ */
    Device_Role_t       peer_role;          /* �Բ��ɫ */
    Role_Switch_Reason_t pending_reason;    /* ���������л�ԭ�� */
    bool                initialized;        /* �Ƿ��ѳ�ʼ�� */
    uint32_t            switch_timeout_ms;  /* �л���ʱ���� */
} sndp_sleep_role_ctx_s;

/**************************************************************************************************
 * Global State
 **************************************************************************************************/
static sndp_sleep_role_ctx_s role_ctx;

/**************************************************************************************************
 * Role Query API
 **************************************************************************************************/
Device_Role_t sndp_sleep_role_get_current(void)
{
    return role_ctx.role;
}

bool sndp_sleep_role_is_active(void)
{
    return (role_ctx.role == ROLE_ACTIVE);
}

bool sndp_sleep_role_is_standby(void)
{
    return (role_ctx.role == ROLE_STANDBY);
}

bool sndp_sleep_role_is_switching(void)
{
    return (role_ctx.role == ROLE_SWITCHING);
}

Device_Role_t sndp_sleep_role_get_peer(void)
{
    return role_ctx.peer_role;
}

bool sndp_sleep_role_is_peer_active(void)
{
    return (role_ctx.peer_role == ROLE_ACTIVE);
}

bool sndp_sleep_role_is_peer_idle(void)
{
    return (role_ctx.peer_role == ROLE_IDLE);
}

bool sndp_sleep_role_is_peer_standby(void)
{
    return (role_ctx.peer_role == ROLE_STANDBY);
}

void sndp_sleep_role_set_peer(Device_Role_t role)
{
    ROLE_TRACE(0, "peer role set to %d", role);
    role_ctx.peer_role = role;
}

static void sndp_sleep_role_suspend(void)
{
#if defined(__SNDP_HEART_RATE_MGR__) 
    if(sndp_dev_sleep_app_get_heartrate_onoff(false)) {
        sndp_hr_suspend(0x00);
        ROLE_TRACE(0, "suspend heart rate");
    }           
    
    if(sndp_dev_sleep_app_get_stage_onoff(false)) {
        sndp_sleep_analysis_suspend();
        ROLE_TRACE(0, "suspend sleep analysis");
    }
#endif
}

static void sndp_sleep_role_resume(void)
{
#if defined(__SNDP_HEART_RATE_MGR__)
    if(sndp_dev_sleep_app_get_heartrate_onoff(false)) {
        sndp_hr_resume(0x00);
        ROLE_TRACE(0, "resume heart rate");
    }            
    
    if(sndp_dev_sleep_app_get_stage_onoff(false)) {
        sndp_sleep_analysis_resume();
        ROLE_TRACE(0, "resume sleep analysis");
    }
#endif
}

/**************************************************************************************************
 * Internal: ��������
 *   - ����PPG��ACC������
 *   - ��ʼ������/˯���㷨
 *   - BLE��ƽ̨���������������Sleepģʽ�½�����
 **************************************************************************************************/
void sndp_sleep_role_start(void)
{
    ROLE_TRACE(0, "starting...");
    role_ctx.role = ROLE_ACTIVE;
    sndp_comm_cmd_send_lr_sync_sleep_role_status(role_ctx.role);
    ROLE_TRACE(0, "Active");
}

void sndp_sleep_role_stop(void)
{
    ROLE_TRACE(0, "stopping...");
    role_ctx.role = ROLE_IDLE;
    sndp_comm_cmd_send_lr_sync_sleep_role_status(role_ctx.role);
    ROLE_TRACE(0, "Stopped");
}
/**************************************************************************************************
 * Internal: Slave��������
 *   - ǿ�ƹر�PPG��ACC������
 *   - �����㷨�ص���ͨ��ֹͣ������ʵ�֣�
 *   - ���뼫�͹���״̬
 **************************************************************************************************/
POSSIBLY_UNUSED static void sndp_sleep_role_standby_start(void)
{
    ROLE_TRACE(0, "starting...");
    role_ctx.role = ROLE_STANDBY;
    sndp_comm_cmd_send_lr_sync_sleep_role_status(role_ctx.role);
    ROLE_TRACE(0, "SStandby");
}

/**************************************************************************************************
 * Internal: 
 *   1. ��� ROLE_SWITCHING���ؼ�����̬��
 *   2. ��ȡ�㷨����
 *   3. ͨ��TWS���Ϳ��յ��Բ�
 *   4. �رձ��ش��������л�ΪStandby
 **************************************************************************************************/
static void sndp_sleep_role_switch_to_standby(void)
{
    Algorithm_Snapshot_t snapshot;
    struct Snapshot lib_snapshot;

    ROLE_TRACE(0, "downgrade starting...");

    role_ctx.role = ROLE_SWITCHING;

    /* Step 1: ��ȡ dbbeats �㷨��ǰ״̬���� */
#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
    dbbeats_get_snapshot(&lib_snapshot);

    /* ӳ�䵽����ṹ�� */
    snapshot.size = lib_snapshot.size;
    if (snapshot.size > SNDP_SLEEP_SNAPSHOT_DATA_SIZE) {
        snapshot.size = SNDP_SLEEP_SNAPSHOT_DATA_SIZE;
    }
    memcpy(snapshot.data, lib_snapshot.data, snapshot.size);
#else
    memset(&snapshot, 0, sizeof(snapshot));
#endif

    /* Step 2: ͨ��TWS��·���Ϳ��յ��Բ� */
#if defined(__SNDP_COMM_MGR__)
    uint8_t send_buf[sizeof(Algorithm_Snapshot_t) + 2]; /* +2 for size field */
    send_buf[0] = (uint8_t)(snapshot.size & 0xFF);
    send_buf[1] = (uint8_t)((snapshot.size >> 8) & 0xFF);
    memcpy(&send_buf[2], snapshot.data, snapshot.size);

    sndp_comm_cmd_send_lr_sync_sleep_snapshot(send_buf, snapshot.size + 2);
    ROLE_TRACE(0, "Snapshot sent, size=%d", snapshot.size);
#endif

    /* Step 3: ֹͣ��������ֹͣ�㷨���л���ɫ */
    sndp_sleep_analysis_stop();
    sndp_hr_mearsuring_stop();

    sndp_sleep_role_standby_start();
    ROLE_TRACE(0, "Switched to Standby");
}

/**************************************************************************************************
 * Internal:
 *   1. ��� ROLE_SWITCHING
 *   2. �����������ݣ����� dbbeats_set_snapshot �ָ��㷨״̬
 *   3. ����������
 *   4. ���³�ʼ���㷨������
 *   5. ��ɫ�л�ΪActive
 **************************************************************************************************/
static void sndp_sleep_role_switch_to_active(Algorithm_Snapshot_t *snapshot)
{
    struct Snapshot lib_snapshot;

    ROLE_TRACE(0, "Slave upgrade starting...");

    role_ctx.role = ROLE_SWITCHING;

    /* Step 1: �������������㷨 */
    sndp_hr_mearsuring_start(sndp_hr_mearsuring_get_sampling_rate(), sndp_mearsuring_get_dump_state(HR_DUMP_STATE));
    sndp_sleep_analysis_start(sndp_get_sleep_control());

    /* Step 2: �ָ��㷨״̬���� */
#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
    if (snapshot != NULL && snapshot->size > 0) {
        lib_snapshot.size = snapshot->size;
        if (lib_snapshot.size > SNAPSHOT_DATA_MAX_SIZE) {
            lib_snapshot.size = SNAPSHOT_DATA_MAX_SIZE;
        }
        memcpy(lib_snapshot.data, snapshot->data, lib_snapshot.size);
        dbbeats_set_snapshot(&lib_snapshot);
        ROLE_TRACE(0, "Snapshot restored, size=%d", lib_snapshot.size);
    }
#endif

    /* Step 3: ����ΪActive */
    role_ctx.role = ROLE_ACTIVE;
    sndp_comm_cmd_send_lr_sync_sleep_role_status(role_ctx.role);
    ROLE_TRACE(0, "Switched to Active");
    
}

/**************************************************************************************************
 * Public API: ģ���ʼ��
 * ����ʱ���ã�����TWS�����Ϣȷ����ʼ��ɫ
 * ����
 **************************************************************************************************/
void sndp_sleep_role_switch_init(void)
{
    ROLE_TRACE(0, "Initializing...");

    memset(&role_ctx, 0, sizeof(role_ctx));

    role_ctx.role = ROLE_IDLE;  /* ��ʼ��ɫΪIDLE */
    role_ctx.peer_role = ROLE_IDLE;
    role_ctx.initialized = true;
}

/**************************************************************************************************
 * Public API: ������ɫ�л�״̬
 * @param reason  ����ԭ������/�͵�/APPǿ�ƣ�
 **************************************************************************************************/
void sndp_sleep_role_switch_trigger(Role_Switch_Reason_t reason)
{
    if (!role_ctx.initialized) {
        ROLE_TRACE(0, "Not initialized, rtn");
        return;
    }


    /* ���TWS���� �Լ��Բ��豸û��� */
    if (!sndp_is_tws_link_connected() || !sndp_dev_wear_is_worn(true)) {
        ROLE_TRACE(0, "TWS link %d, peer worn %d rs:%d", sndp_is_tws_link_connected(), sndp_dev_wear_is_worn(true),reason);
        if(ROLE_SWITCH_REASON_WEAR_OFF == reason){
            sndp_sleep_role_suspend();
        }else if(ROLE_SWITCH_REASON_WEAR_ON == reason){
            sndp_sleep_role_resume();
        }
        return;
    }

    if(sndp_dev_wear_is_worn(true) && sndp_sleep_role_is_peer_active()) {
        ROLE_TRACE(0, "Peer is active, rtn");
        return;
    }

    ROLE_TRACE(0, "Triggered by reason=%d", reason);

    /* ִ���л���Active �� Standby */
    if(ROLE_SWITCH_REASON_WEAR_ON != reason) {
        if(sndp_dev_sleep_app_get_stage_onoff(false)) {
            if(sndp_sleep_role_is_peer_standby() || sndp_sleep_role_is_peer_idle()) {
                sndp_sleep_role_switch_to_standby();
            }
        }
    }
}

/**************************************************************************************************
 * Public API: �������յ��Ľ�ɫ�л�����
 * @param snapshot  �������ݣ�����TWS��·��
 * @param len       ���ݳ���
 *
 * ��ͨ��ģ���յ� COMM_CMDID_LR_SYNC_SLEEP_SNAPSHOT ʱ����
 **************************************************************************************************/
void sndp_sleep_role_switch_recv_snapshot(uint8_t *snapshot_data, uint16_t len)
{
    Algorithm_Snapshot_t snapshot;

    if (!role_ctx.initialized) {
        ROLE_TRACE(0, "Not initialized, rtn");
        return;
    }

    if (snapshot_data == NULL || len < 2) {
        ROLE_TRACE(0, "Invalid snapshot data, rtn");
        return;
    }

    /* �������գ�ǰ2�ֽ�Ϊsize������Ϊ���� */
    snapshot.size = (uint16_t)snapshot_data[0] | ((uint16_t)snapshot_data[1] << 8);
    if (snapshot.size > SNDP_SLEEP_SNAPSHOT_DATA_SIZE) {
        snapshot.size = SNDP_SLEEP_SNAPSHOT_DATA_SIZE;
    }

    uint16_t data_offset = 2;
    uint16_t copy_len = (len > data_offset) ? (len - data_offset) : 0;
    if (copy_len > snapshot.size) {
        copy_len = snapshot.size;
    }
    memset(snapshot.data, 0, sizeof(snapshot.data));
    memcpy(snapshot.data, &snapshot_data[data_offset], copy_len);

    ROLE_TRACE(0, "Snapshot received, size=%d, copy_len=%d", snapshot.size, copy_len);

    /* ��ǰ��ɫ������Standby��Idle�������� */
    if (role_ctx.role != ROLE_STANDBY && role_ctx.role != ROLE_IDLE) {
        /* �����ǰ��Active��˵��������˫Active��ͻ */
        if (role_ctx.role == ROLE_ACTIVE) {
            ROLE_TRACE(0, "Conflict: already Master, rejecting takeover");
            sndp_sleep_role_switch_handle_conflict();
            return;
        }
        ROLE_TRACE(0, "Not Standby (role=%d), rtn", role_ctx.role);
        return;
    }

    /* ִ���л���Standby �� Active */
    sndp_sleep_role_switch_to_active(&snapshot);
}

/**************************************************************************************************
 * Public API: ������ɫ�л�����������շ����ã�
 *
 * �յ��Բ෢���� COMM_CMDID_LR_SYNC_SLEEP_ROLE_SWITCH ʱ����
 * Slave��׼���ý��տ��պ����
 **************************************************************************************************/
void sndp_sleep_role_switch_on_request(void)
{
    if (!role_ctx.initialized) {
        ROLE_TRACE(0, "Not initialized, rtn");
        return;
    }

    ROLE_TRACE(0, "Role switch request received, current role=%d", role_ctx.role);

    /* ��ǰ�����Active���ܾ����󣨷�ֹ˫Active�� */
    if (role_ctx.role == ROLE_ACTIVE) {
        ROLE_TRACE(0, "Already Active, ignoring request");
        return;
    }

    /* ��ǵȴ����� */
    role_ctx.role = ROLE_SWITCHING;
    role_ctx.switch_timeout_ms = 0;
}

/**************************************************************************************************

 **************************************************************************************************/
void sndp_sleep_role_switch_handle_conflict(void)
{
    ROLE_TRACE(0, "Handling conflict");

}

#endif /* __SNDP_SLEEP_APP__ && __SNDP_HEART_RATE_MGR__ */
