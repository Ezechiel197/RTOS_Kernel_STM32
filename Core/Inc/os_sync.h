/*
 * os_sync.h
 *
 *  Created on: May 30, 2026
 *      Author: ezech
 */

#ifndef INC_OS_SYNC_H_
#define INC_OS_SYNC_H_

typedef struct{
	uint8_t u8LockState; 			// 0 = Frei, 1 = Gesperrt
	uint8_t u8OwnerTaskId;
	uint32_t u32WaitingTasksMask ; // Bitmaske für blockierte Tasks
}OS_Mutex_t;

typedef struct{
	uint32_t u32Count;		// Aktueller Zählerstand
	uint32_t u32MaxCount;		 // Maximaler Zählerstand
	uint32_t u32WaitingTasksMask; // Bitmaske für blockierte Tasks
}OS_Semaphore_t;


typedef struct {
    uint32_t* pBuffer;            // Zeiger auf einem Array
    uint32_t u32Capacity;         // Maximale Anzahl an Nachrichten im Array
    uint32_t u32Head;             // Schreib-Index
    uint32_t u32Tail;             // Lese-Index
    uint32_t u32Count;            // Aktuelle Anzahl an Nachrichten im Puffer
    uint32_t u32WaitingReaders;
    uint32_t u32WaitingWriters;
} OS_MessageQueue_t;
#endif /* INC_OS_SYNC_H_ */
