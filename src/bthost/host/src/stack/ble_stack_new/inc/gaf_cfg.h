
/***************************************************************************
 *
 * Copyright (c) 2015-2023 BES Technic
 *
 * Authored by BES CD team (Blueelf Prj).
 *
 * All rights reserved. All unpublished rights reserved.
 *
 * No part of this work may be used or reproduced in any form or by any
 * means, or stored in a database or retrieval system, without prior written
 * permission of BES.
 *
 * Use of this work is governed by a license granted by BES.
 * This work contains confidential and proprietary information of
 * BES. which is protected by copyright, trade secret,
 * trademark and other intellectual property rights.
 *
 ****************************************************************************/
#ifndef __GAF_CFG_INC__
#define __GAF_CFG_INC__

#include "bt_common_define.h"
#include "bt_stack_status.h"
#include "co_bt.h"
#include "co_timer.h"
#include "cobuf.h"

/// GAF layer reserved for btgatt use
#define GAF_BTGATT_CON_MAX              (1)
/// GAF layer total connection for env
#define GAF_CONNECTION_MAX              (gaf_prf_get_max_supp_conn_num() + GAF_BTGATT_CON_MAX)

#define GAF_USE_CACHE_GATT_CCCD         (1)

#define GAF_CONTROLLER_DELAY_US         (0x102)

#define GAF_RX_NTF_IND_TIMEOUT_MS       (1000)

#define GAF_SUPP_MAX_CIS_NUM            (BAP_MAX_CIS_PER_CIG)

#define GAF_SUPP_MAX_BIG_NUM            (BAP_TOTAL_BIG_COUNT)

#if (DEBUG)
#define GAF_MODULE_GEN_AUD              "GEN_AUD"

#define GAF_MODULE_BAP_ISO_AL           "BAP_ISO_AL"

#define GAF_MODULE_BAP_UC_CLI           "BAP_UC_CLI"
#define GAF_MODULE_BAP_UC_SRV           "BAP_UC_SRV"

#define GAF_MODULE_BAP_BC_SINK          "BAP_BC_SINK"
#define GAF_MODULE_BAP_BC_SRC           "BAP_BC_SRC"

#define GAF_MODULE_BAP_SCAN_DELEG       "BAP_SCAN_DELEG"
#define GAF_MODULE_BAP_BC_ASSIST        "BAP_BC_ASSIST"

#define GAF_MODULE_ASCC                 "ASCC"
#define GAF_MODULE_ASCS                 "ASCS"
#define GAF_MODULE_PACC                 "PACC"
#define GAF_MODULE_PACS                 "PACS"
#define GAF_MODULE_BASS                 "BASS"

#define GAF_MODULE_MCC                  "MCC"
#define GAF_MODULE_MCS                  "MCS"
#define GAF_MODULE_TBC                  "TBC"
#define GAF_MODULE_TBS                  "TBS"
#define GAF_MODULE_VCC                  "VCC"
#define GAF_MODULE_VCS                  "VCS"
#define GAF_MODULE_MICC                 "MICC"
#define GAF_MODULE_MICS                 "MICS"
#define GAF_MODULE_AICC                 "AICC"
#define GAF_MODULE_AICS                 "AICS"
#define GAF_MODULE_VOCC                 "VOCC"
#define GAF_MODULE_VOCS                 "VOCS"
#define GAF_MODULE_CSI_CRYPTO           "CSI_CRYPTO"
#define GAF_MODULE_CSI_COORD            "CSI_COORD"
#define GAF_MODULE_CSI_MEMBER           "CSI_MEMBER"
#define GAF_MODULE_CSIS                 "CSI_SRV"
#define GAF_MODULE_CSISC                "CSI_CLI"
#define GAF_MODULE_CAC                  "CAC"
#define GAF_MODULE_CAS                  "CAS"
#define GAF_MODULE_TMAC                 "TMAC"
#define GAF_MODULE_TMAS                 "TMAS"
#define GAF_MODULE_HAC                  "HAC"
#define GAF_MODULE_HAS                  "HAS"
#define GAF_MODULE_OTC                  "OTC"
#define GAF_MODULE_OTS                  "OTS"
#define GAF_MODULE_GMAS                 "GMAS"
#define GAF_MODULE_GMAC                 "GMAC"
#define GAF_MODULE_VAC                  "VAC"
#define GAF_MODULE_VAS                  "VAS"
#define GAF_MODULE_SHS                  "SHS"
#endif

#if (BLE_AUDIO_ENABLED) || defined(BUILD_BTH_ROM)
/// CAP
#define CAP_CAS_ENABLE                  (1)
#define CAP_CAC_ENABLE                  (1)
/// ATC
#define ATC_CSISM_ENABLE                (1)
#define ATC_CSISC_ENABLE                (1)
/// BAP
#define BAP_ASCS_ENABLE                 (1)
#define BAP_ASCC_ENABLE                 (1)
#define BAP_PACS_ENABLE                 (1)
#define BAP_PACC_ENABLE                 (1)
#define BAP_SCAN_DELEGATOR              (1)
#define BAP_BROADCAST_SINK              (1)
#define BAP_BROADCAST_SRC               (1)
#define BAP_BROADCAST_ASSIST            (1)
#define BAP_BASS_ENABLE                 (BAP_SCAN_DELEGATOR)
#define BAP_BROADCAST_SCAN              (BAP_BROADCAST_SINK || BAP_SCAN_DELEGATOR || BAP_BROADCAST_ASSIST)
/// MCP
#define ACC_MCC_ENABLE                  (1)
#define ACC_MCS_ENABLE                  (1)
/// CCP
#define ACC_TBC_ENABLE                  (1)
#define ACC_TBS_ENABLE                  (1)
/// VCP
#define ARC_VCS_ENABLE                  (1)
#define ARC_VCC_ENABLE                  (1)
/// MICP
#define ARC_MICS_ENABLE                 (1)
#define ARC_MICC_ENABLE                 (1)
/// AICP
#define ARC_AICS_ENABLE                 (0)
#define ARC_AICC_ENABLE                 (1)
/// VOCP
#define ARC_VOCS_ENABLE                 (0)
#define ARC_VOCC_ENABLE                 (1)
/// TMAP
#define TMAP_TMAS_ENABLE                (1)
#define TMAP_TMAC_ENABLE                (1)
/// HAP
#define HAP_HAS_ENABLE                  (1)
#define HAP_HAC_ENABLE                  (1)
/// OTP
#define ACC_OTS_ENABLE                  (0)
#define ACC_OTC_ENABLE                  (0)
/// GMAP
#define GMAP_GMAS_ENABLE                (1)
#define GMAP_GMAC_ENABLE                (1)
/// SHP
#define SHP_SHS_ENABLE                  (SHP_LE_SUPPORT)
#define SHP_SHC_ENABLE                  (SHP_LE_SUPPORT)
/// VAP
#define VAP_VAS_ENABLE                  (1)
#define VAP_VAC_ENABLE                  (1)

#else /// BLE_AUDIO_ENABLED
/// CAP
#define CAP_CAS_ENABLE                  (0)
#define CAP_CAC_ENABLE                  (0)
/// ATC
#define ATC_CSISM_ENABLE                (0)
#define ATC_CSISC_ENABLE                (0)
/// BAP
#define BAP_ASCS_ENABLE                 (0)
#define BAP_ASCC_ENABLE                 (0)
#define BAP_PACS_ENABLE                 (0)
#define BAP_PACC_ENABLE                 (0)
#define BAP_BASS_ENABLE                 (0)
#define BAP_SCAN_DELEGATOR              (0)
#define BAP_BROADCAST_SINK              (0)
#define BAP_BROADCAST_SCAN              (0)
#define BAP_BROADCAST_SRC               (0)
#define BAP_BROADCAST_ASSIST            (0)
/// MCP
#define ACC_MCC_ENABLE                  (0)
#define ACC_MCS_ENABLE                  (0)
/// CCP
#define ACC_TBC_ENABLE                  (0)
#define ACC_TBS_ENABLE                  (0)
/// VCP
#define ARC_VCS_ENABLE                  (0)
#define ARC_VCC_ENABLE                  (0)
#define ARC_MICS_ENABLE                 (0)
#define ARC_MICC_ENABLE                 (0)
#define ARC_AICS_ENABLE                 (0)
#define ARC_AICC_ENABLE                 (0)
#define ARC_VOCS_ENABLE                 (0)
#define ARC_VOCC_ENABLE                 (0)
/// TMAP
#define TMAP_TMAS_ENABLE                (0)
#define TMAP_TMAC_ENABLE                (0)
/// HAP
#define HAP_HAS_ENABLE                  (0)
#define HAP_HAC_ENABLE                  (0)
/// OTP
#define ACC_OTS_ENABLE                  (0)
#define ACC_OTC_ENABLE                  (0)
/// GMAP
#define GMAP_GMAS_ENABLE                (0)
#define GMAP_GMAC_ENABLE                (0)
/// SHP
#define SHP_SHS_ENABLE                  (0)
#define SHP_SHC_ENABLE                  (0)
/// VAP
#define VAP_VAS_ENABLE                  (0)
#define VAP_VAC_ENABLE                  (0)

#endif /// BLE_AUDIO_ENABLED

/// CODE SPECIFIC DEFINE
#if defined(BUILD_BTH_ROM)
#undef  __STATIC
#define __STATIC                        // Nothing
#undef  __INLINE
#define __INLINE                        // Nothing
#else
#define __STATIC                        static
#define __INLINE                        inline
#endif /// (BUILD_BTH_ROM)

#define __CONST                         const

#define GAF_INVALID_VAL_U8              (0xFF)
#define GAF_INVALID_VAL_U16             (0xFFFF)
#define GAF_INVALID_VAL_U32             (0xFFFFFFFF)

#define GAF_INVALID_PRF_LID             (GAF_INVALID_VAL_U8)
#define GAF_INVALID_CON_LID             (GAF_INVALID_VAL_U8)
#define GAF_INVALID_ANY_LID             (GAF_INVALID_VAL_U8)
#define GAF_INVALID_CON_HDL             (GAF_INVALID_VAL_U16)

#if __STDC_VERSION__ >= 199901L
#define GAF_ARRAY_EMPTY
#else
#define GAF_ARRAY_EMPTY                 (0)
#endif

#define GAF_PREFERRED_MTU               (512)

#define GAF_COBUF_DBG                   (0)

#if (GAF_COBUF_DBG)

/// GAF MEM
POSSIBLY_UNUSED static uint8_t *gaf_mem_dbg_buf = NULL;

#define GAF_COBUF_MALLOC(size) \
        (gaf_mem_dbg_buf = cobuf_malloc(size)); \
        do { \
            GAF_LOG_D("[MALLOC]: %p, %d", gaf_mem_dbg_buf, size); \
        } while (0)

#define GAF_COBUF_FREE(buf)  \
        cobuf_free(buf); \
        do { \
            GAF_LOG_D("[FREE]: %p", buf); \
        } while (0)
#else

#define GAF_COBUF_MALLOC(size)          cobuf_malloc(size)

#define GAF_COBUF_FREE(buf)             cobuf_free(buf)

#endif /* GAF_COBUF_DBG */

/// GAF TIMER
typedef void (*timer_func)(void *);

#define GAF_TIMER_INVALID_ID            (0)
#define GAF_TIMER_UNCFG_ID              (0xFF)
#define GAF_TIMER_T                     uint8_t

#define GAF_TIMER_ONCE
#define GAF_TIMER_PERIODIC

#define GAF_TIMER_DEF(name, function)
#define GAF_TIMER(name)

#define GAF_TIMER_DBG                   (0)

#if (GAF_TIMER_DBG)

#define GAF_TIMER_CREATE(timer_def, type, argument, p_timer_id)\
            (GAF_LOG_D("[T-CREATE]%p", p_timer_id) - 0) ? GAF_TIMER_UNCFG_ID : GAF_TIMER_UNCFG_ID

#define GAF_TIMER_CONFIGURE(p_timer_id, to_ms, func, arg, times)\
    do { \
        if ((p_timer_id) != NULL && *(p_timer_id) == GAF_TIMER_UNCFG_ID)\
            co_timer_new((p_timer_id), (to_ms), (timer_func)(func), (arg), (times));\
        GAF_LOG_D("[T-CFG]%p, %d, %d", p_timer_id, *(p_timer_id), to_ms); \
    } while (0)

#define GAF_TIMER_START(timer_id, ticks)\
    do { \
        GAF_LOG_D("[T-START]%p, %d", &(timer_id), timer_id); \
        if (timer_id != GAF_TIMER_INVALID_ID && timer_id != GAF_TIMER_UNCFG_ID) \
            co_timer_start(&(timer_id)); \
    } while (0)

#define GAF_TIMER_STOP(timer_id) \
    do { \
        GAF_LOG_D("[T-STOP]%p, %d", &(timer_id), timer_id); \
        if (timer_id != GAF_TIMER_INVALID_ID && timer_id != GAF_TIMER_UNCFG_ID) \
            co_timer_stop(&(timer_id)); \
    } while (0)

#define GAF_TIMER_DELETE(timer_id) \
    do { \
        GAF_LOG_D("[T-DEL]%p, %d", &(timer_id), timer_id); \
        if (timer_id != GAF_TIMER_INVALID_ID && timer_id != GAF_TIMER_UNCFG_ID) \
            co_timer_del(&(timer_id)); \
    } while (0)

#else

#define GAF_TIMER_CREATE(timer_def, type, argument, p_timer_id) (GAF_TIMER_UNCFG_ID)

#define GAF_TIMER_CONFIGURE(p_timer_id, to_ms, func, arg, times)\
    do { \
        if ((p_timer_id) != NULL && *(p_timer_id) == GAF_TIMER_UNCFG_ID)\
            co_timer_new((p_timer_id), (to_ms), (timer_func)(func), (arg), (times));\
    } while (0)

#define GAF_TIMER_START(timer_id, ticks)\
    do { \
        if (timer_id != GAF_TIMER_INVALID_ID && timer_id != GAF_TIMER_UNCFG_ID) \
            co_timer_start(&(timer_id)); \
    } while (0)

#define GAF_TIMER_STOP(timer_id) \
    do { \
        if (timer_id != GAF_TIMER_INVALID_ID && timer_id != GAF_TIMER_UNCFG_ID) \
            co_timer_stop(&(timer_id)); \
    } while (0)

#define GAF_TIMER_DELETE(timer_id) \
    do { \
        if (timer_id != GAF_TIMER_INVALID_ID && timer_id != GAF_TIMER_UNCFG_ID) \
            co_timer_del(&(timer_id)); \
    } while (0)

#endif /* GAF_TIMER_DBG */

/// GAF/GAP CONNECTION INDEX
POSSIBLY_UNUSED static uint8_t gaf_con_lid = 0;

#define GAP_2_GAF_CON_LID(conidx) \
        (gaf_con_lid = gaf_prf_get_con_lid_by_gap_conidx(conidx)); \
        do { \
            if (gaf_con_lid >= GAF_CONNECTION_MAX) {\
                GAF_LOG_E("get directly fail con_idx: %x", conidx); \
                return 0; \
            } \
        } while (0)

#define GAF_2_GAP_CON_IDX(con_lid)      gaf_prf_get_gap_conidx_by_con_lid(con_lid)

#endif /// __GAF_CFG_INC__
