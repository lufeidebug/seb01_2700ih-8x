
#ifndef __SPDRV_DA217E_TAP_ALGO_H__
#define __SPDRV_DA217E_TAP_ALGO_H__
#if defined(__SNDP_GSENSOR_DA217E__)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/******************************************************************************
 * DA217E 敲击（双击/三击）软件检测器。
 *
 * 流式输入，一次一个样本，纯整数运算。前端为去除重力后的 Z 轴：
 * 耳机敲击的法向是 Z。X/Y 仅用于否决明显是转头或侧摆的脉冲，不参与敲击判定。
 *
 *   Z 轴高通 -> 迟滞脉冲捕获 -> 相对形状门限
 *   -> 不应期 -> 噪声底/个人幅度门
 *   -> 分组：双击/三击统一延迟到等待窗口关闭后结算上报
 *   窗口内出现第四击则整组作废（连击超过三下不对应任何功能，三击随之撤销）。
 ******************************************************************************/

/* 加速度量程：±4G，14bit 原始值（驱动把 16bit 字右移 2 位） */
#define TAP_LSB_PER_G                   (2048)

typedef enum {
    TAP_SENS_LOW = 0,       /* 0.45g 峰值门限：误触最少 */
    TAP_SENS_MEDIUM = 1,    /* 0.35g 峰值门限：出厂默认 */
    TAP_SENS_HIGH = 2,      /* 0.25g 峰值门限：适合轻敲用户 */
} tap_sensitivity_e;

/* 一次有效的敲击手势：cnt 恒为 2 或 3 */
typedef void (*tap_algo_event_cb_t)(uint8_t cnt);
/* 调试钩子，可为 NULL */
typedef void (*tap_algo_dbg_cb_t)(const char *tag, int32_t v1, int32_t v2);

void tap_algo_init(tap_algo_event_cb_t event_cb, tap_algo_dbg_cb_t dbg_cb);

/* 使能/关闭整个检测器（复位状态机） */
void tap_algo_enable(bool enable);
bool tap_algo_is_enabled(void);

/* 喂入一个加速度样本，±4G 14bit 原始值，按 ODR(125Hz) 调用 */
void tap_algo_feed(int16_t x, int16_t y, int16_t z);

/* 冲刷未决分组（窗口关闭/摘下时调用）。已凑满 2 或 3 击则上报，否则静默丢弃。 */
void tap_algo_flush(void);

void tap_algo_set_sensitivity(tap_sensitivity_e sens);
tap_sensitivity_e tap_algo_get_sensitivity(void);

#ifdef __cplusplus
}
#endif

#endif  //__SNDP_GSENSOR_DA217E__
#endif  //__SPDRV_DA217E_TAP_ALGO_H__
