#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>

#include "sensors.h"

#define PORT 8080
#define MAX_SENSORS 10
#define BUFFER_SIZE 1024

typedef struct SensorInfoTag {
      int sensorFD;
      struct sockaddr_in address;
      SensorPayload payload;
} SensorInfo;

typedef struct SensorInfoListTag {
      SensorInfo* sensors[MAX_SENSORS];
      size_t active;
      pthread_mutex_t mutex;
} SensorInfoList;

SensorInfoList list;

void initList();
SensorInfo* createSensorInfo(int sensorFD, struct sockaddr_in address);
void addSensor(int sensorFD, struct sockaddr_in address);
void* handleSensor(void* arg);

int main() {
      initList();

      struct sockaddr_in address;
      int serverFD = socket(AF_INET, SOCK_STREAM, 0);
      if(serverFD == 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
      }

      memset(&address, 0, sizeof(address));
      address.sin_family = AF_INET;
      address.sin_port = htons(PORT);
      address.sin_addr.s_addr = INADDR_ANY;

      int bindCheck = bind(serverFD, (struct sockaddr*)&address, sizeof(address));
      if(bindCheck < 0) {
            perror("Binding failed");
            exit(EXIT_FAILURE);
      }

      // listening
      int listenCheck = listen(serverFD, MAX_SENSORS);
      if(listenCheck < 0) {
            perror("Listening failed");
            exit(EXIT_FAILURE);
      }

      printf("Chat server listening on port %d\n", PORT);

      bool listening = true;
      bool clientCreated;
      while(listening) {
            clientCreated = true;
            struct sockaddr_in newClientAddress;
            socklen_t addrLen = sizeof(newClientAddress);
            int newClientFD = accept(serverFD, (struct sockaddr*)&newClientAddress, &addrLen);      

            if(newClientFD < 0) {
                  perror("Client creation failed");
                  clientCreated = false;
            }

            if(clientCreated) {
                  addSensor(newClientFD, newClientAddress);
            }            
      }

      close(serverFD);
      exit(EXIT_SUCCESS);
}

void initList() {
      list.active = 0;

      for(size_t i = 0; i < MAX_SENSORS; i++)
            list.sensors[i] = NULL;

      if(pthread_mutex_init(&list.mutex, NULL) != 0) {
            perror("pthread mutex init failed");
            exit(EXIT_FAILURE);
      }
}

SensorInfo* createSensorInfo(int sensorFD, struct sockaddr_in address) {
      SensorInfo* newSensor = malloc(sizeof(SensorInfo));
      if(!newSensor) {
            perror("Allocation failed");
            close(sensorFD);
            exit(EXIT_FAILURE);
      }

      newSensor->sensorFD = sensorFD;
      newSensor->address = address;

      return newSensor;
}

void addSensor(int sensorFD, struct sockaddr_in address) {
      SensorInfo* sensor = createSensorInfo(sensorFD, address);

      pthread_t threadID;
      int threadCheck = pthread_create(&threadID, NULL, handleSensor, (void*)sensor);
      if(threadCheck != 0) {
            perror("Thread creation failed");
            close(sensor->sensorFD);
            free(sensor);
      } else {
            pthread_detach(threadID);
      }      
}

void* handleSensor(void* arg) {
      SensorInfo* sensor = (SensorInfo*)arg;

      char sensorIP[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &sensor->address.sin_addr, sensorIP, INET_ADDRSTRLEN);
      uint16_t sensorPort = ntohs(sensor->address.sin_port);

      printf("New connection from %s:%u\n", sensorIP, sensorPort);

      pthread_mutex_lock(&list.mutex);
      if(list.active < MAX_SENSORS) {
            list.sensors[list.active] = sensor;
            list.active++;
      } else {
            pthread_mutex_unlock(&list.mutex);
            close(sensor->sensorFD);
            free(sensor);
            printf("Too sensor conneted. Refused from %s:%u\n", sensorIP, sensorPort);
            return NULL;
      }
      pthread_mutex_unlock(&list.mutex);

      bool receiving = true;
      while(receiving) {
            ssize_t bytesReceived = recv(sensor->sensorFD, &sensor->payload, BUFFER_SIZE-1, 0);
            if(bytesReceived == -1) {
                  perror("Receive failed");
                  receiving = false;
                  break;
            } else if(bytesReceived == 0) {
                  printf("Client %s:%d disconnected\n", sensorIP, sensorPort);
                  receiving = false;
                  break;
            }

            char timeBuffer[128];
            struct tm* timeinfo = localtime(&sensor->payload.timestamp);
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeinfo);

            fprintf(stdout, 
                  SENSOR_PAYLOAD_FORMAT_SPECIFIER, 
                  sensor->payload.ID,
                  timeBuffer,
                  sensor->payload.temperature,
                  sensor->payload.humidity,
                  sensor->payload.quality
            );
      }

      pthread_mutex_lock(&list.mutex);
      list.sensors[list.active] = NULL;
      list.active--;
      pthread_mutex_unlock(&list.mutex);

      printf("Sensor %s:%d has disconnected\n", sensorIP, sensorPort);

      close(sensor->sensorFD);
      free(sensor);
      return NULL;
}