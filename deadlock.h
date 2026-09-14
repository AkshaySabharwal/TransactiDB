#ifndef DEADLOCK_H
#define DEADLOCK_H

#define MAX_NODES 32

void resetGraph(void);
void addEdge(int from, int to);
void removeEdge(int from, int to);
int detectDeadlock(void);
void printWaitGraph(void);

#endif