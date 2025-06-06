#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <termios.h>

#define MSG_BUFFER 1024
#define PREFIX_SEND ">> "
#define PREFIX_RECEIVED "<< "

int socketFD; // global socket descriptor for both threads

// For storing the line the user is currently typing
char lineBuffer[MSG_BUFFER];
size_t lineLen = 0;
pthread_mutex_t lineMutex = PTHREAD_MUTEX_INITIALIZER;

struct termios orig_termios;

// Disable non‐canonical mode (restore original terminal settings)
void disableRawMode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// Enable non‐canonical mode (no echo, read char‐by‐char)
void enableRawMode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void checkArgs(int argc, char** argv);
void* receiveHandler(void* arg);
void* sendHandler(void* arg);

int main(int argc, char** argv) {
    checkArgs(argc, argv);

    struct sockaddr_in serverAddress;
    socketFD = socket(AF_INET, SOCK_STREAM, 0); // create socket
    if(socketFD < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // setting port and address
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(atoi(argv[2]));

    // parsing IP address
    if(inet_pton(AF_INET, argv[1], &serverAddress.sin_addr) <= 0) {
        perror("Invalid address");
        close(socketFD);
        exit(EXIT_FAILURE);
    }

    printf("Trying to connect to %s:%s...\n", argv[1], argv[2]);

    if(connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Connection failed");
        close(socketFD);
        exit(EXIT_FAILURE);
    }

    puts("Connected to server. Write your messages (type 'exit' to close the program)");

    // Initialize the line buffer and mutex
    pthread_mutex_lock(&lineMutex);
    lineLen = 0;
    lineBuffer[0] = '\0';
    pthread_mutex_unlock(&lineMutex);

    // Switch terminal to non‐canonical mode so we can read char-by-char
    enableRawMode();

    pthread_t sendThread, receiveThread;
    pthread_create(&receiveThread, NULL, receiveHandler, NULL);
    pthread_create(&sendThread, NULL, sendHandler, NULL);

    pthread_join(sendThread, NULL);
    pthread_join(receiveThread, NULL);

    // Restore terminal settings before exiting
    disableRawMode();

    close(socketFD);
    return 0;
}

void checkArgs(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <IP> <Port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}

void* receiveHandler(void* arg) {
    char buffer[MSG_BUFFER];
    bool communicating = true;

    while (communicating) {
        ssize_t bytesReceived = recv(socketFD, buffer, MSG_BUFFER - 1, 0);
        if (bytesReceived < 0) {
            perror("Receive error");
            communicating = false;
            break;
        } else if (bytesReceived == 0) {
            // Server closed connection
            printf("\nServer closed connection\n");
            communicating = false;
            break;
        }

        buffer[bytesReceived] = '\0';

        // Lock the line buffer, save its content
        pthread_mutex_lock(&lineMutex);
        size_t savedLen = lineLen;
        char savedBuf[MSG_BUFFER];
        memcpy(savedBuf, lineBuffer, savedLen);
        savedBuf[savedLen] = '\0';
        pthread_mutex_unlock(&lineMutex);

        // Clear current line: move to start, overwrite with spaces, move to start again
        int totalLen = strlen(PREFIX_SEND) + savedLen;
        printf("\r");
        for (int i = 0; i < totalLen; i++) {
            putchar(' ');
        }
        printf("\r");

        // Print the incoming message
        printf("%s%s\n", PREFIX_RECEIVED, buffer);

        // Reprint prompt and restored input
        printf("%s", PREFIX_SEND);
        fwrite(savedBuf, 1, savedLen, stdout);
        fflush(stdout);
    }

    return NULL;
}

void* sendHandler(void* arg) {
    bool communicating = true;

    // Print initial prompt
    printf("%s", PREFIX_SEND);
    fflush(stdout);

    while(communicating) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            // read error or EOF
            break;
        }

        if(c == '\r' || c == '\n') {
            // User pressed Enter: send the lineBuffer as a message
            pthread_mutex_lock(&lineMutex);
            lineBuffer[lineLen] = '\0';
            char messageToSend[MSG_BUFFER];
            strcpy(messageToSend, lineBuffer);
            pthread_mutex_unlock(&lineMutex);

            if (strcmp(messageToSend, "exit\n") == 0) {
                send(socketFD, "connection closed", strlen("connection closed"), 0);
                communicating = false;
                break;
            }

            ssize_t bytesSent = send(socketFD, messageToSend, strlen(messageToSend), 0);
            if (bytesSent < 0) {
                perror("Send failed");
                communicating = false;
                break;
            }

            // Move to new line after sending, and reset the line buffer
            printf("\n");
            pthread_mutex_lock(&lineMutex);
            lineLen = 0;
            lineBuffer[0] = '\0';
            pthread_mutex_unlock(&lineMutex);

            // Print a fresh prompt
            printf("%s", PREFIX_SEND);
            fflush(stdout);
        }
        else if (c == 127 || c == '\b') {
            // Backspace: remove last character
            pthread_mutex_lock(&lineMutex);
            if (lineLen > 0) {
                lineLen--;
                lineBuffer[lineLen] = '\0';
                // Visually erase one character
                printf("\b \b");
                fflush(stdout);
            }
            pthread_mutex_unlock(&lineMutex);
        }
        else {
            // Regular character: append to buffer and echo
            pthread_mutex_lock(&lineMutex);
            if (lineLen < MSG_BUFFER - 1) {
                lineBuffer[lineLen++] = c;
                lineBuffer[lineLen] = '\0';
                write(STDOUT_FILENO, &c, 1);
            }
            pthread_mutex_unlock(&lineMutex);
        }
    }

    return NULL;
}