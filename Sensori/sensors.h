#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>

// "{ID};{TIMESTAMP};{TEMP};{HUM}{QUAL}\0"

#define SENSOR_PAYLOAD_FORMAT_SPECIFIER "%u - %s:\tT %3.2f°; H %3u%%; %3u%%\n"

#pragma pack(push, 1)
typedef struct SensorPayloadTag {
      uint8_t ID;
      time_t timestamp;
      float temperature;
      uint8_t humidity;
      uint8_t quality;
} SensorPayload;
#pragma pack(pop)

SensorPayload createPayload(uint8_t ID, float temp, uint8_t humidity, uint8_t quality);
SensorPayload createRandomPayload(uint8_t ID);

#endif // SENSOR_H