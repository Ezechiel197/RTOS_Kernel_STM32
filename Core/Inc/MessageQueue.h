/*
 * MessageQueue.h
 *
 *  Created on: Jun 4, 2026
 *      Author: ezech
 */

#ifndef INC_MESSAGEQUEUE_H_
#define INC_MESSAGEQUEUE_H_
#include "tcb.h"
#include "os_sync.h"
#include "stm32l4xx.h"
#include "os_kernel.h"

void OS_MsgQ_Init(OS_MessageQueue_t* pQ, uint32_t* pBufferArray, uint32_t u32Capacity);
uint8_t OS_MsgQ_IsEmpty(OS_MessageQueue_t* pQ);
uint8_t OS_MsgQ_IsFull(OS_MessageQueue_t* pQ);
uint8_t OS_MsgQ_SendNonBlocking(OS_MessageQueue_t* pQ, uint32_t u32Msg);
void OS_MsgQ_SendBlocking(OS_MessageQueue_t* pQ, uint32_t u32Msg);
uint8_t OS_MsgQ_SendTimeout(OS_MessageQueue_t* pQ, uint32_t u32Msg, uint32_t u32TimeoutTicks);
uint8_t OS_MsgQ_ReadNonBlocking(OS_MessageQueue_t* pQ, uint32_t* pDest);
void OS_MsgQ_ReadBlocking(OS_MessageQueue_t* pQ, uint32_t* pDest);
uint8_t OS_MsgQ_ReadTimeout(OS_MessageQueue_t* pQ, uint32_t* pDest, uint32_t u32TimeoutTicks);

extern TCB_sctTCB_t* g_pCurrentTCB;
extern TCB_sctTCB_t g_asctTCBs[];

#endif /* INC_MESSAGEQUEUE_H_ */
