#ifndef CLIENT_H
#define CLIENT_H


int ui_loop(int socketfd);
void client_prepare(char *input, char *payload, char *channel);
unsigned int payload_size(char *payload);
void client_login(int socketfd, char *nick);
void client_keepalive(int socketfd);
int check_size(char *dgram);
void display(char *dgram, char *input);

#endif
