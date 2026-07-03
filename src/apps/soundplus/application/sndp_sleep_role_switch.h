#ifndef __SNDP_SLEEP_ROLE_SWITCH_H__
#define __SNDP_SLEEP_ROLE_SWITCH_H__

#if defined(__SNDP_SLEEP_APP__) && defined(__SNDP_HEART_RATE_MGR__)

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 * 角色状态机定义
 * TWS睡眠监测
 **************************************************************************************************/

typedef enum {
    ROLE_IDLE           = 0,    /* 初始态：尚未确定角色 */
    ROLE_ACTIVE         = 1,    /* 设备工作中：采集PPG/ACC数据，BLE连接APP，执行算法 */
    ROLE_STANDBY        = 2,    /* 设备待命：关闭传感器和算法，极低功耗 */
    ROLE_SWITCHING      = 3,    /* 切换中：关键保护态，禁止重复触发 */
} Device_Role_t;

/**
 * @brief 算法快照结构体（用于TWS传输）
 * 基于 SleepSense 库的 struct Snapshot，传输实际数据部分
 */
#define SNDP_SLEEP_SNAPSHOT_DATA_SIZE   213   /* 与 SNAPSHOT_PAYLOAD_SIZE 一致 */

typedef struct {
    uint16_t size;                                  /* 实际数据长度 */
    uint8_t  data[SNDP_SLEEP_SNAPSHOT_DATA_SIZE];   /* 算法状态快照数据 */
} Algorithm_Snapshot_t;

/**
 * @brief 角色切换触发原因
 */
typedef enum {
    ROLE_SWITCH_REASON_WEAR_OFF     = 0,    /* Master 脱落触发 */
    ROLE_SWITCH_REASON_WEAR_ON      = 1,    /* Master 戴上触发 */
    ROLE_SWITCH_REASON_LOW_BATTERY  = 2,    /* Master 低电触发 */
    ROLE_SWITCH_REASON_APP_FORCE    = 3,    /* APP 强制切换指令 */
} Role_Switch_Reason_t;

/**
 * @brief 获取当前设备角色
 * @return Device_Role_t 当前角色
 */
Device_Role_t sndp_sleep_role_get_current(void);

/**
 * @brief 判断当前设备是否为 Master
 * @return true=Master, false=非Master
 */
bool sndp_sleep_role_is_active(void);

/**
 * @brief 判断当前设备是否为 Slave
 * @return true=Slave, false=非Slave
 */
bool sndp_sleep_role_is_standby(void);

/**
 * @brief 检查是否处于切换保护态
 * @return true=正在切换中
 */
bool sndp_sleep_role_is_switching(void);

/**
 * @brief 模块初始化
 * 开机时调用，根据TWS配对信息确定初始角色
 */
void sndp_sleep_role_switch_init(void);

/**
 * @brief 触发角色切换（Master侧调用）
 * @param reason 切换触发原因
 * 
 * 执行流程：
 * 1. 标记 ROLE_SWITCHING
 * 2. 停止算法，获取 dbbeats 快照
 * 3. 通过 TWS 链路发送快照到对侧
 * 4. 关闭传感器，降级为 Slave
 */
void sndp_sleep_role_switch_trigger(Role_Switch_Reason_t reason);

/**
 * @brief 处理接收到的角色切换快照（Slave侧调用）
 * @param snapshot 算法快照数据指针
 * @param len      快照数据长度
 * 
 * 执行流程：
 * 1. 解析快照数据
 * 2. 调用 dbbeats_set_snapshot 恢复算法状态
 * 3. 开启传感器
 * 4. 重新初始化算法上下文
 * 5. 升级为 Master
 */
void sndp_sleep_role_switch_recv_snapshot(uint8_t *snapshot, uint16_t len);

/**
 * @brief 处理角色切换请求命令（接收方调用）
 * 当收到对侧发来的角色切换请求时调用
 */
void sndp_sleep_role_switch_on_request(void);

/**
 * @brief 异常处理：对侧重新连接时的冲突解决
 * 如果当前已是Master，拒绝原Master重新接管
 */
void sndp_sleep_role_switch_handle_conflict(void);

void sndp_sleep_role_start(void);

void sndp_sleep_role_stop(void);

void sndp_sleep_role_set_peer(Device_Role_t role);
#ifdef __cplusplus
}
#endif

#endif /* __SNDP_SLEEP_APP__ && __SNDP_HEART_RATE_MGR__ */
#endif /* __SNDP_SLEEP_ROLE_SWITCH_H__ */
