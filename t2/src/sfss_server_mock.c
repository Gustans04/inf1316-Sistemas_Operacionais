#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "sfss_ops.h" // Inclui suas funções de arquivo
#include "aux.h"      // Para CallRequest

#define PORT 3000
#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
}

int main()
{
    int sockfd; /* socket */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */
    CallRequest request; /* received request */
    CallRequest response; /* response to send back */

    printf("Servidor SFSS iniciando na porta %d...\n", PORT);

    // Cria root se a lógica permitir, ou use mkdir direto
    mkdir(ROOT_DIR, 0777);

    /*
     * socket: create the parent socket
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               (const void *)&optval, sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)PORT);

    /*
     * bind: associate the parent socket with a port
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr,
             sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    /*
     * main loop: wait for a CallRequest, process it, and send response
     */
    clientlen = sizeof(clientaddr);
    while (1) {
        /*
         * recvfrom: receive a CallRequest from a client
         */
        bzero(&request, sizeof(CallRequest));
        n = recvfrom(sockfd, &request, sizeof(CallRequest), 0,
                     (struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0)
            error("ERROR in recvfrom");

        printf("Server received CallRequest: tipo_syscall=%d, owner=%d\n",
               request.tipo_syscall, request.owner);

        // Process the request and create response
        response = request; // Copy the request
        response.tipo_syscall = request.tipo_syscall; // Keep same type for response

        // Process based on syscall type

        /*
         * sendto: send the response back to the client
         */
        n = sendto(sockfd, &response, sizeof(CallRequest), 0,
                   (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0)
            error("ERROR in sendto");

        printf("Response sent to client\n\n");
    }

    return 0;
}