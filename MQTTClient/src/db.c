#include "db.h"
#include <stdio.h>
#include <stdlib.h>

static PGconn *db_conn = NULL;

void db_init(const char* conninfo)
{
    db_conn = PQconnectdb(conninfo);
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

PGresult *db_query_params(const char *sql, int nParams,
                          const char *const *paramValues) {
    if (!db_conn) {
        fprintf(stderr, "db_query_params: connection not initialized\n");
        return NULL;
    }

    PGresult *res = PQexecParams(
        db_conn,
        sql,
        nParams,       // number of parameters
        NULL,          // let Postgres infer types
        paramValues,   // actual values
        NULL,          // param lengths (text)
        NULL,          // param formats (text)
        0              // 0 = text results
    );

    ExecStatusType status = PQresultStatus(res);

    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        fprintf(stderr, "Parameterized query failed: %s", PQerrorMessage(db_conn));
        PQclear(res);
        return NULL;
    }

    return res;
}

int db_exec(const char *sql) {
    PGresult *res = db_query(sql);
    if (!res) return -1;

    PQclear(res);
    return 0;
}