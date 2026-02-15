#include "ble_parser.h"
#include <string.h>

bool ble_parse_pvvx_format(const uint8_t *svc_data, uint8_t svc_len,
                           ble_sensor_data_t *sensor_data) {
    if (svc_len < 17) {
        return false;
    }

    int16_t temp_raw = svc_data[8] | (svc_data[9] << 8);
    uint16_t humi_raw = svc_data[10] | (svc_data[11] << 8);

    sensor_data->temperature = temp_raw / 100.0f;
    sensor_data->humidity = humi_raw / 100;
    sensor_data->battery_mv = svc_data[12] | (svc_data[13] << 8);
    sensor_data->battery_pct = svc_data[14];
    strcpy(sensor_data->device_type, "pvvx");
    sensor_data->has_data = true;

    return true;
}

bool ble_parse_atc_format(const uint8_t *svc_data, uint8_t svc_len,
                          ble_sensor_data_t *sensor_data) {
    if (svc_len < 15) {
        return false;
    }

    int16_t temp_raw = (svc_data[8] << 8) | svc_data[9];

    sensor_data->temperature = temp_raw / 10.0f;
    sensor_data->humidity = svc_data[10];
    sensor_data->battery_pct = svc_data[11];
    sensor_data->battery_mv = (svc_data[12] << 8) | svc_data[13];
    strcpy(sensor_data->device_type, "ATC");
    sensor_data->has_data = true;

    return true;
}

bool ble_parse_mibeacon_format(const uint8_t *svc_data, uint8_t svc_len,
                               ble_sensor_data_t *sensor_data) {
    if (svc_len < 11) {
        return false;
    }

    uint8_t flags = svc_data[0];
    bool has_encryption = (flags & 0x08) != 0;
    bool has_data = (flags & 0x40) != 0;

    if (has_encryption) {
        return false;
    }

    if (!has_data) {
        return false;
    }

    uint16_t device_uuid = svc_data[2] | (svc_data[3] << 8);

    if (device_uuid != 0x055b) {
        return false;
    }

    bool has_capability = (flags & 0x20) != 0;
    uint8_t payload_offset = has_capability ? 12 : 11;

    if (svc_len < payload_offset + 3) {
        return false;
    }

    uint8_t pos = payload_offset;
    bool found_temp = false;
    bool found_hum = false;

    while (pos + 3 <= svc_len) {
        uint16_t value_type = svc_data[pos] | (svc_data[pos + 1] << 8);
        uint8_t value_len = svc_data[pos + 2];

        if (pos + 3 + value_len > svc_len) {
            break;
        }

        const uint8_t *data = &svc_data[pos + 3];

        switch (value_type) {
            case 0x1004:
                if (value_len == 2) {
                    int16_t temp_raw = data[0] | (data[1] << 8);
                    sensor_data->temperature = temp_raw / 10.0f;
                    found_temp = true;
                }
                break;

            case 0x1006:
                if (value_len == 2) {
                    int16_t hum_raw = data[0] | (data[1] << 8);
                    sensor_data->humidity = hum_raw / 10;
                    found_hum = true;
                }
                break;

            case 0x100A:
                if (value_len == 1) {
                    sensor_data->battery_pct = data[0];
                    sensor_data->battery_mv = 0;
                }
                break;

            case 0x100D:
                if (value_len == 4) {
                    int16_t temp_raw = data[0] | (data[1] << 8);
                    int16_t hum_raw = data[2] | (data[3] << 8);
                    sensor_data->temperature = temp_raw / 10.0f;
                    sensor_data->humidity = hum_raw / 10;
                    found_temp = true;
                    found_hum = true;
                }
                break;
        }

        pos += 3 + value_len;
    }

    if (found_temp || found_hum) {
        strcpy(sensor_data->device_type, "MiBeacon");
        sensor_data->has_data = true;
        return true;
    }

    return false;
}

bool ble_parse_bthome_v2_format(const uint8_t *svc_data, uint8_t svc_len,
                                ble_sensor_data_t *sensor_data) {
    if (svc_len < 6) {
        return false;
    }

    uint16_t uuid = svc_data[0] | (svc_data[1] << 8);
    if (uuid != 0xFCD2) {
        return false;
    }

    uint8_t dev_info = svc_data[2];

    bool is_encrypted = (dev_info & 0x01) != 0;
    if (is_encrypted) {
        return false;
    }

    uint8_t version = (dev_info >> 5) & 0x07;
    if (version != 2) {
        return false;
    }

    uint8_t pos = 3;
    bool found_temp = false;
    bool found_hum = false;

    while (pos < svc_len) {
        uint8_t object_id = svc_data[pos];
        pos++;

        if (pos >= svc_len) {
            break;
        }

        switch (object_id) {
            case 0x00:
                pos += 1;
                break;

            case 0x01:
                if (pos + 1 <= svc_len) {
                    sensor_data->battery_pct = svc_data[pos];
                    sensor_data->battery_mv = 0;
                    pos += 1;
                }
                break;

            case 0x02:
                if (pos + 2 <= svc_len) {
                    int16_t temp_raw = svc_data[pos] | (svc_data[pos + 1] << 8);
                    sensor_data->temperature = temp_raw / 100.0f;
                    found_temp = true;
                    pos += 2;
                }
                break;

            case 0x03:
                if (pos + 2 <= svc_len) {
                    uint16_t hum_raw = svc_data[pos] | (svc_data[pos + 1] << 8);
                    sensor_data->humidity = hum_raw / 100;
                    found_hum = true;
                    pos += 2;
                }
                break;

            case 0x2E:
                if (pos + 1 <= svc_len) {
                    sensor_data->humidity = svc_data[pos];
                    found_hum = true;
                    pos += 1;
                }
                break;

            case 0x45:
                if (pos + 2 <= svc_len) {
                    int16_t temp_raw = svc_data[pos] | (svc_data[pos + 1] << 8);
                    sensor_data->temperature = temp_raw / 10.0f;
                    found_temp = true;
                    pos += 2;
                }
                break;

            default:
                pos = svc_len;
                break;
        }
    }

    if (found_temp || found_hum) {
        strcpy(sensor_data->device_type, "BTHome");
        sensor_data->has_data = true;
        return true;
    }

    return false;
}

bool ble_parse_sensor_data(const uint8_t *adv_data, uint8_t adv_len,
                           uint16_t company_id, ble_sensor_data_t *sensor_data) {
    memset(sensor_data, 0, sizeof(ble_sensor_data_t));
    sensor_data->has_data = false;
    strcpy(sensor_data->device_type, "Unknown");
    (void)adv_data;
    (void)adv_len;
    (void)company_id;
    return false;
}

const char* ble_get_device_type(uint16_t company_id) {
    switch (company_id) {
        case 0x038F:
            return "Xiaomi";
        case 0x004C:
            return "Apple";
        case 0x0006:
            return "Microsoft";
        case 0x0075:
            return "Samsung";
        case 0x00E0:
            return "Google";
        default:
            return "Unknown";
    }
}
