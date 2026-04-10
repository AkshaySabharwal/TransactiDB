#include "lock.h"
#include <stdio.h>

Lock lockTable[100];

int acquireLock(int id) {
    if(lockTable[id].locked) {
        printf("Record locked. Waiting...\n");
        return 0;
    }
    lockTable[id].locked = 1;
    return 1;
}

void releaseLock(int id) {
    lockTable[id].locked = 0;
}