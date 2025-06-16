#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "protocol.h"

typedef struct ActiveSensorsTag {
      const Sensor* sensors[MAX_SENSORS];
      size_t currentActive;
      pthread_mutex_t mutex;
} ActiveSensors;

typedef struct ReactivationSensorInfoTag {
      SensorAlert alert;
      int sensorSocketFD;
} ReactivationSensorInfo;

ActiveSensors activeSensorList;
int connectionSocketFD;
int sendSocketFD;
int errorSocketFD;

void initList();
int createTCPServer(uint16_t port);
int createUDPServer(uint16_t port);
bool addToList(const Sensor* newSensor);

/* 
This thread routine receive new connection, then it adds the sensor 
to the list of active sensors
*/
void* handleNewConnections(void* arg);
/* 
This thread routine receives data and print it. 
*/
void* handleSensor(void* arg);
/* 
This thread routine wait for any errors from sensor, then it starts
a certain amount of time (defined in protocol.h) and reboot the sensor
*/
void* handleErrors(void* arg);

void* rebootSensor(void* arg);

int main(int argc, char** argv) {
      initList();

      if((connectionSocketFD = createTCPServer(CONNECTION_PORT)) == -1)
            exit(EXIT_FAILURE);   
      if((sendSocketFD = createUDPServer(SEND_PORT)) == -1) 
            exit(EXIT_FAILURE);
      if((errorSocketFD = createTCPServer(ALERT_PORT)) == -1)
            exit(EXIT_FAILURE);

      pthread_t handleConnectionThread, alertsThread;
      pthread_create(&handleConnectionThread, NULL, handleNewConnections, NULL);
      pthread_create(&alertsThread, NULL, handleErrors, NULL);
      pthread_join(handleConnectionThread, NULL);
      pthread_join(alertsThread, NULL);

      close(connectionSocketFD);
      close(sendSocketFD);
      close(errorSocketFD);
      exit(EXIT_SUCCESS);
}

void initList() {
      activeSensorList.currentActive = 0;
      if(pthread_mutex_init(&activeSensorList.mutex, NULL)) {
            perror("Mutex failed");
            exit(EXIT_FAILURE);
      }

      for(size_t i = 0; i < MAX_SENSORS; i++)
            activeSensorList.sensors[i] = NULL;
}

int createTCPServer(uint16_t port) {
      int socketFD;
      struct sockaddr_in addr;
      
      if((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Socket creation failed");
            return -1;
      }

      memset(&addr, 0, sizeof(addr));
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      addr.sin_addr.s_addr = htonl(INADDR_ANY);

      if(bind(socketFD, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("Bind failed");
            return -1;
      }

      if(listen(socketFD, SOMAXCONN) < 0) {
            perror("Listen failed");
            close(socketFD);
            return -1;
      }


      return socketFD;
}

int createUDPServer(uint16_t port) {
      int socketFD;
      struct sockaddr_in addr;
      
      if((socketFD = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("Socket creation failed");
            return -1;
      }

      memset(&addr, 0, sizeof(addr));
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      addr.sin_addr.s_addr = htonl(INADDR_ANY);

      if(bind(socketFD, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("Bind failed");
            return -1;
      }

      return socketFD;
}

bool addToList(const Sensor* newSensor) {
      pthread_mutex_lock(&activeSensorList.mutex);
      if(activeSensorList.currentActive == MAX_SENSORS) {
            pthread_mutex_unlock(&activeSensorList.mutex);
            return false;
      }
      for(size_t i = 0; i < MAX_SENSORS; i++) {
            if(activeSensorList.sensors[i] == NULL) {
                  activeSensorList.sensors[i] = newSensor;
                  activeSensorList.currentActive++;
                  break;
            }
      }
      pthread_mutex_unlock(&activeSensorList.mutex);
      return true;
}

void* handleNewConnections(void* arg) {
      while(true) {
            struct sockaddr_in sensorAddr;
            socklen_t addrlen = sizeof(sensorAddr);

            int clientFD = accept(
                  connectionSocketFD,
                  (struct sockaddr*)&sensorAddr,
                  &addrlen
            );
            if(clientFD < 0) {
                  perror("Accept failed");
                  continue;
            }

            Sensor* newSensor = malloc(sizeof* newSensor);
            if(!newSensor) {
                  perror("Memory allocation failed");
                  close(clientFD);
                  continue;
            }
            ssize_t n = recv(clientFD, newSensor, sizeof* newSensor, 0);
            if(n <= 0) {
                  perror("Received failed");
                  close(clientFD);
                  continue;
            } 
            if(n != sizeof *newSensor) {
                  fprintf(stderr, "Expected %zu bytes, got %zd\n", sizeof *newSensor, n);
                  free(newSensor);
                  close(clientFD);
                  continue;
            }


            // store the client's address inside newSensor.addr:
            newSensor->addr = sensorAddr;
            if(!addToList(newSensor)) {
                  fprintf(stderr, "Adding sensor failed\n");
                  close(clientFD);
                  continue;
            }

            pthread_t tid;
            if(pthread_create(&tid, NULL, handleSensor, newSensor) == 0) {
                  pthread_detach(tid);
            } else {
                  perror("Thread creation failed");
                  close(clientFD);
            }
      }

      return NULL;
}

void* handleSensor(void* arg) {
      Sensor* sensor = (Sensor*)arg;
      socklen_t addrLen = sizeof(sensor->addr);
      while(true) {
            SensorPayload payload;
            ssize_t bytesReceived = recvfrom(
                  sendSocketFD, 
                  &payload, 
                  sizeof(SensorPayload), 
                  0, 
                  (struct sockaddr*)&sensor->addr, 
                  &addrLen
            );

            // no partial message allowed
            if(bytesReceived != sizeof(SensorPayload))
                  continue;

            char timeBuffer[128];
            struct tm* timeinfo = localtime(&payload.timestamp);
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeinfo);

            printf(
                  PAYLOAD_FORMAT_SPECIFIER, 
                  sensor->id,
                  timeBuffer,
                  payload.temperature,
                  payload.humidity,
                  payload.airQuality
            );
      }

      free(sensor);
      return NULL;
}

void* handleErrors(void* arg) {
      while(true) {
            struct sockaddr_in sensorAddr;
            socklen_t addrlen = sizeof(sensorAddr);

            int clientFD = accept(
                  errorSocketFD,
                  (struct sockaddr*)&sensorAddr,
                  &addrlen
            );
            if(clientFD < 0) {
                  perror("Accept failed");
                  continue;
            }

            SensorAlert alertMsg;
            ssize_t bytesReceived = recv(clientFD, &alertMsg, sizeof alertMsg, 0);
            if(bytesReceived <= 0) {
                  perror("Connection failed");
                  continue;
            }
            if(bytesReceived != sizeof alertMsg) {
                  fprintf(stderr,
                        "Received only %zd of %zu bytes\n",
                        bytesReceived, sizeof alertMsg);
                  close(clientFD);
                  continue;
            }
            
            if(alertMsg.type == ALERT) {
                  printf("Alert received from %u\n", alertMsg.sensor.id);
                  ReactivationSensorInfo* info = malloc(sizeof(ReactivationSensorInfo));
                  if(!info) {
                        perror("Memory allocation failed");
                        close(clientFD);
                        continue;
                  }

                  info->alert = alertMsg;
                  info->sensorSocketFD = clientFD;
                  pthread_t waitThread;
                  if(pthread_create(&waitThread, NULL, rebootSensor, info) == 0) {
                        pthread_detach(waitThread);
                  } else {
                        perror("Thread creation failed");
                        close(clientFD);
                        free(info);
                  }

            }
      }
}

void* rebootSensor(void* arg) {
      ReactivationSensorInfo* info = (ReactivationSensorInfo*)arg;
      printf("Sensor %u reactivation...\n", info->alert.sensor.id);
      sleep(SENSOR_REACTIVATE_TIME);
      printf("Sensor %u reactivated\n", info->alert.sensor.id);

      info->alert.type = REACTIVATE;
      ssize_t bytesSent = send(info->sensorSocketFD, &info->alert, sizeof info->alert, 0);
      if(bytesSent <= 0) {
            perror("Send failed");
      }
      if(bytesSent != sizeof info->alert) {
            fprintf(stderr,
                  "Sent only %zd of %zu bytes\n",
                  bytesSent, sizeof info->alert);
      }

      close(info->sensorSocketFD);
      free(info);
      return NULL;
}
