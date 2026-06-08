/*
 * Mutex_and_Semaphoren.h
 *
 *  Created on: May 30, 2026
 *      Author: ezech
 */


#ifndef INC_MUTEX_AND_SEMAPHOREN_H_
#define INC_MUTEX_AND_SEMAPHOREN_H_

#include "tcb.h"
#include "os_sync.h"
#include "stm32l4xx.h"
#include "os_kernel.h"

void OS_Mutex_Init(OS_Mutex_t* pMutex);
void OS_Mutex_AcquireBlocking(OS_Mutex_t* pMutex);
void OS_Mutex_Release(OS_Mutex_t* pMutex);
uint8_t OS_Mutex_AcquireNonBlocking(OS_Mutex_t* pMutex);
uint8_t OS_Mutex_AcquireTimeout(OS_Mutex_t* pMutex, uint32_t u32TimeoutTicks);

void OS_Semaphore_Init(OS_Semaphore_t* pSem, int32_t s32InitCount, uint32_t u32MaxCount);
void OS_Sem_AcquireBlocking(OS_Semaphore_t* pSem);
void OS_Sem_Release(OS_Semaphore_t* pSem);
uint8_t OS_Sem_AcquireNonBlocking(OS_Semaphore_t* pSem);
uint8_t OS_Sem_AcquireTimeout(OS_Semaphore_t* pSem, uint32_t u32TimeoutTicks);

extern TCB_sctTCB_t* g_pCurrentTCB;
extern TCB_sctTCB_t g_asctTCBs[];

#endif /* INC_MUTEX_AND_SEMAPHOREN_H_ */
