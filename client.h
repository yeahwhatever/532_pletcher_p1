#ifndef CLIENT_H
#define CLIENT_H


int ui_loop(int socketfd, struct addrinfo *p);
void client_prepare(char *input, char *payload);
int kbhit();

#endif
