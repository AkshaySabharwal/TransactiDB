#include <string.h>
#include <stdio.h>
#include "index.h"

static HashIndex globalIndex;

static int hashFunc(int key) {
    if (key < 0) key = -key;
    return key % HASH_SIZE;
}

void tableIndexInit(HashIndex* idx) {
    if (!idx) return;
    for (int i = 0; i < HASH_SIZE; i++) {
        idx->hashKeys[i] = -1;
        idx->hashVals[i] = -1;
    }
}

int tableIndexInsert(HashIndex* idx, int id, int rowPos) {
    if (!idx) return -1;
    int h = hashFunc(id);
    int start = h;
    int first_tombstone = -1;

    while (1) {
        if (idx->hashKeys[h] == -1) {
            if (first_tombstone != -1) h = first_tombstone;
            idx->hashKeys[h] = id;
            idx->hashVals[h] = rowPos;
            return 0;
        } else if (idx->hashKeys[h] == -2) {
            if (first_tombstone == -1) first_tombstone = h;
        } else if (idx->hashKeys[h] == id) {
            return -1; // Duplicate key
        }
        h = (h + 1) % HASH_SIZE;
        if (h == start) return -1; // Index full
    }
}

int tableIndexSearch(const HashIndex* idx, int id) {
    if (!idx) return -1;
    int h = hashFunc(id);
    int start = h;

    while (1) {
        if (idx->hashKeys[h] == -1) return -1;
        if (idx->hashKeys[h] == id) return idx->hashVals[h];
        h = (h + 1) % HASH_SIZE;
        if (h == start) return -1;
    }
}

int tableIndexDelete(HashIndex* idx, int id) {
    if (!idx) return -1;
    int h = hashFunc(id);
    int start = h;

    while (1) {
        if (idx->hashKeys[h] == -1) return -1;
        if (idx->hashKeys[h] == id) {
            idx->hashKeys[h] = -2;
            idx->hashVals[h] = -1;
            return 0;
        }
        h = (h + 1) % HASH_SIZE;
        if (h == start) return -1;
    }
}

// Global index functions for backward compatibility
void indexInit(void) {
    tableIndexInit(&globalIndex);
}

int indexInsert(int id, int rowPos) {
    return tableIndexInsert(&globalIndex, id, rowPos);
}

int indexSearch(int id) {
    return tableIndexSearch(&globalIndex, id);
}

int indexDelete(int id) {
    return tableIndexDelete(&globalIndex, id);
}
