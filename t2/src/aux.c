#include "aux.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <errno.h>

sem_t *process_sem;
char hostname[10] = "localhost";
int portno = 3000; // Porta padrão do servidor UDP
struct sockaddr_in serveraddr;
int sockfd;

void init_sem(void) {
    // Remove any existing semaphore
    sem_unlink("/t2_process_sem");
    
    // Create a named semaphore with value 1 (unlocked)
    process_sem = sem_open("/t2_process_sem", O_CREAT, 0666, 1);
    if (process_sem == SEM_FAILED) {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }
}

void sem_lock(void) {
    if (sem_wait(process_sem) == -1) {
        perror("sem_wait failed");
        exit(EXIT_FAILURE);
    }
}

void sem_unlock(void) {
    if (sem_post(process_sem) == -1) {
        perror("sem_post failed");
        exit(EXIT_FAILURE);
    }
}

void cleanup_sem(void) {
    sem_close(process_sem);
    sem_unlink("/t2_process_sem");
}

int criaFIFO(const char* nomeFIFO)
{
    // verifica se a FIFO já existe
    if (access(nomeFIFO, F_OK) == -1)
    {
        if (mkfifo(nomeFIFO, S_IRUSR | S_IWUSR) != 0)
        {
            fprintf(stderr, "Erro ao criar FIFO %s\n", nomeFIFO);
            return -1;
        }
    }
    return 0;
}

int abreFIFO(int* fifo, const char* nomeFIFO, int modo)
{
    if ((*fifo = open(nomeFIFO, modo)) < 0)
    {
        fprintf(stderr, "Erro ao abrir a FIFO %s\n", nomeFIFO);
        return -1;
    }

    return 0;
}

InfoProcesso* encontrarAplicacaoPorPID(InfoProcesso *lista_processos, pid_t pid_desejado)
{
    sem_lock();
    for (int i = 0; i < NUM_APP; i++)
    {
        if (lista_processos[i].pid == pid_desejado)
        {
            sem_unlock();
            return &lista_processos[i];
        }
    }
    sem_unlock();
    return NULL;
}

int processosAcabaram(InfoProcesso *lista_processos)
{
    int qtd_terminados = 0;
    sem_lock();
    for (int i = 0; i < NUM_APP; i++)
    {
        if (lista_processos[i].estado == TERMINADO)
        {
            qtd_terminados++;
        }
    }
    sem_unlock();
    return qtd_terminados == NUM_APP;
}

void inicializarFila(FilaApps *fila) 
{
    fila->inicio = 0;
    fila->fim = 0;
    fila->qtd = 0;
}

int pid_na_fila(FilaApps *f, pid_t pid) 
{
    for (int i = 0, idx = f->inicio; i < f->qtd; i++, idx = (idx + 1) % NUM_APP) {
        if (f->lista[idx] == pid) return 1;
    }
    return 0;
}

void inserirNaFila(FilaApps *f, pid_t pid) 
{
    if (f->qtd == NUM_APP) return; 
    if (pid_na_fila(f, pid)) return; // evita duplicata
    f->lista[(f->inicio + f->qtd) % NUM_APP] = pid;
    f->qtd++;
}

pid_t removerDaFila(FilaApps *fila) 
{
    int valor_removido = fila->lista[fila->inicio];
    if (fila->qtd > 0)
        fila->qtd--;
    fila->inicio = (fila->inicio + 1) % NUM_APP;
    return valor_removido;
}

void removerTodasOcorrencias(FilaApps *f, pid_t pid) 
{
    int nova_qtd = 0;
    int idx = f->inicio;
    for (int i = 0; i < f->qtd; i++) {
        pid_t cur = f->lista[idx];
        idx = (idx + 1) % NUM_APP;
        if (cur != pid) {
            f->lista[(f->inicio + nova_qtd) % NUM_APP] = cur;
            nova_qtd++;
        }
    }
    f->qtd = nova_qtd;
}

int estaVazia(FilaApps *fila) 
{
    return fila->qtd == 0;
}

pid_t procuraNaFila(FilaApps *f, pid_t pid_desejado) 
{
    for (int i = 0, idx = f->inicio; i < f->qtd; i++, idx = (idx+1) % NUM_APP)
        if (f->lista[idx] == pid_desejado) return pid_desejado;
    return -1;
}

int numeroDoProcesso(InfoProcesso* processos, pid_t pid) 
{
    sem_lock();
    for (int i = 0; i < NUM_APP; i++) {
        if (processos[i].pid == pid) {
            sem_unlock();
            return i + 1;
        }
    }
    sem_unlock();
    return -1; // PID não encontrado
}


void print_status(InfoProcesso* processos) 
{
    printf("%-7s %-4s %-12s %-10s %-12s\n", 
           "PID", "PC", "ESTADO", "OPERAÇÃO", "EXECUTANDO");
    printf("---------------------------------------------\n"); // Linha divisória

    sem_lock();
    for (int i = 0; i < 5; i++) {
        char *estado_str = "-";
        char *operacao_str = "-";
        char *executando_str = processos[i].executando ? "SIM" : "NAO"; // Não sem acento para evitar problemas de padding

        switch (processos[i].estado) {
            case PRONTO:     estado_str = "PRONTO";     break;
            case EXECUTANDO: estado_str = "EXECUTANDO"; break;
            case ESPERANDO: estado_str = "ESPERANDO"; break;
            case BLOQUEADO:  estado_str = "BLOQUEADO";  break;
            case TERMINADO:  estado_str = "TERMINADO";  break;
        }

        switch (processos[i].syscall.tipo_syscall) {
            case 0: operacao_str = "WriteCall"; break;
            case 1: operacao_str = "ReadCall";  break;
            case 2: operacao_str = "AddCall";   break;
            case 3: operacao_str = "RemCall";   break;
            case 4: operacao_str = "ListDirCall"; break;
            default: operacao_str = "-"; break;
        }

        printf("%-7d ",   processos[i].pid);
        printf("%-4d ",   processos[i].pc);
        printf("%-12s ",  estado_str);
        printf("%-10s ",  operacao_str);
        printf("%-10s\n",  executando_str);
    }
    sem_unlock();
}

void error(char *msg) {
    perror(msg);
    exit(0);
}

void iniciaUdpClient(void)
{
    struct hostent *server;

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
}

void enviaUdpRequest(CallRequest request) 
{
    int sockfd, n;
    int serverlen;
    int buflen = sizeof(CallRequest);
    char buf[buflen];

    /* get a message from the user */
    bzero(buf, buflen);
    printf("Please enter msg: ");
    fgets(buf, buflen, stdin);

    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");

    // /* serialize the request */
    // memcpy(buf, &request, sizeof(CallRequest));

    // /* send the message to the server */
    // serverlen = sizeof(serveraddr);
    // n = sendto(sockfd, buf, sizeof(CallRequest), 0, &serveraddr, serverlen);
    // if (n < 0) 
    //   error("ERROR in sendto");

    // close(sockfd);
}

CallRequest recebeUdpResponse(void) 
{
    int n;
    int serverlen;
    int buflen = sizeof(CallRequest);
    char buf[buflen];
    CallRequest response;

    /* receive the server's reply */
    serverlen = sizeof(serveraddr);
    bzero(buf, buflen);
    n = recvfrom(sockfd, buf, sizeof(CallRequest), MSG_DONTWAIT, &serveraddr, &serverlen);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            response.tipo_syscall = -1;
            return response; // No response available
        } else {
            error("ERROR in recvfrom");
        }
    }

    memcpy(&response, buf, sizeof(CallRequest));

    return response;
}

void encerraUdpClient(void)
{
    close(sockfd);
}

void inicializarFilaRequests(FilaRequests *fila) 
{
    fila->inicio = 0;
    fila->fim = 0;
    fila->qtd = 0;
}

void inserirNaFilaRequests(FilaRequests *f, CallRequest request) 
{
    if (f->qtd == NUM_APP) return; 
    if (owner_na_fila(f, request.owner)) return; // evita duplicata
    f->lista[(f->inicio + f->qtd) % NUM_APP] = request;
    f->qtd++;
}

int owner_na_fila(FilaRequests *f, int owner) 
{
    for (int i = 0, idx = f->inicio; i < f->qtd; i++, idx = (idx + 1) % NUM_APP) {
        if (f->lista[idx].owner == owner) return 1;
    }
    return 0;
}