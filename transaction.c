#include <stdio.h>
#include <string.h>
#include "transaction.h"
#include "parser.h"

Transaction queue[MAX_TRANSACTIONS];
int front = 0, rear = 0;

void addTransaction(char *query) {
    strcpy(queue[rear].query, query);
    queue[rear].id = rear;
    rear++;
}

extern void processQuery(char *line);

void executeTransactions() {
    while(front < rear) {

        printf("[SCHEDULER] Executing: %s\n", queue[front].query);

        processQuery(queue[front].query);  

        front++;
    }
}