#include "db.h"
#include <stdio.h>
#include <stdlib.h>

static PGconn *db_conn = NULL;

void db_init()
{
    db_conn = PQconnectdb(DB_CONNECTION_STRING);
    if (PQstatus(db_conn) != CONNECTION_OK) {
        fprintf(stderr, "Database Connection failed: %s\n", PQerrorMessage(db_conn));
        PQfinish(db_conn);
        exit(EXIT_FAILURE);
    }
}

void db_close(void) {
    if (db_conn) {
        PQfinish(db_conn);
        db_conn = NULL;
    }
}

PGresult *db_query(const char *sql) {
    if (!db_conn) {
        fprintf(stderr, "db_query: connection not initialized\n");
        return NULL;
    }

    PGresult *res = PQexec(db_conn, sql);
    ExecStatusType status = PQresultStatus(res);

    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        fprintf(stderr, "Query failed: %s", PQerrorMessage(db_conn));
        PQclear(res);
        return NULL;
    }

    return res; // caller must PQclear()
}

int db_exec(const char *sql) {
    PGresult *res = db_query(sql);
    if (!res) return -1;

    PQclear(res);
    return 0;
}