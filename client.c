/* TCP Chat client
 * Adam Schwartz CS 410 2016-12-06
 *
 * A simple client that uses TCP and a simple
 * chat protocol to send messages to a server.
 *
 * Usage: ./client <hostname> <port>
 *
 * Really only useful with the server.
 * Adapted from Linux HowTos.
 */

#include "c-s-socket.h"

#define SIZE 256

void *get_messages(void*);

int main(int argc, char *argv[]) {
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    pthread_t thread_id;

    char buffer[SIZE];

    if (argc < 3) {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[2]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    server = gethostbyname(argv[1]);
    if (server == NULL)
        error("ERROR no such host");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    printf("Establishing connection to chatd server...\n");

    if (connect(sockfd,
                (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    printf("Connected. You may need to press `return` twice to exit the program.\n");

    /* create thread for receiving chat messages from the server */
    if (pthread_create(&thread_id, NULL, get_messages, (void*) &sockfd) < 0)
        error("ERROR creating thread");

    while (1) {
        /* set message prompt */
        printf("> ");

        /* send a message to server */
        bzero(buffer, SIZE);
        fgets(buffer, SIZE-1, stdin);
        n = write(sockfd, buffer, strlen(buffer));
        if (n < 0)
            break;
    }
    printf("Lost connection to server\n");
    pthread_exit(NULL);
    close(sockfd);
    return 0;
}

void *get_messages(void *sock_ptr) {
    int sock, len, n;
    sock = *(int *) sock_ptr;
    char buffer[SIZE], cache[SIZE];

    bzero(cache, SIZE);
    while(1) {
        printf(".\n");  /* needed to ensure writing from thread */
        /* read message into buffer,
         * we don't care about socket errors here */
        bzero(buffer, SIZE);
        n = recv(sock, buffer, SIZE-1, MSG_DONTWAIT); /* non-blocking read */

        len = strlen(buffer);

        /* only display new messages */
        if (len > 0 && strcmp(buffer, cache) != 0) {
            printf("%s", buffer);
            bzero(cache, SIZE);
            memcpy(cache, buffer, len);
        }

        /* give the user time to reply */
        sleep(2);
    }
    close(sock);
    return NULL;
}
