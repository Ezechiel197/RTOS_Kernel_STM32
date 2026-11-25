/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32l4xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l4xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "os_kernel.h"
#include "tcb.h"
#include "SEGGER_SYSVIEW.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TIME_SLICE_TICKS 5   // Es wird für den Präemtive RR verwendet und zu jedem Task 5 ms, die CPU zu besitzen
#define Max_Tasks 3
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static uint8_t g_u8PraemtiveTime = TIME_SLICE_TICKS; // Unser Timer-Variable, die überprüft, ob der 5 ms abgelaufen sind
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern I2C_HandleTypeDef hi2c2;
extern UART_HandleTypeDef huart1;
/* USER CODE BEGIN EV */
extern TCB_sctTCB_t* g_pCurrentTCB;
extern void OS_Scheduler(void);

extern TCB_sctTCB_t g_asctTCBs[];
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
__attribute__((naked))void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */
	//SEGGER_SYSVIEW_RecordEnterISR();   // Start der Aufnahme der Ausführungszeit im PendSV, PendSV-ID = 14
  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */
	/**
		 * @brief Der Context Switch.
		 */

		    __asm volatile (


		        //  Sichere die restlichen Software-Register des alten Tasks
		        "push {r4-r11}                  \n"

		        // 2. Speichere den aktuellen Stack-Pointer im aktuellen TCB
		        "ldr r0, =g_pCurrentTCB         \n" // r0 = Adresse des Pointers
		        "ldr r1, [r0]                   \n" // r1 = g_pCurrentTCB (Adresse der Struktur)
		        "mrs r2, msp                    \n" // Lade den aktuellen MSP in r2
		        "str r2, [r1]                   \n" // Speichere r2 in pNewTask->u32TaskSP ( aktuelle Stack-Pointer sichern)

		        // LR vor den Aufruf von OS_Scheduler() sichern
		        "push {lr}                      \n"
		        "bl OS_Scheduler                \n"
		        "pop {lr}                       \n" // EXC_RETURN wiederherstellen

		        // der Stack-Pointer des neue Tasks wird geladen
		        "ldr r0, =g_pCurrentTCB         \n"
		        "ldr r1, [r0]                   \n"
		        "ldr r2, [r1]                   \n" // r2 = neuer u32TaskSP
		        "msr msp, r2                    \n"

		        // die Software-Register der neue Task wird wieder hergestellt
		        "pop {r4-r11}                   \n"

		        // Die Hardware_Register werden gepoppt und
		    	// der PC zeigt jetzt auf den neuen Task
		        "bx lr                          \n"
		    );

		   // SEGGER_SYSVIEW_RecordExitISR(); // Ende der Ausführungszeitsaufnahme

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */
	SEGGER_SYSVIEW_RecordEnterISR(); // gib die exakte ID = 15 vom SysTick-Interrupt, Ziel: überprüfen, wie lange der CPU im ISR verbringt
  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */
  // Wird überprüft, ob der Timeout abgelaufen ist
  for(uint8_t i = 0; i < Max_Tasks; i++) {
          if (g_asctTCBs[i].eTaskState == TaskState_Blocked && g_asctTCBs[i].u32TimeoutTicks > 0) {
              g_asctTCBs[i].u32TimeoutTicks--; // Timer herunterzählen

              if (g_asctTCBs[i].u32TimeoutTicks == 0) {
                  // Timeout ist abgelaufen! Task aufwecken.
                  g_asctTCBs[i].eTaskState = TaskState_Ready;
                  SEGGER_SYSVIEW_OnTaskStartReady((uint32_t)&g_asctTCBs[i]);
              }
          }
      }
  // Hier wird überprüft, ob die Schalfzeit von den blockierende Task abgelaufen ist, wenn ja, wird er aufgewacht.
  for(int i = 0;i < Max_Tasks;i++){
	  if(g_asctTCBs[i].eTaskState == TaskState_Blocked)
		  {
		  if(g_asctTCBs[i].u32DelayTicks == 0){
			  g_asctTCBs[i].eTaskState = TaskState_Ready;
			 SEGGER_SYSVIEW_OnTaskStartReady((uint32_t)&g_asctTCBs[i]);
		  break;
		  }
		  g_asctTCBs[i].u32DelayTicks--;
		  }
	  }
  	  // Hier wird überprüft, ob die Zeit der Präemtive RR abgelaufen ist, wenn ja wird der PendSV-Interrupt gesetzt
  	 if(g_u8PraemtiveTime > 0) g_u8PraemtiveTime--;
  	 else if (g_u8PraemtiveTime == 0) {
  		g_u8PraemtiveTime = TIME_SLICE_TICKS;
  		// Löst den PendSV-Interrupt aus, um den Task zu wechseln
  		SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
  	 }
  	SEGGER_SYSVIEW_RecordExitISR();   // Hier wird SystemView mitgeteilt, dass die ISR in diesem Moment verlassen wurde
  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32L4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32l4xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles I2C2 event interrupt.
  */
void I2C2_EV_IRQHandler(void)
{
  /* USER CODE BEGIN I2C2_EV_IRQn 0 */

  /* USER CODE END I2C2_EV_IRQn 0 */
  HAL_I2C_EV_IRQHandler(&hi2c2);
  /* USER CODE BEGIN I2C2_EV_IRQn 1 */

  /* USER CODE END I2C2_EV_IRQn 1 */
}

/**
  * @brief This function handles I2C2 error interrupt.
  */
void I2C2_ER_IRQHandler(void)
{
  /* USER CODE BEGIN I2C2_ER_IRQn 0 */

  /* USER CODE END I2C2_ER_IRQn 0 */
  HAL_I2C_ER_IRQHandler(&hi2c2);
  /* USER CODE BEGIN I2C2_ER_IRQn 1 */

  /* USER CODE END I2C2_ER_IRQn 1 */
}

/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */

  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */

  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles EXTI line[15:10] interrupts.
  */
void EXTI15_10_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI15_10_IRQn 0 */

  /* USER CODE END EXTI15_10_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(LPS22HB_INT_DRDY_EXTI0_Pin);
  HAL_GPIO_EXTI_IRQHandler(LSM6DSL_INT1_EXTI11_Pin);
  HAL_GPIO_EXTI_IRQHandler(BUTTON_EXTI13_Pin);
  HAL_GPIO_EXTI_IRQHandler(ARD_D2_Pin);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_15);
  /* USER CODE BEGIN EXTI15_10_IRQn 1 */

  /* USER CODE END EXTI15_10_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
