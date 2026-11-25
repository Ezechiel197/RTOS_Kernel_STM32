/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32l4xx.h"
#include "os_kernel.h"
#include "tcb.h"
#include "Mutex_and_Semaphoren.h"
#include"MessageQueue.h"
#include "os_sync.h"
#include "SEGGER_SYSVIEW.h"
#include "sensor.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
	#define MAX_TASKs 3

	#define HTS221_ADDR         (0x5F << 1) /**< 7-Bit-I2C-Adresse um 1 verschoben: LSB=0 (Schreiben), LSB=1 (Lesen). */
	#define HTS221_WHO_AM_I     0x0F        /**< Sensor-Identifikationsregister (Erwartete Antwort: 0xBC). */
	#define HTS221_CTRL_REG1    0x20        /**< Steuerregister 1: Weckt den Sensor auf, wenn 0x81 geschrieben wird. */
	#define HTS221_STATUS_REG   0x27        /**< Statusregister: Bit 1 (H_DA) = Feuchtedaten bereit, Bit 0 (T_DA) = Tempdaten bereit. */

	#define SENSOR_QUEUE_CAPACITY   5

	#define SYSVIEW_MARKER_I2C_WAIT      10
	#define SYSVIEW_MARKER_UART_TX       11
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
DFSDM_Channel_HandleTypeDef hdfsdm1_channel1;

I2C_HandleTypeDef hi2c2;

QSPI_HandleTypeDef hqspi;

SPI_HandleTypeDef hspi3;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* USER CODE BEGIN PV */
OS_Semaphore_t SensorSemaphore;
OS_MessageQueue_t SensorQueue;
uint32_t sensorQueueBuffer[SENSOR_QUEUE_CAPACITY];

uint8_t i2c_tx_buf[2];
uint8_t i2c_rx_buf[4];
volatile uint8_t i2c_transfer_complete = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
 void MX_GPIO_Init(void);
 void MX_DFSDM1_Init(void);
 void MX_I2C2_Init(void);
 void MX_QUADSPI_Init(void);
 void MX_SPI3_Init(void);
 void MX_USART1_UART_Init(void);
 void MX_USART3_UART_Init(void);
//static void MX_USB_OTG_FS_PCD_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// --- TASK 1: UART-Task ---
 /**
  * @brief RTOS-Task zur Formatierung und seriellen Ausgabe der Sensordaten.
  * @details Der Task wartet blockierend an der `SensorQueue` auf eingehende Nachrichten vom `Task_Sensor`.
  *          Sobald Daten eintreffen, wird Folgendes ausgeführt:
  *          1. Eine Status-LED (GPIOB Pin 14) wird eingeschaltet.
  *          2. Der Inhalt der Nachricht (`SensorMsg_t`) wird evaluiert.
  *          3. Bei Status `Okay` werden Temperatur und Feuchte formatiert; andernfalls wird eine Fehlermeldung generiert.
  *          4. Der String wird über UART1 (`HAL_UART_Transmit`) ausgegeben.
  *          5. Die Status-LED wird ausgeschaltet und der Task blockiert erneut an der Queue.
  *
  * @note Nutzt SystemView Marker (`SYSVIEW_MARKER_UART_TX`), um die Sendezeit der seriellen Schnittstelle aufzuzeichnen.
  *
  * @return void
  */
void Task_UART(){
	SensorMsg_t *pReceivedMsg;   // Zeiger auf die empfangene Nachricht
	char tx_buffer[128];


	while(1){
		OS_MsgQ_ReadBlocking(&SensorQueue,(uint32_t*)&pReceivedMsg);

		// Task wird aktiv, Start-Event
		SEGGER_SYSVIEW_OnTaskStartExec(g_pCurrentTCB->u8TaskId);
		// Status auswerten und String formatieren
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
		        	if(pReceivedMsg->status == Okay) {

		        		// Erfolgsfall: Temperatur und Feuchtigkeit formatieren
		        	            snprintf(tx_buffer, sizeof(tx_buffer),
		        	                     "Temp: %d C, Feuchte: %d %%\r\n",
		        	                     (int)pReceivedMsg->temperature,
		        	                     (int)pReceivedMsg->humidity);
		        } else {
		        	// Fehlerfall: Fehlermeldung mit Code formatieren
		        	            snprintf(tx_buffer, sizeof(tx_buffer),
		        	                     "Fehler: HTS221 Kommunikation fehlgeschlagen (Code %lu)\r\n",
		        	                     pReceivedMsg->hardwareError);
		        }

		        	// SystemView Marker: Beginn der UART-Übertragung
		        	SEGGER_SYSVIEW_MarkStart(SYSVIEW_MARKER_UART_TX);
		        	// String über UART senden
		        	// HAL_MAX_DELAY sorgt dafür, dass er wartet, bis alles gesendet ist
		        	HAL_UART_Transmit(&huart1,(uint8_t*)tx_buffer,strlen(tx_buffer),HAL_MAX_DELAY);
		        	// SystemView Marker: Ende der UART-Übertragung
		        	SEGGER_SYSVIEW_MarkStop(SYSVIEW_MARKER_UART_TX);

		        	// LED wieder aus
		        	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);

		        	// Task-Ausführung für diesen Zyklus beendet
		        	SEGGER_SYSVIEW_OnTaskStopReady(g_pCurrentTCB->u8TaskId, TaskState_Blocked);
		        	//OS_Delay(500);


		//HAL_GPIO_TogglePin(GPIOB,GPIO_PIN_14);
		//OS_Delay(200); // Non-Blocking

		//for(volatile uint32_t i = 0; i < 500000; i++) {}   // Es dient zur Wartezeit ( Blocking)

}
}



// HTS221 KALIBRIERUNGS-DATEN & FUNKTION

/**
 * @struct HTS221_Calib_t
 * @brief Struktur zur Speicherung der werkseitigen HTS221-Kalibrierkoeffizienten.
 * @details Die Koeffizienten werden aus dem Nicht-Flüchtigen Speicher (NVM) des Sensors
 *          ausgelesen und dienen der linearen Interpolation der Messwerte.
 */
typedef struct {
    int16_t T0_out;       /**< Rohwert der Temperatur-Kalibrieradresse T0. */
    int16_t T1_out;       /**< Rohwert der Temperatur-Kalibrieradresse T1. */
    int16_t T0_degC_x8;   /**< Kalibriertemperatur T0 in °C (skaliert mit Faktor 8). */
    int16_t T1_degC_x8;   /**< Kalibriertemperatur T1 in °C (skaliert mit Faktor 8). */
    int16_t H0_out;       /**< Rohwert der Luftfeuchte-Kalibrieradresse H0. */
    int16_t H1_out;       /**< Rohwert der Luftfeuchte-Kalibrieradresse H1. */
    uint8_t H0_rH_x2;     /**< Kalibrierfeuchte H0 in % rH (skaliert mit Faktor 2). */
    uint8_t H1_rH_x2;     /**< Kalibrierfeuchte H1 in % rH (skaliert mit Faktor 2). */
} HTS221_Calib_t;

static HTS221_Calib_t calib;

/**
 * @brief Liest die werkseitigen Kalibrierdaten aus dem HTS221-Sensor aus.
 * @details Greift per I2C im Auto-Increment-Modus auf die Register 0x30 bis 0x3F zu
 *          und konvertiert die Byte-Arrays in die entsprechenden 16-Bit-Kalibrierwerte
 *          für Temperatur und Luftfeuchtigkeit.
 *
 * @note Nutzt blockierenden I2C-Zugriff (`HAL_MAX_DELAY`), da die Kalibrierung
 *       eine einmalige Initialisierungsoperation darstellt.
 *
 * @return void
 */
void HTS221_Read_Calibration(void) {
    uint8_t calib_data[16];

    // Kalibrierdaten liegen in Register 0x30 bis 0x3F (0x30 | 0x80 für Auto-Increment)
    HAL_I2C_Mem_Read(&hi2c2, HTS221_ADDR, (0x30 | 0x80), I2C_MEMADD_SIZE_8BIT, calib_data, 16, HAL_MAX_DELAY);

    // Luftfeuchte-Kalibrierung
    calib.H0_rH_x2 = calib_data[0];
    calib.H1_rH_x2 = calib_data[1];
    calib.H0_out = (int16_t)(calib_data[6] | (calib_data[7] << 8));
    calib.H1_out = (int16_t)(calib_data[10] | (calib_data[11] << 8));

    // Temperatur-Kalibrierung
    uint16_t T0_degC = (uint16_t)(calib_data[2] | ((calib_data[5] & 0x03) << 8));
    uint16_t T1_degC = (uint16_t)(calib_data[3] | ((calib_data[5] & 0x0C) << 6));
    calib.T0_degC_x8 = T0_degC;
    calib.T1_degC_x8 = T1_degC;

    calib.T0_out = (int16_t)(calib_data[12] | (calib_data[13] << 8));
    calib.T1_out = (int16_t)(calib_data[14] | (calib_data[15] << 8));
}


// --- TASK 2: Task_Sensor --
/**
 * @brief RTOS-Task zur zyklischen Datenerfassung vom HTS221-Sensor via I2C.
 * @details Der Task führt folgende Schritte in einer Endlosschleife aus:
 *          1. Aktivierung des Sensors und Auslesen der Kalibrierdaten.
 *          2. Triggern einer One-Shot-Messung per I2C Interrupt (`HAL_I2C_Mem_Write_IT`).
 *          3. Blockieren an einer Semaphor (`SensorSemaphore`), bis der I2C-Transfer abgeschlossen ist.
 *          4. Warten auf die physikalische Wandlung (100 ms) via `OS_Delay`.
 *          5. Asynchrones Auslesen der Rohdaten via I2C Interrupt (`HAL_I2C_Mem_Read_IT`) und erneutes Blockieren.
 *          6. Berechnung von echter Temperatur (°C) und feuchte (% rH) mittels Interpolation.
 *          7. Übertragung der formatierten `SensorMsg_t` an die `SensorQueue`.
 *          8. Versetzen des Tasks in den Schlafzustand für 2000 ms.
 *
 * @note Nutzt Segger SystemView Marker (`SYSVIEW_MARKER_I2C_WAIT`) zur Laufzeitanalyse der I2C-Transaktionen.
 *
 * @return void
 */
void Task_Sensor(){
	static SensorMsg_t msg;
	HAL_StatusTypeDef hal_status;

	// Sensorinitialisierung
				// CTRL_REG1 (0x20): Setze PD (Power Down) auf 1 (Active) und ODR auf 1 (1Hz Messrate)
				uint8_t init_data = 0x81;
	while(1){

		// Task startet Ausführung
		SEGGER_SYSVIEW_OnTaskStartExec(g_pCurrentTCB->u8TaskId);

		HAL_I2C_Mem_Write(&hi2c2, HTS221_ADDR, HTS221_CTRL_REG1, I2C_MEMADD_SIZE_8BIT, &init_data, 1, HAL_MAX_DELAY);
		HTS221_Read_Calibration();

		msg.temperature = 0;
		msg.humidity = 0;
		msg.status = Okay;
		// das ONE_SHOT Bit (Bit 0) des CTRL_REG2 (0x21) setzen, um die Messung zu starten
		uint8_t start_cmd = 0x01;

		// SystemView Marker: I2C-Messung gestartet
		SEGGER_SYSVIEW_MarkStart(SYSVIEW_MARKER_I2C_WAIT);
		hal_status = HAL_I2C_Mem_Write_IT(&hi2c2, HTS221_ADDR, 0x21, I2C_MEMADD_SIZE_8BIT, &start_cmd, 1);

		if(hal_status != HAL_OK) msg.status = I2C_Error;
		else {
			OS_Sem_AcquireBlocking(&SensorSemaphore);

			// Dem Sensor 100ms Zeit geben, um die Messung durchzuführen
			OS_Delay(100);

			uint8_t raw_data[6];
			hal_status = HAL_I2C_Mem_Read_IT(&hi2c2, HTS221_ADDR, (0x28 | 0x80), I2C_MEMADD_SIZE_8BIT, raw_data, 6);  // (0x28 | 0x80) : es soll ab register 28 gelesen werde und
																											// das 80, soll erlauben, mehrere Byte zu lesen.
			if(hal_status != HAL_OK) {msg.status = Sensor_Timeout;  // Fehler beim Lesen der Rohdaten

			SEGGER_SYSVIEW_MarkStop(SYSVIEW_MARKER_I2C_WAIT);
			}
			else {
				// Erneut blockieren, bis die Daten per I2C-Interrupt komplett im RAM (raw_data) gelandet sind
				OS_Sem_AcquireBlocking(&SensorSemaphore);

				// Marker stoppen: I2C-Daten vollständig erhalten
				SEGGER_SYSVIEW_MarkStop(SYSVIEW_MARKER_I2C_WAIT);
				// Rohdaten zusammensetzen
				int16_t H_OUT = (int16_t)(raw_data[0] | (raw_data[1] << 8));
				int16_t T_OUT = (int16_t)(raw_data[2] | (raw_data[3] << 8));


				// ECHTE berechnete Temperatur in °C
				float tmp_deg = (float)(calib.T1_degC_x8 - calib.T0_degC_x8) / 8.0f;
				msg.temperature = ((float)(T_OUT - calib.T0_out) * tmp_deg) / (float)(calib.T1_out - calib.T0_out) + ((float)calib.T0_degC_x8 / 8.0f);

				// ECHTE berechnete Luftfeuchtigkeit in %
				float tmp_hum = (float)(calib.H1_rH_x2 - calib.H0_rH_x2) / 2.0f;
				msg.humidity = ((float)(H_OUT - calib.H0_out) * tmp_hum) / (float)(calib.H1_out - calib.H0_out) + ((float)calib.H0_rH_x2 / 2.0f);
			}
		}



		if(msg.status != Okay) {

			msg.status = I2C_Error;
			    // Exakter HAL-Hardware-Fehlercode für die spätere Diagnose
			    msg.hardwareError = HAL_I2C_GetError(&hi2c2);   // Damit kann ich das genaue Problem wissen
		}


		OS_MsgQ_SendBlocking(&SensorQueue,(uint32_t)&msg);   // Das Paket an den MessageQueue für den UART schicken

		// Task meldet Ende der Verarbeitung, bevor er schlafen geht
		SEGGER_SYSVIEW_OnTaskStopReady((uint32_t)g_pCurrentTCB->u8TaskId, TaskState_Blocked);
		OS_Delay(2000);
	}


}


// --- IDLE Task ---
void Task_Idle(void) {
    while(1) {

    	SEGGER_SYSVIEW_OnIdle(); // melde SystemView, wie lange der CPU wirklich still bleibt
    	//__WFI(); // Wait for Interrupt
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DFSDM1_Init();
  MX_I2C2_Init();
  MX_QUADSPI_Init();
  MX_SPI3_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  //MX_USB_OTG_FS_PCD_Init();
  /* USER CODE BEGIN 2 */
  //Tasks beim Betriebssystem anmelden, Priorität und TaskId angeben
  SEGGER_SYSVIEW_Conf();
  	  OS_TaskCreate(Task_UART,0, 1);
  	  OS_TaskCreate(Task_Sensor, 0, 0);
      OS_TaskCreate(Task_Idle, 255, MAX_TASKs-1);
      OS_MsgQ_Init(&SensorQueue,sensorQueueBuffer,SENSOR_QUEUE_CAPACITY);
      OS_Semaphore_Init(&SensorSemaphore,0,1);

      // Kernel starten
        OS_KernelStart();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief DFSDM1 Initialization Function
  * @param None
  * @retval None
  */
void MX_DFSDM1_Init(void)
{

  /* USER CODE BEGIN DFSDM1_Init 0 */

  /* USER CODE END DFSDM1_Init 0 */

  /* USER CODE BEGIN DFSDM1_Init 1 */

  /* USER CODE END DFSDM1_Init 1 */
  hdfsdm1_channel1.Instance = DFSDM1_Channel1;
  hdfsdm1_channel1.Init.OutputClock.Activation = ENABLE;
  hdfsdm1_channel1.Init.OutputClock.Selection = DFSDM_CHANNEL_OUTPUT_CLOCK_SYSTEM;
  hdfsdm1_channel1.Init.OutputClock.Divider = 2;
  hdfsdm1_channel1.Init.Input.Multiplexer = DFSDM_CHANNEL_EXTERNAL_INPUTS;
  hdfsdm1_channel1.Init.Input.DataPacking = DFSDM_CHANNEL_STANDARD_MODE;
  hdfsdm1_channel1.Init.Input.Pins = DFSDM_CHANNEL_FOLLOWING_CHANNEL_PINS;
  hdfsdm1_channel1.Init.SerialInterface.Type = DFSDM_CHANNEL_SPI_RISING;
  hdfsdm1_channel1.Init.SerialInterface.SpiClock = DFSDM_CHANNEL_SPI_CLOCK_INTERNAL;
  hdfsdm1_channel1.Init.Awd.FilterOrder = DFSDM_CHANNEL_FASTSINC_ORDER;
  hdfsdm1_channel1.Init.Awd.Oversampling = 1;
  hdfsdm1_channel1.Init.Offset = 0;
  hdfsdm1_channel1.Init.RightBitShift = 0x00;
  if (HAL_DFSDM_ChannelInit(&hdfsdm1_channel1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DFSDM1_Init 2 */

  /* USER CODE END DFSDM1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x10D19CE4;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief QUADSPI Initialization Function
  * @param None
  * @retval None
  */
void MX_QUADSPI_Init(void)
{

  /* USER CODE BEGIN QUADSPI_Init 0 */

  /* USER CODE END QUADSPI_Init 0 */

  /* USER CODE BEGIN QUADSPI_Init 1 */

  /* USER CODE END QUADSPI_Init 1 */
  /* QUADSPI parameter configuration*/
  hqspi.Instance = QUADSPI;
  hqspi.Init.ClockPrescaler = 2;
  hqspi.Init.FifoThreshold = 4;
  hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
  hqspi.Init.FlashSize = 23;
  hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;
  hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
  if (HAL_QSPI_Init(&hqspi) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN QUADSPI_Init 2 */

  /* USER CODE END QUADSPI_Init 2 */

}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_4BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 7;
  hspi3.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
 void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief USB_OTG_FS Initialization Function
  * @param None
  * @retval None
  */
//static void MX_USB_OTG_FS_PCD_Init(void)
//{
//
//  /* USER CODE BEGIN USB_OTG_FS_Init 0 */
////
//  /* USER CODE END USB_OTG_FS_Init 0 */
//
//  /* USER CODE BEGIN USB_OTG_FS_Init 1 */
////
//  /* USER CODE END USB_OTG_FS_Init 1 */
//  hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
//  hpcd_USB_OTG_FS.Init.dev_endpoints = 6;
//  hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
//  hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
//  hpcd_USB_OTG_FS.Init.Sof_enable = DISABLE;
//  hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
//  hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
//  hpcd_USB_OTG_FS.Init.battery_charging_enable = DISABLE;
//  hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
//  hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
//  if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
//  {
//    Error_Handler();
//  }
//  /* USER CODE BEGIN USB_OTG_FS_Init 2 */
////
//  /* USER CODE END USB_OTG_FS_Init 2 */
//
//}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, M24SR64_Y_RF_DISABLE_Pin|M24SR64_Y_GPO_Pin|ISM43362_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, ARD_D10_Pin|SPBTLE_RF_RST_Pin|ARD_D9_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, ARD_D8_Pin|ISM43362_BOOT0_Pin|ISM43362_WAKEUP_Pin|LED2_Pin
                          |SPSGRF_915_SDN_Pin|ARD_D5_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, USB_OTG_FS_PWR_EN_Pin|PMOD_RESET_Pin|STSAFE_A100_RESET_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPBTLE_RF_SPI3_CSN_GPIO_Port, SPBTLE_RF_SPI3_CSN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, VL53L0X_XSHUT_Pin|LED3_WIFI__LED4_BLE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPSGRF_915_SPI3_CSN_GPIO_Port, SPSGRF_915_SPI3_CSN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ISM43362_SPI3_CSN_GPIO_Port, ISM43362_SPI3_CSN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : M24SR64_Y_RF_DISABLE_Pin M24SR64_Y_GPO_Pin ISM43362_RST_Pin ISM43362_SPI3_CSN_Pin */
  GPIO_InitStruct.Pin = M24SR64_Y_RF_DISABLE_Pin|M24SR64_Y_GPO_Pin|ISM43362_RST_Pin|ISM43362_SPI3_CSN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_OTG_FS_OVRCR_EXTI3_Pin SPSGRF_915_GPIO3_EXTI5_Pin SPBTLE_RF_IRQ_EXTI6_Pin ISM43362_DRDY_EXTI1_Pin */
  GPIO_InitStruct.Pin = USB_OTG_FS_OVRCR_EXTI3_Pin|SPSGRF_915_GPIO3_EXTI5_Pin|SPBTLE_RF_IRQ_EXTI6_Pin|ISM43362_DRDY_EXTI1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : BUTTON_EXTI13_Pin */
  GPIO_InitStruct.Pin = BUTTON_EXTI13_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BUTTON_EXTI13_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ARD_A5_Pin ARD_A4_Pin ARD_A3_Pin ARD_A2_Pin
                           ARD_A1_Pin ARD_A0_Pin */
  GPIO_InitStruct.Pin = ARD_A5_Pin|ARD_A4_Pin|ARD_A3_Pin|ARD_A2_Pin
                          |ARD_A1_Pin|ARD_A0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : ARD_D1_Pin ARD_D0_Pin */
  GPIO_InitStruct.Pin = ARD_D1_Pin|ARD_D0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : ARD_D10_Pin SPBTLE_RF_RST_Pin ARD_D9_Pin */
  GPIO_InitStruct.Pin = ARD_D10_Pin|SPBTLE_RF_RST_Pin|ARD_D9_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : ARD_D4_Pin */
  GPIO_InitStruct.Pin = ARD_D4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(ARD_D4_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ARD_D7_Pin */
  GPIO_InitStruct.Pin = ARD_D7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ARD_D7_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ARD_D13_Pin ARD_D12_Pin ARD_D11_Pin */
  GPIO_InitStruct.Pin = ARD_D13_Pin|ARD_D12_Pin|ARD_D11_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : ARD_D3_Pin */
  GPIO_InitStruct.Pin = ARD_D3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ARD_D3_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ARD_D6_Pin */
  GPIO_InitStruct.Pin = ARD_D6_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ARD_D6_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ARD_D8_Pin ISM43362_BOOT0_Pin ISM43362_WAKEUP_Pin LED2_Pin
                           SPSGRF_915_SDN_Pin ARD_D5_Pin SPSGRF_915_SPI3_CSN_Pin */
  GPIO_InitStruct.Pin = ARD_D8_Pin|ISM43362_BOOT0_Pin|ISM43362_WAKEUP_Pin|LED2_Pin
                          |SPSGRF_915_SDN_Pin|ARD_D5_Pin|SPSGRF_915_SPI3_CSN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : LPS22HB_INT_DRDY_EXTI0_Pin LSM6DSL_INT1_EXTI11_Pin ARD_D2_Pin PD15
                           PMOD_IRQ_EXTI12_Pin */
  GPIO_InitStruct.Pin = LPS22HB_INT_DRDY_EXTI0_Pin|LSM6DSL_INT1_EXTI11_Pin|ARD_D2_Pin|GPIO_PIN_15
                          |PMOD_IRQ_EXTI12_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_OTG_FS_PWR_EN_Pin SPBTLE_RF_SPI3_CSN_Pin PMOD_RESET_Pin STSAFE_A100_RESET_Pin */
  GPIO_InitStruct.Pin = USB_OTG_FS_PWR_EN_Pin|SPBTLE_RF_SPI3_CSN_Pin|PMOD_RESET_Pin|STSAFE_A100_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : VL53L0X_XSHUT_Pin LED3_WIFI__LED4_BLE_Pin */
  GPIO_InitStruct.Pin = VL53L0X_XSHUT_Pin|LED3_WIFI__LED4_BLE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : VL53L0X_GPIO1_EXTI7_Pin LSM3MDL_DRDY_EXTI8_Pin */
  GPIO_InitStruct.Pin = VL53L0X_GPIO1_EXTI7_Pin|LSM3MDL_DRDY_EXTI8_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PMOD_SPI2_SCK_Pin */
  GPIO_InitStruct.Pin = PMOD_SPI2_SCK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(PMOD_SPI2_SCK_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PMOD_UART2_CTS_Pin PMOD_UART2_RTS_Pin PMOD_UART2_TX_Pin PMOD_UART2_RX_Pin */
  GPIO_InitStruct.Pin = PMOD_UART2_CTS_Pin|PMOD_UART2_RTS_Pin|PMOD_UART2_TX_Pin|PMOD_UART2_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : ARD_D15_Pin ARD_D14_Pin */
  GPIO_InitStruct.Pin = ARD_D15_Pin|ARD_D14_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C2) {
        // Wecke den Sensor-Task aus der ISR auf!
    	OS_Sem_ReleaseFromISR(&SensorSemaphore);
    }
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C2) {
    	OS_Sem_ReleaseFromISR(&SensorSemaphore);
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C2) {
    	OS_Sem_ReleaseFromISR(&SensorSemaphore);
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
