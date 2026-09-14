#include <stdio.h>
#include <string.h>
#include "transaction.h"
#include "db.h"
#include "parser.h"

TransactionContext currentTx = {
    .active = 0,
    .txId = 0,
    .undoCount = 0
};

TransactionTask queue[MAX_TRANSACTIONS];
int front = 0, rear = 0;

int txBegin(void) {
    if (currentTx.active) {
        printf("[TRANSACTION] Warning: Transaction #%d already active.\n", currentTx.txId);
        return -1;
    }
    currentTx.active = 1;
    currentTx.txId++;
    currentTx.undoCount = 0;
    if (DEBUG_MODE) {
        printf("[PLAN] Transaction #%d started.\n", currentTx.txId);
    }
    return currentTx.txId;
}

int txCommit(void) {
    if (!currentTx.active) {
        printf("[TRANSACTION] No active transaction to commit.\n");
        return -1;
    }
    int id = currentTx.txId;
    currentTx.active = 0;
    currentTx.undoCount = 0;
    if (DEBUG_MODE) {
        printf("[PLAN] Transaction #%d committed.\n", id);
    }
    return 0;
}

int txRollback(void) {
    if (!currentTx.active) {
        printf("[TRANSACTION] No active transaction to rollback.\n");
        return -1;
    }

    if (DEBUG_MODE) {
        printf("[PLAN] Rolling back Transaction #%d (%d undo operations)...\n", 
               currentTx.txId, currentTx.undoCount);
    }

    // Replay undo log in reverse (LIFO) order
    for (int i = currentTx.undoCount - 1; i >= 0; i--) {
        UndoEntry* entry = &currentTx.undoLog[i];
        Table* t = findTable(entry->tableName);
        if (!t) continue;

        switch (entry->op) {
            case UNDO_INSERT:
                if (entry->slot >= 0 && entry->slot < MAX_ROWS && t->rows[entry->slot].used) {
                    t->rows[entry->slot].used = 0;
                    t->rowCount--;
                    if (DEBUG_MODE) {
                        printf("[PLAN] Rollback INSERT: Slot %d in '%s' cleared.\n", entry->slot, t->name);
                    }
                }
                break;

            case UNDO_UPDATE:
                if (entry->slot >= 0 && entry->slot < MAX_ROWS) {
                    t->rows[entry->slot] = entry->oldRow;
                    if (DEBUG_MODE) {
                        printf("[PLAN] Rollback UPDATE: Slot %d in '%s' restored to previous state.\n", entry->slot, t->name);
                    }
                }
                break;

            case UNDO_DELETE:
                if (entry->slot >= 0 && entry->slot < MAX_ROWS) {
                    t->rows[entry->slot] = entry->oldRow;
                    t->rowCount++;
                    if (DEBUG_MODE) {
                        printf("[PLAN] Rollback DELETE: Slot %d in '%s' restored.\n", entry->slot, t->name);
                    }
                }
                break;
        }
    }

    currentTx.active = 0;
    currentTx.undoCount = 0;
    return 0;
}

int txIsActive(void) {
    return currentTx.active;
}

int txGetUndoCount(void) {
    return currentTx.undoCount;
}

void txRecordInsert(const char* tableName, int slot, const Row* newRow) {
    (void)newRow;
    if (!currentTx.active) return;
    if (currentTx.undoCount >= MAX_UNDO_LOG) {
        printf("[TRANSACTION] Undo log overflow!\n");
        return;
    }
    UndoEntry* entry = &currentTx.undoLog[currentTx.undoCount++];
    entry->op = UNDO_INSERT;
    strncpy(entry->tableName, tableName, MAX_NAME_LEN - 1);
    entry->tableName[MAX_NAME_LEN - 1] = '\0';
    entry->slot = slot;
    memset(&entry->oldRow, 0, sizeof(Row));
}

void txRecordUpdate(const char* tableName, int slot, const Row* oldRow) {
    if (!currentTx.active) return;
    if (currentTx.undoCount >= MAX_UNDO_LOG) {
        printf("[TRANSACTION] Undo log overflow!\n");
        return;
    }
    UndoEntry* entry = &currentTx.undoLog[currentTx.undoCount++];
    entry->op = UNDO_UPDATE;
    strncpy(entry->tableName, tableName, MAX_NAME_LEN - 1);
    entry->tableName[MAX_NAME_LEN - 1] = '\0';
    entry->slot = slot;
    if (oldRow) {
        entry->oldRow = *oldRow;
    }
}

void txRecordDelete(const char* tableName, int slot, const Row* deletedRow) {
    if (!currentTx.active) return;
    if (currentTx.undoCount >= MAX_UNDO_LOG) {
        printf("[TRANSACTION] Undo log overflow!\n");
        return;
    }
    UndoEntry* entry = &currentTx.undoLog[currentTx.undoCount++];
    entry->op = UNDO_DELETE;
    strncpy(entry->tableName, tableName, MAX_NAME_LEN - 1);
    entry->tableName[MAX_NAME_LEN - 1] = '\0';
    entry->slot = slot;
    if (deletedRow) {
        entry->oldRow = *deletedRow;
    }
}

// Legacy scheduler support
void addTransaction(char *query) {
    if (rear >= MAX_TRANSACTIONS) return;
    strncpy(queue[rear].query, query, sizeof(queue[rear].query) - 1);
    queue[rear].query[sizeof(queue[rear].query) - 1] = '\0';
    queue[rear].id = rear;
    rear++;
}

void executeTransactions(void) {
    while (front < rear) {
        printf("[SCHEDULER] Executing: %s\n", queue[front].query);
        processQuery(queue[front].query);
        front++;
    }
}