/*
 * os_kernel.h
 *
 *  Created on: May 10, 2026
 *      Author: ezechiel
 */

#ifndef INC_OS_KERNEL_H_
#define INC_OS_KERNEL_H_

void OS_StartFirstTask(void);
typedef void (*OS_TaskFunction_t)(void);
uint8_t OS_TaskCreate(OS_TaskFunction_t pTaskCode, uint8_t u8Prio, uint8_t u8TaskId);
void OS_KernelStart(void);

void OS_Scheduler(void);
void OS_Delay(uint32_t tick_time);

#endif /* INC_OS_KERNEL_H_ */
