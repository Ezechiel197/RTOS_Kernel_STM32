/**
 * @file MessageQueue.c
 * @brief Implementierung einer Message Queue für die Inter-Task-Kommunikation.
 *
 * Diese Datei enthält die Implementierung einer threadsicheren Message Queue basierend
 * auf einem Ringpuffer (Circular Buffer). Sie bietet Mechanismen für Non-Blocking,
 * Blocking und Timeout-basiertes Senden und Empfangen von Nachrichten.
 * Die Synchronisation erfolgt über globale Interrupt-Sperren und das Auslösen eines
 * PendSV-Interrupts für den Context Switch.
 *
 * @author ezech
 * @date 4. Juni 2026
 */

#include "MessageQueue.h"

#define Max_Task 3 ///< Maximale Anzahl an Tasks, die vom OS verwaltet werden.

/**
 * @brief Initialisiert eine Message Queue.
 *
 * Setzt alle internen Zeiger und Zähler der Queue auf 0 und verknüpft sie
 * mit dem bereitgestellten Speicherbereich.
 *
 * @param pQ Zeiger auf die zu initialisierende OS_MessageQueue_t Struktur.
 * @param pBufferArray Zeiger auf das Array, das als Ringpuffer dienen soll.
 * @param u32Capacity Maximale Anzahl an Elementen, die die Queue aufnehmen kann.
 */
void OS_MsgQ_Init(OS_MessageQueue_t* pQ, uint32_t* pBufferArray, uint32_t u32Capacity) {
    pQ->pBuffer = pBufferArray;
    pQ->u32Capacity = u32Capacity;
    pQ->u32Head = 0;
    pQ->u32Tail = 0;
    pQ->u32Count = 0;
    pQ->u32WaitingReaders = 0;
    pQ->u32WaitingWriters = 0;
}

/**
 * @brief Prüft, ob die Message Queue leer ist.
 *
 * @param pQ Zeiger auf die Message Queue.
 * @return 1 wenn die Queue leer ist, sonst 0.
 */
uint8_t OS_MsgQ_IsEmpty(OS_MessageQueue_t* pQ) {
    return (pQ->u32Count == 0);
}

/**
 * @brief Prüft, ob die Message Queue voll ist.
 *
 * @param pQ Zeiger auf die Message Queue.
 * @return 1 wenn die Queue voll ist, sonst 0.
 */
uint8_t OS_MsgQ_IsFull(OS_MessageQueue_t* pQ) {
    return (pQ->u32Count >= pQ->u32Capacity);
}

/* ===================================================================== */
/* SEND-FUNKTIONEN (Schreiben in die Queue)                              */
/* ===================================================================== */

/**
 * @brief Sendet eine Nachricht in die Queue (Non-Blocking).
 * * Versucht, eine Nachricht in die Queue zu schreiben. Wenn die Queue voll ist,
 * kehrt die Funktion sofort zurück, ohne den Task zu blockieren.
 * Weckt bei Erfolg prioritätsbasiert einen wartenden Reader-Task auf.
 *
 * @param pQ Zeiger auf die Message Queue.
 * @param u32Msg Die zu sendende Nachricht (32-Bit Wert).
 * @return 1 bei erfolgreichem Senden, 0 wenn die Queue voll war.
 */
uint8_t OS_MsgQ_SendNonBlocking(OS_MessageQueue_t* pQ, uint32_t u32Msg) {
    __disable_irq();

    if (OS_MsgQ_IsFull(pQ)) {
        __enable_irq();
        return 0; // Queue voll
    }

    pQ->pBuffer[pQ->u32Head] = u32Msg;
    pQ->u32Head = (pQ->u32Head + 1) % pQ->u32Capacity;
    pQ->u32Count++;

    // Wartenden Leser aufwecken, falls vorhanden
    if (pQ->u32WaitingReaders != 0) {
        for (uint8_t i = 0; i < Max_Task; i++) { // Prioritätsbasierter Zugriff
            if (pQ->u32WaitingReaders & (1 << i)) {
                pQ->u32WaitingReaders &= ~(1 << i);
                g_asctTCBs[i].eTaskState = TaskState_Ready;
                if (g_pCurrentTCB->eTaskState == TaskState_Running) {
                    g_pCurrentTCB->eTaskState = TaskState_Ready;
                }
                __enable_irq();
                SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; // Context Switch anfordern
                return 1;
            }
        }
    }

    __enable_irq();
    return 1;
}

/**
 * @brief Sendet eine Nachricht in die Queue (Blocking).
 *
 * @details Wenn die Queue voll ist, wird der aufrufende Task blockiert und
 * in der Bitmaske u32WaitingWriters registriert. Er wird erst wieder fortgesetzt,
 * wenn ein anderer Task eine Nachricht ausliest und Platz schafft.
 *
 * @param pQ Zeiger auf die Message Queue.
 * @param u32Msg Die zu sendende Nachricht.
 */
void OS_MsgQ_SendBlocking(OS_MessageQueue_t* pQ, uint32_t u32Msg) {
    while (1) {
        __disable_irq();

        if (!OS_MsgQ_IsFull(pQ)) {
            pQ->pBuffer[pQ->u32Head] = u32Msg;
            pQ->u32Head = (pQ->u32Head + 1) % pQ->u32Capacity;
            pQ->u32Count++;

            if (pQ->u32WaitingReaders != 0) {
                for (uint8_t i = 0; i < Max_Task; i++) {
                    if (pQ->u32WaitingReaders & (1 << i)) {
                        pQ->u32WaitingReaders &= ~(1 << i);
                        g_asctTCBs[i].eTaskState = TaskState_Ready;
                        if (g_pCurrentTCB->eTaskState == TaskState_Running) {
                            g_pCurrentTCB->eTaskState = TaskState_Ready;
                        }
                        __enable_irq();
                        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
                        return;
                    }
                }
            }
            __enable_irq();
            return;
        } else {
            // Queue ist voll, Task blockieren
            g_pCurrentTCB->eTaskState = TaskState_Blocked;
            g_pCurrentTCB->u32TimeoutTicks = 0;
            pQ->u32WaitingWriters |= (1 << g_pCurrentTCB->u8TaskId);
            __enable_irq();
            SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; // Context Switch auslösen
        }
    }
}

/**
 * @brief Sendet eine Nachricht in die Queue mit Timeout.
 *
 * @details Versucht zu schreiben. Ist die Queue voll, blockiert der Task
 * maximal für die angegebene Anzahl an Ticks. Wird er durch den SysTick-Handler
 * vorzeitig geweckt, schlägt das Senden fehl.
 *
 * @param pQ Zeiger auf die Message Queue.
 * @param u32Msg Die zu sendende Nachricht.
 * @param u32TimeoutTicks Maximale Wartezeit in OS-Ticks.
 * @return 1 bei Erfolg, 0 bei einem Timeout.
 */
uint8_t OS_MsgQ_SendTimeout(OS_MessageQueue_t* pQ, uint32_t u32Msg, uint32_t u32TimeoutTicks) {
    __disable_irq();

    if (!OS_MsgQ_IsFull(pQ)) {
        pQ->pBuffer[pQ->u32Head] = u32Msg;
        pQ->u32Head = (pQ->u32Head + 1) % pQ->u32Capacity;
        pQ->u32Count++;

        if (pQ->u32WaitingReaders != 0) {
            for (uint8_t i = 0; i < Max_Task; i++) {
                if (pQ->u32WaitingReaders & (1 << i)) {
                    pQ->u32WaitingReaders &= ~(1 << i);
                    g_asctTCBs[i].eTaskState = TaskState_Ready;
                    if (g_pCurrentTCB->eTaskState == TaskState_Running) {
                        g_pCurrentTCB->eTaskState = TaskState_Ready;
                    }
                    __enable_irq();
                    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
                    return 1;
                }
            }
        }
        __enable_irq();
        return 1;
    }

    if (u32TimeoutTicks == 0) {
        __enable_irq();
        return 0;
    }

    // Blockieren mit Zeitlimit
    g_pCurrentTCB->eTaskState = TaskState_Blocked;
    g_pCurrentTCB->u32TimeoutTicks = u32TimeoutTicks;
    pQ->u32WaitingWriters |= (1 << g_pCurrentTCB->u8TaskId);
    __enable_irq();
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;

    // HIER WACHT DER TASK WIEDER AUF
    __disable_irq();
    if (pQ->u32WaitingWriters & (1 << g_pCurrentTCB->u8TaskId)) {
        // Falls der Task noch da ist (Timeout durch SysTick)
        pQ->u32WaitingWriters &= ~(1 << g_pCurrentTCB->u8TaskId);
        __enable_irq();
        return 0;
    } else {
        // Bit wurde von Read() gelöscht (Platz wurde frei)
        pQ->pBuffer[pQ->u32Head] = u32Msg;
        pQ->u32Head = (pQ->u32Head + 1) % pQ->u32Capacity;
        pQ->u32Count++;
        g_pCurrentTCB->u32TimeoutTicks = 0;

        if (pQ->u32WaitingReaders != 0) {
            for (uint8_t i = 0; i < Max_Task; i++) {
                if (pQ->u32WaitingReaders & (1 << i)) {
                    pQ->u32WaitingReaders &= ~(1 << i);
                    g_asctTCBs[i].eTaskState = TaskState_Ready;
                    if (g_pCurrentTCB->eTaskState == TaskState_Running) {
                        g_pCurrentTCB->eTaskState = TaskState_Ready;
                    }
                    __enable_irq();
                    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
                    return 1;
                }
            }
        }
        __enable_irq();
        return 1;
    }
}

/* ===================================================================== */
/* READ-FUNKTIONEN (Lesen aus der Queue)                                 */
/* ===================================================================== */

/**
 * @brief Liest eine Nachricht aus der Queue (Non-Blocking).
 *
 * Holt die älteste Nachricht aus dem Ringpuffer, falls vorhanden.
 * Weckt bei erfolgreichem Lesen prioritätsbasiert einen wartenden Writer-Task auf.
 *
 * @param pQ Zeiger auf die Message Queue.
 * @param pDest Zeiger auf die Variable, in die die Nachricht kopiert werden soll.
 * @return 1 bei Erfolg, 0 wenn die Queue leer war.
 */
uint8_t OS_MsgQ_ReadNonBlocking(OS_MessageQueue_t* pQ, uint32_t* pDest) {
    __disable_irq();

    if (OS_MsgQ_IsEmpty(pQ)) {
        __enable_irq();
        return 0;
    }

    *pDest = pQ->pBuffer[pQ->u32Tail];
    pQ->u32Tail = (pQ->u32Tail + 1) % pQ->u32Capacity;
    pQ->u32Count--;

    // Wartenden Schreiber aufwecken, falls vorhanden
    if (pQ->u32WaitingWriters != 0) {
        for (uint8_t i = 0; i < Max_Task; i++) {
            if (pQ->u32WaitingWriters & (1 << i)) {
                pQ->u32WaitingWriters &= ~(1 << i);
                g_asctTCBs[i].eTaskState = TaskState_Ready;
                if (g_pCurrentTCB->eTaskState == TaskState_Running) {
                    g_pCurrentTCB->eTaskState = TaskState_Ready;
                }
                __enable_irq();
                SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
                return 1;
            }
        }
    }

    __enable_irq();
    return 1;
}

/**
 * @brief Liest eine Nachricht aus der Queue (Blocking).
 *
 * @details Wenn die Queue leer ist, blockiert der Task in der Bitmaske
 * u32WaitingReaders, bis ein Writer-Task eine neue Nachricht sendet.
 *
 * @param pQ Zeiger auf die Message Queue.
 * @param pDest Zeiger auf die Zielvariable für die gelesene Nachricht.
 */
void OS_MsgQ_ReadBlocking(OS_MessageQueue_t* pQ, uint32_t* pDest) {
    while (1) {
        __disable_irq();

        if (!OS_MsgQ_IsEmpty(pQ)) {
            *pDest = pQ->pBuffer[pQ->u32Tail];
            pQ->u32Tail = (pQ->u32Tail + 1) % pQ->u32Capacity;
            pQ->u32Count--;

            if (pQ->u32WaitingWriters != 0) {
                for (uint8_t i = 0; i < Max_Task; i++) {
                    if (pQ->u32WaitingWriters & (1 << i)) {
                        pQ->u32WaitingWriters &= ~(1 << i);
                        g_asctTCBs[i].eTaskState = TaskState_Ready;
                        if (g_pCurrentTCB->eTaskState == TaskState_Running) {
                            g_pCurrentTCB->eTaskState = TaskState_Ready;
                        }
                        __enable_irq();
                        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
                        return;
                    }
                }
            }
            __enable_irq();
            return;
        } else {
            // Queue ist leer, Task blockieren
            g_pCurrentTCB->eTaskState = TaskState_Blocked;
            g_pCurrentTCB->u32TimeoutTicks = 0;
            pQ->u32WaitingReaders |= (1 << g_pCurrentTCB->u8TaskId);
            __enable_irq();
            SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
        }
    }
}

/**
 * @brief Liest eine Nachricht aus der Queue mit Timeout.
 *
 * @details Versucht zu lesen. Ist die Queue leer, blockiert der Task
 * maximal für die angegebene Zeit. Erfolgt in dieser Zeit kein Schreibvorgang,
 * kehrt die Funktion mit einem Timeout-Fehler zurück.
 *
 * @param pQ Zeiger auf die Message Queue.
 * @param pDest Zeiger auf die Zielvariable für die gelesene Nachricht.
 * @param u32TimeoutTicks Maximale Wartezeit in OS-Ticks.
 * @return 1 bei erfolgreichem Lesen, 0 bei einem Timeout.
 */
uint8_t OS_MsgQ_ReadTimeout(OS_MessageQueue_t* pQ, uint32_t* pDest, uint32_t u32TimeoutTicks) {
    __disable_irq();

    if (!OS_MsgQ_IsEmpty(pQ)) {
        *pDest = pQ->pBuffer[pQ->u32Tail];
        pQ->u32Tail = (pQ->u32Tail + 1) % pQ->u32Capacity;
        pQ->u32Count--;

        if (pQ->u32WaitingWriters != 0) {
            for (uint8_t i = 0; i < Max_Task; i++) {
                if (pQ->u32WaitingWriters & (1 << i)) {
                    pQ->u32WaitingWriters &= ~(1 << i);
                    g_asctTCBs[i].eTaskState = TaskState_Ready;
                    if (g_pCurrentTCB->eTaskState == TaskState_Running) {
                        g_pCurrentTCB->eTaskState = TaskState_Ready;
                    }
                    __enable_irq();
                    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
                    return 1;
                }
            }
        }
        __enable_irq();
        return 1;
    }

    if (u32TimeoutTicks == 0) {
        __enable_irq();
        return 0;
    }

    // Blockieren mit Zeitlimit
    g_pCurrentTCB->eTaskState = TaskState_Blocked;
    g_pCurrentTCB->u32TimeoutTicks = u32TimeoutTicks;
    pQ->u32WaitingReaders |= (1 << g_pCurrentTCB->u8TaskId);
    __enable_irq();
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;

    // HIER WACHT DER TASK WIEDER AUF
    __disable_irq();
    if (pQ->u32WaitingReaders & (1 << g_pCurrentTCB->u8TaskId)) {
        // Bit ist noch da, SysTick hat den Task geweckt (Timeout)
        pQ->u32WaitingReaders &= ~(1 << g_pCurrentTCB->u8TaskId);
        __enable_irq();
        return 0;
    } else {
        // Bit wurde von Send() gelöscht (Nachricht ist da)
        *pDest = pQ->pBuffer[pQ->u32Tail];
        pQ->u32Tail = (pQ->u32Tail + 1) % pQ->u32Capacity;
        pQ->u32Count--;
        g_pCurrentTCB->u32TimeoutTicks = 0;   // damit es nicht später im SysTick_Handler auslöst

        if (pQ->u32WaitingWriters != 0) {
            for (uint8_t i = 0; i < Max_Task; i++) {
                if (pQ->u32WaitingWriters & (1 << i)) {
                    pQ->u32WaitingWriters &= ~(1 << i);
                    g_asctTCBs[i].eTaskState = TaskState_Ready;
                    if (g_pCurrentTCB->eTaskState == TaskState_Running) {
                        g_pCurrentTCB->eTaskState = TaskState_Ready;
                    }
                    __enable_irq();
                    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
                    return 1;
                }
            }
        }
        __enable_irq();
        return 1;
    }
}
