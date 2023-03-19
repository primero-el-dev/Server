#define _XOPEN_SOURCE_EXTENDED 1

#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>

#include "config.c"
#include "file.c"

struct ServerConfig
{
    char base_uri[255];
    int port;
    char file_root[255];
    char error_page_path[255];
    char http_protocol_version[6];
};

char* get_server_file_or_error_page(struct ServerConfig*, char*);

char* get_error_response(struct ServerConfig* server_config, int error);

int main(int argc, char const *argv[])
{
    struct ServerConfig server_config;
    strcpy(server_config.base_uri, "http://127.0.0.1");
    server_config.port = PORT;
    strcpy(server_config.file_root, FILE_ROOT);
    strcpy(server_config.error_page_path, "error.html");
    strcpy(server_config.http_protocol_version, "1.1");

    int server_socket, client_socket;
    long valread;
    struct sockaddr_in server_address;
    int addrlen = sizeof(server_address);

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("In socket");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(server_config.port);

    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));

    if (bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        perror("In bind");
        exit(EXIT_FAILURE);
    }
    if (listen(server_socket, 10) < 0) {
        perror("In listen");
        exit(EXIT_FAILURE);
    }

    char command[255];

    while (true) {

        printf("\nWaiting for new connection...\n\n");
        if ((client_socket = accept(server_socket, (struct sockaddr*) &server_address, (socklen_t*) &addrlen)) < 0) {
            perror("In accept");
            exit(EXIT_FAILURE);
        }

        char buffer[30000] = {0};
        valread = read(client_socket, buffer, 30000);

        // HTTP method
        int i = 0;
        char method[7];
        bzero(method, 7);
        while (buffer[i] != ' ' && buffer[i] != '\n') {
            method[i] = buffer[i];
            i++;
        }

        // URI
        i++;
        int first = i;
        char uri[255];
        bzero(uri, 255);
        while (buffer[i] != ' ' && buffer[i] != '\n') {
            uri[i - first] = buffer[i];
            i++;
        }

        char* response = get_server_file_or_error_page(&server_config, uri);

        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        free(response);
    }

    return 0;
}

char* get_server_file_or_error_page(struct ServerConfig* server_config, char* uri)
{
    char* content = (char*) malloc(60000 * sizeof(char));
    bzero(content, sizeof(content));

    char request_info[255];

    if (strncmp(uri, server_config->base_uri, strlen(server_config->base_uri)) == 0) {
        strncpy(request_info, &uri[strlen(server_config->base_uri)], strlen(uri) - strlen(server_config->base_uri));
    }
    else {
        strcpy(request_info, uri);
    }

    // Return error if there were multiple slashes next to each other in request uri
    bool was_slash = false;
    for (int i = 0; i < strlen(request_info); i++) {
        if (request_info[i] == '/') {
            if (was_slash) {
                return get_error_response(server_config, 404);
            }
            was_slash = true;
        }
        else {
            was_slash = false;
        }
    }

    char full_path[512];
    sprintf(full_path, "%s%s", server_config->file_root, request_info);
    bool file_path_ends_with_slash = full_path[strlen(full_path)-1] == '/';
    char* headers = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";

    if (!access(full_path, F_OK) == 0) {
        return get_error_response(server_config, 404);
    }

    if (is_regular_file(full_path)) {
        sprintf(content, "%s%s", headers, get_file_content(full_path));
        return content;
    }

    struct dirent* entry;
    DIR* directory_pointer;
    directory_pointer = opendir(full_path);
    strcpy(content, headers);
    char* parent_dir = get_parent_dir(uri);

    if (directory_pointer) {
        if (strlen(uri) > 1) {
            char link[512];
            sprintf(link, "<a href=\"%s\"><- Go back</a><br/>\n", parent_dir);
            strcpy(content, strcat(content, link));
        }
        char line[1024];

        bool search = true;
        while (search && (entry = readdir(directory_pointer))) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                sprintf(line, "<a href=\"%s%s%s\">%s</a><br/>\n", uri, (uri[strlen(uri)-1] == '/') ? "" : "/", entry->d_name, entry->d_name);
                strcpy(content, strcat(content, line));
            }
            if (strcmp(entry->d_name, "index.html") == 0 || strcmp(entry->d_name, "index.htm") == 0) {
                char full_path_with_index[768];
                bzero(full_path_with_index, sizeof(full_path_with_index));
                sprintf(full_path_with_index, "%s%s%s", full_path, file_path_ends_with_slash ? "" : "/", entry->d_name);
                bzero(content, sizeof(content));
                sprintf(content, "%s%s", headers, get_file_content(full_path_with_index));
                search = false;
            }
        }
    }
    else {
        strcpy(content, get_error_response(server_config, 500));
    }

    closedir(directory_pointer);
    free(parent_dir);

    return content;
}

char* get_error_response(struct ServerConfig* server_config, int error)
{
    char* response = (char*) malloc(50000 * sizeof(char));
    char http_message[70];
    bzero(http_message, sizeof(http_message));
    char error_message[30];
    bzero(error_message, sizeof(error_message));

    switch (error) {
    case 400:
        strcpy(error_message, "Bad Request");
        break;
    case 401:
        strcpy(error_message, "Unauthorized");
        break;
    case 402:
        strcpy(error_message, "Payment Required");
        break;
    case 403:
        strcpy(error_message, "Forbidden");
        break;
    case 404:
        strcpy(error_message, "Not Found");
        break;
    case 500:
        strcpy(error_message, "Internal Server Error");
        break;
    case 501:
        strcpy(error_message, "Not Implemented");
        break;
    case 502:
        strcpy(error_message, "Bad Gateway");
        break;
    case 503:
        strcpy(error_message, "Service Unavailable");
        break;
    case 504:
        strcpy(error_message, "Gateway Timeout");
        break;
    default:
        strcpy(error_message, "OK");
        break;
    }
    sprintf(http_message, "HTTP/%s %d %s", server_config->http_protocol_version, error, error_message);
    sprintf(response, "%s\r\nContent-Type: text/html\r\n\r\n%s", http_message, get_file_content(server_config->error_page_path));

    return response;
}