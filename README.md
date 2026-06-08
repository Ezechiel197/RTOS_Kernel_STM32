## :sparkles:**Overview**
This repository contains a lightweight, custom preemptive Real-Time Operating System (RTOS) kernel tailored for ARM Cortex-M4 microcontrollers (STM32L4xx). The operating system implements a preemptive Round-Robin scheduling algorithm with configurable time slices. It handles distinct task states (Running, Ready, Blocked) and provides deterministic delay tracking via the SysTick timer. An integrated Idle task guarantees continuous and safe CPU execution even when all user application tasks are currently blocked.

---

## **Key Features**
- **Preemptive Round-Robin Scheduling:** Time-sliced task execution ensuring fair CPU allocation based on `TIME_SLICE_TICKS`.
  
- **Priority-Based Bootstrapping:** Automatically detects and schedules the highest priority task upon kernel initialization.
  
- **Non-Blocking Delay Tracking:** Features an `OS_Delay` mechanism and independent timeout management (`u32TimeoutTicks`) to gracefully transition tasks to a `Blocked` state, preserving processing power.
  
- **Implicit Idle Task:** Seamlessly cascades into a dedicated fallback Idle Task (`MAX_TASKS - 1`) whenever the system experiences zero ready application tasks.
  
- **Low-Level Context Switching:** Leverages the PendSV exception combined with naked assembly routines (`__attribute__((naked))`) for precise, overhead-free register preservation and restoration.

---

## **Architecture & Stack Design**
Every task is allocated an isolated stack boundary mapped within its own Task Control Block (`TCB_sctTCB_t`). During the initialization phase via `OS_TaskCreate`, the stack structure is manually constructed to perfectly mirror an active hardware interrupt stack frame.

## **Software & Hardware Stack Frames**
The ARM Cortex-M4 hardware natively bifurcates exception context saving into two distinct stages:
1. **Hardware Stack Frame:** Triggered instantly by the CPU core when an exception occurs, pushing `xPSR`, `PC`, `LR`, `R12`, `R3`, `R2`, `R1`, and `R0`. The Thumb bit within the `xPSR` must always be initialized to `1`.
2. **Software Stack Frame:** The remaining core registers—`R11` down to `R4`—are manually pushed and popped within the assembly block of the `PendSV_Handler` to completely freeze or resume the task's context state.


Highest Memory Address (Stack Top)
+-------------------+
|   xPSR (0x010000) |  <- Hardware Frame (Saved automatically by CPU)
|   PC (TaskCode)   |
|   LR (EXC_RETURN) |
|   R12             |
|   R3 - R0         |
+-------------------+
|   R11 - R4        |  <- Software Frame (Saved manually in PendSV)
+-------------------+  <- Current u32TaskSP
Lowest Memory Address



## **File Structure**
- **`os_kernel.c` / `os_kernel.h`:** Contains the system core logic, task creation APIs, scheduling loops, time delays, and the naked assembly bootstrapper `OS_StartFirstTask`.
  
- **`tcb.h`:** Declares the Task Control Block structure layout (`TCB_sctTCB_t`), state enums (`TaskState_e`), and stack dimension constants.
  
- **`stm32l4xx_it.c`:** Houses the critical low-level interrupt service routines: `PendSV_Handler` for context switching execution and `SysTick_Handler` for managing system ticks and slices.

---

## **Doxygen-Dokumentation / Documentation**

Das gesamte Projekt ist vollständig nach dem **Doxygen-Standard** dokumentiert. Die Quellcodedokumentation kann lokal über die HTML-Struktur eingesehen werden.

## *Generierung & Anzeige:*
1. Navigiere in den Ausgabeordner des Projekts (z.B. `doc/html/` oder `html/`).
2. Öffne die Datei **`index.html`** in einem beliebigen Webbrowser.
3. Über das Navigationsmenü stehen detaillierte Beschreibungen aller API-Funktionen, Variablenstrukturen sowie automatisch generierte Funktionsaufrufbäume (Call Graphs / Dependency Graphs) über Graphviz bereit.

---

## :man_technologist: **Author**

**Ezechiel Tonkeme**
