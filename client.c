#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT 9034
#define BUF_SIZE 256

typedef struct
{
    int sock;
} UserArgs;

void *recv_thread(void *arg)
{
    int sock = *(int *)arg;
    char buf[BUF_SIZE];

    while (1)
    {
        int n = recv(sock, buf, BUF_SIZE-1, 0);
        if (n <= 0)
        {
            break;
        }

        buf[n] = '\0';
        printf("%s\n", buf);
    }

    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <server-ip>\n", argv[0]);
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        return 1;
    }

    pthread_t recv_t;
    pthread_create(&recv_t, NULL, recv_thread, &sock);

    char input[BUF_SIZE];
    while(fgets(input, BUF_SIZE, stdin))
    {
        if (send(sock, input, strlen(input), 0) < 0)
        {
            break;
        }

        if (strncmp(input, "exit", 4) == 0)
        {
            break;
        }
    }

    close(sock);
    pthread_join(recv_t, NULL);
}