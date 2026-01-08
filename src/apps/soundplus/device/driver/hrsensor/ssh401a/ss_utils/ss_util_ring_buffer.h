#ifndef SS_UTIL_RING_BUFFER_H_
#define SS_UTIL_RING_BUFFER_H_

/**
 * @file ss_util_ring_buffer.h
 */

/**
 * @brief Initializes the ring buffer.
 */
int RingBuffer_Initialization(void);

/**
 * @brief Returns a pointer to the free memory area in the ring buffer.
 */
void* RingBuffer_Get_SaveBuffer(void);

/**
 * @brief Returns a pointer to the data at the specified index among the elements pushed into the ring buffer.
 */
void* RingBuffer_Get_SaveBuffer_Index(unsigned short index);

/**
 * @brief Stores data at the end of the ring buffer.
 */
void RingBuffer_Data_Push(void);

/**
 * @brief Returns a pointer to the first data in the ring buffer (the oldest data that was pushed).
 */
void* RingBuffer_Data_Pop(void);

/**
 * @brief Returns the number of data elements currently pushed into the ring buffer.
 */
int RingBuffer_Get_DataCount(void);

#endif /* SS_UTIL_RING_BUFFER_H_ */

