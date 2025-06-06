#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct ClientInfoTag {
      int socketFD; 
      struct sockaddr_in address;
} ClientInfo;

typedef struct ClientListTag {
      ClientInfo* clients[MAX_CLIENTS];
      size_t active;
      pthread_mutex_t mutex;
} ClientList;

int socketServerFD;  // global socket descriptor for thread access
ClientList clients;

void initClientList();
ClientInfo* createClient(const int* clientSocketFD, const struct sockaddr_in* clientAddress);
void* handleClient(void* arg);
void broadcastMsg(const char* msg, int senderFD);

int main() {
      initClientList();
      struct sockaddr_in address;

      // creating socket
      int serverFD = socket(AF_INET, SOCK_STREAM, 0);
      if(serverFD == 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
      }

      

      // setting address and port
      memset(&address, 0, sizeof(address));
      address.sin_family = AF_INET;
      address.sin_addr.s_addr = INADDR_ANY;  
      address.sin_port = htons(PORT);   

      // binding
      int bindCheck = bind(serverFD, (struct sockaddr*)&address, sizeof(address));
      if(bindCheck < 0) {
            perror("Binding failed");
            exit(EXIT_FAILURE);
      }

      // listening
      int listenCheck = listen(serverFD, MAX_CLIENTS);
      if(listenCheck < 0) {
            perror("Listening failed");
            exit(EXIT_FAILURE);
      }

      printf("Chat server listening on port %d\n", PORT);

      bool listening = true;
      bool clientCreated;

      while(listening) {
            // accepting client connection
            clientCreated = true;
            struct sockaddr_in newClientAddress;
            socklen_t addrLen = sizeof(newClientAddress);
            int newClientFD = accept(serverFD, (struct sockaddr*)&newClientAddress, &addrLen);
            
            if(newClientFD < 0) {
                  perror("Accepting connection went wrong");
                  clientCreated = false;
            }

            if(clientCreated) {
                  // allocating client
                  ClientInfo* newClient = createClient(&newClientFD, &newClientAddress);
                  pthread_t threadID;
                  int threadCheck = pthread_create(&threadID, NULL, handleClient, (void*)newClient);
                  if(threadCheck != 0) {
                        perror("Thread creation failed");
                        close(newClient->socketFD);
                        free(newClient);
                        clientCreated = false;
                  } else {
                        pthread_detach(threadID);
                  }
            }
      }

      close(serverFD);
      pthread_mutex_destroy(&clients.mutex);
      exit(EXIT_SUCCESS);
}

void initClientList() {
    clients.active = 0;
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        clients.clients[i] = NULL;
    }
    if (pthread_mutex_init(&clients.mutex, NULL) != 0) {
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
}

ClientInfo* createClient(const int* clientSocketFD, const struct sockaddr_in* clientAddress) {
      if(clientSocketFD == NULL || clientAddress == NULL)
            return NULL;

      ClientInfo* newClient = malloc(sizeof(ClientInfo));
      if(!newClient) {
            perror("Client memory allocation failed");
            exit(EXIT_FAILURE);
      }
      

      newClient->socketFD = *clientSocketFD;
      newClient->address = *clientAddress;

      return newClient;
}

void* handleClient(void *arg) {
      ClientInfo *info = (ClientInfo*)arg;
      int clientSocket = info->socketFD;
      char buffer[BUFFER_SIZE];
      char clientIP[INET_ADDRSTRLEN];
      
      inet_ntop(AF_INET, &info->address.sin_addr, clientIP, INET_ADDRSTRLEN);
      int clientPort = ntohs(info->address.sin_port);

      printf("New connection from %s:%d\n", clientIP, clientPort);

      // Add client to list
      pthread_mutex_lock(&clients.mutex);
      if(clients.active < MAX_CLIENTS) {
            // insert in "active" index and then increment active
            clients.clients[clients.active] = info;
            clients.active++;
      } else {
            // list full: unlock mutex and close fd
            pthread_mutex_unlock(&clients.mutex);
            close(info->socketFD);
            free(info);
            return NULL;
      }
      pthread_mutex_unlock(&clients.mutex);

      bool connected = true;
      while(connected) {
            ssize_t bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE-1, 0);
            if(bytesReceived == -1) {
                  perror("Receive failed");
                  connected = false;
            } else if(bytesReceived == 0) {
                  printf("Client %s:%d disconnected\n", clientIP, clientPort);
                  connected = false;
            }

            if(connected) {
                  buffer[bytesReceived] = '\0';

                  // sending message to all clients
                  char formattedMsg[BUFFER_SIZE + INET_ADDRSTRLEN];
                  snprintf(formattedMsg, sizeof(formattedMsg), "[%s:%d] %s", clientIP, clientPort, buffer);
                  broadcastMsg(formattedMsg, clientSocket);

                  if(strcmp(buffer, "exit") == 0)
                        connected = false;

                  printf("%s\n", formattedMsg);
            }
      }

      // removing client
      bool removing = true;
      pthread_mutex_lock(&clients.mutex);
      for(size_t i = 0; i < clients.active && removing; i++) {
            if(clients.clients[i]->socketFD == clientSocket) {
                  free(clients.clients[i]);
                  // left shift
                  for(size_t j = i; j < clients.active - 1; j++) 
                        clients.clients[j] = clients.clients[j + 1];

                  clients.clients[clients.active - 1] = NULL;
                  clients.active--;

                  removing = false;
            }
      }
      pthread_mutex_unlock(&clients.mutex);

      close(clientSocket);
      return NULL;
}

void broadcastMsg(const char* msg, int senderFD) {
    pthread_mutex_lock(&clients.mutex);
    for (size_t i = 0; i < clients.active; i++) {
        ClientInfo* c = clients.clients[i];
        if (c != NULL && c->socketFD != senderFD) {
            send(c->socketFD, msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&clients.mutex);
}