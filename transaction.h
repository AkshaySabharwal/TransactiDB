#ifndef TRANSACTION_H
#define TRANSACTION_H

#define MAX_TRANSACTIONS 100

typedef struct {
    int id;
    char query[256];
    int priority;
} Transaction;

extern Transaction queue[MAX_TRANSACTIONS];
extern int front, rear;

void addTransaction(char *query);
void executeTransactions();

#endif