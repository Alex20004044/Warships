#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../utils.h"
#include "../warships_field.h"

void ProcessRecievedMessage(char* message, int len)
{
    message_data_t messageData = BytesToMessageData(message);
    if(messageData.type == MSG_TYPE_INFO)
    {
        printf("Server message: %s\n", messageData.data);
    }
    else if(messageData.type == MSG_TYPE_GAME_DATA) {
        warships_gamedata_t* warshipsGamedata = (warships_gamedata_t*)&messageData.data;
        PrintGameData(warshipsGamedata, PLAYER_ONE);
    }
    else
    {
        printf("Received unknown message type\n");
    }
}

int main(int argc, char *argv[]) {
    printf("Starting client...\n");
    if (argc != 3) {
        fprintf(stderr, "Usage: %s host port\n", argv[0]);
        exit(1);
    }

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    if (inet_aton(argv[1], &(servaddr.sin_addr)) == 0) {
        fprintf(stderr, "Invalid address\n");
        exit(EXIT_FAILURE);
    }
    servaddr.sin_port = htons(to_long(argv[2]));

    if (connect(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr))) {
        fprintf(stderr, "Could not connect to server\n");
        exit(0);
    }
    printf("Connecting. Waiting for response from server...\n");

    char buffer1[4048] = {0};
    int len;
    len = read(sockfd, &buffer1, sizeof(buffer1));
    ProcessRecievedMessage(buffer1, len);

    char firstWord[15] = {0};
    char secondWord[15] = {0};

    fd_set readfds;
    int activity;
    int maxDescriptor;
    for(;;)
    {

        FD_ZERO(&readfds);
        //add master socket to set
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);
        maxDescriptor = max(STDIN_FILENO, sockfd);
        activity = select(maxDescriptor + 1, &readfds, NULL, NULL, NULL);

        if(FD_ISSET(STDIN_FILENO, &readfds)) {
            //printf("Enter command for server: ");
            scanf("%s %s", firstWord, secondWord);

            message_data_t messageData = {0};
            //Parse message
            if (!strcmp("join", firstWord)) {
                printf("Join command was entered\n");
                messageData.type = MSG_TYPE_COMMAND_JOIN;
                strcpy(messageData.data, secondWord);

                if (write(sockfd, (char *) &messageData, sizeof(messageData)) == -1) {
                    fprintf(stderr, "could not write to sock\n");
                    exit(EXIT_FAILURE);
                }
            } else if (!strcmp("start", firstWord)) {
                printf("Start command was entered\n");
                messageData.type = MSG_TYPE_COMMAND_START;
                strcpy(messageData.data, secondWord);

                if (write(sockfd, (char *) &messageData, sizeof(messageData)) == -1) {
                    fprintf(stderr, "could not write to sock\n");
                    exit(EXIT_FAILURE);
                }
            } else {
                printf("Unknown command. Try again\n");
                continue;
            }
        }

        if(FD_ISSET(sockfd, &readfds)) {
            char buffer[4048] = {0};
            if ((len = read(sockfd, &buffer, sizeof(buffer))) == -1)
                perror("read");

            ProcessRecievedMessage(buffer, len);
        }
    }
    close(sockfd);
    return 0;
}