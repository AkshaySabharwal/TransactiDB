#ifndef DB_H
#define DB_H

#define MAX_ROWS 1000
#define NAME_LEN 64

typedef struct {
    int id;
    char name[NAME_LEN];
    int used;
} Row;

extern Row table[MAX_ROWS];
extern int rowCount;

void init_db(void);
int insertRow(int id, const char* name);
Row* selectRow(int id);
int updateRow(int id, const char* name);
int deleteRow(int id);
extern int DEBUG_MODE;


#endif
