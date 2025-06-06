#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_CLIENT 4
#define MSG_LEN 256

int create_server(uint16_t port){
      int socketServerFD;
      struct sockaddr_in addr;
      socklen_t addr_len = sizeof(addr);
      if ((socketServerFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("socket failed");
            return -1;
      }

      memset(&addr, 0, sizeof(addr));
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      addr.sin_addr.s_addr = htonl(INADDR_ANY);

      // Bind and listen
      if (bind(socketServerFD, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind failed");
            return -2;
      }

      if (listen(socketServerFD, MAX_CLIENT) < 0) {
            perror("listen failed");
            return -3;
      }

      return socketServerFD;
}

int main() {
      int socketServerFD, socketClientFD;
      struct sockaddr_in sa;
      struct sockaddr saClientConnected;
      socklen_t clientLen = sizeof(saClientConnected);

      /* 1) Create a TCP socket */
      socketServerFD = create_server(8080);

      bool comunicating = true;
      while(comunicating) {
            socketClientFD = accept(socketServerFD, (struct sockaddr*)&saClientConnected, &clientLen);

            pid_t pid = fork();
            printf("%d\n", pid);
            
            if(pid == 0) {
                  // Child process that will handle socket client file descriptor that has been accepted

                  // we need no more file descriptor of server, we'll handle only fd of client
                  close(socketServerFD);

                  char msg[MSG_LEN] = "Ciao da Manu\n\0";
                  // scanf("%s", &msg);

                  write(socketClientFD, msg, MSG_LEN);

                  close(socketClientFD);
                  _exit(0);
            } else {
                  // I'm still server, don't need to handle client socket file descriptor
                  close(socketClientFD);
            }
      }
}
