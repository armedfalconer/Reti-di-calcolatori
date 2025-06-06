#include <time.h>
#include <stdlib.h>
#include "sensors.h"

SensorPayload createPayload(uint8_t ID, float temp, uint8_t humidity, uint8_t quality) {
      SensorPayload newPayload;
      newPayload.ID = ID;
      newPayload.temperature = temp;
      newPayload.humidity = humidity;
      newPayload.quality = quality;

      newPayload.timestamp = time(NULL);

      return newPayload;
}

SensorPayload createRandomPayload(uint8_t ID) {
      return createPayload(
            ID,
            (rand() % 10) + 30, // 30 - 39
            rand() % 100,
            rand() % 100
      );
}