#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "db.h"

#define MAX_TRANSACTIONS 100
#define MAX_UNDO_LOG 1000

typedef enum {
    UNDO_INSERT,  // Row was inserted; undo action = mark slot unused & decrement count
    UNDO_UPDATE,  // Row was updated; undo action = restore old row content
    UNDO_DELETE   // Row was deleted; undo action = restore deleted row & increment count
} UndoOp;

typedef struct {
    UndoOp op;
    char tableName[MAX_NAME_LEN];
    int slot;
    Row oldRow;
} UndoEntry;

// Transaction engine state
typedef struct {
    int active;
    int txId;
    int undoCount;
    UndoEntry undoLog[MAX_UNDO_LOG];
} TransactionContext;

extern TransactionContext currentTx;

// Transaction Management API
int txBegin(void);
int txCommit(void);
int txRollback(void);
int txIsActive(void);
int txGetUndoCount(void);

// Logging operations for rollback
void txRecordInsert(const char* tableName, int slot, const Row* newRow);
void txRecordUpdate(const char* tableName, int slot, const Row* oldRow);
void txRecordDelete(const char* tableName, int slot, const Row* deletedRow);

// Batch Queue (Legacy scheduler support)
typedef struct {
    int id;
    char query[256];
    int priority;
} TransactionTask;

extern TransactionTask queue[MAX_TRANSACTIONS];
extern int front, rear;

void addTransaction(char *query);
void executeTransactions(void);

#endif