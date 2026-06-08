/**
 * @file Mutex_and_Semaphoren.c
 * @brief Implementierung von Mutexen und Semaphoren für die Task-Synchronisation.
 *
 * Diese Datei enthält die threadsicheren Implementierungen für Mutexe
 * und zählende Semaphoren. Sie ermöglichen das sichere Teilen von Ressourcen
 * sowie die Signalisierung von Events zwischen verschiedenen Tasks.
 * Die Funktionen unterstützen Non-Blocking, Blocking und Timeout-basierte Zugriffe.
 *
 * @author Ezechiel Tonkeme
 * @date 30. Mai 2026
 */

#include "tcb.h"
#include "os_sync.h"
#include "Mutex_and_Semaphoren.h"
#include "os_sync.h"

#define Max_Task 3

/* ===================================================================== */
/* MUTEX IMPLEMENTIERUNG                                                 */
/* ===================================================================== */

/**
 * @brief Initialisiert ein Mutex-Objekt.
 *
 * Setzt den Lock-Status auf frei und weist eine ungültige Owner-ID (255) zu,
 * um zu signalisieren, dass der Mutex aktuell keinem Task gehört.
 *
 * @param pMutex Zeiger auf das zu initialisierende OS_Mutex_t Objekt.
 */
void OS_Mutex_Init(OS_Mutex_t* pMutex){

pMutex->u8LockState = 0;
pMutex->u8OwnerTaskId = 255; // signalisiert, dass diese Mutex aktuell niemand gehört
pMutex->u32WaitingTasksMask = 0;
}

/**
 * @brief Fordert einen Mutex an (Non-Blocking).
 *
 * Versucht, den Mutex zu sperren. Ist dieser bereits belegt, kehrt die
 * Funktion sofort zurück, ohne den aufrufenden Task zu blockieren.
 *
 * @param pMutex Zeiger auf das angeforderte Mutex-Objekt.
 * @return 1 bei erfolgreichem Acquire, 0 wenn der Mutex bereits belegt ist.
 */
uint8_t OS_Mutex_AcquireNonBlocking(OS_Mutex_t* pMutex) {
    __disable_irq();

    if (pMutex->u8LockState == 0) { // Mutex ist frei
        pMutex->u8LockState = 1;
        pMutex->u8OwnerTaskId = g_pCurrentTCB->u8TaskId;
        __enable_irq();
        return 1; // Erfolg!
    }

    // Mutex war belegt -> Direkt abbrechen, nicht blockieren!
    __enable_irq();
    return 0; // Fehlgeschlagen
}

/**
 * @brief Fordert einen Mutex an (Blocking).
 *
 * @details Ist der Mutex belegt, wird der aktuelle Task in den Zustand
 * TaskState_Blocked versetzt und in der Wartemaske des Mutex registriert.
 * Anschließend wird ein Context Switch (PendSV) erzwungen.
 *
 * @param pMutex Zeiger auf das angeforderte Mutex-Objekt.
 */
void OS_Mutex_AcquireBlocking(OS_Mutex_t* pMutex){
	__disable_irq();

	if(pMutex->u8LockState == 0){ // Mutex ist frei
		pMutex->u8LockState = 1;
		pMutex->u8OwnerTaskId = g_pCurrentTCB->u8TaskId;
		__enable_irq();
	}
	else{ // Mutex belegt, aktuellen Task blockieren

		g_pCurrentTCB->eTaskState = TaskState_Blocked;
		pMutex->u32WaitingTasksMask |= (1 << g_pCurrentTCB->u8TaskId);
		__enable_irq();

		// Kontextwechsel erzwingen, CPU abgeben, weil der Mutex nicht frei ist
		 SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
	}
}

/**
 * @brief Fordert einen Mutex mit Timeout an.
 *
 * @details Blockiert den Task maximal für die angegebene Zeitspanne (Ticks).
 * Wird der Task durch einen anderen Task geweckt (Release), erhält er den Mutex.
 * Läuft der Timer ab (SysTick), schlägt das Acquire fehl.
 *
 * @param pMutex Zeiger auf das angeforderte Mutex-Objekt.
 * @param u32TimeoutTicks Maximale Wartezeit in OS-Ticks.
 * @return 1 bei erfolgreichem Acquire, 0 bei einem Timeout.
 */
// 					Timeout-Mutex
// Gibt 1 zurück wenn Mutex erhalten, 0 wenn Timeout abgelaufen ist
uint8_t OS_Mutex_AcquireTimeout(OS_Mutex_t* pMutex, uint32_t u32TimeoutTicks) {
    __disable_irq();

    // Direkt prüfen, ob der Mutex ohnehin frei ist
    if (pMutex->u8LockState == 0) {
        pMutex->u8LockState = 1;
        pMutex->u8OwnerTaskId = g_pCurrentTCB->u8TaskId;
        __enable_irq();
        return 1;
    }

    if (u32TimeoutTicks == 0) {
        __enable_irq();
        return 0;
    }

    //  Mutex ist belegt -> Blockieren und Countdown setzen
    g_pCurrentTCB->eTaskState = TaskState_Blocked;
    g_pCurrentTCB->u32TimeoutTicks = u32TimeoutTicks;
    pMutex->u32WaitingTasksMask |= (1 << g_pCurrentTCB->u8TaskId);

    __enable_irq();
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;

    // wenn der compiler später zurückkommt, um den Task aufzuwecken
    __disable_irq();

    // Wenn ein anderer Task "Release" gerufen hat, hat er der schlafende Task als Owner eingetragen
    if (pMutex->u8OwnerTaskId == g_pCurrentTCB->u8TaskId) {
        g_pCurrentTCB->u32TimeoutTicks = 0; // Sicherheits-Reset des Timers
        __enable_irq();
        return 1;
    } else { // falls der Timeout abgelaufen ist

        pMutex->u32WaitingTasksMask &= ~(1 << g_pCurrentTCB->u8TaskId);
        __enable_irq();
        return 0;
    }
}

/**
 * @brief Gibt einen Mutex frei.
 *
 * @details Überprüft zunächst, ob der aufrufende Task tatsächlich der Besitzer ist.
 * Sind andere Tasks in der Warteliste, wird der Mutex direkt an den wartenden Task
 * mit der entsprechenden Priorität (bzw. ID) übergeben und ein Context Switch angefordert.
 *
 * @param pMutex Zeiger auf das freizugebende Mutex-Objekt.
 */
void OS_Mutex_Release(OS_Mutex_t * pMutex){
	__disable_irq();
	// Hier wird überprüft, ob es wirklich der Besitzer, der der Mutex freigeben will

	if(pMutex->u8OwnerTaskId != g_pCurrentTCB->u8TaskId){
		__enable_irq();
		return;
	}
	// Hier wird überprüft, ob ein Task auf den Mutex in der Warteschlange
	// warte, wenn ja, wird er drangenommen
	if(pMutex->u32WaitingTasksMask != 0){
		for(uint8_t i=0; i<Max_Task; i++){
			if(pMutex->u32WaitingTasksMask & (1<<i)){ // Hier wird verglichen, ob sich die jeweiligen Tasks in der Warteliste befindet
				// Der Task aus der Warteliste austragen
				pMutex->u32WaitingTasksMask &= ~(1 << i);
				// Übergabe des Mutex an Task i
				pMutex->u8OwnerTaskId = i;
				g_asctTCBs[i].eTaskState = TaskState_Ready;
				if (g_pCurrentTCB->eTaskState == TaskState_Running) {
				    g_pCurrentTCB->eTaskState = TaskState_Ready;
				}
				__enable_irq();
				SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
				return;
			}
		}
	}else {
		// falls keine Tasks in der Warteliste sind, Muxtex komplett freigeben
		pMutex->u8LockState = 0;
		pMutex->u8OwnerTaskId = 255;
	}
	__enable_irq();
}

/* ===================================================================== */
/* SEMAPHORE IMPLEMENTIERUNG                                             */
/* ===================================================================== */

/**
 * @brief Initialisiert eine Semaphore.
 *
 * @param pSem Zeiger auf das zu initialisierende OS_Semaphore_t Objekt.
 * @param u32InitCount Anfängliche Anzahl der verfügbaren Token/Ressourcen.
 * @param u32MaxCount Maximale Anzahl an Token, die diese Semaphore aufnehmen kann.
 */
// Semaphore Implementierung
void OS_Semaphore_init(OS_Semaphore_t* pSem,uint32_t u32InitCount,uint32_t u32MaxCount){
	pSem->u32Count = u32InitCount;
	pSem->u32MaxCount = u32MaxCount;
	pSem->u32WaitingTasksMask = 0;
}

/**
 * @brief Fordert ein Semaphore-Token an (Non-Blocking).
 *
 * @param pSem Zeiger auf das angeforderte Semaphore-Objekt.
 * @return 1 wenn ein Token erfolgreich abgezogen wurde, 0 wenn kein Token verfügbar war.
 */
// Gibt 1 zurück wenn Token erhalten, 0 wenn kein Token da war
uint8_t OS_Sem_AcquireNonBlocking(OS_Semaphore_t* pSem) {
    __disable_irq();

    if (pSem->u32Count > 0) { // Token ist vorhanden
        pSem->u32Count--;
        __enable_irq();
        return 1;
    }

    // Keine Token da, Direkt abbrechen
    __enable_irq();
    return 0;
}

/**
 * @brief Fordert ein Semaphore-Token an (Blocking).
 *
 * @details Wenn keine Token verfügbar sind (Count == 0), wird der Task blockiert
 * und ein Context Switch ausgelöst, um CPU-Zeit zu sparen.
 *
 * @param pSem Zeiger auf das angeforderte Semaphore-Objekt.
 */
// Hier wird auf einen Token/Events der Semaphore zugegriffen
void OS_Sem_AcquireBlocking(OS_Semaphore_t* pSem){
	__disable_irq();
	if(pSem->u32Count > 0){
		pSem->u32Count--;
		__enable_irq();
	}
	else {
		// Falls keine Token da sind, Task blockieren
		g_pCurrentTCB->eTaskState = TaskState_Blocked;
		pSem->u32WaitingTasksMask |= (1<< g_pCurrentTCB->u8TaskId);

		__enable_irq();
		// Context Wechsel, damit die Zeit der CPU nicht unnötig verschwendet wird
		SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
	}
}

/**
 * @brief Fordert ein Semaphore-Token an mit Timeout.
 *
 * @details Versetzt den Task in den Blocked-Status, falls kein Token vorhanden ist.
 * Der Task wacht entweder durch ein Timeout (ausgelöst vom SysTick) oder durch
 * die Freigabe eines Tokens (Release) wieder auf.
 *
 * @param pSem Zeiger auf das angeforderte Semaphore-Objekt.
 * @param u32TimeoutTicks Maximale Wartezeit in OS-Ticks.
 * @return 1 wenn das Token innerhalb der Zeit erhalten wurde, 0 bei Timeout.
 */
// 				 					Timeout-Semaphore
// Gibt 1 zurück wenn Token erhalten, 0 wenn Timeout abgelaufen ist
uint8_t OS_Sem_AcquireTimeout(OS_Semaphore_t* pSem, uint32_t u32TimeoutTicks) {
    __disable_irq();

    //  Direkt prüfen, ob Token vorhanden sind
    if (pSem->u32Count > 0) {
        pSem->u32Count--;
        __enable_irq();
        return 1;
    }

    if (u32TimeoutTicks == 0) {
        __enable_irq();
        return 0;
    }

    //  Keine Token da, Blockieren und Countdown setzen
    g_pCurrentTCB->eTaskState = TaskState_Blocked;
    g_pCurrentTCB->u32TimeoutTicks = u32TimeoutTicks;
    pSem->u32WaitingTasksMask |= (1 << g_pCurrentTCB->u8TaskId);

    __enable_irq();
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;

    // 	 HIER WACHT DER TASK WIEDER AUF
    __disable_irq();


    // falls ein erfolgreicher Release der Bit aus der Warteliste gelöscht hat.
    if (pSem->u32WaitingTasksMask & (1 << g_pCurrentTCB->u8TaskId)) {
        // Das Bit ist noch drin(Timeout)
        // Das bit aus der Warteliste gelöscht
        pSem->u32WaitingTasksMask &= ~(1 << g_pCurrentTCB->u8TaskId);
        __enable_irq();
        return 0;
    } else {
        // Ein anderer Task hat Release gerufen, und somit der Task geweckt

        g_pCurrentTCB->u32TimeoutTicks = 0;
        __enable_irq();
        return 1;
    }
}

/**
 * @brief Gibt ein Semaphore-Token frei oder signalisiert ein Event.
 *
 * @details Wenn sich Tasks in der Warteschlange befinden, wird der Zähler (Count)
 * nicht erhöht. Stattdessen wird direkt ein wartender Task geweckt (TaskState_Ready)
 * und ein Context Switch angefordert. Andernfalls wird der Token-Zähler inkrementiert.
 *
 * @param pSem Zeiger auf das freizugebende Semaphore-Objekt.
 */
void OS_Sem_Release(OS_Semaphore_t* pSem){
	__disable_irq();
	if(pSem->u32WaitingTasksMask != 0){
		// Überprüfen, ob ein Task auf das Event wartet
		for(uint8_t i=0; i< Max_Task;i++){
			if(pSem->u32WaitingTasksMask & (1<<i)){
				pSem->u32WaitingTasksMask &= ~(1 << i);
				g_asctTCBs[i].eTaskState = TaskState_Ready;

				if(g_pCurrentTCB->eTaskState == TaskState_Running){
					g_pCurrentTCB->eTaskState = TaskState_Ready;
				}
				__enable_irq();
				SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
				return;
			}
		}
	}else { // Falls nicht, der Token oder Events-zahl im Semaphore wird erhöht
		if(pSem->u32Count < pSem->u32MaxCount){
			pSem->u32Count++;
		}
	}
	__enable_irq();
}
