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
#define ROLE_TRACE(num, str, ...)   SNDP_TRACE(num, "[SLEEP_ROLE] %s, " str, __func__, ##__VA_ARGS__)

/**************************************************************************************************
 * Types & Constants
 **************************************************************************************************/
typedef struct {
    Device_Role_t       role;               /* 当前角色 */
    Role_Switch_Reason_t pending_reason;    /* 待处理的切换原因 */
    bool                initialized;        /* 是否已初始化 */
    uint32_t            switch_timeout_ms;  /* 切换超时计数 */
} sndp_sleep_role_ctx_s;

#define ROLE_SWITCH_TIMEOUT_MS_MAX      (5000)  /* 切换保护态最大持续时间 5s */

/**************************************************************************************************
 * Global State
 **************************************************************************************************/
static sndp_sleep_role_ctx_s role_ctx;

/* 前向声明 */
static void sndp_sleep_role_master_start(void);
static void sndp_sleep_role_slave_start(void);

/**************************************************************************************************
 * Role Query API
 **************************************************************************************************/
Device_Role_t sndp_sleep_role_get_current(void)
{
    return role_ctx.role;
}

bool sndp_sleep_role_is_master(void)
{
    return (role_ctx.role == ROLE_MASTER_ACTIVE);
}

bool sndp_sleep_role_is_slave(void)
{
    return (role_ctx.role == ROLE_SLAVE_STANDBY);
}

bool sndp_sleep_role_is_switching(void)
{
    return (role_ctx.role == ROLE_SWITCHING);
}

/**************************************************************************************************
 * Internal: Master启动流程
 *   - 开启PPG、ACC传感器
 *   - 初始化心率/睡眠算法
 *   - BLE由平台层管理（假设已在Sleep模式下建立）
 **************************************************************************************************/
static void sndp_sleep_role_master_start(void)
{
    ROLE_TRACE(0, "Master starting...");

    /* 开启PPG传感器 */
    sndp_hr_switch_reading_ppg(true);

    /* 开启ACC加速度传感器 */
    sndp_hr_switch_reading_acc_raw_data(true);

#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
    /* 初始化心率算法 */
    dbbeats_initialize_heartrate_data(
        (int8_t)sndp_hr_mearsuring_get_sampling_rate(),
        sndp_hr_mearsuring_get_dump_state());

    /* 初始化睡眠算法 */
    dbbeats_initialize_sleep_data(sndp_get_sleep_control());
#endif

    role_ctx.role = ROLE_MASTER_ACTIVE;
    ROLE_TRACE(0, "Master active");
}

/**************************************************************************************************
 * Internal: Slave启动流程
 *   - 强制关闭PPG、ACC传感器
 *   - 屏蔽算法回调（通过停止传感器实现）
 *   - 进入极低功耗状态
 **************************************************************************************************/
static void sndp_sleep_role_slave_start(void)
{
    ROLE_TRACE(0, "Slave starting...");

    /* 关闭PPG传感器 */
    sndp_hr_switch_reading_ppg(false);

    /* 关闭ACC传感器 */
    sndp_hr_switch_reading_acc_raw_data(false);

    role_ctx.role = ROLE_SLAVE_STANDBY;
    ROLE_TRACE(0, "Slave standby");
}

/**************************************************************************************************
 * Internal: Master → Slave 降级
 *   1. 标记 ROLE_SWITCHING（关键保护态）
 *   2. 获取算法快照
 *   3. 通过TWS发送快照到对侧
 *   4. 关闭本地传感器，降级为Slave
 **************************************************************************************************/
static void sndp_sleep_role_master_downgrade(void)
{
    Algorithm_Snapshot_t snapshot;
    struct Snapshot lib_snapshot;

    ROLE_TRACE(0, "Master downgrade starting...");

    role_ctx.role = ROLE_SWITCHING;

    /* Step 1: 获取 dbbeats 算法当前状态快照 */
#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
    dbbeats_get_snapshot(&lib_snapshot);

    /* 映射到传输结构体 */
    snapshot.size = lib_snapshot.size;
    if (snapshot.size > SNDP_SLEEP_SNAPSHOT_DATA_SIZE) {
        snapshot.size = SNDP_SLEEP_SNAPSHOT_DATA_SIZE;
    }
    memcpy(snapshot.data, lib_snapshot.data, snapshot.size);
#else
    memset(&snapshot, 0, sizeof(snapshot));
#endif

    /* Step 2: 通过TWS链路发送快照到对侧 */
#if defined(__SNDP_COMM_MGR__)
    uint8_t send_buf[sizeof(Algorithm_Snapshot_t) + 2]; /* +2 for size field */
    send_buf[0] = (uint8_t)(snapshot.size & 0xFF);
    send_buf[1] = (uint8_t)((snapshot.size >> 8) & 0xFF);
    memcpy(&send_buf[2], snapshot.data, snapshot.size);

    sndp_comm_cmd_send_lr_sync_sleep_snapshot(send_buf, snapshot.size + 2);
    ROLE_TRACE(0, "Snapshot sent, size=%d", snapshot.size);
#endif

    /* Step 3: 停止传感器，切换角色 */
    sndp_hr_switch_reading_ppg(false);
    sndp_hr_switch_reading_acc_raw_data(false);

    role_ctx.role = ROLE_SLAVE_STANDBY;
    ROLE_TRACE(0, "Downgraded to Slave");
}

/**************************************************************************************************
 * Internal: Slave → Master 升级
 *   1. 标记 ROLE_SWITCHING
 *   2. 解析快照数据，调用 dbbeats_set_snapshot 恢复算法状态
 *   3. 开启传感器
 *   4. 重新初始化算法上下文
 *   5. 角色升级为Master
 **************************************************************************************************/
static void sndp_sleep_role_slave_upgrade(Algorithm_Snapshot_t *snapshot)
{
    struct Snapshot lib_snapshot;

    ROLE_TRACE(0, "Slave upgrade starting...");

    role_ctx.role = ROLE_SWITCHING;

    /* Step 1: 恢复算法状态快照 */
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

    /* Step 2: 开启传感器 */
    sndp_hr_switch_reading_ppg(true);
    sndp_hr_switch_reading_acc_raw_data(true);

    /* Step 3: 重新初始化算法上下文 */
#if defined(__SNDP_HR_ALGO_SLEEPSENSE__)
    dbbeats_initialize_heartrate_data(
        (int8_t)sndp_hr_mearsuring_get_sampling_rate(),
        sndp_hr_mearsuring_get_dump_state());
    dbbeats_initialize_sleep_data(sndp_get_sleep_control());
#endif

    /* Step 4: 升级为Master */
    role_ctx.role = ROLE_MASTER_ACTIVE;
    ROLE_TRACE(0, "Upgraded to Master");

    /* Step 5: 通知APP数据恢复 */
#if defined(__SNDP_COMM_MGR__)
    sndp_comm_cmd_sleepapp_proximity_role_switch_update();
#endif
}

/**************************************************************************************************
 * Public API: 模块初始化
 * 开机时调用，根据TWS配对信息确定初始角色
 * 规则：右耳默认为Master，左耳为Slave（支持通过配置更改）
 **************************************************************************************************/
void sndp_sleep_role_switch_init(void)
{
    ROLE_TRACE(0, "Initializing...");

    memset(&role_ctx, 0, sizeof(role_ctx));

    /* 初始角色判定：右耳=Master, 左耳=Slave */
    if (sndp_dev_is_right_earphone()) {
        role_ctx.role = ROLE_IDLE;
        ROLE_TRACE(0, "Right ear - designated Master, entering IDLE");
        /* 延迟启动Master，等待TWS连接建立 */
    } else if (sndp_dev_is_left_earphone()) {
        role_ctx.role = ROLE_IDLE;
        ROLE_TRACE(0, "Left ear - designated Slave, entering IDLE");
    } else {
        role_ctx.role = ROLE_IDLE;
        ROLE_TRACE(0, "Unknown side - entering IDLE");
    }

    role_ctx.initialized = true;
}

/**************************************************************************************************
 * Public API: 触发角色切换（Master侧调用）
 * @param reason  触发原因（脱落/低电/APP强制）
 *
 * 前置条件：
 *   - 必须是Master角色
 *   - 不在SWITCHING保护态
 *   - TWS链路已连接
 **************************************************************************************************/
void sndp_sleep_role_switch_trigger(Role_Switch_Reason_t reason)
{
    if (!role_ctx.initialized) {
        ROLE_TRACE(0, "Not initialized, rtn");
        return;
    }

    if (role_ctx.role != ROLE_MASTER_ACTIVE) {
        ROLE_TRACE(0, "Not Master (role=%d), rtn", role_ctx.role);
        return;
    }

    /* 防止重复触发切换 */
    if (role_ctx.role == ROLE_SWITCHING) {
        ROLE_TRACE(0, "Already switching, rtn");
        return;
    }

    /* 检查TWS连接 */
    if (!sndp_is_tws_link_connected()) {
        ROLE_TRACE(0, "TWS not connected, cannot switch");
        /* TWS断开时尝试本地恢复 */
        sndp_sleep_role_switch_handle_tws_lost();
        return;
    }

    /* 确保对侧耳机已佩戴（Slave必须佩戴才能接管） */
    if (!sndp_dev_wear_is_worn(true)) {
        ROLE_TRACE(0, "Peer not worn, rtn");
        return;
    }

    ROLE_TRACE(0, "Triggered by reason=%d", reason);
    role_ctx.pending_reason = reason;

    /* 执行降级切换：Master → Slave */
    sndp_sleep_role_master_downgrade();
}

/**************************************************************************************************
 * Public API: 处理接收到的角色切换快照（Slave侧调用）
 * @param snapshot  快照数据（来自TWS链路）
 * @param len       数据长度
 *
 * 由通信模块收到 COMM_CMDID_LR_SYNC_SLEEP_SNAPSHOT 时调用
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

    /* 解析快照：前2字节为size，后续为数据 */
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

    /* 当前角色必须是Slave才能升级 */
    if (role_ctx.role != ROLE_SLAVE_STANDBY) {
        /* 如果当前是Master，说明出现了双Master冲突 */
        if (role_ctx.role == ROLE_MASTER_ACTIVE) {
            ROLE_TRACE(0, "Conflict: already Master, rejecting takeover");
            sndp_sleep_role_switch_handle_conflict();
            return;
        }
        ROLE_TRACE(0, "Not Slave (role=%d), rtn", role_ctx.role);
        return;
    }

    /* 执行升级：Slave → Master */
    sndp_sleep_role_slave_upgrade(&snapshot);
}

/**************************************************************************************************
 * Public API: 处理角色切换请求命令（接收方调用）
 *
 * 收到对侧发来的 COMM_CMDID_LR_SYNC_SLEEP_ROLE_SWITCH 时调用
 * Slave侧准备好接收快照后调用
 **************************************************************************************************/
void sndp_sleep_role_switch_on_request(void)
{
    if (!role_ctx.initialized) {
        ROLE_TRACE(0, "Not initialized, rtn");
        return;
    }

    ROLE_TRACE(0, "Role switch request received, current role=%d", role_ctx.role);

    /* 当前如果是Master，拒绝请求（防止双Master） */
    if (role_ctx.role == ROLE_MASTER_ACTIVE) {
        ROLE_TRACE(0, "Already Master, ignoring request");
        return;
    }

    /* 标记等待快照 */
    role_ctx.role = ROLE_SWITCHING;
    role_ctx.switch_timeout_ms = 0;
}

/**************************************************************************************************
 * Public API: 异常处理 - 双Master冲突解决
 *
 * 场景：Slave已切换为Master，但原Master重新连接尝试接管
 * 策略：维持当前Master状态，拒绝原Master重新接管
 **************************************************************************************************/
void sndp_sleep_role_switch_handle_conflict(void)
{
    ROLE_TRACE(0, "Handling conflict");

    /* 维持当前Master状态不变 */
    /* 可以在此处添加日志、上报APP等操作 */
}

/**************************************************************************************************
 * Public API: 异常处理 - TWS断开恢复
 *
 * 场景：切换过程中TWS链路断开
 * 策略：Master侧尝试本地恢复，保持Master角色等待重连；
 *       Slave侧若未完全接管，保持Standby等待重连
 **************************************************************************************************/
void sndp_sleep_role_switch_handle_tws_lost(void)
{
    ROLE_TRACE(0, "Handling TWS lost, current role=%d", role_ctx.role);

    if (role_ctx.role == ROLE_SWITCHING) {
        /* 切换过程中断开：回退到原角色 */
        ROLE_TRACE(0, "TWS lost during switching, recovering...");
        // 保持当前传感器状态不变，等待TWS重连后重新发起切换
        role_ctx.role = ROLE_MASTER_ACTIVE;
        ROLE_TRACE(0, "Reverted to Master, waiting for TWS reconnect");
    }
    /* 其他状态下断开不做特殊处理 */
}

/**************************************************************************************************
 * Public API: 定时器回调 - 处理切换超时
 *
 * 在sndp_ui_timing_to_do中周期性调用
 * 如果切换保护态超过5秒，强制恢复
 **************************************************************************************************/
void sndp_sleep_role_switch_timeout_check(void)
{
    if (role_ctx.role != ROLE_SWITCHING) {
        role_ctx.switch_timeout_ms = 0;
        return;
    }

    role_ctx.switch_timeout_ms += 10000; /* 假设每10秒调用一次，实际增量由调用方决定 */

    if (role_ctx.switch_timeout_ms >= ROLE_SWITCH_TIMEOUT_MS_MAX) {
        ROLE_TRACE(0, "Switch timeout, force recovery");
        role_ctx.switch_timeout_ms = 0;

        /* 超时回退：Slave降级侧保持Slave，等待下次触发 */
        if (sndp_dev_is_right_earphone()) {
            /* 右耳默认Master */
            sndp_sleep_role_master_start();
        } else {
            sndp_sleep_role_slave_start();
        }
    }
}

#endif /* __SNDP_SLEEP_APP__ && __SNDP_HEART_RATE_MGR__ */
