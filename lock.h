#ifndef LOCK_H
#define LOCK_H

#define MAX_LOCKS 1000

typedef enum {
    LOCK_NONE,
    LOCK_SHARED,    // S-Lock (Read)
    LOCK_EXCLUSIVE  // X-Lock (Write)
} LockMode;

typedef struct {
    int recordId;
    int locked;
    LockMode mode;
    int ownerTxId;
} Lock;

extern Lock lockTable[MAX_LOCKS];

void initLockManager(void);
int acquireLock(int id);
int acquireLockEx(int id, LockMode mode, int txId);
void releaseLock(int id);
void releaseTxLocks(int txId);
void printLockStatus(void);

#endif