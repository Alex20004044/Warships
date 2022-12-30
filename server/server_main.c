#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/select.h>
#include "../warships_field.h"
#include "../random_dev.h"
#include "../utils.h"
#include "field_generator.c"

#define BACKLOG 5
#define CONFIGFILE "warships.conf"
#define MAXNAME 255
#define LOG_MAX_MSG_SIZE 255

#define MAX_CLIENTS 3
const char *successConnectMessage = "You connected to Warships server";
const char *errorConnectMessage = "Connection to Warships server is interrupted, because there no empty slots. Try again later";
const char *unknownMessageReceivedMessage = "Unknown message type was received";


#define PASSWORD_LENGTH 256

typedef struct game_lobby {
    char password[PASSWORD_LENGTH];
    int firstPlayerSD;
    int secondPlayerSD;
    warships_gamedata_t gameData;
} game_lobby_t;

typedef struct shared {
    int semid;
    game_lobby_t gameLobby
} shared_t;

struct shared *shared;

game_lobby_t *GetGameLobby() {
    return &shared->gameLobby;
}


char config_name[MAXNAME + 1], log_name[MAXNAME + 1], server_name[MAXNAME + 1];
int sockfd, logfd, port, shmid, semid;


void logger(const char *message);

void append_time_to_log_name() {
    char nowstr[40];
    struct tm *timenow;

    time_t now = time(NULL);
    timenow = gmtime(&now);

    strftime(nowstr, sizeof(nowstr), "_%Y-%m-%d_%H-%M-%S", timenow);
    strcat(log_name, nowstr);
}

void log_open() {
    if ((logfd = open(log_name, O_WRONLY | O_CREAT, 0666)) == -1) {
        syslog(LOG_INFO, "%s %s: %s", "Cannot open log file", log_name, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void log_lock() {
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(logfd, F_SETLKW, &lock) == -1) {
        syslog(LOG_INFO, "Failed to lock log: %s", strerror(errno));
    }
}

void log_unlock() {
    struct flock lock;
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(logfd, F_SETLKW, &lock) == -1) {
        syslog(LOG_INFO, "Failed to lock log: %s", strerror(errno));
    }
}

void logger(const char *message) {
    log_lock();
    write(logfd, message, strlen(message));
    write(logfd, "\n", 1);
    log_unlock();
}

void logger_err(const char *message) {
    log_lock();
    write(logfd, message, strlen(message));
    write(logfd, ": ", 2);
    write(logfd, strerror(errno), strlen(strerror(errno)));
    write(logfd, "\n", 1);
    log_unlock();
}


int socket_start(struct sockaddr_in *address) {
    // accept client connections
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        logger("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    logger("Created socket");

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr))) {
        logger("Socket bind failed");
        logger(strerror(errno));
        exit(EXIT_FAILURE);
    }
    logger("Bind socket");

    if (listen(sockfd, BACKLOG)) {
        logger("Listen failed");
        exit(EXIT_FAILURE);
    }
    logger("Listen...");
    *address = servaddr;
    return sockfd;
}

void sighup_lock() {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGHUP);
    sigprocmask(SIG_BLOCK, &sigset, NULL);
}

void sighup_unlock() {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGHUP);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
}

#pragma region Daemon server features

void term() {
    logger("Finishing, waiting for children to exit...");
    wait(NULL); // wait for all children to exit
    logger("Detaching shared memory...");
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        logger_err("Failed to remove shared memory");
    }

    if (shmdt(shared) == -1) {
        logger_err("Failed to detach shared memory");
    }
    logger("Removing semaphore...");
    if (semctl(semid, 1, IPC_RMID) == -1) {
        logger_err("Failed to remove semaphore");
    }
    logger("Closing socket...");
    close(sockfd);
    logger("Closing log...");
    close(logfd);
    exit(0);
}

void hup() {
    logger("Reconfiguring...");
    int f;
    struct stat st;
    if ((f = open(config_name, O_RDONLY)) == -1) {
        syslog(LOG_INFO, "%s: %s", config_name, strerror(errno));
        logger("Cannot open new config file.");
        term();
    }
    if (fstat(f, &st) == -1) { // если не удалось считать размер файла
        syslog(LOG_INFO, "%s: %s", config_name, strerror(errno));
        logger("Cannot retrieve config file stats.");
        term();
    }

    char *config = NULL;
    if ((config = malloc(st.st_size + 1)) == NULL) {
        perror("malloc");
        exit(1);
    }
    read(f, config, st.st_size);
    config[st.st_size] = '\0';
    close(f);

    char *conf_start = config;
    int new_port = to_long(strsep(&config, "\n"));
    log_name[0] = '\0';
    strcpy(log_name, strsep(&config, "\n"));
    // add current timestamp to log file name
    append_time_to_log_name();
    close(logfd);
    log_open();
    logger("Master restarted logging");

    if (new_port != port) {
        logger("Port changed");
        port = new_port;
        logger("Set new port");
        syslog(LOG_INFO, "Started listening on %d", port);
        if (close(sockfd) == -1) {
            logger_err("Failed to close old socket");
        };
        logger("Recreating socket for new port...");
        struct sockaddr_in t;
        socket_start(&t);
    }

    free(conf_start);
    close(f);
    logger("Reconfigured.");
}

#pragma endregion



#pragma region Workers

void InitGameWorker()
{
    GenerateGameData(&(GetGameLobby()->gameData));
    exit(0);
}


#pragma endregion
void SendGameData()
{
    game_lobby_t* gameLobby = GetGameLobby();
    message_data_t messageData = {0};
    messageData.type = MSG_TYPE_GAME_DATA;
    memcpy(messageData.data, &gameLobby->gameData, sizeof (gameLobby->gameData));
    if (send(gameLobby->firstPlayerSD, (char *) &messageData, sizeof(messageData), 0) == -1) {
        perror("send");
    }
    if (send(gameLobby->secondPlayerSD, (char *) &messageData, sizeof(messageData), 0) == -1) {
        perror("send");
    }
    logger("Send game data to players");
}
int SendInfoMessageToClient(int clientSocket, char *message) {
    message_data_t messageData = {0};
    messageData.type = MSG_TYPE_INFO;
    strcpy(messageData.data, message);
    if (send(clientSocket, (char *) &messageData, sizeof(messageData), 0) == -1) {
        perror("send");
        return -1;
    }
    char str[300];
    strcpy(str, "Send to client: ");
    strcat(str, message);
    logger(str);
    return 0;
}

//if Message from client to start or join game => process it
//if Message from client with command => send it for client_input_worker
//if unknown message => send error to client
void ProcessMessageFromClient(char *message, int len, int clientSocket) {
/*    //Check for null terminator
    int isTerminatorExists = false;
    for (int i = 1; i < len; ++i) {
        if(message[i] == '\0')
            isTerminatorExists=true;
    }
    if(!isTerminatorExists)
    {
        SendInfoMessageToClient(clientSocket, "");
    }*/
    game_lobby_t *gameLobby = GetGameLobby();
    message_data_t messageData = BytesToMessageData(message);
    if (messageData.type == MSG_TYPE_COMMAND_JOIN) {
        if (gameLobby->firstPlayerSD == 0) {
            gameLobby->firstPlayerSD = clientSocket;
            strcpy(gameLobby->password, messageData.data);
            if(!fork())
            {
                InitGameWorker();
            }
            SendInfoMessageToClient(clientSocket, "Game is created");
        } else {
            SendInfoMessageToClient(clientSocket,
                                    "Game is not created. There no empty rooms on server. Try again later");
        }
    } else if (messageData.type == MSG_TYPE_COMMAND_START) {
        //Create input check worker and send to it
        if (gameLobby->firstPlayerSD == 0) {
            SendInfoMessageToClient(clientSocket, "Can't connect there is no lobby on server");
            return;
        }
        if (gameLobby->secondPlayerSD != 0) {
            SendInfoMessageToClient(clientSocket, "Can't connect there is no empty seat in lobby");
            logger("Cant connect, because there is no empty seat in lobby");
            return;
        }
        if (gameLobby->firstPlayerSD == clientSocket) {
            SendInfoMessageToClient(clientSocket, "You already in this lobby as Host");
            return;
        }
        if (strcmp(gameLobby->password, messageData.data)) {
            SendInfoMessageToClient(clientSocket, "Can't connect. Incorrect password");
            return;
        }
        gameLobby->secondPlayerSD = clientSocket;
        SendInfoMessageToClient(clientSocket, "Successively connect. Starting game...");
        SendInfoMessageToClient(gameLobby->firstPlayerSD, "Successively connect. Starting game...");
        SendGameData();
    } else if (messageData.type == MSG_TYPE_WARSHIPS_TURN) {
        //create worker to check correctness of command
    } else {
        logger("Send to client message: unknown  command type. message is ignored");
        if (
                send(clientSocket, unknownMessageReceivedMessage, strlen(unknownMessageReceivedMessage),
                     0) == -1) {
            logger_err("send");
        }
    }
}

void master() {
    //sleep(30);
#pragma region Master Init
    setsid();
    openlog("warships_master", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "%s", "Started");

    log_open();
    logger("Master started logging");

    key_t ipc_key = ftok(server_name, IPC_PRIVATE);
    logger("Got IPC KEY for shared memory");

    if ((semid = semget(ipc_key, 1, IPC_CREAT | 0666)) == -1) {
        logger_err("Failed to get semaphore ID");
    }
    logger("Got semaphore ID");
    if ((shmid = shmget(ipc_key, sizeof(shared_t), IPC_CREAT | 0666)) == -1) {
        logger_err("Unable to get shared memory ID");
    }
    logger("Got shared memory ID");
    if ((shared = shmat(shmid, NULL, SHM_RND)) == (void *) -1) {
        logger_err("Failed to attach shared memory");
    }
    logger("Attached shared memory with id");
    char buffer1[10];
    snprintf(buffer1, 10, "%d", shmid);
    logger(buffer1);

    shared->semid = semid;
    shared->gameLobby.firstPlayerSD = 0;
    shared->gameLobby.secondPlayerSD = 0;

#pragma endregion
//create shared memory with game data
//create successConnectMessage interprocess queue

    struct sockaddr_in address;
    char buffer[1025];
    int master_socket, addrlen, newSocket, activity, read_len, sd, maxDescriptor;
    master_socket = socket_start(&address);
    //set of socket descriptors
    fd_set readfds;
    //If socket in array == 0 then it is free
    int clientSockets[MAX_CLIENTS];
    //initialise all clientSockets[] to 0 so not checked
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clientSockets[i] = 0;
    }

    addrlen = sizeof(address);
    for (;;) {
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(master_socket, &readfds);
        maxDescriptor = master_socket;
        //add successConnectMessage queue to set

        //add child sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clientSockets[i];
            if (sd > 0)
                FD_SET(sd, &readfds);
            //highest file descriptor number, need it for the select function
            if (sd > maxDescriptor)
                maxDescriptor = sd;
        }


        activity = select(maxDescriptor + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            printf("select error");
        }
#pragma region ListenConnectRequests
        //if connect request => connect player to server
        if (FD_ISSET(master_socket, &readfds)) {
            if ((newSocket = accept(master_socket, (struct sockaddr *) &address,
                                    (socklen_t *) &addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            printf("New connection request. Socket fd is %d , ip is : %s , port : %d\n", newSocket,
                   inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            int emptySlot = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                //if position is empty
                if (clientSockets[i] == 0) {
                    emptySlot = i;
                    break;
                }
            }
            if (emptySlot >= 0) {
                logger("Connection allowed");
                clientSockets[emptySlot] = newSocket;
                printf("Adding to list of sockets as %d\n", emptySlot);

                if (SendInfoMessageToClient(newSocket, successConnectMessage))
                    perror("send");
            } else {
                if (SendInfoMessageToClient(newSocket, errorConnectMessage))
                    perror("send");
            }
        }
#pragma endregion

#pragma region ListenMessagesFromClients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clientSockets[i];
            if (sd == 0) continue;
            if (FD_ISSET(sd, &readfds)) {
                //Check if it was for closing, and read incoming message
                if ((read_len = read(sd, buffer, 1024)) == 0) {
                    //DISCONNECT CASE
                    getpeername(sd, (struct sockaddr *) &address, (socklen_t *) &addrlen);
                    logger("Client disconnected");
                    printf("Client disconnected. Ip %s, port %d\n", inet_ntoa(address.sin_addr),
                           ntohs(address.sin_port));
                    //Close the socket and mark as 0 in list for reuse
                    close(sd);
                    clientSockets[i] = 0;

                    if(GetGameLobby()->firstPlayerSD == sd || GetGameLobby()->secondPlayerSD == sd)
                    {
                        //send end game message to clients;
                        SendInfoMessageToClient(GetGameLobby()->firstPlayerSD, "Player is exit from game. Game terminated");
                        SendInfoMessageToClient(GetGameLobby()->secondPlayerSD, "Player is exit from game. Game terminated");
                        //end game
                        GetGameLobby()->firstPlayerSD = 0;
                        GetGameLobby()->secondPlayerSD = 0;
                    }
                } else {
                    //printf("Process message from client");
                    buffer[read_len] = '\0';
                    ProcessMessageFromClient(buffer, read_len, sd);
                }
            }
        }
#pragma endregion
#pragma region ListenInterprocessQueue
        //ADD to readfs internal interprocess queue?
        //if Message to client => send it
        //if Message to worker => create worker and send message
#pragma endregion
    }
}

int main(int argc, char *argv[]) {
    char *p;
    struct stat st;
    struct sigaction act;
    int configfd;

#pragma region Configuration
    // configuration
    if (argc > 2) {
        fprintf(stderr, "Usage: %s ( configfile )\n", argv[0]);
        exit(1);
    }
    if (argc > 1) {
        if (strlen(argv[1]) > MAXNAME) {
            fprintf(stderr, "Name '%s' of configfile is too long\n", argv[1]);
            exit(1);
        }
        strcpy(config_name, argv[1]);
    } else if ((p = getenv("CONFIG")) != NULL) {
        if (strlen(p) > MAXNAME) {
            fprintf(stderr, "Name '%s' of configfile is too long\n", p);
            exit(1);
        }
        strcpy(config_name, p);
    } else
        strcpy(config_name, CONFIGFILE);

    strcpy(server_name, argv[0]);

    if ((configfd = open(config_name, O_RDONLY)) == -1) {
        perror(config_name);
        exit(1);
    }
    if (fstat(configfd, &st) == -1) {
        perror(config_name);
        exit(1);
    }

    printf("Opened config file\n");

    char *config = NULL;
    if ((config = malloc(st.st_size + 1)) == NULL) {
        perror("malloc");
        exit(1);
    }
    read(configfd, config, st.st_size);
    config[st.st_size] = '\0';
    close(configfd);

    printf("Read config, close file\n");

    char *conf_start = config;
    port = to_long(strsep(&config, "\n"));
    strcpy(log_name, strsep(&config, "\n"));

    // add current timestamp to log file name
    append_time_to_log_name();
    printf("Config name: %s\n", log_name);

    free(conf_start);
#pragma endregion
    printf("Becoming a daemon...\n");

    // becoming a daemon
    act.sa_handler = hup;
    sigfillset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &act, NULL);
    act.sa_handler = term;
    sigaction(SIGTERM, &act, NULL);

    for (int f = 0; f < 256; f++) close(f);

    // sudo needed
    // chdir("/");

    switch (fork()) {
        case -1:
            perror("fork");
            exit(1);
        case 0:
            master();
    }

    return 0;
}
