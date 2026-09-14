#include <stdio.h>
#include <string.h>
#include "deadlock.h"

static int waitGraph[MAX_NODES][MAX_NODES];

void resetGraph(void) {
    memset(waitGraph, 0, sizeof(waitGraph));
}

void addEdge(int from, int to) {
    if (from >= 0 && from < MAX_NODES && to >= 0 && to < MAX_NODES) {
        waitGraph[from][to] = 1;
    }
}

void removeEdge(int from, int to) {
    if (from >= 0 && from < MAX_NODES && to >= 0 && to < MAX_NODES) {
        waitGraph[from][to] = 0;
    }
}

static int dfsCycle(int node, int* visited, int* recStack) {
    visited[node] = 1;
    recStack[node] = 1;

    for (int next = 0; next < MAX_NODES; next++) {
        if (waitGraph[node][next]) {
            if (!visited[next]) {
                if (dfsCycle(next, visited, recStack)) return 1;
            } else if (recStack[next]) {
                return 1; // Cycle found!
            }
        }
    }

    recStack[node] = 0;
    return 0;
}

int detectDeadlock(void) {
    int visited[MAX_NODES] = {0};
    int recStack[MAX_NODES] = {0};

    for (int i = 0; i < MAX_NODES; i++) {
        if (!visited[i]) {
            if (dfsCycle(i, visited, recStack)) {
                return 1;
            }
        }
    }
    return 0;
}

void printWaitGraph(void) {
    printf("=== Wait-For Graph (Deadlock Analysis) ===\n");
    int edgeCount = 0;
    for (int i = 0; i < MAX_NODES; i++) {
        for (int j = 0; j < MAX_NODES; j++) {
            if (waitGraph[i][j]) {
                printf("  Tx #%d ---> Tx #%d (Waiting for lock)\n", i, j);
                edgeCount++;
            }
        }
    }
    if (edgeCount == 0) {
        printf("  (No active waiting dependencies)\n");
    }
    printf("Deadlock Status: %s\n", detectDeadlock() ? "DEADLOCK DETECTED! (Cycle in graph)" : "No deadlock");
    printf("==========================================\n");
}