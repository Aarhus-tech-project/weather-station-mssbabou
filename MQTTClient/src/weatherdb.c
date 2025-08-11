#include "weatherdb.h"
#include "db.h"
#include <stdio.h>

int wdb_insert(const weather_data_t *data) {
    const char *params[7];
    char temp_str[32], hum_str[32], pres_str[32], eco2_str[16], tvoc_str[16];

    snprintf(temp_str, sizeof(temp_str), "%.2f", data->temperature);
    snprintf(hum_str, sizeof(hum_str), "%.2f", data->humidity);
    snprintf(pres_str, sizeof(pres_str), "%.2f", data->pressure);
    snprintf(eco2_str, sizeof(eco2_str), "%d", data->eco2);
    snprintf(tvoc_str, sizeof(tvoc_str), "%d", data->tvoc);

    params[0] = data->device_id;
    params[1] = data->timestamp;
    params[2] = temp_str;
    params[3] = hum_str;
    params[4] = pres_str;
    params[5] = eco2_str;
    params[6] = tvoc_str;

    PGresult *res = db_query_params(
        "INSERT INTO weatherdata "
        "(device_id, timestamp, temperature, humidity, pressure, eco2, tvoc) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7)",
        7, params);

    if (!res) return -1;
    PQclear(res);
    return 0;
}