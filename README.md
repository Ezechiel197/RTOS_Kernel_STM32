# RTOS_Kernel_STM32

Custom-built Real-Time Operating System (RTOS) kernel for ARM Cortex-M4 (STM32L4) featuring inter-task communication, synchronization primitives, non-blocking sensor integration (HTS221 via I2C interrupts), real-time tracing with **SEGGER SystemView**, and runtime specification monitoring with **TeSSLa**.

---

## Table of Contents

1. [Project Overview & Requirements](#project-overview--requirements)
2. [System Architecture & Tasks](#system-architecture--tasks)
3. [Kernel & Scheduling (`os_kernel.c`, `stm32l4xx_it.c`)](#kernel--scheduling)
4. [Synchronization & Inter-Task Communication](#synchronization--inter-task-communication)
   - [Message Queue (`MessageQueue.c`)](#message-queue)
   - [Mutex & Semaphore (`Mutex_and_Semaphoren.c`)](#mutex--semaphore)
5. [Sensors & Hardware Integration (`main.c`)](#sensors--hardware-integration)
6. [Tracing & Verification (SystemView & TeSSLa)](#tracing--verification-systemview--tessla)



---

## Project Overview & Requirements

The project covers a complete embedded RTOS workflow:

- **Sensor Integration (HTS221):** Temperature and humidity sensor connected via I2C (`I2C2`), controlled using asynchronous interrupt-driven transfers (`_IT`) and a dedicated `Task_Sensor`.
- **Serial Output (`Task_UART`):** Decoupled formatting and transmission of sensor readings (or hardware error codes upon communication failures) via `USART1` (`115200 Baud`).
- **Inter-Task Communication:** Transfer of sensor data packets (`SensorMsg_t`) through a circular-buffer `MessageQueue` (`SensorQueue`, capacity: `5`).
- **Periodic Acquisition & Diagnostics:** Cyclic measurement/transmission (`OS_Delay(2000)` and `100 ms` conversion wait) along with error handling including `HAL_I2C_GetError(&hi2c2)`.
- **Tracing & Rule Checking:** Runtime and event tracing via SEGGER SystemView as well as monitoring of OS primitives and events using TeSSLa (`generated_rules.txt`).

---

## System Architecture & Tasks

Currently, the kernel manages `MAX_TASKs = 3` tasks:

| Task Function | Role / Responsibility | Synchronization & Outputs |
| :--- | :--- | :--- |
| `Task_Sensor` | Initializes/reads HTS221 calibration data, triggers a one-shot measurement via `HAL_I2C_Mem_Write_IT`, waits using `OS_Sem_AcquireBlocking(&SensorSemaphore)` and `OS_Delay(100)`, reads raw data via `HAL_I2C_Mem_Read_IT`, linearly interpolates °C and % rH, and sends `SensorMsg_t` to `SensorQueue`. | Waits on `SensorSemaphore` (released from I2C callbacks), sends to `SensorQueue`, uses marker `SYSVIEW_MARKER_I2C_WAIT`. |
| `Task_UART` | Blocks on `SensorQueue` (`OS_MsgQ_ReadBlocking`), turns on status LED (`GPIOB Pin 14`), formats sensor values or error codes, and transmits over `HAL_UART_Transmit(&huart1, ...)`. | Receives from `SensorQueue`, uses marker `SYSVIEW_MARKER_UART_TX`. |
| `Task_Idle` | Runs when no application tasks are in `Ready` state; reports idle state to SystemView (`SEGGER_SYSVIEW_OnIdle()`). | Fallback task (`Max_Task - 1`). |

---

## Kernel & Scheduling

- **Task Control Block (TCB) & Stack Initialization (`OS_TaskCreate`):**
  - Each task has a TCB (`g_asctTCBs`) storing its ID, priority (`u8TaskPrio`, lower value = higher priority), state (`TaskState_Ready`, `TaskState_Blocked`, `TaskState_Running`), `u32DelayTicks`, `u32TimeoutTicks`, dedicated stack (`au32TaskStack`), and saved stack pointer (`u32TaskSP`).
  - Initializes an initial hardware stack frame (`xPSR` with Thumb bit `0x01000000`, `PC`, `LR = 0xFFFFFFF9`, `R12`, `R3–R0`) and software stack frame (`R11–R4`).
- **Kernel Startup (`OS_KernelStart`):**
  - Selects the highest-priority task, sets `PendSV` priority to the lowest level (`15`), configures `SysTick` for `1 ms` (`SystemCoreClock / 1000`), and jumps into the first task via `OS_StartFirstTask()`.
- **`__attribute__((naked))` in `OS_StartFirstTask` and `PendSV_Handler`:**
  - Prevents compiler-generated prologue/epilogue code so that stack pointers (`MSP`), registers (`R4–R11`, `APSR`), and branches (`bx r3` / `bx lr`) remain under exact manual control.
- **Preemptive Round-Robin & Time-Slicing (`SysTick_Handler` & `OS_Scheduler`):**
  - `SysTick_Handler` decrements `u32TimeoutTicks` and `u32DelayTicks` and wakes expired tasks (`TaskState_Ready`).
  - A `TIME_SLICE_TICKS` counter (`5` ticks = `5 ms`) triggers a `PendSV` context switch via `SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk` when the slice expires.
  - `OS_Scheduler()` cyclically searches for the next `TaskState_Ready` task (skipping the idle task on the initial pass) and falls back to `Task_Idle` if none are ready.
- **Blocking Delay (`OS_Delay`):**
  - Marks the current task as `TaskState_Blocked`, sets `u32DelayTicks`, and triggers `PendSV` to yield the CPU immediately.

---

## Synchronization & Inter-Task Communication

### Message Queue
Implemented as a circular buffer (`OS_MessageQueue_t`) with `u32Head`, `u32Tail`, `u32Count`, `u32Capacity`, and waiting bitmasks for readers (`u32WaitingReaders`) and writers (`u32WaitingWriters`):

- **API Variants:** `OS_MsgQ_SendNonBlocking`, `OS_MsgQ_SendBlocking`, `OS_MsgQ_SendTimeout`, and `OS_MsgQ_ReadNonBlocking`, `OS_MsgQ_ReadBlocking`, `OS_MsgQ_ReadTimeout`.
- **Waking & Context Switch:** On send/read, waiting tasks in the corresponding bitmask are transitioned to `TaskState_Ready` in ascending task-ID order and a `PendSV` context switch is requested.
- **SystemView Events:** `SYSVIEW_EV_QUEUE_SEND` (`40`) and `SYSVIEW_EV_QUEUE_RECEIVE` (`41`).

### Mutex & Semaphore
- **Mutex (`OS_Mutex_t`):**
  - Tracks ownership (`u8OwnerTaskId`, `255` = unowned) and lock state (`u8LockState`) so only the owning task can release it (`OS_Mutex_Release`).
  - Supports `AcquireNonBlocking`, `AcquireBlocking`, `AcquireTimeout`, and `Release`.
  - SystemView Events: `SYSVIEW_EV_MUTEX_TAKE` (`32`), `SYSVIEW_EV_MUTEX_GIVE` (`33`).
- **Semaphore (`OS_Semaphore_t`):**
  - Counting semaphore without ownership (`u32Count`, `u32MaxCount`, `u32WaitingTasksMask`).
  - Supports `AcquireNonBlocking`, `AcquireBlocking`, `AcquireTimeout`, `Release`, and `OS_Sem_ReleaseFromISR` for ISR callbacks.
  - SystemView Events: `SYSVIEW_EV_SEMAPHORE_WAIT` (`34`), `SYSVIEW_EV_SEMAPHORE_POST` (`35`).

---

## Sensors & Hardware Integration

- **Calibration (`HTS221_Read_Calibration`):** Reads 16 calibration bytes from the HTS221 NVM in auto-increment mode (`0x30 | 0x80`) and extracts `H0_rH_x2`, `H1_rH_x2`, `H0_out`, `H1_out`, `T0_degC_x8`, `T1_degC_x8`, `T0_out`, and `T1_out`.
- **Asynchronous Measurement (`Task_Sensor`):**
  1. Enables the sensor (`CTRL_REG1 = 0x20`, value `0x81`) and reads calibration coefficients.
  2. Starts a one-shot conversion (`CTRL_REG2 = 0x21`, bit 0 = `0x01`) using `HAL_I2C_Mem_Write_IT` and blocks on `SensorSemaphore`.
  3. Waits `100 ms` via `OS_Delay(100)` for sensor conversion.
  4. Reads 6 bytes starting at register `0x28 | 0x80` via `HAL_I2C_Mem_Read_IT` and blocks again on `SensorSemaphore`.
  5. Computes temperature (°C) and relative humidity (% rH) via linear interpolation and enqueues the message into `SensorQueue`.
- **I2C Callbacks (`main.c`):** `HAL_I2C_MemRxCpltCallback`, `HAL_I2C_MemTxCpltCallback`, and `HAL_I2C_ErrorCallback` call `OS_Sem_ReleaseFromISR(&SensorSemaphore)` for `I2C2`.

---

## Tracing & Verification (SystemView & TeSSLa)

- **SEGGER SystemView:** Traces task execution starts/stops, idle periods, ISR entry/exit (`SysTick`), queue/mutex/semaphore events, and execution intervals via markers (`SYSVIEW_MARKER_I2C_WAIT = 10`, `SYSVIEW_MARKER_UART_TX = 11`).
- **TeSSLa Specification (`generated_rules.txt`):** Monitors the `events: Events[String]` stream for:
  - **Queue state (`queue_count`, capacity `5`):** Detects `queue_overflow` (`> 5`) and `queue_underflow` (`< 0`).
  - **Mutex state (`mutex_state`):** Checks for `mutex_error_double_take` (`> 1`) and `mutex_error_invalid_give` (`< 0`).
  - **Semaphore count (`sem_count`):** Checks for `sem_underflow` (`< 0`).
  - **Forwarded marker events:** `ev_i2c_wait`, `ev_uart_tx`.

---


## :man_technologist: **Author**

**Ezechiel Tonkeme** 

---

## Pictures

<img width="964" height="556" alt="image" src="https://github.com/user-attachments/assets/ccf102fb-76d3-498d-9105-00c219b31668" />


