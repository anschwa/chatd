/* TCP Chat server
 * Adam Schwartz CS 410 2016-12-06
 *
 *
 * A simple TCP chat server that establishes
 * multiple client connections using pthreads.
 *
 * Usage: ./server <port>
 *
 * Really only useful with the client.
 * Adapted from Linux HowTos.
 */

#include "c-s-socket.h"

#define SIZE 256

void *client_thread(void*);

char shared_buffer[SIZE], shared_users[SIZE][SIZE];
pthread_t thread_id;
pthread_mutex_t lock;

struct thread_args {
    int sock;
    int id;
};

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno, n, i = 0;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    if (pthread_mutex_init(&lock, NULL) < 0)
        error("ERROR initializing mutex");

    /* zero out shared_users */
    for (n = 0; n < SIZE; n++) {
        memset(shared_users[n], 0, SIZE);
    }

    printf("Running chatd server...\n");

    while (1) {
        newsockfd = accept(sockfd,
                           (struct sockaddr *) &cli_addr, &clilen);

        if (newsockfd < 0)
            error("ERROR on accept");

        /* create new thread for client */
        struct thread_args args;
        args.sock = newsockfd;
        args.id = i++;

        /* set default username to shared_users*/
        bzero(shared_users[args.id], SIZE);
        sprintf(shared_users[args.id], "client-%d", args.id);

        if (pthread_create(&thread_id, NULL, client_thread, (void*) &args) < 0) {
            error("ERROR creating thread");
        }

    } /* end of while */

    pthread_join(thread_id, NULL);
    pthread_mutex_destroy(&lock);
    close(sockfd);
    return 0; /* we never get here */
}

/********* CLIENT_THREAD() **************
           Establish socket connection for each client in a separate thread
*****************************************/
void *client_thread(void *args_ptr) {
    struct thread_args *args = (struct thread_args *) args_ptr;
    int sock, id, n;
    sock = args->sock;
    id = args->id;

    char buffer[SIZE], message[SIZE], command[SIZE], msg_token;

    /* set default name */
    pthread_mutex_lock(&lock);
    bzero(shared_users[id], SIZE);
    sprintf(shared_users[id], "client-%d", id);
    pthread_mutex_unlock(&lock);

    /* send welcome message to user */
    bzero(buffer, SIZE);
    sprintf(buffer, "Welcome to the Chatd Server: type /help for available commands");
    n = write(sock, buffer, SIZE);
    if (n < 0) return NULL;

    /* write new connections to shared buffer*/
    printf("%s has joined\n", shared_users[id]);

    pthread_mutex_lock(&lock);
    bzero(shared_buffer, SIZE);
    sprintf(shared_buffer, "%s has joined", shared_users[id]);
    pthread_mutex_unlock(&lock);

    while (1) {
        /* read new message into buffer */
        bzero(buffer, SIZE);
        n = recv(sock, buffer, SIZE-1, MSG_DONTWAIT); /* non-blocking read */
        if (n < 0) {
            /* write current message back to client */
            n = write(sock, shared_buffer, strlen(shared_buffer));
            if (n < 0) {
                /* if we can't write to the client that means they
                 * have disconnected from the server */
                printf("%s has disconnected\n", shared_users[id]);

                /* let other users know of disconnection */
                pthread_mutex_lock(&lock);
                bzero(shared_buffer, SIZE);
                sprintf(shared_buffer, "%s has disconnected", shared_users[id]);
                pthread_mutex_unlock(&lock);
                break;
            }
        } else {
            msg_token = buffer[0];
            if (msg_token == '/') {
                /* read buffer into command and message strings */
                bzero(command, SIZE);
                bzero(message, SIZE);
                sscanf(buffer, "%s %[^\n]", command, message);

                /* process chat commands */
                if (strcmp(command, "/help") == 0) {
                    bzero(buffer, SIZE);
                    /* send list of commands */
                    sprintf(buffer, "Commands: /user, /post, /who, /help, /quit\n");
                    n = write(sock, buffer, SIZE);
                    if (n < 0 ) break;
                } else if (strcmp(command, "/user") == 0) {
                    bzero(buffer, SIZE);
                    pthread_mutex_lock(&lock);
                    sscanf(message, "%s", buffer);
                    printf("%s has changed name to %s\n", shared_users[id], buffer);
                    sprintf(shared_buffer, "%s has changed name to %s", shared_users[id], buffer);
                    /* change to new name */
                    bzero(shared_users[id], SIZE);
                    strcpy(shared_users[id], buffer);
                    pthread_mutex_unlock(&lock);
                } else if (strcmp(command, "/who") == 0) {
                    /* warning: if usernames are long, or there are a
                     * lot of users, we may not fit them all within
                     * the buffer SIZE */
                    bzero(buffer, SIZE);
                    strncat(buffer, "connected users: ", SIZE);
                    for (n = 0; n < SIZE; n++) {
                        if (strlen(shared_users[n]) > 0) {
                            strncat(buffer, shared_users[n], SIZE);
                            strncat(buffer, ", ", SIZE);
                        }

                    }
                    printf("%s\n", buffer);

                    n = write(sock, buffer, strlen(buffer));
                    if (n < 0)
                        break;

                } else if (strcmp(command, "/quit") == 0) {
                    printf("%s has disconnected\n", shared_users[id]);

                    /* let other users know of disconnection */
                    pthread_mutex_lock(&lock);
                    bzero(shared_buffer, SIZE);
                    sprintf(shared_buffer, "%s has disconnected", shared_users[id]);
                    pthread_mutex_unlock(&lock);

                    /* send final confirmation to client */
                    sprintf(buffer, "You have been disconnected");
                    n = write(sock, buffer, strlen(buffer));
                    break;
                } else if (strcmp(command, "/post") == 0) {
                    /* write new message to shared_buffer */
                    printf("%s: %s", shared_users[id], buffer);

                    pthread_mutex_lock(&lock);
                    bzero(shared_buffer, SIZE);
                    sprintf(shared_buffer, "%s: %s", shared_users[id], message);
                    pthread_mutex_unlock(&lock);
                } else {
                    bzero(buffer, SIZE);
                    sprintf(buffer, "%s: command not found, try /help", command);
                    n = write(sock, buffer, strlen(buffer));
                    if (n < 0)
                        break;
                }
            }
        }

        /* don't bombard the client */
        sleep(2);
    }
    printf("%s has exited\n", shared_users[id]);
    /* zero out username If client does not exit cleanly (telnet q)
     * then username may not get removed */
    pthread_mutex_lock(&lock);
    memset(shared_users[id], 0, SIZE);
    pthread_mutex_unlock(&lock);

    close(sock);
    return NULL;
}
