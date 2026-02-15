#include <stdio.h>
#include <string.h>
#include "db.h"
#include "index.h"
int DEBUG_MODE = 0;

Row table[MAX_ROWS];
int rowCount = 0;

void init_db(void) {
    rowCount = 0;
    for (int i = 0; i < MAX_ROWS; i++) {
        table[i].used = 0;
    }
    indexInit();
}

int find_free_slot(void) {
    for (int i = 0; i < MAX_ROWS; i++) {
        if (!table[i].used) return i;
    }
    return -1;
}

int insertRow(int id, const char* name) {

    if (DEBUG_MODE) {
        printf("[PLAN] Operation: INSERT\n");
        printf("[PLAN] Checking if ID exists in index...\n");
    }

    if (indexSearch(id) != -1) {
        if (DEBUG_MODE)
            printf("[PLAN] Duplicate ID found. Abort.\n");
        return -1;
    }

    if (DEBUG_MODE)
        printf("[PLAN] Searching for free slot in table...\n");

    int slot = find_free_slot();
    if (slot == -1) {
        if (DEBUG_MODE)
            printf("[PLAN] No free slot available.\n");
        return -1;
    }

    if (DEBUG_MODE)
        printf("[PLAN] Writing record to memory slot %d...\n", slot);

    table[slot].id = id;
    strncpy(table[slot].name, name, NAME_LEN-1);
    table[slot].name[NAME_LEN-1] = '\0';
    table[slot].used = 1;
    rowCount++;

    if (DEBUG_MODE)
        printf("[PLAN] Updating hash index...\n");

    if (indexInsert(id, slot) != 0) {
        table[slot].used = 0;
        rowCount--;
        return -1;
    }

    if (DEBUG_MODE)
        printf("[PLAN] INSERT completed successfully.\n");

    return 0;
}


Row* selectRow(int id) {

    if (DEBUG_MODE) {
        printf("[PLAN] Operation: SELECT\n");
        printf("[PLAN] Searching hash index...\n");
    }

    int pos = indexSearch(id);

    if (pos != -1 && table[pos].used && table[pos].id == id) {
        if (DEBUG_MODE)
            printf("[PLAN] Record found at slot %d.\n", pos);
        return &table[pos];
    }

    if (DEBUG_MODE)
        printf("[PLAN] Record not found.\n");

    return NULL;
}


int updateRow(int id, const char* name) {

    if (DEBUG_MODE)
        printf("[PLAN] Operation: UPDATE\n");

    Row* r = selectRow(id);
    if (!r) {
        if (DEBUG_MODE)
            printf("[PLAN] Record not found. Cannot update.\n");
        return -1;
    }

    if (DEBUG_MODE)
        printf("[PLAN] Updating record in memory...\n");

    strncpy(r->name, name, NAME_LEN-1);
    r->name[NAME_LEN-1] = '\0';

    if (DEBUG_MODE)
        printf("[PLAN] UPDATE completed.\n");

    return 0;
}


int deleteRow(int id) {

    if (DEBUG_MODE)
        printf("[PLAN] Operation: DELETE\n");

    int pos = indexSearch(id);
    if (pos == -1) {
        if (DEBUG_MODE)
            printf("[PLAN] Record not found.\n");
        return -1;
    }

    if (DEBUG_MODE)
        printf("[PLAN] Removing record from slot %d...\n", pos);

    table[pos].used = 0;
    rowCount--;
    indexDelete(id);

    if (DEBUG_MODE)
        printf("[PLAN] DELETE completed.\n");

    return 0;
}

