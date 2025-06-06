#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#include "sensors.h"

int socketFD;

void checkArgs(int argc, char** argv);

int main(int argc, char** argv) {
      srand(time(NULL));
      checkArgs(argc, argv);

      struct sockaddr_in serverAddr;
      socketFD = socket(AF_INET, SOCK_STREAM, 0);
      if(socketFD < 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
      }

      // setting ip version family and port
      memset(&serverAddr, 0, sizeof(serverAddr));
      serverAddr.sin_family = AF_INET;
      serverAddr.sin_port = htons(atoi(argv[3]));

      // parsing ip
      if(inet_pton(AF_INET, argv[2], &serverAddr.sin_addr) <= 0) {
            perror("Invalid address");
            close(socketFD);
            exit(EXIT_FAILURE);
      }

      printf("Trying to connect to %s:%s\n", argv[2], argv[3]);

      if(connect(socketFD, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            perror("Connection failed");
            close(socketFD);
            exit(EXIT_FAILURE);
      }

      puts("Connection reached");

      bool sending = true;
      while(sending) {
            SensorPayload payload = createRandomPayload(atoi(argv[1]));
            ssize_t bytesSent = send(socketFD, &payload, sizeof(payload), 0);
            if(bytesSent <= 0) {
                  perror("Sending failed");
            }

            sleep(3);
      }

      close(socketFD);
      exit(EXIT_SUCCESS);
}

void checkArgs(int argc, char** argv) {
      // 1: ID
      // 2: IP
      // 3: PORT

      if(argc < 4) {
            fprintf(stderr, "USAGE: %s <ID> <IP> <PORT>\n", argv[0]);
            exit(EXIT_FAILURE);
      }

      int IDCheck = atoi(argv[1]);
      if(IDCheck < 0 || IDCheck > UINT8_MAX) {
            perror("Invalid ID");
            exit(EXIT_FAILURE);
      }

      int portCheck = atoi(argv[3]);
      if(portCheck < 0 || portCheck > UINT16_MAX) {
            perror("Invalid port");
            exit(EXIT_FAILURE);
      }
}