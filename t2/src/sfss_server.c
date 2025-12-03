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

void cria_diretorios();

int main()
{
    int sockfd;                    /* socket */
    int clientlen;                 /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval;                    /* flag value for setsockopt */
    int n;                         /* message byte size */

    CallRequest request;  /* received request */
    CallRequest response; /* response to send back */

    printf("Servidor SFSS iniciando na porta %d...\n", PORT);

    // Cria root e os diretórios /A0 ... /A5
    cria_diretorios();

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
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)PORT);

    /*
     * bind: associate the parent socket with a port
     */
    if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    printf("Servidor rodando na porta %d. Aguardando requisições...\n", PORT);

    /*
     * main loop: wait for a CallRequest, process it, and send response
     */
    clientlen = sizeof(clientaddr);
    while (1)
    {
        // Recebe CallRequest do Kernel
        bzero(&request, sizeof(CallRequest));
        n = recvfrom(sockfd, &request, sizeof(CallRequest), 0, (struct sockaddr *)&clientaddr, &clientlen);
        if (n < 0)
            error("ERROR in recvfrom");

        printf("Server received CallRequest: tipo_syscall=%d, owner=%d\n", request.tipo_syscall, request.owner);

        // Copia cabeçalho para resposta
        response = request;
        response.tipo_syscall = request.tipo_syscall;

        // Variáveis auxiliares para retornos
        int res;

        // Process based on syscall type
        switch (request.tipo_syscall)
        {
            case 0: // WRITE
                res = sfss_write(request.owner,
                                request.call.writecall.path,
                                request.call.writecall.payload,
                                request.call.writecall.offset);

                // Preenche resposta
                response.call.writecall.offset = res; // Retorna offset ou erro negativo
                // O payload de volta deve ser vazio
                memset(response.call.writecall.payload, 0, 16);

                if (res >= 0)
                    printf("   -> Write Sucesso. Novo Offset: %d\n", res);
                else
                    printf("   -> Write Erro: %d\n", res);
                break;

            case 1: // READ
                // Prepara buffer na resposta para receber os dados
                memset(response.call.readcall.buffer, 0, 16);

                res = sfss_read(request.owner,
                                request.call.readcall.path,
                                request.call.readcall.offset,
                                response.call.readcall.buffer); // Escreve direto no buffer da resposta

                response.call.readcall.len = res; // Quantos bytes leu

                // Se deu erro (res < 0), o offset deve indicar o erro na resposta
                if (res < 0)
                    response.call.readcall.offset = res;
                else
                    response.call.readcall.offset = request.call.readcall.offset; // Mantém offset original se sucesso

                if (res >= 0)
                    printf("   -> Read Sucesso. Bytes: %d\n", res);
                else
                    printf("   -> Read Erro: %d\n", res);
                break;

            case 2: // ADD
                res = sfss_add(request.owner,
                            request.call.addcall.path,
                            request.call.addcall.dirname);

                // Se sucesso, concatena o dirname no path para retornar o novo path completo
                if (res == 0)
                {
                    char temp[MAX_NAME_LEN];
                    snprintf(temp, MAX_NAME_LEN, "%s/%s", request.call.addcall.path, request.call.addcall.dirname);
                    strncpy(response.call.addcall.path, temp, MAX_NAME_LEN);
                    response.call.addcall.len1 = strlen(temp); // Retorna tamanho positivo
                }
                else
                {
                    response.call.addcall.len1 = res; // Retorna código de erro
                }

                if (res == 0)
                    printf("   -> Add Dir Sucesso.\n");
                else
                    printf("   -> Add Dir Erro: %d\n", res);
                break;

            case 3: // REM
                res = sfss_rem(request.owner,
                            request.call.remcall.path,
                            request.call.remcall.name);

                response.call.remcall.len1 = res; // 0 sucesso, < 0 erro

                if (res == 0)
                    printf("   -> Rem Sucesso.\n");
                else
                    printf("   -> Rem Erro: %d\n", res);
                break;

            case 4: // LIST DIR
            {
                IndexInfo temp_idx[40];

                res = sfss_listDir(request.owner,
                                request.call.listdircall.path,
                                response.call.listdircall.alldirinfo,
                                temp_idx);

                response.call.listdircall.nrnames = res;
                
                memcpy(response.call.listdircall.fstlstpositions, temp_idx, sizeof(temp_idx));

                if (res >= 0)
                    printf("   -> List Dir Sucesso. Itens: %d\n", res);
                else
                    printf("   -> List Dir Erro: %d\n", res);
                break;
            }

            default:
                printf("   -> Syscall desconhecida: %d\n", request.tipo_syscall);
                break;
        }

        /*
         * sendto: send the response back to the client
         */
        n = sendto(sockfd, &response, sizeof(CallRequest), 0, (struct sockaddr *)&clientaddr, clientlen);
        if (n < 0)
            error("ERROR in sendto");

        printf("Response sent to client\n\n");
    }

    return 0;
}

// Função para garantir que as pastas existam, criando elas
void cria_diretorios()
{
    mkdir(ROOT_DIR, 0777); // Cria SFSS-root-dir

    char path[100];

    // Cria pasta pública A0 e homes A1..A5
    for (int i = 0; i <= 5; i++)
    {
        snprintf(path, sizeof(path), "%s/A%d", ROOT_DIR, i);
        if (mkdir(path, 0777) == 0)
        {
            printf("Criado diretório: %s\n", path);
        }
        else if (errno != EEXIST)
        {
            perror("Erro ao criar diretório home");
        }
    }
}
