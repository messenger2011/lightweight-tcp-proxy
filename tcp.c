#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
//
#define PACKET_SIZE 4096
//
int init_sock(int port);
int init_connection();
//
void loop();
void handle_client(int client_sock, struct sockaddr_in client);
void relay_data(int source_sock, int destination_sock);

int server_fd, client_fd, remote_fd, remote_port = 0;
char * bind_host;

int init_sock(int port) {
    int s, opt = 1;
    struct sockaddr_in server;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) 
        return -1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
        return -1;
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (struct sockaddr*)&server, sizeof(server)) != 0) 
        return -1;
    if (listen(s, 20) < 0) 
        return -1;
    return s;
}

void loop() {
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client, &len);
        if (fork() == 0) {
            close(server_fd);
            handle_client(client_fd, client);
            exit(0);
        }
        close(client_fd);
    }
}

void handle_client(int c_fd, struct sockaddr_in client)
{
    if ((remote_fd = init_connection()) < 0) {
        close(remote_fd);
        close(c_fd);
    }else{
        if (fork() == 0) {
            relay_data(c_fd, remote_fd);
            exit(0);
        }
        if (fork() == 0) {
            relay_data(remote_fd, c_fd);
            exit(0);
        }
    }
}

void relay_data(int src_fd, int des_fd) {
    ssize_t n;
    char buffer[PACKET_SIZE];
    while ((n = recv(src_fd, buffer, PACKET_SIZE, 0)) > 0)
        send(des_fd, buffer, n, 0);
    if (n < 0) {
        exit(-1);
    }
    shutdown(des_fd, SHUT_RDWR);
    close(des_fd);
    //
    shutdown(src_fd, SHUT_RDWR);
    close(src_fd);
}

int init_connection() {
    struct sockaddr_in server;
    struct hostent *dns;
    int sfd;
    if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;
    if ((dns = gethostbyname(bind_host)) == NULL)
        return -1;
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    memcpy(&server.sin_addr.s_addr, dns->h_addr, dns->h_length);
    server.sin_port = htons(remote_port);
    if (connect(sfd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        return -1;
    }
    return sfd;
}

int main(int argc, char *argv[]) {
    if (argc <= 1)
        return 0;
    int local_port = atoi(argv[1]);
    bind_host = argv[2];
    remote_port = atoi(argv[3]);
    if ((server_fd = init_sock(local_port)) < 0)
        return -1;
    loop();
    return 1;
}