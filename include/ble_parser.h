#ifndef BLE_PARSER_H
#define BLE_PARSER_H

#include <stdbool.h>
#include <stdint.h>

#define BLE_DEVICE_TYPE_MAX_LEN 16

typedef struct {
    float temperature;
    float humidity;
    uint16_t battery_mv;
    uint8_t battery_pct;
    char device_type[BLE_DEVICE_TYPE_MAX_LEN];
    bool has_data;
} ble_sensor_data_t;

bool ble_parse_pvvx_format(const uint8_t *svc_data, uint8_t svc_len,
                           ble_sensor_data_t *sensor_data);

bool ble_parse_atc_format(const uint8_t *svc_data, uint8_t svc_len,
                          ble_sensor_data_t *sensor_data);

bool ble_parse_mibeacon_format(const uint8_t *svc_data, uint8_t svc_len,
                               ble_sensor_data_t *sensor_data);

bool ble_parse_bthome_v2_format(const uint8_t *svc_data, uint8_t svc_len,
                                ble_sensor_data_t *sensor_data);

bool ble_parse_sensor_data(const uint8_t *adv_data, uint8_t adv_len,
                           uint16_t company_id, ble_sensor_data_t *sensor_data);

const char* ble_get_device_type(uint16_t company_id);

#endif
