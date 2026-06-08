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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TIME_SLICE_TICKS 5   // Es wird für den Präemtive RR verwendet und zu jedem Task 5 ms, die CPU zu besitzen
#define Max_Task 3
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
/**
 * @brief Führt den Context Switch (Kontextwechsel) durch.
 *
 * @details Diese Funktion ist der PendSV-Interrupt-Handler. Das Attribut
 * `__attribute__((naked))` verhindert, dass der Compiler eigenen Prolog/Epilog-Code
 * generiert. Die Funktion sichert die verbleibenden Software-Register (r4-r11)
 * des aktuell laufenden Tasks auf dessen Stack, speichert den aktuellen Stackpointer (MSP)
 * im Task Control Block (TCB) und ruft den `OS_Scheduler` auf, um den nächsten
 * Task zu bestimmen. Danach lädt sie den Stackpointer des neuen Tasks, stellt
 * dessen Software-Register wieder her und triggert über den Rücksprung (`bx lr`
 * mit EXC_RETURN) die automatische Wiederherstellung der Hardware-Register durch die CPU.
 */
__attribute__((naked))void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

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

  /* USER CODE END PendSV_IRQn 1 */
}

/**
 * @brief Behandelt den System Tick Timer Interrupt (SysTick).
 *
 * @details Diese Funktion wird periodisch von der Hardware aufgerufen ( jede Millisekunde).
 * Sie übernimmt neben der grundlegenden HAL-Zeitbasis (`HAL_IncTick`) drei zentrale
 * Aufgaben für das RTOS:
 * 1. Sie verringert die Timeout-Zähler (`u32TimeoutTicks`) von blockierten Tasks
 * ( Wartezeiten bei Semaphoren und Queues) und weckt diese bei Ablauf auf.
 * 2. Sie aktualisiert die Delay-Zähler (`u32DelayTicks`) von pausierten Tasks
 * (ausgelöst durch `OS_Delay`) und setzt diese bei Ablauf wieder auf `TaskState_Ready`.
 * 3. Sie verwaltet das Time-Slicing für das präemptive Round-Robin-Scheduling.
 * Sobald die festgelegte Zeitscheibe (`g_u8PraemtiveTime`) abgelaufen ist,
 * wird ein PendSV-Interrupt ausgelöst, um den Context Switch zu erzwingen.
 */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();


  /* USER CODE BEGIN SysTick_IRQn 1 */
  // Wird überprüft, ob der Timeout abgelaufen ist
  for(uint8_t i = 0; i < Max_Task; i++) {
          if (g_asctTCBs[i].eTaskState == TaskState_Blocked && g_asctTCBs[i].u32TimeoutTicks > 0) {
              g_asctTCBs[i].u32TimeoutTicks--; // Timer herunterzählen

              if (g_asctTCBs[i].u32TimeoutTicks == 0) {
                  // Timeout ist abgelaufen! Task aufwecken.
                  g_asctTCBs[i].eTaskState = TaskState_Ready;
              }
          }
      }


  // Hier wird überprüft, ob die Schalfzeit von den blockierende Task abgelaufen ist, wenn ja, wird er aufgewacht.
  for(int i = 0;i < Max_Task;i++){
	  if(g_asctTCBs[i].eTaskState == TaskState_Blocked)
		  {


		  if(g_asctTCBs[i].u32DelayTicks == 0){
			  g_asctTCBs[i].eTaskState = TaskState_Ready;
		  break;
		  }
		  g_asctTCBs[i].u32DelayTicks--;
		  }

	  }
  	  // Hier wird überprüft, ob die Zeit der Präemtive RR abgelaufen ist, wenn ja wird der PendSV-Interrupt gesetzt
  	 if(g_u8PraemtiveTime > 0) g_u8PraemtiveTime--;
  	 else if (g_u8PraemtiveTime == 0) {
  		g_u8PraemtiveTime = TIME_SLICE_TICKS;
  		//g_asctTCBs[0].eTaskState = TaskState_Blocked;
  		//g_asctTCBs[1].eTaskState = TaskState_Ready;
  		// Löst den PendSV-Interrupt aus, um den Task zu wechseln
  		SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
  	 }

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32L4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32l4xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles EXTI line[9:5] interrupts.
  */
void EXTI9_5_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI9_5_IRQn 0 */

  /* USER CODE END EXTI9_5_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(SPSGRF_915_GPIO3_EXTI5_Pin);
  HAL_GPIO_EXTI_IRQHandler(SPBTLE_RF_IRQ_EXTI6_Pin);
  HAL_GPIO_EXTI_IRQHandler(VL53L0X_GPIO1_EXTI7_Pin);
  HAL_GPIO_EXTI_IRQHandler(LSM3MDL_DRDY_EXTI8_Pin);
  /* USER CODE BEGIN EXTI9_5_IRQn 1 */

  /* USER CODE END EXTI9_5_IRQn 1 */
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
  HAL_GPIO_EXTI_IRQHandler(HTS221_DRDY_EXTI15_Pin);
  /* USER CODE BEGIN EXTI15_10_IRQn 1 */

  /* USER CODE END EXTI15_10_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
