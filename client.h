#ifndef CLIENT_H
#define CLIENT_H


int ui_loop(int socketfd, struct addrinfo *p);
void client_prepare(char *input, char *payload, char *channel);
unsigned int payload_size(char *payload);
void client_login(int socketfd, struct addrinfo *p, char *nick);
int kbhit();

#endif
