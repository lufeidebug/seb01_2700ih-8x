
export SNDP_PROJ := 1
ifeq ($(SNDP_PROJ),1)

KBUILD_CPPFLAGS += -D__SNDP_PROJ__

export HARDWARE_VERSION ?= 1.0
export SOFTWARE_VERSION ?= 0.0.0.22

# platform macro configuration
export TRACE_BAUD_RATE 							:= 1152000
export IGNORE_POWER_ON_KEY_DURING_BOOT_UP		:= 1

export I2C2_IOMUX_INDEX                         :=34
export I2C3_IOMUX_INDEX                         :=36

export FLASH_SIZE_4M		:= 0
export FLASH_SIZE_8M		:= 1
export FLASH_SIZE_16M		:= 0
export FLASH_SIZE_32M		:= 0

ifeq ($(FLASH_SIZE_4M),1)
export FLASH_SIZE := 0x400000
export SNDP_PARAM_SECTION_SIZE			:= 0x2000
export SNDP_KEY_SECTION_SIZE            := 0x1000
KBUILD_CPPFLAGS += -DSNDP_PARAM_SECTION_SIZE=$(SNDP_PARAM_SECTION_SIZE)
KBUILD_CPPFLAGS += -DSNDP_KEY_SECTION_SIZE=$(SNDP_KEY_SECTION_SIZE)
endif

ifeq ($(FLASH_SIZE_8M),1)
export FLASH_SIZE := 0x800000
export SNDP_PARAM_SECTION_SIZE			:= 0x2000
export SNDP_KEY_SECTION_SIZE            := 0x1000
KBUILD_CPPFLAGS += -DSNDP_PARAM_SECTION_SIZE=$(SNDP_PARAM_SECTION_SIZE)
KBUILD_CPPFLAGS += -DSNDP_KEY_SECTION_SIZE=$(SNDP_KEY_SECTION_SIZE)
endif

ifeq ($(FLASH_SIZE_16M),1)
export FLASH_SIZE := 0x800000
export SNDP_PARAM_SECTION_SIZE			:= 0x2000
export SNDP_KEY_SECTION_SIZE            := 0x1000
KBUILD_CPPFLAGS += -DSNDP_PARAM_SECTION_SIZE=$(SNDP_PARAM_SECTION_SIZE)
KBUILD_CPPFLAGS += -DSNDP_KEY_SECTION_SIZE=$(SNDP_KEY_SECTION_SIZE)
endif

ifeq ($(FLASH_SIZE_32M),1)
export FLASH_SIZE := 0x800000
export SNDP_PARAM_SECTION_SIZE			:= 0x2000
export SNDP_KEY_SECTION_SIZE            := 0x1000
KBUILD_CPPFLAGS += -DSNDP_PARAM_SECTION_SIZE=$(SNDP_PARAM_SECTION_SIZE)
KBUILD_CPPFLAGS += -DSNDP_KEY_SECTION_SIZE=$(SNDP_KEY_SECTION_SIZE)
endif



#-----------------------------------------------------------------------------
# sndp macro configuration
export SNDP_UI                                  := 1
export SNDP_DEV_THREAD                          := 1
export SNDP_BUS_MGR                             := 1
export SNDP_PMU_MGR                             := 1
export SNDP_CHARGER_PLUG_MGR                    := 1
export SNDP_BATTERY_MGR                         := 1
export SNDP_CHARGER_MGR                         := 1
export SNDP_TEMPERATURE_MGR                     := 1
export SNDP_COVER_SWITCH_MGR                    := 0
export SNDP_IOBOX_MGR                           := 1
export SNDP_WEAR_DETECT_MGR                     := 1
export SNDP_GESTURE_MGR                         := 1
export SNDP_COMMUNICATION_MGR                   := 1
export SNDP_PRODUCT_TEST                        := 1
export SNDP_ALGO_MGR                            := 1
export SNDP_MIC_GAIN_SETTING                    := 1
export SNDP_REBOOT_FORCE_PAIRING                := 1
export SNDP_AUDIO_TEST                          := 0
export SNDP_BT_NAME                             := 1
export SNDP_TEST_TWS_PAIRING                    := 0
export SNDP_TEST_FREEMAN_PAIRING                := 0
export SNDP_TEST_RF_DUT                         := 0
export SNDP_CRASH_REBOOT                        := 1
export SNDP_BLE_MODIFY                          := 0
export SNDP_HEART_RATE_MGR                      := 1
export SNDP_KEY_TEST                            := 1
export SNDP_APP_WHITE_NOISE                     := 1
export SNDP_SLEEP_APP                           := 1
export SNDP_BAT_SWITCH_ROLE                     := 1
export SNDP_PROMPT_TEST                         := 1
export SNDP_ANC_MODIFY                          := 1


ifeq ($(SNDP_UI),1)
    KBUILD_CPPFLAGS += -D__SNDP_UI__
endif

ifeq ($(SNDP_DEV_THREAD),1)
    KBUILD_CPPFLAGS += -D__SNDP_DEV_THREAD__
endif

ifeq ($(SNDP_BUS_MGR),1)
    KBUILD_CPPFLAGS += -D__SNDP_BUS_MGR__

    export SNDP_BUS_HW_I2C                      := 1
    export SNDP_BUS_SW_I2C                      := 0

    ifeq ($(SNDP_BUS_HW_I2C),1)
        KBUILD_CPPFLAGS += -D__SNDP_BUS_HW_I2C__
    endif

    ifeq ($(SNDP_BUS_SW_I2C),1)
        KBUILD_CPPFLAGS += -D__SNDP_BUS_SW_I2C__
    endif
endif

ifeq ($(SNDP_PMU_MGR),1)
    KBUILD_CPPFLAGS += -D__SNDP_PMU_MGR__
endif

ifeq ($(SNDP_CHARGER_PLUG_MGR),1)
    KBUILD_CPPFLAGS += -D__SNDP_CHARGER_PLUG_MGR__

    export SNDP_CHARGER_PLUG_PMU_INT            := 1

    ifeq ($(SNDP_CHARGER_PLUG_PMU_INT),1)
        KBUILD_CPPFLAGS += -D__SNDP_CHARGER_PLUG_PMU_INT__
    endif
endif

ifeq ($(SNDP_BATTERY_MGR),1)
    KBUILD_CPPFLAGS += -D__SNDP_BATTERY_MGR__
    export SNDP_BAT_LOOKUP_TABLE                := 0
    export SNDP_BAT_CUMULATIVE                  := 1

    ifeq ($(SNDP_BAT_LOOKUP_TABLE),1)
        KBUILD_CPPFLAGS += -D__SNDP_BAT_LOOKUP_TABLE__
    endif

    ifeq ($(SNDP_BAT_CUMULATIVE),1)
        KBUILD_CPPFLAGS += -D__SNDP_BAT_CUMULATIVE__
    endif

endif

ifeq ($(SNDP_CHARGER_MGR),1)
    KBUILD_CPPFLAGS += -D__SNDP_CHARGER_MGR__
    export SNDP_CHARGER_HP4554                  := 0
    export SNDP_CHARGER_BES                     := 1

    ifeq ($(SNDP_CHARGER_HP4554),1)
        KBUILD_CPPFLAGS += -D__SNDP_CHARGER_HP4554__
    endif

    ifeq ($(SNDP_CHARGER_BES),1)
        KBUILD_CPPFLAGS += -D__SNDP_CHARGER_BES__
    endif

endif

ifeq ($(SNDP_TEMPERATURE_MGR),1)

    KBUILD_CPPFLAGS += -D__SNDP_TEMPERATURE_MGR__
    export SNDP_TEMPERATURE_NTC 		        := 1

    ifeq ($(SNDP_TEMPERATURE_NTC),1)
        KBUILD_CPPFLAGS += -D__SNDP_TEMPERATURE_NTC__
    endif
endif

ifeq ($(SNDP_COVER_SWITCH_MGR),1)
    KBUILD_CPPFLAGS += -D__SNDP_COVER_SWITCH_MGR__

    export SNDP_COVER_SWITCH_HALL               := 0
    export SNDP_COVER_SWITCH_BOX_NOTIFY         := 1

    ifeq ($(SNDP_COVER_SWITCH_HALL),1)
        KBUILD_CPPFLAGS += -D__SNDP_COVER_SWITCH_HALL__
    endif

    ifeq ($(SNDP_COVER_SWITCH_BOX_NOTIFY),1)
        KBUILD_CPPFLAGS += -D__SNDP_COVER_SWITCH_BOX_NOTIFY__
    endif

endif

ifeq ($(SNDP_IOBOX_MGR),1)
    KBUILD_CPPFLAGS += -D__SNDP_IOBOX_MGR__
    export SNDP_IOBOX_PMU_INT                   := 0
    export SNDP_IOBOX_GPIO_INT                  := 1

    ifeq ($(SNDP_IOBOX_PMU_INT),1)
        KBUILD_CPPFLAGS += -D__SNDP_IOBOX_PMU_INT__
    endif

    ifeq ($(SNDP_IOBOX_GPIO_INT),1)
        KBUILD_CPPFLAGS += -D__SNDP_IOBOX_GPIO_INT__
    endif
endif

ifeq ($(SNDP_WEAR_DETECT_MGR),1)
    KBUILD_CPPFLAGS += -D__SNDP_WEAR_DETECT_MGR__
    export SNDP_WEAR_DETECT_HRSENSOR            := 1

    ifeq ($(SNDP_WEAR_DETECT_HRSENSOR),1)
        export SNDP_HRSENSOR_SUPPORT                := 1
    endif
endif

ifeq ($(SNDP_GESTURE_MGR),1)
    KBUILD_CPPFLAGS += -D__SNDP_GESTURE_MGR__
    export SNDP_GESTURE_KEY                     := 0
    export SNDP_GESTURE_GSENSOR                 := 1

    ifeq ($(SNDP_GESTURE_KEY),1)
        KBUILD_CPPFLAGS += -D__SNDP_GESTURE_KEY__
    endif

    ifeq ($(SNDP_GESTURE_GSENSOR),1)
        export SNDP_GSENSOR_SUPPORT             := 1
    endif
endif

ifeq ($(SNDP_GSENSOR_SUPPORT),1)
    KBUILD_CPPFLAGS += -D__SNDP_GSENSOR_SUPPORT__
    export SNDP_GSENSOR_DA217E                  := 1

    ifeq ($(SNDP_GSENSOR_DA217E),1)
        KBUILD_CPPFLAGS += -D__SNDP_GSENSOR_DA217E__
    endif
endif

ifeq ($(SNDP_HRSENSOR_SUPPORT),1)
    KBUILD_CPPFLAGS += -D__SNDP_HRSENSOR_SUPPORT__
    export SNDP_HRSENSOR_SSH202C                := 0

    ifeq ($(SNDP_HRSENSOR_SSH202C),1)
        KBUILD_CPPFLAGS += -D__SNDP_HRSENSOR_SSH202C__
    endif
endif


ifeq ($(SNDP_COMMUNICATION_MGR),1)
    KBUILD_CPPFLAGS += -D__SNDP_COMM_MGR__

    export SNDP_COMM_TRACE_UART                 := 1
    export SNDP_COMM_POGOPIN                    := 1
    export SNDP_COMM_BLE                        := 1
    export SNDP_COMM_SPP                        := 1
    export SNDP_COMM_MS                         := 1
endif

ifeq ($(SNDP_COMM_TRACE_UART),1)
    KBUILD_CPPFLAGS += -D__SNDP_COMM_TRACE_UART__
    KBUILD_CPPFLAGS += -D__SNDP_LOG_OUTPUT_SWITCH__
    # FT (production test) F-MIC -> SPK loopback
    # 复用 BES 自带 app_factorymode_audioloop（通过 app_audio_sendrequest），
    # 参数覆盖：io_path=SNDP_FT_MIC_LOOPBACK, channel=1, rate=16k
    KBUILD_CPPFLAGS += -D__SNDP_FT_MIC_LOOPBACK__

endif

ifeq ($(SNDP_COMM_POGOPIN),1)
    KBUILD_CPPFLAGS += -D__SNDP_COMM_POGOPIN__

    export SNDP_POGOPIN_SINGLE_UART             := 0
    export SNDP_POGOPIN_DUAL_UART               := 1
    export SNDP_POGOPIN_COMM_PULSE              := 0

    export SNDP_POGOPIN_FW_UPGRADE              := 0

    ifeq ($(SNDP_POGOPIN_SINGLE_UART),1)
        KBUILD_CPPFLAGS += -D__SNDP_POGOPIN_SINGLE_UART__
    endif

    ifeq ($(SNDP_POGOPIN_DUAL_UART),1)
        KBUILD_CPPFLAGS += -D__SNDP_POGOPIN_DUAL_UART__
    endif

    ifeq ($(SNDP_POGOPIN_COMM_PULSE),1)
        KBUILD_CPPFLAGS += -D__SNDP_POGOPIN_COMM_PULSE__
    endif
endif

ifeq ($(SNDP_COMM_BLE),1)
    KBUILD_CPPFLAGS += -D__SNDP_COMM_BLE__
    KBUILD_CPPFLAGS += -D__SNDP_COMM_BLE_ADV_SET__
endif

ifeq ($(SNDP_COMM_SPP),1)
    KBUILD_CPPFLAGS += -D__SNDP_COMM_SPP__
    KBUILD_CPPFLAGS += -D__SNDP_COMM_SPP_USE_OTA_CHANNEL__
endif

ifeq ($(SNDP_COMM_MS),1)
    KBUILD_CPPFLAGS += -D__SNDP_COMM_MS__
endif

ifeq ($(SNDP_PRODUCT_TEST),1)
    KBUILD_CPPFLAGS += -D__SNDP_PRODUCT_TEST__
endif


ifeq ($(SNDP_ALGO_MGR),1)
    KBUILD_CPPFLAGS += -D__SNDP_ALGO_MGR__

    export SNDP_ALGO_ENC                        := 0
    export SNDP_USE_BES_ALGO                    := 1
endif

ifeq ($(SNDP_USE_BES_ALGO), 1)
    export SNDP_BES_ALGO_CHANNEL_NUM            := 2
    KBUILD_CPPFLAGS += -DSNDP_BES_ALGO_CHANNEL_NUM=$(SNDP_BES_ALGO_CHANNEL_NUM)
    KBUILD_CPPFLAGS += -D__SNDP_USE_BES_ALGO__
#     KBUILD_CPPFLAGS += -DSNDP_TX_DUMP_ENABLE
endif

ifeq ($(SNDP_ALGO_ENC),1)
    export SPEECH_THIRDPARTY_SNDP               := 1
    export SNDP_SPEECH_RUN_M55                  := 0

    ifeq ($(SNDP_SPEECH_RUN_M55),1)
        KBUILD_CPPFLAGS += -DSNDP_SPEECH_RUN_M55
    endif

    ifeq ($(SPEECH_THIRDPARTY_SNDP),1)
        KBUILD_CPPFLAGS += -DSPEECH_THIRDPARTY_SNDP
        export SNDP_TX_AI_ENABLE                := 1
        KBUILD_CPPFLAGS += -DSNDP_TX_AI_ENABLE
        # KBUILD_CPPFLAGS += -DSNDP_RX_AI_ENABLE
        # KBUILD_CPPFLAGS += -DSNDP_RX_DUMP_ENABLE
        # KBUILD_CPPFLAGS += -DSNDP_TX_DUMP_ENABLE
        # KBUILD_CPPFLAGS += -DSNDP_TX_TIME_ENABLE
#       KBUILD_CPPFLAGS += -DSNDP_TX_BYPASS_ENABLE
        export SPEECH_TX_AEC_CODEC_REF          := 1
        KBUILD_CPPFLAGS += -DSPEECH_TX_AEC_CODEC_REF
        export SPEECH_PROCESS_FRAME_MS          := 15
        KBUILD_CPPFLAGS += -DHFP_DISABLE_NREC
        KBUILD_CPPFLAGS += -DSNDP_MAC_LICENSE
        # export FLASH_UNIQUE_ID                ?= 1
        # KBUILD_CPPFLAGS += -DSNDP_USB_LICENSE
#        KBUILD_CPPFLAGS += -DSCO_OPTIMIZE_FOR_RAM
        export SPEECH_CODEC_CAPTURE_CHANNEL_NUM := 2
    endif
endif


ifeq ($(SNDP_MIC_GAIN_SETTING),1)
# adc_db[] = { -9, -6, -3, 0, 3, 6, 9, 12, }
    KBUILD_CPPFLAGS += -DANALOG_ADC_A_GAIN_DB=12
    KBUILD_CPPFLAGS += -DANALOG_ADC_B_GAIN_DB=12
    KBUILD_CPPFLAGS += -DANALOG_ADC_C_GAIN_DB=6
    KBUILD_CPPFLAGS += -DANALOG_ADC_D_GAIN_DB=6
    KBUILD_CPPFLAGS += -DANALOG_ADC_E_GAIN_DB=6

# codec_adc_vol[TGT_ADC_VOL_LEVEL_QTY] = { -99, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28};
    KBUILD_CPPFLAGS += -DCODEC_MIC_CH0_SADC_VOL=12
    KBUILD_CPPFLAGS += -DCODEC_MIC_CH1_SADC_VOL=12
    KBUILD_CPPFLAGS += -DCODEC_MIC_CH2_SADC_VOL=12
    KBUILD_CPPFLAGS += -DCODEC_MIC_CH3_SADC_VOL=12
    KBUILD_CPPFLAGS += -DCODEC_MIC_CH4_SADC_VOL=12
endif

ifeq ($(SNDP_REBOOT_FORCE_PAIRING),1)
    KBUILD_CPPFLAGS += -D__SNDP_REBOOT_FORCE_PAIRING__
endif

ifeq ($(SNDP_AUDIO_TEST),1)
    KBUILD_CPPFLAGS += -D__SNDP_AUDIO_TEST__
    export AUDIO_OUTPUT_DAC2                    ?= 1


    export SNDP_AUDIO_TEST_MIC_DUMP             ?= 1
    ifeq ($(SNDP_AUDIO_TEST_MIC_DUMP),1)
        KBUILD_CPPFLAGS += -D__SNDP_AUDIO_TEST_MIC_DUMP__
        AUDIO_DEBUG := 1
    endif
endif

ifeq ($(SNDP_BT_NAME),1)
    KBUILD_CPPFLAGS += -D__SNDP_BT_NAME__
    
    KBUILD_CPPFLAGS += -D__SNDP_BT_NAME_INVARIABLE__
endif

ifeq ($(SNDP_TEST_TWS_PAIRING),1)
    KBUILD_CPPFLAGS += -D__SNDP_TWS_TEST_LR_USE_SAME_ADDR__
endif

ifeq ($(SNDP_TEST_FREEMAN_PAIRING),1)
    export POWER_ON_ENTER_FREEMAN_PAIRING_ENABLED   := 1
endif	

ifeq ($(SNDP_TEST_RF_DUT),1)
    export FORCE_SIGNALINGMODE                  := 1
endif


ifeq ($(SNDP_CRASH_REBOOT),1)
    KBUILD_CPPFLAGS += -D__SNDP_CRASH_REBOOT__
    export CRASH_REBOOT                         := 1
endif

ifeq ($(SNDP_BLE_MODIFY),1)
    KBUILD_CPPFLAGS += -D__SNDP_BLE_MODIFY__
endif


ifeq ($(SNDP_HEART_RATE_MGR),1)
    KBUILD_CPPFLAGS += -D__SNDP_HEART_RATE_MGR__
    #KBUILD_CPPFLAGS += -D__SNDP_HEART_RATE_DUMP__
    
    export SNDP_HRSENSOR_SUPPORT                := 1
    export SNDP_HR_ALGO                         := 1

endif

ifeq ($(SNDP_HR_ALGO),1)
    KBUILD_CPPFLAGS += -D__SNDP_HR_ALGO__

    export SNDP_HR_ALGO_SLEEPSENSE              := 1
    
    ifeq ($(SNDP_HR_ALGO_SLEEPSENSE),1)
        KBUILD_CPPFLAGS += -D__SNDP_HR_ALGO_SLEEPSENSE__
    endif
    
endif


ifeq ($(SNDP_HRSENSOR_SUPPORT),1)
    KBUILD_CPPFLAGS += -D__SNDP_HRSENSOR_SUPPORT__
    
    export SNDP_HRSENSOR_SSH401A                := 1

    ifeq ($(SNDP_HRSENSOR_SSH401A),1)
        KBUILD_CPPFLAGS += -D__SNDP_HRSENSOR_SSH401A__
    endif

endif

ifeq ($(SNDP_KEY_TEST),1)
    KBUILD_CPPFLAGS += -D__SNDP_KEY_TEST__
endif

ifeq ($(SNDP_APP_WHITE_NOISE),1)
    KBUILD_CPPFLAGS += -D__SNDP_APP_WHITE_NOISE__
endif

ifeq ($(SNDP_SLEEP_APP),1)
    KBUILD_CPPFLAGS += -D__SNDP_SLEEP_APP__
    #KBUILD_CPPFLAGS += -D__SNDP_GESTURE_MAP__
    KBUILD_CPPFLAGS += -D__SNDP_EQ_PARAM_SETTING__
    KBUILD_CPPFLAGS += -D__SNDP_FINDME__
#     KBUILD_CPPFLAGS += -D__SNDP_SEND_GESTURE__
endif

ifeq ($(SNDP_BAT_SWITCH_ROLE),1)
    KBUILD_CPPFLAGS += -D__SNDP_BAT_SWITCH_ROLE__
endif

ifeq ($(SNDP_PROMPT_TEST),1)
    KBUILD_CPPFLAGS += -D__SNDP_PROMPT_TEST__
endif

ifeq ($(SNDP_ANC_MODIFY),1)
    KBUILD_CPPFLAGS += -D__SNDP_ANC_MODIFY__
endif



endif 
# SNDP_PROJ Total Control

