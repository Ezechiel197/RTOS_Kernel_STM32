/**
 * @file os_kernel.c
 * @brief Kernfunktionen des Real-Time Operating Systems (RTOS).
 *
 * Diese Datei enthält die Kernverwaltung des RTOS, einschließlich der
 * Task-Erstellung, der Initialisierung des Schedulers, der Kontextwechsel-Logik
 * sowie der Delay-Funktionalität. Der Scheduler implementiert ein präemptives
 * Round-Robin (RR) Verfahren unter Berücksichtigung eines Idle-Tasks.
 *
 * @author Ezechiel Tonkeme
 * @date 10. Mai 2026
 */

#include "stm32l4xx.h"
#include "os_kernel.h"
#include "tcb.h"
#define Max_Task 3 ///< Maximale Anzahl an Tasks, die vom OS verwaltet werden (inkl. Idle-Task).

TCB_sctTCB_t g_asctTCBs[Max_Task]; ///< Array zur Speicherung der Task Control Blocks (TCBs).
static uint8_t g_u8TaskCount = 0;  ///< Zähler für die aktuell erstellten Tasks.
TCB_sctTCB_t* g_pCurrentTCB;       ///< Zeiger auf den TCB des aktuell auszuführenden Tasks.
extern uint32_t MAX_TASKS;         ///< Externe Variable, definiert die absolute maximale Task-Anzahl.

//static uint8_t currentTaskId = 0;

/* Funktionsprototyp für den Assembler-Teil */
void OS_StartFirstTask(void);
// Funktionszeiger
typedef void (*OS_TaskFunction_t)(void);

/**
 * @brief Erstellt einen neuen Task und initialisiert dessen Stack.
 *
 * @details Diese Funktion weist dem neuen Task einen TCB zu und bereitet den
 * Stack-Frame so vor, als wäre der Task durch einen Interrupt unterbrochen worden.
 * Dies umfasst den Hardware-Stack (xPSR, PC, LR, R12, R3-R0) sowie den
 * Software-Stack (R11-R4) für die ARM Cortex-M Architektur.
 *
 * @param pTaskCode Funktionszeiger auf die auszuführende Task-Funktion.
 * @param u8Prio Priorität des Tasks (niedriger Wert = höhere Priorität).
 * @param u8TaskId Eindeutige ID des Tasks.
 * @return 0 bei erfolgreicher Erstellung, 1 wenn die maximale Task-Anzahl (Max_Task) erreicht ist.
 */
uint8_t OS_TaskCreate(OS_TaskFunction_t pTaskCode, uint8_t u8Prio, uint8_t u8TaskId){
    if(g_u8TaskCount >= Max_Task ) return 1; // Buffer ist voll

    TCB_sctTCB_t *pNewTask = &g_asctTCBs[g_u8TaskCount];
    pNewTask->u8TaskId = u8TaskId;
    pNewTask->u8TaskPrio = u8Prio;
    pNewTask->eTaskState = TaskState_Blocked;
    pNewTask->u32DelayTicks = 0;
    pNewTask->u32TimeoutTicks = 0;


    uint32_t *pStack = &pNewTask->au32TaskStack[TCB_TASK_STACK_SIZE];

    // 1. Hardware Stack Frame (Wird beim bx lr automatisch von der CPU geladen)
    pStack--; *pStack = 0x01000000;          // xPSR (Thumb-Bit MUSS 1 sein)
    pStack--; *pStack = (uint32_t)pTaskCode; // PC (Startadresse des Tasks)
    pStack--; *pStack = 0xFFFFFFF9;          // LR (EXC_RETURN: Zurück in Thread Mode mit PSP)
    pStack--; *pStack = 0x00000000;          // R12
    pStack--; *pStack = 0x00000000;          // R3
    pStack--; *pStack = 0x00000000;          // R2
    pStack--; *pStack = 0x00000000;          // R1
    pStack--; *pStack = 0x00000000;          // R0

    // 2. Software Stack Frame
    pStack--; *pStack = 0x00000000;          // R11
    pStack--; *pStack = 0x00000000;          // R10
    pStack--; *pStack = 0x00000000;          // R9
    pStack--; *pStack = 0x00000000;          // R8
    pStack--; *pStack = 0x00000000;          // R7
    pStack--; *pStack = 0x00000000;          // R6
    pStack--; *pStack = 0x00000000;          // R5
    pStack--; *pStack = 0x00000000;          // R4

    // Aktuellen Stack-Pointer im TCB speichern
    pNewTask->u32TaskSP = (uint32_t)pStack;

    g_u8TaskCount++;

    return 0; //Erfolg
}

/**
 * @brief Startet den RTOS-Kernel und übergibt die Kontrolle an den ersten Task.
 *
 * @details Sucht den Task mit der höchsten Priorität (niedrigster Zahlenwert),
 * setzt die PendSV-Priorität auf den niedrigsten Wert (um Interrupt-Konflikte
 * zu vermeiden), konfiguriert den SysTick-Timer für eine Millisekunde
 * und führt den Assembler-Sprung in den ersten Task aus.
 */
void OS_KernelStart(void){
    if(g_u8TaskCount == 0) return;

    // Task mit der höchsten Prio suchen
    TCB_sctTCB_t *pPrioTask = &g_asctTCBs[0];

    for (uint8_t i = 1; i < g_u8TaskCount; i++) {
        //  Kleinere Zahl = Höhere Priorität
        if (g_asctTCBs[i].u8TaskPrio < pPrioTask->u8TaskPrio) {
            pPrioTask = &g_asctTCBs[i];
        }
    }

    g_pCurrentTCB = pPrioTask;
    g_pCurrentTCB->eTaskState = TaskState_Running;

    // PendSV Priorität auf den niedrigsten Wert setzen (0xF)
        NVIC_SetPriority(PendSV_IRQn, 15);
    // SysTick starten
    HAL_SYSTICK_Config(SystemCoreClock/1000);

    // Ersten Task anspringen
    OS_StartFirstTask();

}

/**
 * @brief Führt die Task-Planung (Scheduling) durch.
 *
 * @details Implementiert ein Round-Robin-Scheduling. Es wird im Array der TCBs
 * zyklisch nach dem nächsten Task im Zustand `TaskState_Ready` gesucht. Der Idle-Task
 * (letzter Task im Array) wird dabei zunächst übersprungen. Findet sich kein
 * Ready-Task, wird der Idle-Task in den Zustand `TaskState_Running` versetzt.
 */
void OS_Scheduler(void) {  // Hier wird der Schedulder Präemptives RR implementiert


	uint8_t u8nextTaskID = g_pCurrentTCB->u8TaskId;

	for( int i = 0; i < MAX_TASKS; i++){

		u8nextTaskID = (u8nextTaskID + 1) % MAX_TASKS; // somit fragen wir in einer Schleife alle Task ab

		if(u8nextTaskID == (MAX_TASKS-1) ) {continue;}  // hier wird überptüft, ob wir gerade im IDLE-Task sind, wenn ja, soll die
														// soll i um 1 erhöht werden.

		if(g_asctTCBs[u8nextTaskID].eTaskState == TaskState_Ready){    // Hier wird überprüft, ist der aktuelle Task in Zustand Ready ?
																	// wenn ja, wird es im Zustand Running gesetzt.
			g_pCurrentTCB = &g_asctTCBs[u8nextTaskID];
			g_pCurrentTCB->eTaskState = TaskState_Running;

			return;
		}
	}


	// falls keine der Tasks der Zustand Ready hat, wird der IDLE-Task aufgerufen
	if(g_asctTCBs[u8nextTaskID].eTaskState == TaskState_Blocked){

		g_pCurrentTCB = &g_asctTCBs[MAX_TASKS-1];  // der MAX_TASKS-1 ist der ID-Nummer der IDLE-Task
		g_pCurrentTCB->eTaskState = TaskState_Running;
	}

//    uint8_t u8CurrentIndex = 0;
//
//    // Finde den Index des aktuell laufenden Tasks
//    for (uint8_t i = 0; i < g_u8TaskCount; i++) {
//        if (&g_asctTCBs[i] == g_pCurrentTCB) {
//            u8CurrentIndex = i;
//            break;
//        }
//    }
//
//    // Zum nächsten Task wechseln (Round Robin)
//    u8CurrentIndex++;
//    if (u8CurrentIndex >= g_u8TaskCount) {
//        u8CurrentIndex = 0; // Wieder von vorne anfangen
//    }
//
//    // Aktuellen Task blockieren, neuen auf Running setzen
//    g_pCurrentTCB->eTaskState = TaskState_Blocked;
//    g_pCurrentTCB = &g_asctTCBs[u8CurrentIndex];
//    g_pCurrentTCB->eTaskState = TaskState_Running;
}

/**
 * @brief  Startet den ersten Task (Assembler-Einsprung).
 * * @details Das Attribut `__attribute__((naked))` verhindert, dass der C-Compiler
 * eigenen Stack-Code hinzufügt, der unsere Register durcheinanderbringt.
 * Die Funktion lädt den Stackpointer des ersten Tasks, stellt die Register wieder her
 * und führt einen Branch (`bx r3`) zur Startadresse aus.
 */
__attribute__((naked)) void OS_StartFirstTask(void)
{
    __asm volatile (

    	        "ldr r0, =g_pCurrentTCB \n"   // Lade Adresse des TCB-Pointers
    	        "ldr r1, [r0]           \n"   // Dereferenziere: r1 = Adresse des aktuellen TCBs
    	        "ldr r2, [r1]           \n"   // Lade erstes Element des TCBs (u32TaskSP) in r2

    	        "msr msp, r2            \n"   // Schreibe r2 in den Process Stack Pointer (PSP)

    	    	"pop {r4-r11} \n"
    	    	"pop {r0-r3, r12, lr} \n"
    	    	"pop {r3} \n"
    	    	"pop {r2} \n"

    	    	"msr apsr_nzcvq, r2 \n" // Hier werden die Flags wieder hergestellt
    	    	"bx r3 \n"

    );
}


/**
 * @brief Blockiert den aktuellen Task für eine definierte Anzahl an Ticks.
 *
 * @details Setzt den Task in den Zustand `TaskState_Blocked`, weist ihm die
 * übergebene Wartezeit zu und erzwingt sofort einen Context Switch durch Auslösen
 * des PendSV-Interrupts. Dadurch wird die CPU an den nächsten bereiten Task abgegeben.
 * Interrupts werden kurzzeitig deaktiviert, um Dateninkonsistenzen zu vermeiden.
 *
 * @param tick_time Anzahl der OS-Ticks (Millisekunden), für die der Task pausieren soll.
 */
void OS_Delay(uint32_t tick_time) // Diese Funktion wird aufgerufen, wenn der Task vollständigt verarbeitet wurden und
									// für die nächste x Milisekunde der CPU in Schlafmodus geht und die CPU abgibt
{
	if(tick_time == 0) return ;

	__disable_irq(); // Interrupt ausschalten, um das Verfahren nicht zu stören

	g_pCurrentTCB->eTaskState = TaskState_Blocked;
	g_pCurrentTCB->u32DelayTicks = tick_time;

	__enable_irq();

		//OS_Scheduler();
	// Context Switch erzwingen (CPU an den nächsten Task abgeben)
	    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}
