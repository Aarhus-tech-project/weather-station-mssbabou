#pragma once

#include <libpq-fe.h>

void db_init(const char *conninfo);
void db_close();
PGresult *db_query(const char *sql);
PGresult *db_query_params(const char *sql, int nParams, const char *const *paramValues);
int db_exec(const char *sql);