#include "lock.h"
#include <stdio.h>
#include <string.h>

Lock lockTable[MAX_LOCKS];

void initLockManager(void) {
    for (int i = 0; i < MAX_LOCKS; i++) {
        lockTable[i].recordId = i;
        lockTable[i].locked = 0;
        lockTable[i].mode = LOCK_NONE;
        lockTable[i].ownerTxId = 0;
    }
}

int acquireLock(int id) {
    return acquireLockEx(id, LOCK_EXCLUSIVE, 1);
}

int acquireLockEx(int id, LockMode mode, int txId) {
    if (id < 0 || id >= MAX_LOCKS) {
        printf("[LOCK] Record ID %d out of bounds (0-%d).\n", id, MAX_LOCKS - 1);
        return 0;
    }

    if (lockTable[id].locked) {
        if (lockTable[id].ownerTxId == txId) {
            // Already owned by same transaction
            if (mode == LOCK_EXCLUSIVE) {
                lockTable[id].mode = LOCK_EXCLUSIVE;
            }
            return 1;
        }
        if (lockTable[id].mode == LOCK_SHARED && mode == LOCK_SHARED) {
            // Shared lock compatibility
            return 1;
        }
        printf("[LOCK] Record %d currently locked by Tx #%d (%s). Waiting...\n", 
               id, lockTable[id].ownerTxId, lockTable[id].mode == LOCK_EXCLUSIVE ? "X-Lock" : "S-Lock");
        return 0;
    }

    lockTable[id].locked = 1;
    lockTable[id].mode = mode;
    lockTable[id].ownerTxId = txId;
    return 1;
}

void releaseLock(int id) {
    if (id < 0 || id >= MAX_LOCKS) return;
    lockTable[id].locked = 0;
    lockTable[id].mode = LOCK_NONE;
    lockTable[id].ownerTxId = 0;
}

void releaseTxLocks(int txId) {
    for (int i = 0; i < MAX_LOCKS; i++) {
        if (lockTable[i].ownerTxId == txId) {
            releaseLock(i);
        }
    }
}

void printLockStatus(void) {
    printf("=== Active Locks Table ===\n");
    int activeCount = 0;
    for (int i = 0; i < MAX_LOCKS; i++) {
        if (lockTable[i].locked) {
            printf("  Record ID %d: Locked by Tx #%d [%s]\n", 
                   i, lockTable[i].ownerTxId, 
                   lockTable[i].mode == LOCK_EXCLUSIVE ? "EXCLUSIVE (X)" : "SHARED (S)");
            activeCount++;
        }
    }
    if (activeCount == 0) {
        printf("  (No active locks)\n");
    }
    printf("==========================\n");
}