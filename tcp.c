#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
//
#define BUFF_SIZE 4096
#define PROTOCOL "HTTP/1.0"
//
int init_sock(int port);
int init_connection(char * host, int port);
//
void loop();
void handle_client(int client_sock, struct sockaddr_in client);
void send_headers(FILE * sockw, int status, char* title, char* extra_header, char* mime_type, int length);
void send_error(FILE* sockw, int status, char* title, char* extra_header, char* text);
void relay_data(char* method, char* path, char* protocol, FILE* remote_r, FILE * remote_w, FILE* client_r, FILE* client_w);
//
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
    char method[4], url[100], protocol[100], host[100], path[100];
    int i_port;
    long l_port;

    char buffer[BUFF_SIZE];

    FILE* sockr = fdopen(c_fd, "r");
    FILE* sockw = fdopen(c_fd, "w");

    int first_line;
    long content_length;

    if (fgets(buffer, sizeof(buffer), sockr) == (char*) 0)
    {
        send_error(sockw, 400, (char *) "Bad Request", (char *) '\0', (char *) "No Request Found");
    }else{
        if (sscanf(buffer, "%[^ ] %[^ ] %[^ ]", method, url, protocol) != 3)
        {
            send_error(sockw, 400, (char *) "Bad Request", (char *)  '\0', (char *) "Not Support Protocol");
        }
        if (url[0] == '\0'){
            send_error(sockw, 400, (char *) "Bad Request", (char *)  '\0', (char *) "Null URL");
        }
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
                send_error(sockw, 400, (char *) "Bad Request", (char *)  '\0', (char *) "Cannot parse URL.");
            }

            int remote_fd = init_connection(host, l_port);

            FILE* remote_r = fdopen(remote_fd, "r");
            FILE* remote_w = fdopen(remote_fd, "w");

            relay_data(method, url, protocol, remote_r, remote_w, sockr, sockw);
        }else{
        }
    }
    close(c_fd);
}

void relay_data(char* method, char* path, char* protocol, FILE* remote_r, FILE * remote_w, FILE* client_r, FILE* client_w)
{
    char buffer[BUFF_SIZE];

    // reading client request
    //
    fprintf(remote_w, "%s %s %s\r\n", method, path, protocol);
    while (fgets(buffer, sizeof(buffer), client_r) != (char*) 0)
    {
        if (strcmp(buffer, "\n") == 0 || strcmp(buffer, "\r\n") == 0)
            break;
        fputs(buffer, remote_w);
    }
    fflush(remote_w);
    // relay data back to client
    while (fgets(buffer, sizeof(buffer), remote_r) != (char*) 0)
    {
        if (strcmp(buffer, "\n") == 0 || strcmp(buffer, "\r\n") == 0)
            break;
        fputs(buffer, client_w);
    }
    // fputs("Connection: close\r\n", client_w);
    fflush(client_w);
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

void send_headers(FILE* sockw, int status, char* title, char* extra_header, char* mime_type, int length) {
    fprintf(sockw, "%s %d %s\r\n", PROTOCOL, status, title);
    fprintf(sockw, "Server: %s\r\n", "DEMO");
    if (extra_header != (char*) 0)
        fprintf(sockw, "%s\r\n", extra_header);
    if (mime_type != (char*) 0)
        fprintf(sockw, "Content-Type: %s\r\n", mime_type);
    if (length >= 0)
        fprintf(sockw, "Content-Length: %d\r\n", length);
    fprintf(sockw, "Connection: close\r\n");
    fprintf(sockw, "\r\n");
    fflush(sockw);
}

void send_error(FILE* sockw, int status, char* title, char* extra_header, char* text) {
    send_headers(sockw, status, title, extra_header, (char *) "text/html", -1);

    fprintf(sockw, "\
        <!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n\
        <html>\n\
          <head>\n\
            <meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\">\n\
            <title>%d %s</title>\n\
          </head>\n\
          <body bgcolor=\"#cc9999\" text=\"#000000\" link=\"#2020ff\" vlink=\"#4040cc\">\n\
            <h4>%d %s</h4>\n\n",
            status, title, status, title);
    fprintf(sockw, "%s\n\n", text);
    fprintf(sockw, "\
            <hr>\n\
            <address><a href=\"127.0.0.1\">Lightweight Proxy</a></address>\n\
          </body>\n\
        </html>\n");
    fflush(sockw);
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