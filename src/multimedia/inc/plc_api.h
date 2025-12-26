#pragma once
#include <stdint.h>

#ifndef EXTERNC
#ifdef __cplusplus
#define EXTERNC extern "C" 
#else
#define EXTERNC
#endif
#endif


#define PLC_API_RET_SUCCESS			(0)
#define PLC_API_RET_INPUT_ERROR		(-1)
#define PLC_API_RET_FAIL			(-2)
#define PLC_API_RET_NOT_SUPPORT		(-3)

#define PLC_API_KEY					3

enum PlcApiMode_e {
	PLC_API_MODE_MUSIC_PLC = 0,
	PLC_API_MODE_MAX,
};

enum PlcApiDataType_e {
	PLC_API_DATA_TYPE_SHORT_16 = 0,
	PLC_API_DATA_TYPE_INT_32,
	PLC_API_DATA_TYPE_FLOAT_32,
	PLC_API_DATA_TYPE_MAX,
};

enum PlcApiApplication_e {
	PLC_API_APPLICATION_AUTO = 0,
	PLC_API_APPLICATION_VOICE,
	PLC_API_APPLICATION_MUSIC,
	PLC_API_APPLICATION_192K,
	PLC_API_APPLICATION_LOW_LATENCY,
	PLC_API_APPLICATION_LFE,
	PLC_API_APPLICATION_MAX,
};

enum PlcApiSet_e {
	PLC_API_SET_NOONE = 0,
	PLC_API_SET_MAX,
};

enum PlcApiGet_e {
	PLC_API_GET_HEAP_SIZE = 0,
	PLC_API_GET_MAX,
};

typedef struct {
	//common
	int32_t fsHz;
	int16_t channels;
	int16_t frameSamples;
	int16_t width;
	uint16_t channelSelect;
	enum PlcApiMode_e mode;
	enum PlcApiDataType_e dataType;
	enum PlcApiApplication_e application;
	int32_t key;
	void* (*cb_malloc)(int size);
	void (*cb_free)(void* ptr);
	void (*cb_printf)(const char* fmt, ...);
	void* other;
}PlcApiParam_t;

typedef int32_t PlcApiRet;

EXTERNC PlcApiRet PlcApiCreate(void** pHd, PlcApiParam_t* param);
EXTERNC PlcApiRet PlcApiRun(void* hd, uint8_t* in, int32_t inLen, int32_t* inUsed, uint8_t* out, int32_t* pOutLen, uint16_t isLost);
EXTERNC PlcApiRet PlcApiSet(void* hd, enum PlcApiSet_e choose, void* val);
EXTERNC PlcApiRet PlcApiGet(void* hd, enum PlcApiGet_e choose, void* val);
EXTERNC PlcApiRet PlcApiDestory(void* hd);

