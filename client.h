#ifndef CLIENT_H
#define CLIENT_H


int ui_loop(int socketfd, struct addrinfo *p);
char* client_prepare(char *input);

#endif
