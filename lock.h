#ifndef LOCK_H
#define LOCK_H

typedef struct {
    int recordId;
    int locked;
} Lock;

extern Lock lockTable[100];

int acquireLock(int id);
void releaseLock(int id);

#endif