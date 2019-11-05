//
// Created by Dillon Higa on 2019-04-01.
// And someone else

#ifndef FTP_H
#define FTP_H

#endif
int interpretResponse(int socket, char buffer[], char ** msg);

int loginProtocol(int socket);

int createNewSocket(char* portNum);

void handlePASV(int oldSocket);

void passivePortHandler(int socket);

void *listenAndAccept(void* socket);

void handleRETR(char* filePath);

void nlist(char* path);

int travrseCWD(char *buffer);
int travrseCDUP();