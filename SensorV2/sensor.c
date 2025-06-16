#include "protocol.h"
#include <stdlib.h>
#include <time.h>

SensorPayload createPayload(uint8_t temperature, uint8_t humidity, uint8_t airQuality) {
      SensorPayload payload;
      payload.timestamp = time(NULL);
      payload.temperature = temperature;
      payload.humidity = humidity;
      payload.airQuality = airQuality;
      
      return payload;
}

SensorPayload createRandomPayload() {
      return createPayload(
            rand() % MAX_TEMPERATURE,           
            rand() % MAX_HUMIDITY,             
            (rand() % MAX_AIR_QUALITY)                      
      );
}