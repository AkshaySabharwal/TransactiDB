#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>
#include "db.h"
#include "index.h"
#include "transaction.h"

#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

int DEBUG_MODE = 0;

Table tables[MAX_TABLES];
int tableCount = 0;

// Legacy single-table compatibility
Row table[MAX_ROWS];
int rowCount = 0;

static void syncLegacyTable(void) {
    if (tableCount > 0) {
        Table* t = &tables[0];
        rowCount = t->rowCount;
        for (int i = 0; i < MAX_ROWS; i++) {
            table[i] = t->rows[i];
        }
    }
}

int getColumnIndex(const Table* t, const char* colName) {
    if (!t || !colName) return -1;
    // Strip table prefix if present (e.g., "STUDENT.ID" -> "ID")
    const char* dot = strchr(colName, '.');
    const char* targetName = dot ? dot + 1 : colName;

    for (int i = 0; i < t->colCount; i++) {
        if (strcasecmp(t->cols[i].name, targetName) == 0) {
            return i;
        }
    }
    return -1;
}

Table* findTable(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < tableCount; i++) {
        if (strcasecmp(tables[i].name, name) == 0) {
            return &tables[i];
        }
    }
    return NULL;
}

Table* createTable(const char* name, int colCount, const Column* cols, int primaryKeyCol) {
    if (tableCount >= MAX_TABLES || !name || colCount <= 0 || colCount > MAX_COLS) {
        return NULL;
    }

    if (findTable(name) != NULL) {
        if (DEBUG_MODE) printf("[PLAN] Table '%s' already exists.\n", name);
        return NULL;
    }

    Table* t = &tables[tableCount++];
    strncpy(t->name, name, MAX_NAME_LEN - 1);
    t->name[MAX_NAME_LEN - 1] = '\0';
    t->colCount = colCount;
    t->rowCount = 0;
    t->primaryKeyCol = primaryKeyCol;

    for (int i = 0; i < colCount; i++) {
        t->cols[i] = cols[i];
    }

    for (int i = 0; i < MAX_ROWS; i++) {
        t->rows[i].used = 0;
        for (int c = 0; c < MAX_COLS; c++) {
            t->rows[i].values[c].type = TYPE_INT;
            t->rows[i].values[c].intVal = 0;
            t->rows[i].values[c].strVal[0] = '\0';
        }
    }

    if (DEBUG_MODE) {
        printf("[PLAN] Created table '%s' with %d columns.\n", t->name, colCount);
    }

    return t;
}

static int findFreeTableRowSlot(const Table* t) {
    for (int i = 0; i < MAX_ROWS; i++) {
        if (!t->rows[i].used) return i;
    }
    return -1;
}

int insertTableRow(Table* t, const Value* values) {
    if (!t || !values) return -1;

    if (DEBUG_MODE) {
        printf("[PLAN] Operation: INSERT into table '%s'\n", t->name);
    }

    // Check primary key uniqueness if defined
    if (t->primaryKeyCol >= 0 && t->primaryKeyCol < t->colCount) {
        const Value* pkVal = &values[t->primaryKeyCol];
        for (int i = 0; i < MAX_ROWS; i++) {
            if (t->rows[i].used) {
                const Value* existingPk = &t->rows[i].values[t->primaryKeyCol];
                if (pkVal->type == TYPE_INT && existingPk->type == TYPE_INT && pkVal->intVal == existingPk->intVal) {
                    if (DEBUG_MODE) printf("[PLAN] Duplicate Primary Key %d in table '%s'. Abort.\n", pkVal->intVal, t->name);
                    return -1;
                } else if (pkVal->type == TYPE_VARCHAR && existingPk->type == TYPE_VARCHAR && 
                           strcasecmp(pkVal->strVal, existingPk->strVal) == 0) {
                    if (DEBUG_MODE) printf("[PLAN] Duplicate Primary Key '%s' in table '%s'. Abort.\n", pkVal->strVal, t->name);
                    return -1;
                }
            }
        }
    }

    int slot = findFreeTableRowSlot(t);
    if (slot == -1) {
        if (DEBUG_MODE) printf("[PLAN] Table '%s' is full (MAX_ROWS reached).\n", t->name);
        return -1;
    }

    // Copy values
    for (int i = 0; i < t->colCount; i++) {
        t->rows[slot].values[i] = values[i];
    }
    t->rows[slot].used = 1;
    t->rowCount++;

    // Record for transaction rollback
    txRecordInsert(t->name, slot, &t->rows[slot]);

    syncLegacyTable();

    if (DEBUG_MODE) {
        printf("[PLAN] Record inserted into '%s' at slot %d (Total rows: %d).\n", t->name, slot, t->rowCount);
    }

    return 0;
}

int updateTableRow(Table* t, int slot, int colIdx, const Value* newVal) {
    if (!t || slot < 0 || slot >= MAX_ROWS || !t->rows[slot].used || colIdx < 0 || colIdx >= t->colCount || !newVal) {
        return -1;
    }

    if (DEBUG_MODE) {
        printf("[PLAN] Operation: UPDATE table '%s' slot %d column %s\n", t->name, slot, t->cols[colIdx].name);
    }

    // Record previous row for transaction rollback
    txRecordUpdate(t->name, slot, &t->rows[slot]);

    t->rows[slot].values[colIdx] = *newVal;

    syncLegacyTable();
    return 0;
}

int deleteTableRow(Table* t, int slot) {
    if (!t || slot < 0 || slot >= MAX_ROWS || !t->rows[slot].used) {
        return -1;
    }

    if (DEBUG_MODE) {
        printf("[PLAN] Operation: DELETE from table '%s' slot %d\n", t->name, slot);
    }

    // Record deleted row for transaction rollback
    txRecordDelete(t->name, slot, &t->rows[slot]);

    t->rows[slot].used = 0;
    t->rowCount--;

    syncLegacyTable();
    return 0;
}

static int strCaseLike(const char* str, const char* pattern) {
    if (!str || !pattern) return 0;
    // Simple case-insensitive substring or exact match
    char s_lower[MAX_STR_LEN], p_lower[MAX_STR_LEN];
    int i = 0;
    while (str[i] && i < MAX_STR_LEN - 1) { s_lower[i] = tolower((unsigned char)str[i]); i++; }
    s_lower[i] = '\0';

    i = 0;
    while (pattern[i] && i < MAX_STR_LEN - 1) { p_lower[i] = tolower((unsigned char)pattern[i]); i++; }
    p_lower[i] = '\0';

    // Remove surrounding % if any
    char* cleanPat = p_lower;
    if (cleanPat[0] == '%') cleanPat++;
    size_t len = strlen(cleanPat);
    if (len > 0 && cleanPat[len - 1] == '%') {
        cleanPat[len - 1] = '\0';
    }

    return strstr(s_lower, cleanPat) != NULL;
}

int evaluateCondition(const Table* t, const Row* r, const WhereCondition* cond) {
    if (!t || !r || !cond) return 1;

    int colIdx = getColumnIndex(t, cond->colName);
    if (colIdx == -1) return 0;

    const Value* leftVal = &r->values[colIdx];
    Value rightVal = cond->targetVal;

    if (cond->isColComparison) {
        int rightColIdx = getColumnIndex(t, cond->targetColName);
        if (rightColIdx == -1) return 0;
        rightVal = r->values[rightColIdx];
    }

    if (leftVal->type == TYPE_INT && rightVal.type == TYPE_INT) {
        int l = leftVal->intVal;
        int r_val = rightVal.intVal;
        switch (cond->op) {
            case OP_EQ:  return l == r_val;
            case OP_NEQ: return l != r_val;
            case OP_GT:  return l > r_val;
            case OP_LT:  return l < r_val;
            case OP_GTE: return l >= r_val;
            case OP_LTE: return l <= r_val;
            default:     return l == r_val;
        }
    } else {
        // String comparison
        const char* s1 = (leftVal->type == TYPE_VARCHAR) ? leftVal->strVal : "";
        const char* s2 = (rightVal.type == TYPE_VARCHAR) ? rightVal.strVal : "";
        int cmp = strcasecmp(s1, s2);
        switch (cond->op) {
            case OP_EQ:   return cmp == 0;
            case OP_NEQ:  return cmp != 0;
            case OP_GT:   return cmp > 0;
            case OP_LT:   return cmp < 0;
            case OP_GTE:  return cmp >= 0;
            case OP_LTE:  return cmp <= 0;
            case OP_LIKE: return strCaseLike(s1, s2);
            default:      return cmp == 0;
        }
    }
}

void seedDefaultData(void) {
    // 1. STUDENT table: ID INT, NAME VARCHAR, DEPT_ID INT
    Column studentCols[] = {
        {.name = "ID", .type = TYPE_INT},
        {.name = "NAME", .type = TYPE_VARCHAR},
        {.name = "DEPT_ID", .type = TYPE_INT}
    };
    Table* tStudent = createTable("STUDENT", 3, studentCols, 0);

    if (tStudent) {
        Value row1[] = {
            {.type = TYPE_INT, .intVal = 1, .strVal = ""},
            {.type = TYPE_VARCHAR, .intVal = 0, .strVal = "Akshay"},
            {.type = TYPE_INT, .intVal = 10, .strVal = ""}
        };
        insertTableRow(tStudent, row1);

        Value row2[] = {
            {.type = TYPE_INT, .intVal = 2, .strVal = ""},
            {.type = TYPE_VARCHAR, .intVal = 0, .strVal = "John"},
            {.type = TYPE_INT, .intVal = 20, .strVal = ""}
        };
        insertTableRow(tStudent, row2);

        Value row3[] = {
            {.type = TYPE_INT, .intVal = 3, .strVal = ""},
            {.type = TYPE_VARCHAR, .intVal = 0, .strVal = "Sarah"},
            {.type = TYPE_INT, .intVal = 10, .strVal = ""}
        };
        insertTableRow(tStudent, row3);

        Value row4[] = {
            {.type = TYPE_INT, .intVal = 4, .strVal = ""},
            {.type = TYPE_VARCHAR, .intVal = 0, .strVal = "Emily"},
            {.type = TYPE_INT, .intVal = 30, .strVal = ""}
        };
        insertTableRow(tStudent, row4);
    }

    // 2. DEPARTMENT table: DEPT_ID INT, DEPT_NAME VARCHAR
    Column deptCols[] = {
        {.name = "DEPT_ID", .type = TYPE_INT},
        {.name = "DEPT_NAME", .type = TYPE_VARCHAR}
    };
    Table* tDept = createTable("DEPARTMENT", 2, deptCols, 0);

    if (tDept) {
        Value d1[] = {
            {.type = TYPE_INT, .intVal = 10, .strVal = ""},
            {.type = TYPE_VARCHAR, .intVal = 0, .strVal = "CSE"}
        };
        insertTableRow(tDept, d1);

        Value d2[] = {
            {.type = TYPE_INT, .intVal = 20, .strVal = ""},
            {.type = TYPE_VARCHAR, .intVal = 0, .strVal = "IT"}
        };
        insertTableRow(tDept, d2);

        Value d3[] = {
            {.type = TYPE_INT, .intVal = 30, .strVal = ""},
            {.type = TYPE_VARCHAR, .intVal = 0, .strVal = "ECE"}
        };
        insertTableRow(tDept, d3);

        Value d4[] = {
            {.type = TYPE_INT, .intVal = 40, .strVal = ""},
            {.type = TYPE_VARCHAR, .intVal = 0, .strVal = "MECH"}
        };
        insertTableRow(tDept, d4);
    }
}

void init_db(void) {
    tableCount = 0;
    rowCount = 0;
    for (int i = 0; i < MAX_ROWS; i++) {
        table[i].used = 0;
    }
    indexInit();
    seedDefaultData();
    syncLegacyTable();
}

void printTableSchema(const Table* t) {
    if (!t) return;
    printf("Table: %s (Rows: %d)\n", t->name, t->rowCount);
    printf("Columns:\n");
    for (int i = 0; i < t->colCount; i++) {
        printf("  - %s (%s)%s\n", 
               t->cols[i].name, 
               t->cols[i].type == TYPE_INT ? "INT" : "VARCHAR",
               (t->primaryKeyCol == i) ? " [PRIMARY KEY]" : "");
    }
}

void printAllTables(void) {
    printf("+----------------------+---------+---------+\n");
    printf("| Table Name           | Columns | Rows    |\n");
    printf("+----------------------+---------+---------+\n");
    for (int i = 0; i < tableCount; i++) {
        printf("| %-20s | %-7d | %-7d |\n", 
               tables[i].name, tables[i].colCount, tables[i].rowCount);
    }
    printf("+----------------------+---------+---------+\n");
}

// Legacy single-table compatibility implementations
int insertRow(int id, const char* name) {
    Table* t = &tables[0];
    if (!t) return -1;
    Value vals[3];
    vals[0].type = TYPE_INT; vals[0].intVal = id; vals[0].strVal[0] = '\0';
    vals[1].type = TYPE_VARCHAR; vals[1].intVal = 0; 
    strncpy(vals[1].strVal, name, MAX_STR_LEN - 1);
    vals[1].strVal[MAX_STR_LEN - 1] = '\0';
    vals[2].type = TYPE_INT; vals[2].intVal = 0; vals[2].strVal[0] = '\0';

    return insertTableRow(t, vals);
}

Row* selectRow(int id) {
    Table* t = &tables[0];
    if (!t) return NULL;
    for (int i = 0; i < MAX_ROWS; i++) {
        if (t->rows[i].used && t->rows[i].values[0].intVal == id) {
            return &t->rows[i];
        }
    }
    return NULL;
}

int updateRow(int id, const char* name) {
    Table* t = &tables[0];
    if (!t) return -1;
    for (int i = 0; i < MAX_ROWS; i++) {
        if (t->rows[i].used && t->rows[i].values[0].intVal == id) {
            Value newVal;
            newVal.type = TYPE_VARCHAR;
            newVal.intVal = 0;
            strncpy(newVal.strVal, name, MAX_STR_LEN - 1);
            newVal.strVal[MAX_STR_LEN - 1] = '\0';
            return updateTableRow(t, i, 1, &newVal);
        }
    }
    return -1;
}

int deleteRow(int id) {
    Table* t = &tables[0];
    if (!t) return -1;
    for (int i = 0; i < MAX_ROWS; i++) {
        if (t->rows[i].used && t->rows[i].values[0].intVal == id) {
            return deleteTableRow(t, i);
        }
    }
    return -1;
}
