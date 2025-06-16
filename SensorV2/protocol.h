#ifndef PROTOCOL_H

#define PROTOCOL_H
#define CONNECTION_PORT 4040
#define SEND_PORT 5050
#define ALERT_PORT 6060
#define MAX_SENSORS UINT8_MAX
#define SENSOR_REACTIVATE_TIME 3
//                                ID    ts  t    h    aq
#define PAYLOAD_FORMAT_SPECIFIER "%u at %s: %u C %u H %u %%\n"

// payload attributes 
#define MAX_ALERT_TEMPERATURE 50 // celsius
#define MAX_TEMPERATURE 60 // celsius

#define MAX_ALERT_HUMIDITY 60
#define MAX_HUMIDITY 70

#define MAX_AIR_QUALITY 100
#define MIN_ALERT_AIR_QUALITY 10

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#pragma pack(push, 1)
typedef struct SensorTag {
      uint8_t id;
      struct sockaddr_in addr;
} Sensor;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct SensorPayloadTag {
      time_t timestamp;
      uint8_t temperature;
      uint8_t humidity;
      uint8_t airQuality;
} SensorPayload;

typedef enum SensorAlertTypeTag {
      ALERT,
      REACTIVATE
} SensorAlertType;

typedef struct SensorAlertTag {
      SensorAlertType type;
      Sensor sensor;
} SensorAlert;
#pragma pack(pop)

SensorPayload createPayload(uint8_t temperature, uint8_t humidity, uint8_t airQuality);
SensorPayload createRandomPayload();

#endif