#pragma once
#include "weatherdata.h"

#define DB_CONNECTION_STRING "host=127.0.0.1 port=5432 dbname=Weather user=markus password=Datait2025!"

int wdb_insert(const weather_data_t *data);