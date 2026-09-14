#ifndef DB_H
#define DB_H

#define MAX_TABLES 32
#define MAX_COLS 16
#define MAX_ROWS 1000
#define MAX_NAME_LEN 64
#define MAX_STR_LEN 128

typedef enum {
    TYPE_INT,
    TYPE_VARCHAR
} DataType;

typedef struct {
    char name[MAX_NAME_LEN];
    DataType type;
} Column;

typedef struct {
    DataType type;
    int intVal;
    char strVal[MAX_STR_LEN];
} Value;

typedef struct {
    Value values[MAX_COLS];
    int used;
} Row;

// Forward declaration for table index
typedef struct HashIndex HashIndex;

typedef struct {
    char name[MAX_NAME_LEN];
    int colCount;
    Column cols[MAX_COLS];
    int rowCount;
    Row rows[MAX_ROWS];
    int primaryKeyCol; // -1 if none, otherwise index of PK column
} Table;

// Global database state
extern Table tables[MAX_TABLES];
extern int tableCount;
extern int DEBUG_MODE;

// Single-table legacy compatibility
extern Row table[MAX_ROWS];
extern int rowCount;

// Database core API
void init_db(void);
Table* findTable(const char* name);
Table* createTable(const char* name, int colCount, const Column* cols, int primaryKeyCol);
int getColumnIndex(const Table* t, const char* colName);

// Row CRUD operations (table-aware)
int insertTableRow(Table* t, const Value* values);
int updateTableRow(Table* t, int slot, int colIdx, const Value* newVal);
int deleteTableRow(Table* t, int slot);

// Query evaluation conditions
typedef enum {
    OP_EQ,      // =
    OP_NEQ,     // != or <>
    OP_GT,      // >
    OP_LT,      // <
    OP_GTE,     // >=
    OP_LTE,     // <=
    OP_LIKE     // LIKE
} ConditionOp;

typedef struct {
    char tableName[MAX_NAME_LEN];
    char colName[MAX_NAME_LEN];
    ConditionOp op;
    Value targetVal;
    int isColComparison; // 1 if comparing against another column
    char targetTableName[MAX_NAME_LEN];
    char targetColName[MAX_NAME_LEN];
} WhereCondition;

// Condition matching
int evaluateCondition(const Table* t, const Row* r, const WhereCondition* cond);

// Legacy single-table API wrappers for backward compatibility
int insertRow(int id, const char* name);
Row* selectRow(int id);
int updateRow(int id, const char* name);
int deleteRow(int id);

// Table seeding and utilities
void seedDefaultData(void);
void printTableSchema(const Table* t);
void printAllTables(void);

#endif
