#ifndef INDEX_H
#define INDEX_H

#define HASH_SIZE 1031

typedef struct HashIndex {
    int hashKeys[HASH_SIZE];
    int hashVals[HASH_SIZE];
} HashIndex;

void indexInit(void);
int indexInsert(int id, int rowPos);
int indexSearch(int id);
int indexDelete(int id);

// Per-table hash index API
void tableIndexInit(HashIndex* idx);
int tableIndexInsert(HashIndex* idx, int id, int rowPos);
int tableIndexSearch(const HashIndex* idx, int id);
int tableIndexDelete(HashIndex* idx, int id);

#endif
