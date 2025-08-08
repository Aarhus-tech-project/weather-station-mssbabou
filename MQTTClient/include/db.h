#pragma once

#include <libpq-fe.h>

#define DB_CONNECTION_STRING "host=192.168.108.11 port=5432 dbname=Weather user=markus password=Datait2025!"

void db_init();
void db_close();
PGresult *db_query(const char *sql);
PGresult *db_query_params(const char *sql, int nParams, const char *const *paramValues);
int db_exec(const char *sql);