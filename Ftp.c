#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "dir.h"
#include "usage.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <dirent.h>
#include "Ftp.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0x2000/* don't raise SIGPIPE */
#define DIR_MAXSIZE 512
#endif

#define BACKLOG 10 // how many pending connections queue will hold. We are only to handle one client!
int pasvSocket = -1;
int acceptedPasvSock = -1;
int controlSocket = -1;
char initialDirectory[DIR_MAXSIZE];
char currDirectory[DIR_MAXSIZE];

/** Signal handler used to destroy zombie children (forked) processes
 *  once they finish executing.
 */
static void sigchld_handler(int s) {

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

/** Returns the IPv4 or IPv6 object for a socket address, depending on
 *  the family specified in that address.
 */
static void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);
    else
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Handle client code
void handleClient(int socket) {

    char buffer[256]; // 1024 Bytes as max size
    char* msg = malloc(512);
    int length = 0;
    int flag = 0;
    msg = "202\n";
    send(socket, msg, 4, MSG_NOSIGNAL);
    if(loginProtocol(socket) == 1){
     return;
    }
    while (1) {
        // Ur code goes here and its output willl use the write method below to send reply
        printf("\nReading");

        // read message from server
        msg = "";
        read(socket, buffer, 256);
        flag = interpretResponse(socket, buffer, &msg);

        if(flag == -1){
            send(socket, "500 Command not implemented\n", 28, MSG_NOSIGNAL);
        }
        if (flag == 0) {
                printf("\nFrom client: %s", buffer);
                printf("\nSending");
                send(socket, msg, length, MSG_NOSIGNAL);
        }
        if(flag == 1) {
            send(socket, "500 Syntax error, command unrecognized and the requested action did not take place. "
                         "This may include errors such as command line too long\n", 138, MSG_NOSIGNAL);
            free(msg);
            return;
        }
        if(flag == 4){
            send(socket,"212 Directory Successfully Changed\n", 32, MSG_NOSIGNAL);
        }
        if(flag == 5){
            send(socket,"550 No Directory for given name\n", 32, MSG_NOSIGNAL);
        }
        if(flag == 6){
            send(socket,"550 User not authorized to access higher than the current directory\n", 64, MSG_NOSIGNAL);
        }
        if(flag == 7){
            send(socket,"550 Server does not support arguments with ./\n", 46, MSG_NOSIGNAL);
        }

        buffer[0] = '\0';
        bzero(buffer, 256);
    }
}

int loginProtocol(int socket){
    char buffer[256];
    while(1) {
        send(socket, "\nEnter Username: ", 17, MSG_NOSIGNAL);
        read(socket, buffer, 256);
        if(strcasecmp(buffer, "cs317\n") == 0){
            send(socket, "\nLogin Successful\n", 25, MSG_NOSIGNAL);
            break;
        } else {
            buffer[0] = '\0';
            send(socket, "503 Bad sequence of commands\n", 29, MSG_NOSIGNAL);
            send(socket, "\nUsername Incorrect. Would you like to try again? y/n\n", 54, MSG_NOSIGNAL);
            read(socket, buffer, 256);
            if((strcasecmp(buffer, "n\n\n") == 0)) {
                send(socket, "\nAborting login protocol\n", 25, MSG_NOSIGNAL);
                return 1;
            }
            buffer[0] = '\0';
        }
    }
    buffer[0] = '\0';
    send(socket,"Enter Password: ",21,MSG_NOSIGNAL);
    read(socket, buffer, 256);
}

int travrseCWD(char *buffer){
    struct dirent *d;
    if(strstr("./", buffer) != NULL) {
        return 1;
    }
   if(chdir(buffer)== -1){
       return 3;
   }
   return 2;
}


int travrseCDUP(){
    if(strcasecmp(initialDirectory, currDirectory) >= 0){
        return 1;
    }
    printf("%s", "return");
    getcwd(currDirectory, DIR_MAXSIZE);
    chdir("../");
    return 4;
}

int interpretResponse(int socket, char buffer[], char **msg){
    char arg0[256] = "";
    char arg1[256] = "";
    int check = 0;
    int arg1Base = 0;
    *msg = malloc(512);

    //splitting arguments
    for (int i = 0; ((buffer[i] >= 48 && buffer[i] <= 122) ||
            buffer[i] == 32) || buffer[i] == 46; ++i) {
        if(buffer[i] != ' ') {
            if(check != 1) {
                arg0[i] = buffer[i];
            } else {
                arg1[arg1Base++] = buffer[i];
            }
        } else {
            check = 1;
        }
    }

    //handling supported messages
    if(strcasecmp(arg0, "user") == 0) {

        printf("\nUser %s", arg0);
    }

    if(strcasecmp(arg0, "quit")== 0){
        return 1;
    }
    if(strcasecmp(arg0, "cwd")== 0){
        int temp = travrseCWD(arg1);
        if(temp == 1){
            return 7;
        }
        if (temp == 0){
            return 0;
        }

        if(temp == 2){
            return 5;
        }
        return 4;
    }
    if(strcasecmp(arg0, "cdup")== 0){
        if(travrseCDUP() == 1){
            return 6;
        }
        return 4;
    }
    if(strcasecmp(arg0, "type")== 0){
        (*msg)[0] = '5';
        (*msg)[1] = '0';
        (*msg)[2] = '5';
        (*msg)[3] = '\n';
        (*msg)[4] = '\0';
        return 0;
    }
    if(strcasecmp(arg0, "mode")== 0){
        printf("\nmode %s", arg0);
    }
    if(strcasecmp(arg0, "stru")== 0){
        printf("\nstru %s", arg0);
    }
    if(strcasecmp(arg0, "retr")== 0){
        handleRETR(arg1);
        return 2;
    }
    if(strcasecmp(arg0, "pasv")== 0){
        handlePASV(socket);
        return 2; // Does not return 0 so server does not try to send again.
    }
    if(strcasecmp(arg0, "nlst")== 0){
        if (strlen(arg1) > 0) {
            send(controlSocket, "501 NLST cannot have arguments\n", strlen("501 NLST cannot have arguments\n"), MSG_NOSIGNAL);
            return 2;
        }
        nlist(NULL);
        return 2;
    }
    return -1;
}

// Handles PASV request
void handlePASV(int oldSocket) {

    if (createNewSocket("1024") == -1) {
        char failed[] = "425. Cannot open data connection.\n";
        send(controlSocket, failed, strlen(failed), MSG_NOSIGNAL);
        return;
    }

    char* ipAddr;
    char hostname[256];
    struct hostent* hostent;

    gethostname(hostname, sizeof(hostname));

    hostent = gethostbyname(hostname);

    struct in_addr* inAdd = ((struct in_addr*)
            hostent->h_addr_list[0]);

    // converts to string
    ipAddr = inet_ntoa(*inAdd);

    for(int i = 0; i < strlen(ipAddr); i++){
        if(ipAddr[i] == '.') {
            ipAddr[i] = ',';
        }
    }

    char msgToSend[] = "227 Entering passive mode (";
    char* portNumStr = ",4,0)"; // a5*256 + a6 = 1024. a6 = 0. a5= 4.
    char*  finalStr = malloc(strlen(msgToSend)+strlen(portNumStr)+strlen(ipAddr)+1);
    finalStr[0] = '\0';
    strcat(finalStr, msgToSend);
    strcat(finalStr, ipAddr);
    strcat(finalStr, portNumStr);
    strcat(finalStr, "\n");

    send(oldSocket, finalStr, strlen(finalStr), MSG_NOSIGNAL);
    free(finalStr);
}

// Function handles the RETR command
void handleRETR(char* filePath) {

    char msg[256];
    char err[] = "550. Requested action not taken. File unavailable, not found, not accessible\n";
    char err2[] = "426. Connection closed; transfer aborted. The command opens a data connection to perform an action, but that action is canceled, and the data connection is closed.\n";

    FILE* filePointer = fopen(filePath, "r");

    fseek(filePointer, 0L, SEEK_END);
    long size = ftell(filePointer);

    if (size <= 0) {
        send(controlSocket, err, strlen(err), MSG_NOSIGNAL);
        return; // Error while trying to find file. Exit
    }

    fseek(filePointer, 0L, SEEK_SET); // Resets pointer back to start of file

    char* buffer = malloc(size); // Making a buffer the size of the file

    long bytesRead = fread(buffer, sizeof(char), size, filePointer); // Read file onto buffer

    if (bytesRead < 0) {
        send(controlSocket, err, strlen(err), MSG_NOSIGNAL);
        return; // Read error. Exit
    }

    sprintf(msg, "150 File works. Going to transmit data. "
            "Opening BINARY mode data connection for %s (%d bytes).\n", filePath, size);

    send(controlSocket, msg, strlen(msg), MSG_NOSIGNAL);


    void *pToBuff = buffer; // Points to start of buffer
    while (bytesRead > 0) {
        long bytes_written = write(acceptedPasvSock, pToBuff, bytesRead);
        if (bytes_written <= 0) {
            send(controlSocket, err2, strlen(err2), MSG_NOSIGNAL);
            break;
        }
        bytesRead -= bytes_written;
        pToBuff += bytes_written;
    }
    fclose(filePointer);

    free(buffer);
    bzero(msg, sizeof(msg));
    strcat(msg, "226 Closing data connection. Transfer complete.\n");
    send(controlSocket, msg, strlen(msg), MSG_NOSIGNAL);

    close(acceptedPasvSock);
    close(pasvSocket);
    acceptedPasvSock = -1;
    pasvSocket = -1;
}

// 0 Returns if socket was successfully created
// We return -1 if we fail
int createNewSocket(char* portNum) {

    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    int rv;
    pthread_t pasvSocketThread;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;   // use IPv4
    hints.ai_socktype = SOCK_STREAM; // create a stream (TCP) socket server
    hints.ai_flags    = AI_PASSIVE;  // use any available connection

    // Gets information about available socket types and protocols
    if ((rv = getaddrinfo(NULL, portNum, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {

        // create socket object
        if ((pasvSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        // specify that, once the program finishes, the port can be reused by other processes
        if (setsockopt(pasvSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            return -1;
        }

        // bind to the specified port number
        if (bind(pasvSocket, p->ai_addr, p->ai_addrlen) == -1) {
            close(pasvSocket);
            perror("server: bind");
            return -1;
        }

        // if the code reaches this point, the socket was properly created and bound
        break;
    }

    // all done with this structure
    freeaddrinfo(servinfo);

    // if p is null, the loop above could create a socket for any given address
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return -1;
    }

    if (pthread_create(&pasvSocketThread, NULL, listenAndAccept, (void*) &pasvSocket)) {
        perror("BIG F");
        return -1;
    }

    printf("server: waiting for connections...\n");
    return 0;
}


void *listenAndAccept(void *socket) {

    int *sockfd = ((int *) socket);
    char failed[] = "425. Cannot open data connection.\n";

    // sets up a queue of incoming connections to be received by the server
    if (listen(*sockfd, 3) == -1) {
        perror("listen");
        send(controlSocket, failed, strlen(failed), MSG_NOSIGNAL);
        return NULL;
    }

    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int new_fd;
    char s[INET6_ADDRSTRLEN];


    // wait for new client to connect
    sin_size = sizeof(their_addr);
    // fcntl(sockfd, F_SETFL, O_NONBLOCK);
    new_fd = accept(*sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
        perror("accept");
        send(controlSocket, failed, strlen(failed), MSG_NOSIGNAL);
        return NULL;
    }

    acceptedPasvSock = new_fd;

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof(s));
    printf("server: got data connection from %s\n", s);
}

void nlist(char* path) {

    char directory[1024];
    char msg[256];
    char err2[] = "425 Data connection is not open. Call PASV first.\n";

    if(pasvSocket == -1) {
        send(controlSocket, err2, strlen(err2), MSG_NOSIGNAL);
        return;
    }
    while (acceptedPasvSock == -1); // must wait for server to accept

    bzero(msg, sizeof(msg));
    strcat(msg, "150. Initiating data transfer through passive socket\n");
    send(controlSocket, msg, strlen(msg), MSG_NOSIGNAL);

    getcwd(directory, 1024);
    int rv = listFiles(acceptedPasvSock, directory);

    if(rv == -1) {
        bzero(msg, sizeof(msg));
        strcat(msg, "550. Requested action not taken. File unavailable, not found, not accessible\n");
        send(controlSocket, msg, strlen(msg), MSG_NOSIGNAL);
        goto close;
    } else if (rv == -2){
        bzero(msg, sizeof(msg));
        strcat(msg, "552. Requested file action aborted. Exceeded storage allocation.\n");
        send(controlSocket, msg, strlen(msg), MSG_NOSIGNAL);
        goto close;
    }

    bzero(msg, sizeof(msg));
    strcat(msg, "226. Directory sent. Closed socket.\n");
    send(controlSocket, msg, strlen(msg), MSG_NOSIGNAL);

    close:
    close(acceptedPasvSock);
    close(pasvSocket);
    acceptedPasvSock = -1;
    pasvSocket = -1;

    return;
}


int main(int argc, char *argv[]) {

    // This is some sample code feel free to delete it
    // This is the main program for the thread version of nc

    // Check the command line arguments
    if (argc != 2) {
        usage(argv[0]);
        return -1;
    }

    int sockfd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    char tester[256];
    getcwd(tester, DIR_MAXSIZE);
    getcwd(currDirectory, DIR_MAXSIZE);
    chdir(initialDirectory);
    printf("%s", tester);

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;   // use IPv4
    hints.ai_socktype = SOCK_STREAM; // create a stream (TCP) socket server
    hints.ai_flags    = AI_PASSIVE;  // use any available connection

    // Gets information about available socket types and protocols
    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {

        // create socket object
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        // specify that, once the program finishes, the port can be reused by other processes
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        // bind to the specified port number
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        // if the code reaches this point, the socket was properly created and bound
        break;
    }

    // all done with this structure
    freeaddrinfo(servinfo);

    // if p is null, the loop above could create a socket for any given address
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    // sets up a queue of incoming connections to be received by the server
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    // set up a signal handler to kill zombie forked processes when they exit
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {
        // wait for new client to connect
        sin_size = sizeof(their_addr);
        controlSocket = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (controlSocket == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof(s));
        printf("server: got connection from %s\n", s);

        // I haven't figured out how to get it to close the new connection and wait to listen but there it is.
        handleClient(controlSocket);
        close(controlSocket);
        printf("Closed socket\n");
    }
    // This is how to call the function in dir.c to get a listing of a directory.
    // It requires a file descriptor, so in your code you would pass in the file descriptor 
    // returned for the ftp server's data connection

    /*
    printf("Printed %d directory entries\n", listFiles(1, "."));
    return 0;
    */
}
