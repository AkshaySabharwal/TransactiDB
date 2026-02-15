#include <stdio.h>
#include <string.h>
#include "db.h"
#include "parser.h"

int main(void) {
    char line[256];
    init_db();
    printf("TransactiDB Prototype\n");
    printf("Commands: INSERT <id> <name>, SELECT <id>, UPDATE <id> <name>, DELETE <id>, EXIT\n");
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        if (strlen(line) <= 1) continue;
        processQuery(line);
    }
    return 0;
}
