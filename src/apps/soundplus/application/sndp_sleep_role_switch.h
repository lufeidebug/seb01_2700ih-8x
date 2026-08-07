#ifndef __SNDP_SLEEP_ROLE_SWITCH_H__
#define __SNDP_SLEEP_ROLE_SWITCH_H__

#if defined(__SNDP_SLEEP_APP_ROLE_SWITCH__)

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 * ��ɫ״̬������
 * TWS˯�߼��
 **************************************************************************************************/

typedef enum {
    ROLE_IDLE           = 0,    /* ��ʼ̬����δȷ����ɫ */
    ROLE_ACTIVE         = 1,    /* �豸�����У��ɼ�PPG/ACC���ݣ�BLE����APP��ִ���㷨 */
    ROLE_STANDBY        = 2,    /* �豸�������رմ��������㷨�����͹��� */
    ROLE_SWITCHING      = 3,    /* �л��У��ؼ�����̬����ֹ�ظ����� */
} Device_Role_t;

/**
 * @brief �㷨���սṹ�壨����TWS���䣩
 * ���� SleepSense ��� struct Snapshot������ʵ�����ݲ���
 */
#define SNDP_SLEEP_SNAPSHOT_DATA_SIZE   213   /* �� SNAPSHOT_PAYLOAD_SIZE һ�� */

typedef struct {
    uint16_t size;                                  /* ʵ�����ݳ��� */
    uint8_t  data[SNDP_SLEEP_SNAPSHOT_DATA_SIZE];   /* �㷨״̬�������� */
} Algorithm_Snapshot_t;

/**
 * @brief ��ɫ�л�����ԭ��
 */
typedef enum {
    ROLE_SWITCH_REASON_WEAR_OFF     = 0,    /* Master ���䴥�� */
    ROLE_SWITCH_REASON_WEAR_ON      = 1,    /* Master ���ϴ��� */
    ROLE_SWITCH_REASON_LOW_BATTERY  = 2,    /* Master �͵紥�� */
    ROLE_SWITCH_REASON_APP_FORCE    = 3,    /* APP ǿ���л�ָ�� */
} Role_Switch_Reason_t;

/**
 * @brief ��ȡ��ǰ�豸��ɫ
 * @return Device_Role_t ��ǰ��ɫ
 */
Device_Role_t sndp_sleep_role_get_current(void);

/**
 * @brief �жϵ�ǰ�豸�Ƿ�Ϊ Master
 * @return true=Master, false=��Master
 */
bool sndp_sleep_role_is_active(void);

/**
 * @brief �жϵ�ǰ�豸�Ƿ�Ϊ Slave
 * @return true=Slave, false=��Slave
 */
bool sndp_sleep_role_is_standby(void);

/**
 * @brief ����Ƿ����л�����̬
 * @return true=�����л���
 */
bool sndp_sleep_role_is_switching(void);

/**
 * @brief ģ���ʼ��
 * ����ʱ���ã�����TWS�����Ϣȷ����ʼ��ɫ
 */
void sndp_sleep_role_switch_init(void);

/**
 * @brief ������ɫ�л���Master����ã�
 * @param reason �л�����ԭ��
 * 
 * ִ�����̣�
 * 1. ��� ROLE_SWITCHING
 * 2. ֹͣ�㷨����ȡ dbbeats ����
 * 3. ͨ�� TWS ��·���Ϳ��յ��Բ�
 * 4. �رմ�����������Ϊ Slave
 */
void sndp_sleep_role_switch_trigger(Role_Switch_Reason_t reason);

/**
 * @brief �������յ��Ľ�ɫ�л����գ�Slave����ã�
 * @param snapshot �㷨��������ָ��
 * @param len      �������ݳ���
 * 
 * ִ�����̣�
 * 1. ������������
 * 2. ���� dbbeats_set_snapshot �ָ��㷨״̬
 * 3. ����������
 * 4. ���³�ʼ���㷨������
 * 5. ����Ϊ Master
 */
void sndp_sleep_role_switch_recv_snapshot(uint8_t *snapshot, uint16_t len);

/**
 * @brief ������ɫ�л�����������շ����ã�
 * ���յ��Բ෢���Ľ�ɫ�л�����ʱ����
 */
void sndp_sleep_role_switch_on_request(void);

/**
 * @brief �쳣�������Բ���������ʱ�ĳ�ͻ���
 * �����ǰ����Master���ܾ�ԭMaster���½ӹ�
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
