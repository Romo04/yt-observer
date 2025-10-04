#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <semaphore.h>

#define PORT 9034
#define MAX_CLIENTS 50
#define BUF_SIZE 256
#define WORKER_COUNT 4

typedef struct
{
    int fd;
} Subscriber;

typedef struct
{
    Subscriber subs[MAX_CLIENTS];
    int sub_count;
    pthread_mutex_t sub_lock;

    int task_queue[MAX_CLIENTS];
    int queue_start, queue_end;
    pthread_mutex_t queue_lock;
    sem_t not_empty;
    sem_t not_full;
} ServerState;

int create_listener(uint16_t port)
{
    int sock;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return -1;
    }

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 0;
    }

    if (listen(sock, 10) < 0)
    {
        perror("listen");
        return 0;
    }

    return sock;
}

void enqueue_task(ServerState *state, int user_fd)
{
    sem_wait(&state->not_full);
    pthread_mutex_lock(&state->queue_lock);

    state->task_queue[state->queue_end] = user_fd;
    state->queue_end = (state->queue_end + 1) % MAX_CLIENTS;

    pthread_mutex_unlock(&state->queue_lock);
    sem_post(&state->not_empty);
}

int dequeue_task(ServerState *state)
{
    sem_wait(&state->not_empty);
    pthread_mutex_lock(&state->queue_lock);

    int fd = state->task_queue[state->queue_start];
    state->queue_start = (state->queue_start + 1) % MAX_CLIENTS;

    pthread_mutex_unlock(&state->queue_lock);
    sem_post(&state->not_full);

    return fd;
}


void notify(ServerState *state, const char *msg)
{
    pthread_mutex_lock(&state->sub_lock);

    for (int i = 0; i < state->sub_count; i++)
    {
        send(state->subs[i].fd, msg, strlen(msg), 0);
    }

    pthread_mutex_unlock(&state->sub_lock);
}

void handle_user(int user_fd, ServerState *state)
{
    char buf[BUF_SIZE];

    printf("New user connected: fd %d\n", user_fd);

    const char *welcome_msg = "Welcome to youtube, type subscribe to subscribe to the only channel there is :D\n";

    send(user_fd, welcome_msg, strlen(welcome_msg), 0);

    while (1)
    {
        int n = recv(user_fd, buf, BUF_SIZE-1, 0);

        if (n <= 0)
        {
            break;
        }

        buf[n] = '\0';

        if (strncmp(buf, "subscribe", 9) == 0)
        {
            printf("You got a new subscriber!\n User: fd %d\n", user_fd);

            pthread_mutex_lock(&state->sub_lock);

            if (state->sub_count < MAX_CLIENTS)
            {
                state->subs[state->sub_count++].fd = user_fd;
                send(user_fd, "Subscribed!\n", 12, 0);
            }

            pthread_mutex_unlock(&state->sub_lock);
        }
        else if (strncmp(buf, "unsubscribe", 11) == 0)
        {
            printf("You lost a subscriber :(\n User: fd %d\n", user_fd);

            pthread_mutex_lock(&state->sub_lock);

            for (int i = 0; i < state->sub_count; i++)
            {
                if (state->subs[i].fd == user_fd)
                {
                    state->subs[i] = state->subs[state->sub_count-1];
                    state->sub_count--;
                    break;
                }
            }

            pthread_mutex_unlock(&state->sub_lock);
            send(user_fd, "Unsubscribed!\n", 14, 0);
        }
        else if (strncmp(buf, "exit", 4) == 0)
        {
            printf("User: fd %d left the server\n", user_fd);
            break;
        }
    }

    pthread_mutex_lock(&state->sub_lock);
    for (int i = 0; i < state->sub_count; i++)
    {
        if (state->subs[i].fd == user_fd)
        {
            state->subs[i] = state->subs[state->sub_count-1];
            state->sub_count--;
            break;
        }
    }

    pthread_mutex_unlock(&state->sub_lock);
    close(user_fd);
}

void *worker_thread(void *arg)
{
    ServerState *state = (ServerState *)arg;

    while (1)
    {
        int fd = dequeue_task(state);
        handle_user(fd, state);
    }

    return NULL;
}

void *admin_thread(void *arg)
{
    ServerState *state = (ServerState *)arg;
    char buf[BUF_SIZE];

    printf("Welcome to youtube creator:\n"
        "Type 'upload <video title>' to upload your first video :D\n");

    while (fgets(buf, BUF_SIZE, stdin))
    {
        if (strncmp(buf, "upload ", 7) == 0)
        {
            char msg[BUF_SIZE];
            snprintf(msg, BUF_SIZE, "New video uploaded: %s", buf+7);
            notify(state, msg);
        }
    }

    return NULL;
}

int main()
{
    ServerState state = {0};

    pthread_mutex_init(&state.sub_lock, NULL);
    pthread_mutex_init(&state.queue_lock, NULL);

    sem_init(&state.not_empty, 0, 0);
    sem_init(&state.not_full, 0, MAX_CLIENTS);

    int listener = create_listener(PORT);
    if (listener < 0)
    {
        exit(1);
    }

    pthread_t workers[WORKER_COUNT];

    for (int i = 0; i < WORKER_COUNT; i++)
    {
        pthread_create(&workers[i], NULL, worker_thread, &state);
    }

    pthread_t admin;

    pthread_create(&admin, NULL, admin_thread, &state);

    while (1)
    {
        int user_fd = accept(listener, NULL, NULL);

        if (user_fd < 0)
        {
            continue;
        }

        enqueue_task(&state, user_fd);
    }

    close(listener);
}