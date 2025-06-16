#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "protocol.h"

#define USAGE "<sensor ID> <server IPv4>"
#define TICK 2

void checkArgs(int argc, char** argv);
int registerToServer(Sensor* sensor, const char* addr);
bool alert(const SensorPayload* p);
void* alertWait(void* arg);

int main(int argc, char** argv) {
      srand(time(NULL));
      checkArgs(argc, argv);
      
      // registration
      int registrationCheck;
      Sensor s;
      s.id = (uint8_t) atoi(argv[1]);
      if((registrationCheck = registerToServer(&s, argv[2])) == -1)
            exit(EXIT_FAILURE);

      puts("Registration complete");

      // changing port listening to 5050
      s.addr.sin_port = htons(SEND_PORT);

      // getting send socket
      int socketFD = socket(AF_INET, SOCK_DGRAM, 0);
      if(socketFD < 0) {
            perror("Send connection failed");
            exit(EXIT_FAILURE);
      }

      puts("Starting to send");

      bool sending = true;
      while(sending) {
            SensorPayload payload = createRandomPayload();
            if(alert(&payload)) {
                  puts("ALERT");
                  s.addr.sin_port = htons(ALERT_PORT);
                  pthread_t alertThread;
                  pthread_create(&alertThread, NULL, alertWait, (void*)&s);
                  pthread_join(alertThread, NULL);
                  s.addr.sin_port = htons(SEND_PORT);
            }
            printf("Sending data: ");

            ssize_t bytesSent = sendto(
                  socketFD, 
                  &payload, 
                  sizeof(SensorPayload), 
                  0, 
                  (struct sockaddr*)&s.addr,
                  sizeof(s.addr)
            );

            char timeBuffer[128];
            struct tm* timeinfo = localtime(&payload.timestamp);
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeinfo);

            printf(
                  PAYLOAD_FORMAT_SPECIFIER, 
                  s.id,
                  timeBuffer,
                  payload.temperature,
                  payload.humidity,
                  payload.airQuality
            );

            if(bytesSent == -1) {
                  perror("Sending failed");
                  sending = false;
            } else if(bytesSent != sizeof(SensorPayload)) {
                  perror("Partial message sent");
                  sending = false;
            }

            sleep(TICK);
      }
}

void checkArgs(int argc, char** argv) {
      if(argc != 3) {
            fprintf(stderr, "USAGE: %s %s\n", argv[0], USAGE);
            exit(EXIT_FAILURE);
      }

      int sensorID = atoi(argv[1]);
      if(sensorID < 0 || sensorID > UINT8_MAX) {
            fprintf(stderr, "%s\n", "ID MUST BE BETWEEN 0 AND 255");
            exit(EXIT_FAILURE);
      }
}

int registerToServer(Sensor* sensor, const char* addr) {
      int socketFD = socket(AF_INET, SOCK_STREAM, 0);
      if(socketFD < 0) {
            perror("Socket Creation failed");
            close(socketFD);
            return -1;
      }

      memset(&sensor->addr, 0, sizeof sensor->addr);
      sensor->addr.sin_family = AF_INET;
      sensor->addr.sin_port = htons(CONNECTION_PORT);
      if(inet_pton(AF_INET, addr, &sensor->addr.sin_addr) != 1) {
            fprintf(stderr, "Invalid IP address: %s\n", addr);
            return -1;
      }

      if(connect(socketFD, (struct sockaddr*)&sensor->addr, sizeof sensor->addr) < 0) {
            perror("Connection failed");
            close(socketFD);
            return -1;
      }

      ssize_t bytesSent = send(socketFD, sensor, sizeof* sensor, 0);
      if(bytesSent < 0) {
            perror("Send failed");
            close(socketFD);
            return -1;
      } 
      if(bytesSent != sizeof *sensor) {
            fprintf(stderr,
                  "Sent only %zd of %zu bytes\n",
                  bytesSent, sizeof *sensor);
            close(socketFD);
            return -1;
      }

      close(socketFD);
      return 0;
}

bool alert(const SensorPayload* p) {
      return p->temperature > MAX_ALERT_TEMPERATURE 
          || p->humidity > MAX_ALERT_HUMIDITY 
          || p->airQuality < MIN_ALERT_AIR_QUALITY;
}

void* alertWait(void* arg) {
      const Sensor* sensor = (const Sensor*)arg;
      int alertSocketFD = socket(AF_INET, SOCK_STREAM, 0);
      if(alertSocketFD < 0) {
            perror("Alert socket creation failed");
            close(alertSocketFD);
            exit(EXIT_FAILURE);
      }

      if(connect(alertSocketFD, (struct sockaddr*)&sensor->addr, sizeof sensor->addr) < 0) {
            perror("Alert connection failed");
            close(alertSocketFD);
            exit(EXIT_FAILURE);
      }

      SensorAlert alertMsg;
      alertMsg.sensor = *sensor;
      alertMsg.type = ALERT;

      ssize_t bytesSent = send(alertSocketFD, &alertMsg, sizeof alertMsg, 0);
      if(bytesSent < 0) {
            perror("Send failed");
            close(alertSocketFD);
            exit(EXIT_FAILURE);

      } 
      if(bytesSent != sizeof alertMsg) {
            fprintf(stderr,
                  "Sent only %zd of %zu bytes\n",
                  bytesSent, sizeof *sensor);
            close(alertSocketFD);
            exit(EXIT_FAILURE);
      }

      ssize_t bytesReceived = recv(alertSocketFD, &alertMsg, sizeof alertMsg, 0);
      if(bytesReceived < 0) {
            perror("Reactivate receive failed");
            close(alertSocketFD);
            exit(EXIT_FAILURE);

      }
      if(bytesReceived != sizeof alertMsg) {
            perror("Partial message reactivation");
            close(alertSocketFD);
            exit(EXIT_FAILURE);
      }

      if(alertMsg.type == ALERT) {
            perror("Alert Error message received");
            exit(EXIT_FAILURE);
      }

      close(alertSocketFD);
      return NULL;
}