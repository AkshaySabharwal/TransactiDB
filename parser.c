#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "parser.h"
#include "db.h"
#include "transaction.h"
#include "lock.h"
#include "deadlock.h"

#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#define HEADER_LEN 128

static void trim_whitespace(char *s) {
    if (!s) return;
    char *end;
    char *start = s;
    while (isspace((unsigned char)*start)) start++;
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    end = s + strlen(s) - 1;
    while (end >= s && (isspace((unsigned char)*end) || *end == ';' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
}

static char *next_token(char **p) {
    if (!p || !*p) return NULL;
    while (**p && isspace((unsigned char)**p)) (*p)++;
    if (**p == '\0') return NULL;

    char *start;
    if (**p == '\'' || **p == '"') {
        char quote = **p;
        (*p)++;
        start = *p;
        while (**p && **p != quote) (*p)++;
        if (**p == quote) {
            **p = '\0';
            (*p)++;
        }
    } else {
        start = *p;
        while (**p && !isspace((unsigned char)**p) && **p != ',' && **p != '(' && **p != ')' && **p != ';') {
            (*p)++;
        }
        if (**p) {
            if (isspace((unsigned char)**p)) {
                **p = '\0';
                (*p)++;
            } else if (**p == ',' || **p == '(' || **p == ')' || **p == ';') {
                if (start == *p) {
                    (*p)++;
                    char *single = *p - 1;
                    *p = '\0';
                    return single;
                } else {
                    **p = '\0';
                    (*p)++;
                }
            }
        }
    }
    return start;
}

static ConditionOp parseOp(const char *opStr) {
    if (strcmp(opStr, "=") == 0 || strcasecmp(opStr, "EQ") == 0 || strcmp(opStr, "==") == 0) return OP_EQ;
    if (strcmp(opStr, "!=") == 0 || strcmp(opStr, "<>") == 0 || strcasecmp(opStr, "NEQ") == 0) return OP_NEQ;
    if (strcmp(opStr, ">") == 0) return OP_GT;
    if (strcmp(opStr, "<") == 0) return OP_LT;
    if (strcmp(opStr, ">=") == 0) return OP_GTE;
    if (strcmp(opStr, "<=") == 0) return OP_LTE;
    if (strcasecmp(opStr, "LIKE") == 0) return OP_LIKE;
    return OP_EQ;
}

static Value parseLiteralValue(const char *token) {
    Value val;
    val.type = TYPE_INT;
    val.intVal = 0;
    val.strVal[0] = '\0';

    if (!token) return val;

    char *endptr;
    long num = strtol(token, &endptr, 10);
    if (*endptr == '\0' && endptr != token) {
        val.type = TYPE_INT;
        val.intVal = (int)num;
        snprintf(val.strVal, sizeof(val.strVal), "%d", (int)num);
    } else {
        val.type = TYPE_VARCHAR;
        val.intVal = 0;
        size_t len = strlen(token);
        if (len >= MAX_STR_LEN) len = MAX_STR_LEN - 1;
        memcpy(val.strVal, token, len);
        val.strVal[len] = '\0';
    }
    return val;
}

static int parseValuesList(char *str, Value *vals, int maxVals) {
    if (!str) return 0;
    char *p = str;
    while (*p && (isspace((unsigned char)*p) || *p == '(')) p++;

    int count = 0;
    while (*p && *p != ')' && *p != ';' && count < maxVals) {
        while (*p && (isspace((unsigned char)*p) || *p == '(' || *p == ',')) p++;
        if (*p == '\0' || *p == ')' || *p == ';') break;

        char buf[MAX_STR_LEN];
        int bidx = 0;

        if (*p == '\'' || *p == '"') {
            char q = *p++;
            while (*p && *p != q && bidx < MAX_STR_LEN - 1) {
                buf[bidx++] = *p++;
            }
            if (*p == q) p++;
        } else {
            while (*p && *p != ',' && *p != ')' && *p != ';' && !isspace((unsigned char)*p) && bidx < MAX_STR_LEN - 1) {
                buf[bidx++] = *p++;
            }
        }
        buf[bidx] = '\0';
        if (bidx > 0) {
            vals[count++] = parseLiteralValue(buf);
        }

        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == ',') p++;
    }
    return count;
}

static void renderAsciiTable(int colCount, const char colHeaders[][HEADER_LEN], int rowCount, const char rowData[][MAX_COLS][MAX_STR_LEN]) {
    int colWidths[MAX_COLS];

    for (int c = 0; c < colCount; c++) {
        colWidths[c] = (int)strlen(colHeaders[c]);
        for (int r = 0; r < rowCount; r++) {
            int len = (int)strlen(rowData[r][c]);
            if (len > colWidths[c]) {
                colWidths[c] = len;
            }
        }
        colWidths[c] += 2;
    }

    printf("+");
    for (int c = 0; c < colCount; c++) {
        for (int i = 0; i < colWidths[c]; i++) printf("-");
        printf("+");
    }
    printf("\n");

    printf("|");
    for (int c = 0; c < colCount; c++) {
        printf(" %-*s|", colWidths[c] - 1, colHeaders[c]);
    }
    printf("\n");

    printf("+");
    for (int c = 0; c < colCount; c++) {
        for (int i = 0; i < colWidths[c]; i++) printf("-");
        printf("+");
    }
    printf("\n");

    for (int r = 0; r < rowCount; r++) {
        printf("|");
        for (int c = 0; c < colCount; c++) {
            printf(" %-*s|", colWidths[c] - 1, rowData[r][c]);
        }
        printf("\n");
    }

    printf("+");
    for (int c = 0; c < colCount; c++) {
        for (int i = 0; i < colWidths[c]; i++) printf("-");
        printf("+");
    }
    printf("\n");

    printf("(%d row%s found)\n", rowCount, rowCount == 1 ? "" : "s");
}

static int parseWhereClause(char **p, WhereCondition *cond) {
    if (!p || !*p) return 0;
    char *col = next_token(p);
    if (!col) return 0;
    char *opStr = next_token(p);
    if (!opStr) return 0;
    char *valTok = next_token(p);
    if (!valTok) return 0;

    size_t colLen = strlen(col);
    if (colLen >= MAX_NAME_LEN) colLen = MAX_NAME_LEN - 1;
    memcpy(cond->colName, col, colLen);
    cond->colName[colLen] = '\0';

    cond->op = parseOp(opStr);
    cond->targetVal = parseLiteralValue(valTok);
    cond->isColComparison = 0;
    return 1;
}

static void handleSelect(char *p) {
    char *projStr = next_token(&p);
    if (!projStr) {
        printf("Usage: SELECT * FROM <table> [WHERE <col> = <val>]\n");
        return;
    }

    char *endptr;
    long maybeId = strtol(projStr, &endptr, 10);
    if (*endptr == '\0' && !p) {
        Row *r = selectRow((int)maybeId);
        if (r) {
            printf("Found: %d %s\n", r->values[0].intVal, r->values[1].strVal);
        } else {
            printf("Not found\n");
        }
        return;
    }

    char projectionTokens[16][HEADER_LEN];
    int projCount = 0;

    char *currTok = projStr;
    while (currTok) {
        if (strcasecmp(currTok, "FROM") == 0 || strcasecmp(currTok, "WHERE") == 0) {
            break;
        }
        if (strcmp(currTok, ",") != 0 && strlen(currTok) > 0) {
            char clean[HEADER_LEN];
            size_t cLen = strlen(currTok);
            if (cLen >= HEADER_LEN) cLen = HEADER_LEN - 1;
            memcpy(clean, currTok, cLen);
            clean[cLen] = '\0';
            if (clean[strlen(clean) - 1] == ',') clean[strlen(clean) - 1] = '\0';
            if (strlen(clean) > 0 && projCount < 16) {
                size_t pLen = strlen(clean);
                if (pLen >= HEADER_LEN) pLen = HEADER_LEN - 1;
                memcpy(projectionTokens[projCount], clean, pLen);
                projectionTokens[projCount][pLen] = '\0';
                projCount++;
            }
        }
        currTok = next_token(&p);
    }

    Table *table1 = NULL;
    Table *table2 = NULL;
    int isJoin = 0;
    char joinLeftCol[MAX_NAME_LEN] = "";
    char joinRightCol[MAX_NAME_LEN] = "";
    WhereCondition whereCond;
    int hasWhere = 0;

    if (currTok && strcasecmp(currTok, "FROM") == 0) {
        char *t1Name = next_token(&p);
        if (!t1Name) {
            printf("Error: Missing table name after FROM\n");
            return;
        }
        table1 = findTable(t1Name);
        if (!table1) {
            printf("Error: Table '%s' not found.\n", t1Name);
            return;
        }

        char *afterT1 = next_token(&p);
        if (afterT1 && (strcasecmp(afterT1, "JOIN") == 0 || strcasecmp(afterT1, "INNER") == 0)) {
            if (strcasecmp(afterT1, "INNER") == 0) {
                char *joinWord = next_token(&p);
                (void)joinWord;
            }
            char *t2Name = next_token(&p);
            if (!t2Name) {
                printf("Error: Missing table name after JOIN\n");
                return;
            }
            table2 = findTable(t2Name);
            if (!table2) {
                printf("Error: Joined table '%s' not found.\n", t2Name);
                return;
            }

            char *onWord = next_token(&p);
            if (!onWord || strcasecmp(onWord, "ON") != 0) {
                printf("Error: Expected ON condition in JOIN query.\n");
                return;
            }

            char *leftCol = next_token(&p);
            char *eqSign = next_token(&p);
            char *rightCol = next_token(&p);
            if (!leftCol || !eqSign || !rightCol) {
                printf("Error: Invalid ON condition format. Expected ON <t1.col> = <t2.col>\n");
                return;
            }
            size_t lLen = strlen(leftCol);
            if (lLen >= MAX_NAME_LEN) lLen = MAX_NAME_LEN - 1;
            memcpy(joinLeftCol, leftCol, lLen);
            joinLeftCol[lLen] = '\0';

            size_t rLen = strlen(rightCol);
            if (rLen >= MAX_NAME_LEN) rLen = MAX_NAME_LEN - 1;
            memcpy(joinRightCol, rightCol, rLen);
            joinRightCol[rLen] = '\0';
            isJoin = 1;

            char *maybeWhere = next_token(&p);
            if (maybeWhere && strcasecmp(maybeWhere, "WHERE") == 0) {
                hasWhere = parseWhereClause(&p, &whereCond);
            }
        } else if (afterT1 && strcasecmp(afterT1, "WHERE") == 0) {
            hasWhere = parseWhereClause(&p, &whereCond);
        }
    } else if (currTok && strcasecmp(currTok, "WHERE") == 0) {
        table1 = (tableCount > 0) ? &tables[0] : NULL;
        if (!table1) {
            printf("Error: No table available in database.\n");
            return;
        }
        hasWhere = parseWhereClause(&p, &whereCond);
    } else {
        table1 = (tableCount > 0) ? &tables[0] : NULL;
        if (!table1) {
            printf("Error: No table available in database.\n");
            return;
        }
    }

    if (isJoin && table1 && table2) {
        if (DEBUG_MODE) {
            printf("[PLAN] Executing INNER JOIN: '%s' JOIN '%s' ON %s = %s\n",
                   table1->name, table2->name, joinLeftCol, joinRightCol);
        }

        int t1ColIdx = getColumnIndex(table1, joinLeftCol);
        int t2ColIdx = getColumnIndex(table2, joinRightCol);

        if (t1ColIdx == -1) {
            t1ColIdx = getColumnIndex(table1, joinRightCol);
            t2ColIdx = getColumnIndex(table2, joinLeftCol);
        }

        if (t1ColIdx == -1 || t2ColIdx == -1) {
            printf("Error: Invalid column specified in JOIN ON clause.\n");
            return;
        }

        char headers[MAX_COLS][HEADER_LEN];
        int outCols = 0;
        int projIsAll = (projCount == 0 || (projCount == 1 && strcmp(projectionTokens[0], "*") == 0));

        if (projIsAll) {
            for (int i = 0; i < table1->colCount && outCols < MAX_COLS; i++) {
                snprintf(headers[outCols++], HEADER_LEN, "%s.%s", table1->name, table1->cols[i].name);
            }
            for (int i = 0; i < table2->colCount && outCols < MAX_COLS; i++) {
                snprintf(headers[outCols++], HEADER_LEN, "%s.%s", table2->name, table2->cols[i].name);
            }
        } else {
            for (int i = 0; i < projCount && outCols < MAX_COLS; i++) {
                size_t pLen = strlen(projectionTokens[i]);
                if (pLen >= HEADER_LEN) pLen = HEADER_LEN - 1;
                memcpy(headers[outCols], projectionTokens[i], pLen);
                headers[outCols][pLen] = '\0';
                outCols++;
            }
        }

        char resultData[MAX_ROWS][MAX_COLS][MAX_STR_LEN];
        int matchCount = 0;

        for (int i = 0; i < MAX_ROWS && matchCount < MAX_ROWS; i++) {
            if (!table1->rows[i].used) continue;
            const Value *v1 = &table1->rows[i].values[t1ColIdx];

            for (int j = 0; j < MAX_ROWS && matchCount < MAX_ROWS; j++) {
                if (!table2->rows[j].used) continue;
                const Value *v2 = &table2->rows[j].values[t2ColIdx];

                int match = 0;
                if (v1->type == TYPE_INT && v2->type == TYPE_INT) {
                    match = (v1->intVal == v2->intVal);
                } else if (v1->type == TYPE_VARCHAR && v2->type == TYPE_VARCHAR) {
                    match = (strcasecmp(v1->strVal, v2->strVal) == 0);
                }

                if (match) {
                    if (hasWhere) {
                        int passed = 1;
                        if (getColumnIndex(table1, whereCond.colName) != -1) {
                            passed = evaluateCondition(table1, &table1->rows[i], &whereCond);
                        } else if (getColumnIndex(table2, whereCond.colName) != -1) {
                            passed = evaluateCondition(table2, &table2->rows[j], &whereCond);
                        }
                        if (!passed) continue;
                    }

                    if (projIsAll) {
                        int colIdx = 0;
                        for (int c = 0; c < table1->colCount; c++) {
                            if (table1->cols[c].type == TYPE_INT) {
                                snprintf(resultData[matchCount][colIdx++], MAX_STR_LEN, "%d", table1->rows[i].values[c].intVal);
                            } else {
                                snprintf(resultData[matchCount][colIdx++], MAX_STR_LEN, "%s", table1->rows[i].values[c].strVal);
                            }
                        }
                        for (int c = 0; c < table2->colCount; c++) {
                            if (table2->cols[c].type == TYPE_INT) {
                                snprintf(resultData[matchCount][colIdx++], MAX_STR_LEN, "%d", table2->rows[j].values[c].intVal);
                            } else {
                                snprintf(resultData[matchCount][colIdx++], MAX_STR_LEN, "%s", table2->rows[j].values[c].strVal);
                            }
                        }
                    } else {
                        for (int c = 0; c < projCount; c++) {
                            const char *reqCol = projectionTokens[c];
                            int c1 = getColumnIndex(table1, reqCol);
                            int c2 = getColumnIndex(table2, reqCol);

                            if (c1 != -1) {
                                if (table1->cols[c1].type == TYPE_INT) {
                                    snprintf(resultData[matchCount][c], MAX_STR_LEN, "%d", table1->rows[i].values[c1].intVal);
                                } else {
                                    snprintf(resultData[matchCount][c], MAX_STR_LEN, "%s", table1->rows[i].values[c1].strVal);
                                }
                            } else if (c2 != -1) {
                                if (table2->cols[c2].type == TYPE_INT) {
                                    snprintf(resultData[matchCount][c], MAX_STR_LEN, "%d", table2->rows[j].values[c2].intVal);
                                } else {
                                    snprintf(resultData[matchCount][c], MAX_STR_LEN, "%s", table2->rows[j].values[c2].strVal);
                                }
                            } else {
                                snprintf(resultData[matchCount][c], MAX_STR_LEN, "NULL");
                            }
                        }
                    }
                    matchCount++;
                }
            }
        }

        renderAsciiTable(outCols, headers, matchCount, resultData);
        return;
    }

    if (table1) {
        if (DEBUG_MODE) {
            printf("[PLAN] Executing SELECT on table '%s'%s\n", 
                   table1->name, hasWhere ? " with WHERE filter" : "");
        }

        int projIsAll = (projCount == 0 || (projCount == 1 && strcmp(projectionTokens[0], "*") == 0));
        char headers[MAX_COLS][HEADER_LEN];
        int outCols = 0;

        if (projIsAll) {
            for (int i = 0; i < table1->colCount; i++) {
                size_t nLen = strlen(table1->cols[i].name);
                if (nLen >= HEADER_LEN) nLen = HEADER_LEN - 1;
                memcpy(headers[outCols], table1->cols[i].name, nLen);
                headers[outCols][nLen] = '\0';
                outCols++;
            }
        } else {
            for (int i = 0; i < projCount; i++) {
                size_t pLen = strlen(projectionTokens[i]);
                if (pLen >= HEADER_LEN) pLen = HEADER_LEN - 1;
                memcpy(headers[outCols], projectionTokens[i], pLen);
                headers[outCols][pLen] = '\0';
                outCols++;
            }
        }

        char resultData[MAX_ROWS][MAX_COLS][MAX_STR_LEN];
        int matchCount = 0;

        for (int i = 0; i < MAX_ROWS && matchCount < MAX_ROWS; i++) {
            if (!table1->rows[i].used) continue;

            if (hasWhere && !evaluateCondition(table1, &table1->rows[i], &whereCond)) {
                continue;
            }

            if (projIsAll) {
                for (int c = 0; c < table1->colCount; c++) {
                    if (table1->cols[c].type == TYPE_INT) {
                        snprintf(resultData[matchCount][c], MAX_STR_LEN, "%d", table1->rows[i].values[c].intVal);
                    } else {
                        snprintf(resultData[matchCount][c], MAX_STR_LEN, "%s", table1->rows[i].values[c].strVal);
                    }
                }
            } else {
                for (int c = 0; c < projCount; c++) {
                    int cIdx = getColumnIndex(table1, projectionTokens[c]);
                    if (cIdx != -1) {
                        if (table1->cols[cIdx].type == TYPE_INT) {
                            snprintf(resultData[matchCount][c], MAX_STR_LEN, "%d", table1->rows[i].values[cIdx].intVal);
                        } else {
                            snprintf(resultData[matchCount][c], MAX_STR_LEN, "%s", table1->rows[i].values[cIdx].strVal);
                        }
                    } else {
                        snprintf(resultData[matchCount][c], MAX_STR_LEN, "NULL");
                    }
                }
            }
            matchCount++;
        }

        renderAsciiTable(outCols, headers, matchCount, resultData);
    }
}

static void handleInsert(char *p) {
    if (!p) {
        printf("Usage: INSERT INTO <table> VALUES (val1, val2, ...)\n");
        printf("   or: INSERT <id> <name> [dept_id]\n");
        return;
    }

    char *firstTok = next_token(&p);
    if (!firstTok) return;

    Table *targetTable = NULL;
    Value vals[MAX_COLS];
    int valCount = 0;

    if (strcasecmp(firstTok, "INTO") == 0) {
        char *tableName = next_token(&p);
        if (!tableName) {
            printf("Error: Missing table name in INSERT INTO\n");
            return;
        }
        targetTable = findTable(tableName);
        if (!targetTable) {
            printf("Error: Table '%s' does not exist.\n", tableName);
            return;
        }

        char *valWord = next_token(&p);
        if (valWord && strcasecmp(valWord, "VALUES") == 0) {
            valCount = parseValuesList(p, vals, targetTable->colCount);
        } else if (valWord && (valWord[0] == '(' || isdigit((unsigned char)valWord[0]))) {
            char combined[512];
            snprintf(combined, sizeof(combined), "%s %s", valWord, p ? p : "");
            valCount = parseValuesList(combined, vals, targetTable->colCount);
        }
    } else {
        Table *t = findTable(firstTok);
        if (t) {
            targetTable = t;
            char *valWord = next_token(&p);
            if (valWord && strcasecmp(valWord, "VALUES") == 0) {
                valCount = parseValuesList(p, vals, targetTable->colCount);
            } else if (valWord) {
                char combined[512];
                snprintf(combined, sizeof(combined), "%s %s", valWord, p ? p : "");
                valCount = parseValuesList(combined, vals, targetTable->colCount);
            }
        } else {
            targetTable = (tableCount > 0) ? &tables[0] : NULL;
            if (!targetTable) {
                printf("Error: No active table.\n");
                return;
            }
            vals[valCount++] = parseLiteralValue(firstTok);

            char *nametok = next_token(&p);
            if (nametok) {
                vals[valCount++] = parseLiteralValue(nametok);
            }
            char *deptTok = next_token(&p);
            if (deptTok && valCount < targetTable->colCount) {
                vals[valCount++] = parseLiteralValue(deptTok);
            }
        }
    }

    if (!targetTable) {
        printf("Error: Invalid INSERT syntax.\n");
        return;
    }

    while (valCount < targetTable->colCount) {
        vals[valCount].type = targetTable->cols[valCount].type;
        vals[valCount].intVal = 0;
        vals[valCount].strVal[0] = '\0';
        valCount++;
    }

    if (insertTableRow(targetTable, vals) == 0) {
        printf("Inserted into '%s': ", targetTable->name);
        for (int i = 0; i < targetTable->colCount; i++) {
            if (targetTable->cols[i].type == TYPE_INT) {
                printf("%d ", vals[i].intVal);
            } else {
                printf("%s ", vals[i].strVal);
            }
        }
        printf("\n");
    } else {
        printf("Insert failed.\n");
    }
}

static void handleUpdate(char *p) {
    if (!p) {
        printf("Usage: UPDATE <table> SET <col> = <val> WHERE <col> = <val>\n");
        printf("   or: UPDATE WHERE <col> = <val> SET <col> = <val>\n");
        return;
    }

    char *firstTok = next_token(&p);
    if (!firstTok) return;

    Table *targetTable = NULL;
    char setColName[MAX_NAME_LEN] = "";
    Value setVal;
    WhereCondition whereCond;
    int hasWhere = 0;

    if (strcasecmp(firstTok, "WHERE") == 0) {
        targetTable = (tableCount > 0) ? &tables[0] : NULL;
        hasWhere = parseWhereClause(&p, &whereCond);

        char *setWord = next_token(&p);
        if (!setWord || strcasecmp(setWord, "SET") != 0) {
            printf("Error: Expected SET clause in UPDATE.\n");
            return;
        }
        char *col = next_token(&p);
        char *eq = next_token(&p);
        char *val = next_token(&p);
        if (!col || !eq || !val) {
            printf("Error: Invalid SET expression.\n");
            return;
        }
        size_t cLen = strlen(col);
        if (cLen >= MAX_NAME_LEN) cLen = MAX_NAME_LEN - 1;
        memcpy(setColName, col, cLen);
        setColName[cLen] = '\0';
        setVal = parseLiteralValue(val);
    } else {
        Table *t = findTable(firstTok);
        if (t) {
            targetTable = t;
            char *setWord = next_token(&p);
            if (!setWord || strcasecmp(setWord, "SET") != 0) {
                printf("Error: Expected SET after table name.\n");
                return;
            }
            char *col = next_token(&p);
            char *eq = next_token(&p);
            char *val = next_token(&p);
            if (!col || !eq || !val) {
                printf("Error: Invalid SET expression.\n");
                return;
            }
            size_t cLen = strlen(col);
            if (cLen >= MAX_NAME_LEN) cLen = MAX_NAME_LEN - 1;
            memcpy(setColName, col, cLen);
            setColName[cLen] = '\0';
            setVal = parseLiteralValue(val);

            char *whereWord = next_token(&p);
            if (whereWord && strcasecmp(whereWord, "WHERE") == 0) {
                hasWhere = parseWhereClause(&p, &whereCond);
            }
        } else {
            targetTable = (tableCount > 0) ? &tables[0] : NULL;
            char *nameTok = next_token(&p);
            if (nameTok && targetTable) {
                int id = atoi(firstTok);
                if (updateRow(id, nameTok) == 0) {
                    printf("Updated: %d %s\n", id, nameTok);
                } else {
                    printf("Update failed\n");
                }
                return;
            }
        }
    }

    if (!targetTable) {
        printf("Error: Table not found.\n");
        return;
    }

    int setColIdx = getColumnIndex(targetTable, setColName);
    if (setColIdx == -1) {
        printf("Error: Column '%s' not found in table '%s'.\n", setColName, targetTable->name);
        return;
    }

    int updatedCount = 0;
    for (int i = 0; i < MAX_ROWS; i++) {
        if (!targetTable->rows[i].used) continue;
        if (!hasWhere || evaluateCondition(targetTable, &targetTable->rows[i], &whereCond)) {
            if (updateTableRow(targetTable, i, setColIdx, &setVal) == 0) {
                updatedCount++;
            }
        }
    }

    if (updatedCount > 0) {
        printf("Updated %d row%s in '%s'.\n", updatedCount, updatedCount == 1 ? "" : "s", targetTable->name);
    } else {
        printf("No rows matched for UPDATE.\n");
    }
}

static void handleDelete(char *p) {
    if (!p) {
        printf("Usage: DELETE FROM <table> WHERE <col> = <val>\n");
        printf("   or: DELETE WHERE <col> = <val>\n");
        return;
    }

    char *firstTok = next_token(&p);
    if (!firstTok) return;

    Table *targetTable = NULL;
    WhereCondition whereCond;
    int hasWhere = 0;

    if (strcasecmp(firstTok, "FROM") == 0) {
        char *tableName = next_token(&p);
        if (!tableName) {
            printf("Error: Missing table name after FROM.\n");
            return;
        }
        targetTable = findTable(tableName);
        if (!targetTable) {
            printf("Error: Table '%s' not found.\n", tableName);
            return;
        }
        char *whereWord = next_token(&p);
        if (whereWord && strcasecmp(whereWord, "WHERE") == 0) {
            hasWhere = parseWhereClause(&p, &whereCond);
        }
    } else if (strcasecmp(firstTok, "WHERE") == 0) {
        targetTable = (tableCount > 0) ? &tables[0] : NULL;
        hasWhere = parseWhereClause(&p, &whereCond);
    } else {
        int id = atoi(firstTok);
        if (deleteRow(id) == 0) {
            printf("Deleted: %d\n", id);
        } else {
            printf("Delete failed\n");
        }
        return;
    }

    if (!targetTable) {
        printf("Error: Table not found.\n");
        return;
    }

    int deletedCount = 0;
    for (int i = 0; i < MAX_ROWS; i++) {
        if (!targetTable->rows[i].used) continue;
        if (!hasWhere || evaluateCondition(targetTable, &targetTable->rows[i], &whereCond)) {
            if (deleteTableRow(targetTable, i) == 0) {
                deletedCount++;
            }
        }
    }

    if (deletedCount > 0) {
        printf("Deleted %d row%s from '%s'.\n", deletedCount, deletedCount == 1 ? "" : "s", targetTable->name);
    } else {
        printf("No rows matched for DELETE.\n");
    }
}

static void handleCreateTable(char *p) {
    char *tableName = next_token(&p);
    if (!tableName) {
        printf("Usage: CREATE TABLE <name> (<col1> <type>, <col2> <type>, ...)\n");
        return;
    }

    Column cols[MAX_COLS];
    int colCount = 0;

    char *colDefStr = p;
    char *openParen = strchr(colDefStr, '(');
    if (openParen) colDefStr = openParen + 1;

    char *curr = colDefStr;
    while (*curr && *curr != ')' && *curr != ';' && colCount < MAX_COLS) {
        while (*curr && (isspace((unsigned char)*curr) || *curr == ',' || *curr == '(')) curr++;
        if (*curr == '\0' || *curr == ')' || *curr == ';') break;

        char cName[MAX_NAME_LEN] = "";
        char cType[MAX_NAME_LEN] = "";
        int nIdx = 0, tIdx = 0;

        while (*curr && !isspace((unsigned char)*curr) && *curr != ',' && *curr != ')' && nIdx < MAX_NAME_LEN - 1) {
            cName[nIdx++] = *curr++;
        }
        cName[nIdx] = '\0';

        while (*curr && isspace((unsigned char)*curr)) curr++;

        while (*curr && !isspace((unsigned char)*curr) && *curr != ',' && *curr != ')' && *curr != ';' && tIdx < MAX_NAME_LEN - 1) {
            cType[tIdx++] = *curr++;
        }
        cType[tIdx] = '\0';

        if (nIdx > 0) {
            size_t nameLen = strlen(cName);
            if (nameLen >= MAX_NAME_LEN) nameLen = MAX_NAME_LEN - 1;
            memcpy(cols[colCount].name, cName, nameLen);
            cols[colCount].name[nameLen] = '\0';

            if (strcasecmp(cType, "INT") == 0 || strcasecmp(cType, "INTEGER") == 0) {
                cols[colCount].type = TYPE_INT;
            } else {
                cols[colCount].type = TYPE_VARCHAR;
            }
            colCount++;
        }

        while (*curr && (isspace((unsigned char)*curr) || *curr == ',')) curr++;
    }

    if (colCount == 0) {
        printf("Error: No columns specified.\n");
        return;
    }

    Table *t = createTable(tableName, colCount, cols, 0);
    if (t) {
        printf("Table '%s' created successfully with %d columns.\n", tableName, colCount);
    } else {
        printf("Failed to create table '%s'.\n", tableName);
    }
}

void printHelp(void) {
    printf("\n======================== TransactiDB Help ========================\n");
    printf("1. SELECT & JOIN Queries:\n");
    printf("   - SELECT * FROM STUDENT\n");
    printf("   - SELECT * WHERE ID = 1\n");
    printf("   - SELECT * WHERE NAME = AKSHAY\n");
    printf("   - SELECT STUDENT.NAME, DEPARTMENT.DEPT_NAME FROM STUDENT JOIN DEPARTMENT ON STUDENT.DEPT_ID = DEPARTMENT.DEPT_ID\n\n");
    printf("2. Data Manipulation (CRUD):\n");
    printf("   - INSERT INTO STUDENT VALUES (5, 'Alice', 10)\n");
    printf("   - INSERT INTO DEPARTMENT VALUES (50, 'CIVIL')\n");
    printf("   - UPDATE WHERE ID = 1 SET NAME = RAHUL\n");
    printf("   - UPDATE STUDENT SET DEPT_ID = 20 WHERE ID = 3\n");
    printf("   - DELETE WHERE ID = 2\n");
    printf("   - DELETE FROM STUDENT WHERE DEPT_ID = 30\n\n");
    printf("3. ACID Transactions:\n");
    printf("   - BEGIN                  (Starts an isolated transaction)\n");
    printf("   - COMMIT                 (Persists all changes)\n");
    printf("   - ROLLBACK               (Reverts all changes since BEGIN)\n");
    printf("   - STATUS                 (Displays active transaction state)\n\n");
    printf("4. Batch Scheduling & Scripts:\n");
    printf("   - QUEUE <query>          (Enqueues a query for batch execution)\n");
    printf("   - RUN                    (Executes all queued transactions in FIFO order)\n");
    printf("   - RUN <file.sql>         (Executes all SQL commands from a script file)\n\n");
    printf("5. Schema & Concurrency:\n");
    printf("   - SHOW TABLES / TABLES   (Lists all relational tables)\n");
    printf("   - DESCRIBE <table_name>  (Displays table schema)\n");
    printf("   - CREATE TABLE <name> (<col1> <type>, ...)\n");
    printf("   - LOCKS / DEADLOCK       (Lock manager and wait graph analysis)\n");
    printf("   - DEBUG ON / DEBUG OFF   (Query planner execution trace)\n");
    printf("   - EXIT / QUIT            (Close database)\n");
    printf("==================================================================\n\n");
}

void processQuery(char *line) {
    if (!line) return;
    trim_whitespace(line);
    if (strlen(line) == 0) return;

    char copy[512];
    size_t lineLen = strlen(line);
    if (lineLen >= sizeof(copy)) lineLen = sizeof(copy) - 1;
    memcpy(copy, line, lineLen);
    copy[lineLen] = '\0';

    char *p = copy;
    char *cmd = next_token(&p);
    if (!cmd) return;

    for (char *q = cmd; *q; q++) *q = toupper((unsigned char)*q);

    if (strcmp(cmd, "BEGIN") == 0) {
        int txId = txBegin();
        if (txId > 0) {
            printf("Transaction #%d started.\n", txId);
        }
    } else if (strcmp(cmd, "COMMIT") == 0) {
        if (txCommit() == 0) {
            printf("Transaction committed.\n");
        }
    } else if (strcmp(cmd, "ROLLBACK") == 0) {
        if (txRollback() == 0) {
            printf("Transaction rolled back.\n");
        }
    } else if (strcmp(cmd, "STATUS") == 0) {
        printf("Database Status:\n");
        printf("  - Active Transaction: %s\n", txIsActive() ? "YES" : "NO (Autocommit)");
        if (txIsActive()) {
            printf("  - Transaction ID: #%d\n", currentTx.txId);
            printf("  - Undo Log Depth: %d operation(s)\n", txGetUndoCount());
        }
        printf("  - Tables Loaded: %d\n", tableCount);
        printf("  - Debug Mode: %s\n", DEBUG_MODE ? "ON" : "OFF");
    } else if (strcmp(cmd, "SHOW") == 0 || strcmp(cmd, "TABLES") == 0) {
        printAllTables();
    } else if (strcmp(cmd, "DESCRIBE") == 0 || strcmp(cmd, "DESC") == 0 || strcmp(cmd, "SCHEMA") == 0) {
        char *tableName = next_token(&p);
        if (!tableName) {
            printAllTables();
        } else {
            Table *t = findTable(tableName);
            if (t) printTableSchema(t);
            else printf("Table '%s' not found.\n", tableName);
        }
    } else if (strcmp(cmd, "CREATE") == 0) {
        char *sub = next_token(&p);
        if (sub && strcasecmp(sub, "TABLE") == 0) {
            handleCreateTable(p);
        } else {
            printf("Unknown CREATE command. Try: CREATE TABLE ...\n");
        }
    } else if (strcmp(cmd, "SELECT") == 0) {
        handleSelect(p);
    } else if (strcmp(cmd, "INSERT") == 0) {
        handleInsert(p);
    } else if (strcmp(cmd, "UPDATE") == 0) {
        handleUpdate(p);
    } else if (strcmp(cmd, "DELETE") == 0) {
        handleDelete(p);
    }
    // 3. Batch Scheduler & Script Execution (RUN / QUEUE)
    else if (strcmp(cmd, "QUEUE") == 0 || strcmp(cmd, "ADD") == 0) {
        if (!p || strlen(p) == 0) {
            printf("Usage: QUEUE <query>\n");
            return;
        }
        addTransaction(p);
        printf("[SCHEDULER] Queued transaction #%d: '%s'\n", rear - 1, p);
    } else if (strcmp(cmd, "RUN") == 0 || strcmp(cmd, "EXECUTE") == 0 || strcmp(cmd, "EXEC") == 0) {
        char *arg = next_token(&p);
        if (arg) {
            // Run script file: RUN <file.sql>
            FILE *f = fopen(arg, "r");
            if (!f) {
                printf("Error: Could not open script file '%s'\n", arg);
                return;
            }
            printf("[RUNNER] Executing script '%s'...\n", arg);
            char fileLine[512];
            int count = 0;
            while (fgets(fileLine, sizeof(fileLine), f)) {
                trim_whitespace(fileLine);
                if (strlen(fileLine) == 0 || fileLine[0] == '-' || fileLine[0] == '#') continue;
                printf("[RUNNER] > %s\n", fileLine);
                processQuery(fileLine);
                count++;
            }
            fclose(f);
            printf("[RUNNER] Finished executing %d statement%s from '%s'.\n", count, count == 1 ? "" : "s", arg);
        } else {
            // Execute in-memory queued transactions
            if (front >= rear) {
                printf("[SCHEDULER] Transaction queue is empty. Use 'QUEUE <query>' or 'RUN <script.sql>'.\n");
            } else {
                printf("[SCHEDULER] Executing %d queued transaction%s...\n", rear - front, (rear - front) == 1 ? "" : "s");
                executeTransactions();
                front = 0;
                rear = 0;
            }
        }
    } else if (strcmp(cmd, "LOCKS") == 0) {
        printLockStatus();
    } else if (strcmp(cmd, "DEADLOCK") == 0 || strcmp(cmd, "GRAPH") == 0) {
        printWaitGraph();
    } else if (strcmp(cmd, "DEBUG") == 0) {
        char *arg = next_token(&p);
        if (!arg) {
            printf("Usage: DEBUG ON/OFF\n");
            return;
        }
        if (strcasecmp(arg, "ON") == 0) {
            DEBUG_MODE = 1;
            printf("Debug mode enabled.\n");
        } else if (strcasecmp(arg, "OFF") == 0) {
            DEBUG_MODE = 0;
            printf("Debug mode disabled.\n");
        } else {
            printf("Usage: DEBUG ON/OFF\n");
        }
    } else if (strcmp(cmd, "HELP") == 0 || strcmp(cmd, "?") == 0) {
        printHelp();
    } else if (strcmp(cmd, "EXIT") == 0 || strcmp(cmd, "QUIT") == 0) {
        printf("Exiting TransactiDB.\n");
        exit(0);
    } else {
        printf("Unknown command '%s'. Type HELP for a list of supported commands.\n", cmd);
    }
}
