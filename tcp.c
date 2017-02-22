#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
//
#define PACKET_SIZE 4096
#define TIMEOUT 10000
//
int init_sock(int port);
int init_connection(char * host, int port);
//
void loop();
void handle_client(int client_sock, struct sockaddr_in client);

int proxy_fd;


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
        int client_fd = accept(proxy_fd, (struct sockaddr*)&client, &len);
        if (client_fd > 0 && fork() == 0) {
            close(proxy_fd);
            handle_client(client_fd, client);
            exit(0);
        }
        close(client_fd);
    }
}

void handle_client(int c_fd, struct sockaddr_in client)
{
    struct sockaddr_in request;
    int flag = 0;
    char buffer[PACKET_SIZE], method[10000], url[10000], protocol[10000], host[10000], path[10000];
    int i_port;
    long l_port;

    bzero((char*)buffer, PACKET_SIZE);
    //
    recv(c_fd, buffer, PACKET_SIZE, 0);
    //

    if (sscanf(buffer, "%[^ ] %[^ ] %[^ ]", method, url, protocol) != 3)
        send(c_fd,"400 : Bad Request\nNot Support Protocol", 1000, 0);
    if (url[0] == '\0')
        send(c_fd, "400 : BAD REQUEST - Null URL.", 1000, 0);
    if (strncasecmp(url, "http://", 7) == 0)
    {
        if (sscanf(url, "http://%[^:/]:%d%s", host, &i_port, path) == 3)
            l_port = (long) i_port;
        else if (sscanf(url, "http://%[^:/]%s", host, path) == 2)
            l_port = 80;
        else if (sscanf(url, "http://%[^/]:%d", host, &i_port) == 2  || sscanf(url, "http://%[^/]", host) == 1)
        {
            l_port = i_port ? 80 : (long) i_port;
            * path = '\0';
        }else
        {
            send(c_fd, "400 : BAD REQUEST - Cannot parse URL.", 1000, 0);
        }

        int sockfd = init_connection(host, l_port);
        if (sockfd > 0)
        {
            bzero((char *) buffer, sizeof(buffer));
            ssize_t n = send(sockfd, buffer, sizeof(buffer), 0);
            if (n > 0)
            {
                do
                {
                    bzero((char *)buffer, PACKET_SIZE);
                    n = recv(sockfd, buffer, PACKET_SIZE, 0);
                    if (n > 0)
                        send(c_fd, buffer, n, 0);
                }while(n>0);
            }else{
                send(c_fd, "404 - Cannot connect", 1000, 0);
            }
        }else{
            send(c_fd, "400 : BAD REQUEST:", 1000, 0);
        }
        
    }else{
        
    }
    close(c_fd);
}

int init_connection(char * host, int port) {
    struct sockaddr_in server;
    struct hostent *dns;
    int sfd;
    if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        return -1;
    }
    if ((dns = gethostbyname(host)) == NULL)
        return -1;
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    memcpy(&server.sin_addr.s_addr, dns->h_addr, dns->h_length);
    server.sin_port = htons(port);
    if (connect(sfd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        return -1;
    }
    return sfd;
}

int main(int argc, char *argv[]) {
    if (argc <= 1)
        return 0;
    int local_port = atoi(argv[1]);
    if ((proxy_fd = init_sock(local_port)) < 0)
        return -1;
    loop();
    return 1;
}