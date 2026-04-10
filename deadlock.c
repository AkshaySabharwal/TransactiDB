#include <stdio.h>

int waitGraph[10][10];

void addEdge(int from, int to) {
    waitGraph[from][to] = 1;
}

int detectDeadlock() {
    for(int i=0;i<10;i++) {
        if(waitGraph[i][i])
            return 1;
    }
    return 0;
}