#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define SETTINGS_FORMAT_VERSION 3u
#define SETTINGS_PARAMETERS 48
#define SETTINGS_RECORD_BYTES 512
#define SETTINGS_LEGACY_BYTES 244
#define SETTINGS_LEGACY_OFFSET 0x1ff000u
#define SETTINGS_SECTOR_A 0x3fe000u
#define SETTINGS_SECTOR_B 0x3ff000u
typedef struct {
    uint8_t key,scale,instrument,imu_axes,volume,contrast,chord;
    uint8_t presets[4][SETTINGS_PARAMETERS];
    uint8_t scales[4][12];
} settings_data_t;
#ifdef __cplusplus
extern "C" {
#endif
bool settings_validate(const settings_data_t *data,uint8_t scale_count);
bool settings_decode(const uint8_t *record,size_t size,uint8_t scale_count,settings_data_t *data,uint32_t *generation);
bool settings_decode_legacy(const uint8_t *record,size_t size,uint8_t scale_count,settings_data_t *data);
bool settings_encode(uint8_t *record,size_t size,uint8_t scale_count,const settings_data_t *data,uint32_t generation);
int settings_select(const uint8_t *a,const uint8_t *b,uint8_t scale_count,settings_data_t *data,uint32_t *generation);
#ifdef __cplusplus
}
#endif
