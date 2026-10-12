#if defined(__SNDP_GSENSOR_DA217E__)

#include "stdio.h"
#include "string.h"
#include "sndp_tap_algo.h"

/******************************************************************************
 * Constant
 ******************************************************************************/
/* 采样率，必须与 DA217E_REG_ODR_AXIS 配置一致（0x07 = 125Hz） */
#define TAP_FS_HZ                       (125)

/* 重力去除/噪声底 EMA 移位位数 */
#define TAP_BASE_SHIFT                  (6)     /* 125Hz 下等效约 0.3Hz 高通 */
#define TAP_NF_SHIFT                    (5)

/* 迟滞与冲击性门限，单位 LSB（±4G 下 2048 LSB = 1g）
 * 20261012 实测分布：成功第二击 >=580、被拒第二击最强 505、环境噪声最高 ~365；
 * 532 卡在成功/失败分界上（两次失败仅差 27/28），降到 440 救回 504/505 档，
 * 对噪声仍留 20% 裕量。 */
#define TAP_THR_ON                      (440)
#define TAP_THR_OFF                     (266)
/* 单样本斜率下限（用于捕获后的 rej_slope 校验）。左耳 20261012 实测正常敲击
 * 起点斜率 644~3632，慢起沿敲击 264 被 512 起检门盲杀（skip_slope 案例）；
 * 降到 384 并移出起检条件，慢动作仍由 rise/half/dur 形状校验拦截。 */
#define TAP_SLOPE_MIN                   (384)

/* 有效脉冲的形状限制，单位 ms */
#define TAP_RISE_MAX_MS                 (40)
#define TAP_FALL_MAX_MS                 (90)
#define TAP_HALF_MAX_MS                 (45)
#define TAP_DUR_MAX_MS                  (140)

/* 有效敲击后的不应期。外壳振铃出现在主峰后 90..140ms、幅度仅 0.2..0.3g，
 * 不应期短于 140ms 会把双击误判成三击。 */
#define TAP_REFR_MS                     (170)

/* 自适应个人门限：至少一次有效敲击后，只要新脉冲起点距上一次有效敲击
 * 不超过 TAP_ADAPT_WITHIN_MS，幅度门就降至 ema>>TAP_EMA_SHIFT（但不低于
 * TAP_THR_ON）。20261012 实测双击节奏 328~435ms，原 280ms 窗从未生效，
 * 弱第二击（483）死于固定 peak_min=512；放宽到 500ms 覆盖真实节奏，
 * 仍小于 650ms 组窗，孤立噪声享受不到个人折扣。 */
#define TAP_ADAPT_WITHIN_MS             (500)
#define TAP_EMA_SHIFT                   (2)   /* 个人门限 = ema / 4 */

/* 分组 */
#define TAP_GAP_MAX_MS                  (650)
/* 结算窗口与最大峰间隔一致，截止样本上的脉冲也允许先完成验证。
 * 三击同样等窗口关闭才上报：窗口内的第四击使整组作废。 */
#define TAP_CLOSE_MS                    (TAP_GAP_MAX_MS)

/* 单个脉冲最多捕获的样本数（也是脉冲时长的硬上限） */
#define TAP_PBUF_N                      (40)

/* 预热期：检测器启用/恢复后的前 64 个样本（约 512ms）只跟踪重力与噪声底，
 * 不做脉冲判定。用于隔离心率测量结束时 FIFO 里的残留样本，防止误敲击。 */
#define TAP_WARMUP_S                    (64)

/* 横向否决：转头/侧摆时 X/Y 的能量会超过敲击法向。
 * max(|dx|,|dy|) 超过 Z 峰值 2.0 倍则丢弃该脉冲。
 * 20261012 左耳实测：快速连击（208ms）真实第二击横向 1225/法向 763=1.61 倍
 * 被 1.5 倍门槛擦线拒绝（rej_lat），放宽到 2.0 倍；转头/侧摆仍远超此比值。 */
#define TAP_LAT_NUM                     (2)
#define TAP_LAT_DEN                     (1)

/* 组内幅度跳变门（第二道防线，首选防线是静默间隔门）：
 * 20261012 实测真实双击第一击轻、第二击重时比值可达 3.25 倍（1207→3926），
 * 3 倍门槛会误杀；回撞/磕碰类伪迹实测 4.5 倍（706→3189）。放宽到 4 倍。
 * 伪迹与主峰间通常是连续激励，先被 rej_quiet 拦截，本门兜底。 */
#define TAP_JUMP_NUM                    (4)
#define TAP_JUMP_DEN                    (1)

/* 组内静默间隔门：真实双击两击之间必有安静期（20261012 实测 ≥150ms，
 * 能量低于 TAP_THR_OFF），而回弹类伪迹与主峰之间是连续激励、回落不足
 * 20ms。组内新脉冲起检前若无至少 TAP_QUIET_MIN_MS 的连续静默，判为
 * 回声丢弃，与幅度跳变门互补（回撞大脉冲由跳变门管、连续激励回弹由
 * 本门管）。 */
#define TAP_QUIET_MIN_MS                (100)

/* 毫秒 -> 检测器样本数。模块内所有时间标记都以 ODR 样本为单位，
 * 保证离线原型与固件行为一致。 */
#define TAP_MS2S(ms)                    ((uint32_t)(((uint32_t)(ms) * TAP_FS_HZ) / 1000))

/******************************************************************************
 * Type
 ******************************************************************************/
typedef struct {
    /* 重力估计与上一次采样 */
    int32_t bx;
    int32_t by;
    int32_t bz;
    int32_t px;
    int32_t py;
    int32_t pz;

    uint8_t inited;
    uint8_t state;            /* 0 = 空闲，1 = 脉冲捕获中 */
    uint8_t dbg_candidate;    /* 同一段超过低阈值的波动只记录一次起检受阻。 */
    uint8_t warmup_left;      /* >0 为预热期：仅跟踪背景，不做判定 */

    /* 当前脉冲 */
    int32_t buf[TAP_PBUF_N];
    uint32_t len;
    uint32_t onset_s;         /* 进入捕获态时的样本序号（用于自适应门） */
    uint32_t peak_s;
    int32_t peak;
    int32_t slope_pk;
    int32_t lat_pk;           /* 捕获期间 max(|dx|,|dy|) 的峰值 */

    /* 噪声底 */
    int32_t nf;

    /* 时间标记，样本单位，无符号回绕安全 */
    uint32_t now_s;
    uint32_t refr_until_s;
    uint32_t grp_close_at_s;

    /* 当前敲击组：只保留最后一击及其计数 */
    uint8_t ntaps;
    uint8_t gpending;         /* 有一组未决，等待结算 */
    uint32_t tap_last_s;
    int32_t tap_peak;

    /* 自适应个人门限：有效敲击峰值的 EMA（0 = 暂无数据） */
    int32_t tap_peak_ema;

    /* 静默间隔门：quieting=1 表示正处于连续静默段（样本能量均低于
     * TAP_THR_OFF），quiet_start_s 为该段起点，样本单位。
     * pulse_quiet_s 是起检瞬间快照的起检前静默时长：捕获期间 quieting
     * 已被清零，入组校验必须用快照，不能读实时标志。 */
    uint8_t quieting;
    uint32_t quiet_start_s;
    uint32_t pulse_quiet_s;

    /* 派生门限，样本单位 */
    uint16_t refr_s;
    uint16_t gap_max_s;
    uint16_t close_s;
    uint16_t rise_max_s;
    uint16_t fall_max_s;
    uint16_t half_max_s;
    uint16_t dur_max_s;
    uint16_t adapt_within_s;

    int32_t peak_min;
} tap_algo_ctx_t;

/******************************************************************************
 * Variable
 ******************************************************************************/
static tap_algo_ctx_t tap_ctx;
static tap_algo_event_cb_t tap_event_cb = NULL;
static tap_algo_dbg_cb_t tap_dbg_cb = NULL;
static bool tap_enabled = false;
static tap_sensitivity_e tap_sens = TAP_SENS_HIGH;

/* 0.45 / 0.35 / 0.25g，截断方式与 C 浮点强转一致 */
static const int32_t tap_peak_min_tbl[TAP_SENS_HIGH + 1] = {
    921,
    716,
    512,
};

/******************************************************************************
 * Function
 ******************************************************************************/
static void tap_dbg(const char *tag, int32_t v1, int32_t v2)
{
    if(tap_dbg_cb) {
        tap_dbg_cb(tag, v1, v2);
    }
}

/* 无符号回绕安全的时间比较 */
static bool tap_time_reached(uint32_t now, uint32_t target)
{
    return ((int32_t)(now - target)) >= 0;
}

static void tap_apply_sensitivity(void)
{
    tap_ctx.peak_min = tap_peak_min_tbl[tap_sens];
}

void tap_algo_init(tap_algo_event_cb_t event_cb, tap_algo_dbg_cb_t dbg_cb)
{
    tap_event_cb = event_cb;
    tap_dbg_cb = dbg_cb;

    memset(&tap_ctx, 0, sizeof(tap_ctx));

    /* 毫秒 -> 样本，按检测器自身采样率换算 */
    tap_ctx.refr_s          = (uint16_t)TAP_MS2S(TAP_REFR_MS);
    tap_ctx.gap_max_s       = (uint16_t)TAP_MS2S(TAP_GAP_MAX_MS);
    tap_ctx.close_s         = (uint16_t)TAP_MS2S(TAP_CLOSE_MS);
    tap_ctx.rise_max_s      = (uint16_t)TAP_MS2S(TAP_RISE_MAX_MS);
    tap_ctx.fall_max_s      = (uint16_t)TAP_MS2S(TAP_FALL_MAX_MS);
    tap_ctx.half_max_s      = (uint16_t)TAP_MS2S(TAP_HALF_MAX_MS);
    tap_ctx.dur_max_s       = (uint16_t)TAP_MS2S(TAP_DUR_MAX_MS);
    tap_ctx.adapt_within_s  = (uint16_t)TAP_MS2S(TAP_ADAPT_WITHIN_MS);

    tap_apply_sensitivity();
}

void tap_algo_enable(bool enable)
{
    if(enable == tap_enabled) {
        return;
    }

    tap_enabled = enable;

    /* 从干净状态重启，否则残留的重力估计或半截分组会泄漏到下一次手势 */
    tap_ctx.inited = 0;
    tap_ctx.state = 0;
    tap_ctx.dbg_candidate = 0;
    tap_ctx.warmup_left = enable ? (uint8_t)TAP_WARMUP_S : 0;
    tap_ctx.len = 0;
    tap_ctx.peak = 0;
    tap_ctx.slope_pk = 0;
    tap_ctx.lat_pk = 0;
    tap_ctx.ntaps = 0;
    tap_ctx.gpending = 0;
    tap_ctx.nf = 0;
    tap_ctx.tap_peak_ema = 0;   /* 摘下时清空个人历史 */
    tap_ctx.refr_until_s = tap_ctx.now_s;
    tap_ctx.quieting = 0;
    tap_ctx.quiet_start_s = tap_ctx.now_s;
}

bool tap_algo_is_enabled(void)
{
    return tap_enabled;
}

void tap_algo_set_sensitivity(tap_sensitivity_e sens)
{
    if(sens > TAP_SENS_HIGH) {
        return;
    }
    tap_sens = sens;
    tap_apply_sensitivity();
}

tap_sensitivity_e tap_algo_get_sensitivity(void)
{
    return tap_sens;
}

/* 结算当前组：双击等待窗口关闭，三击在第三个有效脉冲完成后立即上报。 */
static void tap_settle_group(void)
{
    if(!tap_ctx.gpending) {
        return;
    }

    tap_ctx.gpending = 0;

    if(tap_ctx.ntaps == 2 || tap_ctx.ntaps == 3) {
        tap_dbg("evt", tap_ctx.ntaps, (int32_t)tap_ctx.tap_last_s);
        if(tap_event_cb) {
            tap_event_cb(tap_ctx.ntaps);
        }
    } else if(tap_ctx.ntaps < 2) {
        /* 单击：本产品只定义双击和三击 */
        tap_dbg("solo", tap_ctx.ntaps, (int32_t)tap_ctx.tap_last_s);
    } else {
        /* 4 击及以上整组作废：第四击撤销三击 */
        tap_dbg("drop", tap_ctx.ntaps, (int32_t)tap_ctx.tap_last_s);
    }

    tap_ctx.ntaps = 0;
}

static void tap_check_group_timeout(void)
{
    /* 捕获中的脉冲先完成验证，再按峰值间隔决定是否属于当前组。 */
    if(tap_ctx.gpending && tap_ctx.state == 0 &&
       tap_time_reached(tap_ctx.now_s, tap_ctx.grp_close_at_s + 1u)) {
        tap_settle_group();
    }
}

static void tap_add_tap(uint32_t t_s, int32_t peak)
{
    if(tap_ctx.ntaps) {
        uint32_t gap = t_s - tap_ctx.tap_last_s;
        tap_dbg("gap", (int32_t)gap, tap_ctx.gap_max_s);
        if(gap > (uint32_t)tap_ctx.gap_max_s) {
            /* 上一组已超时失效，另起新组 */
            tap_settle_group();
        } else {
            /* 组内静默间隔门：本脉冲起检前的连续静默时长不足（或起检前
             * 一直处于激励中）判为回声，不计入、不刷新组闭合时间 */
            uint32_t quiet_len = tap_ctx.pulse_quiet_s;
            if(quiet_len < (uint32_t)TAP_MS2S(TAP_QUIET_MIN_MS)) {
                tap_dbg("rej_quiet", (int32_t)quiet_len, (int32_t)TAP_MS2S(TAP_QUIET_MIN_MS));
                return;
            }
            if(peak >= (tap_ctx.tap_peak * TAP_JUMP_NUM) / TAP_JUMP_DEN) {
                /* 组内幅度跳变门：伪迹（回撞/磕碰）比上一击重数倍，不计入，
                 * 不更新 EMA、不刷新组闭合时间。仅比较组内上一击。 */
                tap_dbg("rej_jump", peak, tap_ctx.tap_peak);
                return;
            }
        }
    }

    tap_ctx.gpending = 1;
    tap_ctx.tap_last_s = t_s;
    tap_ctx.tap_peak = peak;
    /* 更新个人 EMA：3/4 旧值 + 1/4 新值（整数运算，无需乘法） */
    if(tap_ctx.tap_peak_ema == 0) {
        tap_ctx.tap_peak_ema = peak;
    } else {
        tap_ctx.tap_peak_ema = (tap_ctx.tap_peak_ema - (tap_ctx.tap_peak_ema >> 2))
                              + (peak >> 2);
    }
    tap_ctx.ntaps++;
    tap_ctx.grp_close_at_s = t_s + tap_ctx.close_s;
    tap_dbg("tap", tap_ctx.ntaps, peak);
}

/* 单个捕获脉冲的形状校验。所有门限都相对脉冲自身峰值，
 * 0.3g 的轻敲和 3g 的重敲使用同一套标准。 */
static bool tap_validate_pulse(void)
{
    tap_algo_ctx_t *c = &tap_ctx;
    int32_t pk = 0;
    int32_t th20, th30, th50, nf_thr, need;
    uint32_t i, ip, i20, i30, n50;

    if(c->len == 0) {
        return false;
    }

    ip = 0;
    for(i = 0; i < c->len; i++) {
        if(c->buf[i] > pk) {
            pk = c->buf[i];
            ip = i;
        }
    }
    c->peak = pk;
    if(pk <= 0) {
        return false;
    }

    th20 = pk / 5;                     /* 峰值的 20% */
    th30 = (pk * 3) / 10;              /* 峰值的 30% */
    th50 = pk / 2;                     /* 峰值的一半 */

    /* 上升沿：冲击斜率；揉搓或倚靠会持续爬升更久 */
    i20 = ip;
    while(i20 > 0 && c->buf[i20] > th20) {
        i20--;
    }
    if((ip - i20) > c->rise_max_s) {
        tap_dbg("rej_rise", (int32_t)(ip - i20), c->rise_max_s);
        return false;
    }

    /* 下降沿：长尾巴是外壳晃动或布料摩擦，不是敲击 */
    i30 = ip;
    while(i30 + 1 < c->len && c->buf[i30] > th30) {
        i30++;
    }
    if((i30 - ip) > c->fall_max_s) {
        tap_dbg("rej_fall", (int32_t)(i30 - ip), c->fall_max_s);
        return false;
    }

    /* 半高宽：经典的冲击脉宽度量 */
    n50 = 0;
    for(i = 0; i < c->len; i++) {
        if(c->buf[i] > th50) {
            n50++;
        }
    }
    if(n50 > c->half_max_s) {
        tap_dbg("rej_half", (int32_t)n50, c->half_max_s);
        return false;
    }

    if(c->len > c->dur_max_s) {
        tap_dbg("rej_dur", (int32_t)c->len, c->dur_max_s);
        return false;
    }

    if(c->slope_pk < TAP_SLOPE_MIN) {
        tap_dbg("rej_slope", c->slope_pk, TAP_SLOPE_MIN);
        return false;
    }

    /* 转头/侧摆：横向动态加速度压过敲击法向 */
    if(c->lat_pk > (pk * TAP_LAT_NUM) / TAP_LAT_DEN) {
        tap_dbg("rej_lat", c->lat_pk, pk);
        return false;
    }

    /* 幅度门限：
     * 1. 噪声底：峰值须 >= 3 倍 nf
     * 2. 配置的 peak_min：灵敏度档位
     * 3. 自适应个人门限，仅当新脉冲起点距上次有效敲击不超过
     *    TAP_ADAPT_WITHIN_MS 时生效，且不低于 THR_ON。孤立噪声仍按 peak_min 判定。 */
    nf_thr = c->nf + (c->nf << 1);            /* 3 倍噪声底 */
    need = c->peak_min;
    if(c->tap_peak_ema > 0) {
        uint32_t since_last = c->onset_s - c->tap_last_s;
        if(since_last <= (uint32_t)c->adapt_within_s) {
            int32_t personal = c->tap_peak_ema >> TAP_EMA_SHIFT;
            if(personal < TAP_THR_ON) {
                personal = TAP_THR_ON;
            }
            if(personal < need) {
                need = personal;
            }
        }
    }
    if(nf_thr > need) {
        need = nf_thr;
    }
    if(pk < need) {
        tap_dbg("rej_amp", pk, need);
        return false;
    }

    return true;
}

void tap_algo_feed(int16_t x, int16_t y, int16_t z)
{
    tap_algo_ctx_t *c = &tap_ctx;
    int32_t dx, dy, dz, e, sl, lat, lat_y;

    if(!tap_enabled) {
        return;
    }

    c->now_s++;

    if(!c->inited) {
        c->bx = x; c->by = y; c->bz = z;
        c->px = x; c->py = y; c->pz = z;
        c->inited = 1;
        return;
    }

    /* 重力与缓慢姿态去除。本耳机敲击法向为 Z，
     * 检测能量与单样本斜率都只取自 Z 轴。 */
    c->bx += (x - c->bx) >> TAP_BASE_SHIFT;
    c->by += (y - c->by) >> TAP_BASE_SHIFT;
    c->bz += (z - c->bz) >> TAP_BASE_SHIFT;
    dx = x - c->bx;
    dy = y - c->by;
    dz = z - c->bz;
    e = (dz < 0) ? -dz : dz;

    sl = z - c->pz;
    if(sl < 0) {
        sl = -sl;
    }
    c->px = x; c->py = y; c->pz = z;

    lat = (dx < 0) ? -dx : dx;
    lat_y = (dy < 0) ? -dy : dy;
    if(lat_y > lat) {
        lat = lat_y;
    }

    /* 预热期：重力/噪声底继续收敛，但不做任何判定 */
    if(c->warmup_left > 0) {
        c->warmup_left--;
        if(c->warmup_left == 0) {
            tap_dbg("warm", (int32_t)TAP_WARMUP_S, c->nf);
        }
        if(e < TAP_THR_OFF) {
            c->nf += (e - c->nf) >> TAP_NF_SHIFT;
            if(c->nf < 0) {
                c->nf = 0;
            }
        }
        return;
    }

    tap_check_group_timeout();

    if(c->state == 0) {
        /* 静默快照必须先于本样本的标志更新：起检样本自身能量超阈值，
         * 下面的 else 分支会先清 quieting，若清完再快照将永远读到 0
         * （包括久置后的第一击）。快照的是上一样本为止的静默段。 */
        uint32_t quiet_now = c->quieting ? (c->now_s - c->quiet_start_s) : 0;

        /* 仅在静止时跟踪背景噪声；同时记录连续静默段（能量低于 THR_OFF）
         * 的起点，供组内静默间隔门判回声使用。266~440 的爬升带不算打破
         * 静默——慢攻击的真实第二击会先经过爬升带（实测 <266→300→760），
         * 若爬升带清标志，快照会被误清成 0。只有超起检门限的真激励
         * （含不应期内的 skip_refr 样本）才结束静默段。 */
        if(e < TAP_THR_OFF) {
            if(!c->quieting) {
                c->quieting = 1;
                c->quiet_start_s = c->now_s;
            }
            c->dbg_candidate = 0;
            c->nf += (e - c->nf) >> TAP_NF_SHIFT;
            if(c->nf < 0) {
                c->nf = 0;
            }
        } else if(e >= TAP_THR_ON) {
            c->quieting = 0;
        }

        /* 起检只看能量：起点单样本斜率属过早盲判（慢起沿但峰值合法的敲击
         * 会被拦截，slope_pk 无从测得），斜率下限由脉冲捕获后的 rej_slope 把关。 */
        if(tap_time_reached(c->now_s, c->refr_until_s) &&
           e >= TAP_THR_ON) {
            c->state = 1;
            c->pulse_quiet_s = quiet_now;
            /* 诊断：每个起检打印起检前静默快照，定位完成后移除。 */
            tap_dbg("onq", (int32_t)quiet_now, 0);
            c->peak = e;
            c->slope_pk = sl;
            c->lat_pk = lat;
            c->buf[0] = e;
            c->len = 1;
            c->onset_s = c->now_s;
            c->peak_s = c->now_s;
            tap_dbg("start", e, sl);
        } else if(e >= TAP_THR_OFF && !c->dbg_candidate) {
            c->dbg_candidate = 1;
            /* 记录候选波动被阻止的首个原因，不逐样本刷日志。 */
            if(!tap_time_reached(c->now_s, c->refr_until_s)) {
                tap_dbg("skip_refr", (int32_t)(c->refr_until_s - c->now_s), e);
            } else if(e < TAP_THR_ON) {
                tap_dbg("skip_on", e, TAP_THR_ON);
            }
        }
        return;
    }

    /* 脉冲捕获中 */
    if(c->len < TAP_PBUF_N) {
        c->buf[c->len++] = e;
    }
    if(e > c->peak) {
        c->peak = e;
        c->peak_s = c->now_s;
    }
    if(sl > c->slope_pk) {
        c->slope_pk = sl;
    }
    if(lat > c->lat_pk) {
        c->lat_pk = lat;
    }

    if(e < TAP_THR_OFF || c->len >= TAP_PBUF_N) {
        tap_dbg("pulse", c->peak, (int32_t)c->len);
        if(tap_validate_pulse()) {
            /* 冲击时刻取峰值时刻，而不是衰减结束时刻 */
            tap_add_tap(c->peak_s, c->peak);
            c->refr_until_s = c->peak_s + c->refr_s;
        }
        c->state = 0;
        c->len = 0;
    }
}

void tap_algo_flush(void)
{
    tap_settle_group();
}

#endif  //__SNDP_GSENSOR_DA217E__
