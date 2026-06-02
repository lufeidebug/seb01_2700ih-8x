#include "ss_ppg.h"
#include "ss_os_api.h"

/**
 * @file ss_util_ring_buffer.c
 * @brief This file contains the ring buffer implementation used by @ref ss_ppg.c to store PPG data.
 *
 * @note This code is used internally by the ss_ppg.c's API, and users do not need to call it directly.
 */
 
#define DATA_RINGBUFFER_SIZE    (32*4)

static int nHead;
static int nTail;
static SS_PPG PPG_Buffer[DATA_RINGBUFFER_SIZE];

int RingBuffer_Initialization(void)
{
    nHead = 0;
    nTail = 0;

    return os_api_memset(PPG_Buffer, 0, sizeof(PPG_Buffer));
}

void* RingBuffer_Get_SaveBuffer(void)
{
    return &PPG_Buffer[nHead];
}

void* RingBuffer_Get_SaveBuffer_Index(unsigned short index)
{
    return &PPG_Buffer[index];
}

void RingBuffer_Data_Push(void)
{
    nHead++;
    nHead %= DATA_RINGBUFFER_SIZE;
}

void* RingBuffer_Data_Pop(void)
{
    SS_PPG* pRet = &PPG_Buffer[nTail];
    nTail++;
    nTail %= DATA_RINGBUFFER_SIZE;
    return pRet;
}

int RingBuffer_Get_DataCount(void)
{
    return (nHead - nTail + DATA_RINGBUFFER_SIZE) % DATA_RINGBUFFER_SIZE;
}