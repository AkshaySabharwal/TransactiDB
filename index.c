#include <string.h>
#include <stdio.h>
#include "index.h"

#define HASH_SIZE 1031
static int hashKeys[HASH_SIZE];
static int hashVals[HASH_SIZE];

static int hashFunc(int key) {
    if (key < 0) key = -key;
    return key % HASH_SIZE;
}

void indexInit(void) {
    for (int i = 0; i < HASH_SIZE; i++) {
        hashKeys[i] = -1;
        hashVals[i] = -1;
    }
}

int indexInsert(int id, int rowPos) {
    int h = hashFunc(id);
    int start = h;
    int first_tombstone = -1;

    while (1) {
        if (hashKeys[h] == -1) {
            if (first_tombstone != -1) h = first_tombstone;
            hashKeys[h] = id;
            hashVals[h] = rowPos;
            return 0;
        } else if (hashKeys[h] == -2) {
            if (first_tombstone == -1) first_tombstone = h;
        } else if (hashKeys[h] == id) {
            return -1;
        }
        h = (h + 1) % HASH_SIZE;
        if (h == start) return -1;
    }
}

int indexSearch(int id) {
    int h = hashFunc(id);
    int start = h;

    while (1) {
        if (hashKeys[h] == -1) return -1;
        if (hashKeys[h] == id) return hashVals[h];
        h = (h + 1) % HASH_SIZE;
        if (h == start) return -1;
    }
}

int indexDelete(int id) {
    int h = hashFunc(id);
    int start = h;

    while (1) {
        if (hashKeys[h] == -1) return -1;
        if (hashKeys[h] == id) {
            hashKeys[h] = -2;
            hashVals[h] = -1;
            return 0;
        }
        h = (h + 1) % HASH_SIZE;
        if (h == start) return -1;
    }
}
