#include <stdio.h>
#include <string.h>
#include "db.h"
#include "parser.h"
#include "transaction.h"
#include "lock.h"

int main(void) {
    char line[512];
    init_db();
    initLockManager();

    printf("===================================================================\n");
    printf("   _______                     __  _ ___  ____  \n");
    printf("  /_  __/________ _____  _____/ /_(_) _ \\/ __ ) \n");
    printf("   / / / ___/ __ `/ __ \\/ ___/ __/ / // / __  | \n");
    printf("  / / / /  / /_/ / / / (__  ) /_/ / // / /_/ /  \n");
    printf(" /_/ /_/   \\__,_/_/ /_/____/\\__/_/____/_____/   \n");
    printf(" TransactiDB - Relational & Transactional In-Memory Engine\n");
    printf("===================================================================\n");
    printf("Features: Multiple Tables, Relational JOINs, WHERE Filters, ACID TXs\n");
    printf("Type 'HELP' for commands or 'SHOW TABLES' to inspect preloaded tables.\n\n");

    while (1) {
        if (txIsActive()) {
            printf("TransactiDB [TX#%d]> ", currentTx.txId);
        } else {
            printf("TransactiDB> ");
        }
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;
        if (strlen(line) <= 1) continue;
        processQuery(line);
    }

    printf("\nGoodbye!\n");
    return 0;
}
