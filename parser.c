#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "parser.h"
#include "db.h"



static void trim_newline(char *s)
{
    char *p = strchr(s, '\n');
    if (p)
        *p = '\0';
}

static char *next_token(char **p)
{
    if (!p || !*p)
        return NULL;
    while (**p && isspace((unsigned char)**p))
        (*p)++;
    if (**p == '\0')
        return NULL;
    char *start = *p;
    while (**p && !isspace((unsigned char)**p))
        (*p)++;
    if (**p)
    {
        **p = '\0';
        (*p)++;
    }
    return start;
}

void processQuery(char *line)
{
    trim_newline(line);
    char copy[256];
    strncpy(copy, line, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    char *p = copy;
    char *cmd = next_token(&p);
    if (!cmd)
        return;

    for (char *q = cmd; *q; q++)
        *q = toupper((unsigned char)*q);

    if (strcmp(cmd, "INSERT") == 0)
    {
        char *idtok = next_token(&p);
        char *nametok = p;
        if (!idtok || !nametok)
        {
            printf("Usage: INSERT <id> <name>\n");
            return;
        }
        int id = atoi(idtok);
        while (*nametok && isspace((unsigned char)*nametok))
            nametok++;
        if (insertRow(id, nametok) == 0)
            printf("Inserted: %d %s\n", id, nametok);
        else
            printf("Insert failed\n");
    }

    else if (strcmp(cmd, "SELECT") == 0)
    {
        char *idtok = next_token(&p);
        if (!idtok)
        {
            printf("Usage: SELECT <id>\n");
            return;
        }
        int id = atoi(idtok);
        Row *r = selectRow(id);
        if (r)
            printf("Found: %d %s\n", r->id, r->name);
        else
            printf("Not found\n");
    }

    else if (strcmp(cmd, "UPDATE") == 0)
    {
        char *idtok = next_token(&p);
        char *nametok = p;
        if (!idtok || !nametok)
        {
            printf("Usage: UPDATE <id> <name>\n");
            return;
        }
        int id = atoi(idtok);
        while (*nametok && isspace((unsigned char)*nametok))
            nametok++;
        if (updateRow(id, nametok) == 0)
            printf("Updated: %d %s\n", id, nametok);
        else
            printf("Update failed\n");
    }

    else if (strcmp(cmd, "DELETE") == 0)
    {
        char *idtok = next_token(&p);
        int id = atoi(idtok);
        if (deleteRow(id) == 0)
            printf("Deleted: %d\n", id);
        else
            printf("Delete failed\n");
    }

    else if (strcmp(cmd, "EXIT") == 0 || strcmp(cmd, "QUIT") == 0)
    {
        printf("Exiting.\n");
        exit(0);
    }

    else if (strcmp(cmd, "DEBUG") == 0)
    {
        char *arg = next_token(&p);
        if (!arg)
        {
            printf("Usage: DEBUG ON/OFF\n");
            return;
        }

        for (char *q = arg; *q; q++)
            *q = toupper((unsigned char)*q);

        if (strcmp(arg, "ON") == 0)
        {
            DEBUG_MODE = 1;
            printf("Debug mode enabled.\n");
        }
        else if (strcmp(arg, "OFF") == 0)
        {
            DEBUG_MODE = 0;
            printf("Debug mode disabled.\n");
        }
        else
        {
            printf("Usage: DEBUG ON/OFF\n");
        }
    }

    else
    {
        printf("Unknown command.\n");
    }
}
