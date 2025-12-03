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

void init_sem(void)
{
    sem_unlink("/t2_process_sem");

    process_sem = sem_open("/t2_process_sem", O_CREAT, 0666, 1);
    if (process_sem == SEM_FAILED)
    {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }
}

void sem_lock(void)
{
    if (sem_wait(process_sem) == -1)
    {
        perror("sem_wait failed");
        exit(EXIT_FAILURE);
    }
}

void sem_unlock(void)
{
    if (sem_post(process_sem) == -1)
    {
        perror("sem_post failed");
        exit(EXIT_FAILURE);
    }
}

void cleanup_sem(void)
{
    sem_close(process_sem);
    sem_unlink("/t2_process_sem");
}

int criaFIFO(const char *nomeFIFO)
{
    // Verifica se a FIFO já existe
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

int abreFIFO(int *fifo, const char *nomeFIFO, int modo)
{
    if ((*fifo = open(nomeFIFO, modo)) < 0)
    {
        fprintf(stderr, "Erro ao abrir a FIFO %s\n", nomeFIFO);
        return -1;
    }

    return 0;
}

InfoProcesso *encontrarAplicacaoPorPID(InfoProcesso *lista_processos, pid_t pid_desejado)
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
    for (int i = 0, idx = f->inicio; i < f->qtd; i++, idx = (idx + 1) % NUM_APP)
    {
        if (f->lista[idx] == pid)
            return 1;
    }
    return 0;
}

void inserirNaFila(FilaApps *f, pid_t pid)
{
    if (f->qtd == NUM_APP)
        return;
    if (pid_na_fila(f, pid))
        return; // evita duplicata
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
    for (int i = 0; i < f->qtd; i++)
    {
        pid_t cur = f->lista[idx];
        idx = (idx + 1) % NUM_APP;
        if (cur != pid)
        {
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
    for (int i = 0, idx = f->inicio; i < f->qtd; i++, idx = (idx + 1) % NUM_APP)
        if (f->lista[idx] == pid_desejado)
            return pid_desejado;
    return -1;
}

int numeroDoProcesso(InfoProcesso *processos, pid_t pid)
{
    sem_lock();
    for (int i = 0; i < NUM_APP; i++)
    {
        if (processos[i].pid == pid)
        {
            sem_unlock();
            return i + 1;
        }
    }
    sem_unlock();
    return -1; // PID não encontrado
}

void removerPidDaFila(FilaApps *fila, pid_t pid)
{
    int idx = fila->inicio;
    for (int i = 0; i < fila->qtd; i++)
    {
        pid_t cur = fila->lista[idx];
        idx = (idx + 1) % NUM_APP;
        if (cur == pid)
        {
            // Shifta para esquerda
            for (int j = i; j < fila->qtd - 1; j++)
            {
                fila->lista[(fila->inicio + j) % NUM_APP] = fila->lista[(fila->inicio + j + 1) % NUM_APP];
            }
            break;
        }
    }
    fila->qtd--;
    fila->fim = (fila->fim - 1 + NUM_APP) % NUM_APP; // Garante que o fim seja positivo
}

void print_status(InfoProcesso *processos, char *listaFiles[NUM_APP][50])
{
    printf("=== Tabela de Status dos Processos ===\n");
    printf("%-7s %-4s %-12s %-14s %-80s %-12s\n",
           "PID", "PC", "ESTADO", "OPERAÇÃO", "PARAMETROS", "RESPONDIDO");
    printf("------------------------------------------------------------------------------------------------------------------------------\n"); // Linha divisória

    sem_lock();
    for (int i = 0; i < 5; i++)
    {
        char *estado_str = "-";
        char *operacao_str = "-";
        char param_str[1024] = "-";
        char *executando_str;
        if (processos[i].estado == 0 || processos[i].estado == 1 || processos[i].estado == 4)
            executando_str = "SIM";
        else
            executando_str = "NAO";

        switch (processos[i].estado)
        {
        case PRONTO:
            estado_str = "PRONTO";
            break;
        case EXECUTANDO:
            estado_str = "EXECUTANDO";
            break;
        case ESPERANDO:
            estado_str = "ESPERANDO";
            break;
        case BLOQUEADO:
            estado_str = "BLOQUEADO";
            break;
        case TERMINADO:
            estado_str = "TERMINADO";
            break;
        }

        switch (processos[i].syscall.tipo_syscall)
        {
        case 0:
            operacao_str = "WriteCall";
            char *escaped_payload = escape(processos[i].syscall.call.writecall.payload);
            snprintf(param_str, sizeof(param_str), "(%s, %d, %s, %d)",
                     processos[i].syscall.call.writecall.path,
                     processos[i].syscall.call.writecall.len,
                     escaped_payload,
                     processos[i].syscall.call.writecall.offset);
            free(escaped_payload);
            break;
        case 1:
            operacao_str = "ReadCall";
            snprintf(param_str, sizeof(param_str), "(%s, %d, &buffer, %d)",
                     processos[i].syscall.call.readcall.path,
                     processos[i].syscall.call.readcall.len,
                     processos[i].syscall.call.readcall.offset);
            break;
        case 2:
            operacao_str = "AddCall";
            snprintf(param_str, sizeof(param_str), "(%s, %d, %s, %d)",
                     processos[i].syscall.call.addcall.path,
                     processos[i].syscall.call.addcall.len1,
                     processos[i].syscall.call.addcall.dirname,
                     processos[i].syscall.call.addcall.len2);
            break;
        case 3:
            operacao_str = "RemCall";
            snprintf(param_str, sizeof(param_str), "(%s, %d, %s, %d)",
                     processos[i].syscall.call.remcall.path,
                     processos[i].syscall.call.remcall.len1,
                     processos[i].syscall.call.remcall.name,
                     processos[i].syscall.call.remcall.len2);
            break;
        case 4:
            operacao_str = "ListDirCall";
            snprintf(param_str, sizeof(param_str), "(%s, %d, alldirinfo, fstlstpositions, &nrnames)",
                     processos[i].syscall.call.listdircall.path,
                     processos[i].syscall.call.listdircall.len1);
            break;
        }

        printf("%-7d ", processos[i].pid);
        printf("%-4d ", processos[i].pc);
        printf("%-12s ", estado_str);
        printf("%-12s ", operacao_str);
        printf("%-80s ", param_str);
        printf("%-12s\n", executando_str);
    }
    sem_unlock();

    printf("------------------------------------------------------------------------------------------------------------------------------\n\n"); // Linha divisória
    printf("=== Posições em Arquivos ===\n");
    printf("%-7s %-50s %-35s %-15s\n",
           "PID", "Diretório Atual", "Arquivo Atual", "Posição Atual");
    printf("------------------------------------------------------------------------------------------------------------------------------\n"); // Linha divisória

    sem_lock();
    for (int i = 0; i < 5; i++)
    {
        char dir[256] = "-";
        char file[256] = "-";
        char pos[256] = "-";
        char **parts = NULL;
        int aux = 0; // variável auxiliar para cálculos de posição

        printf("%-7d ", processos[i].pid);

        switch (processos[i].syscall.tipo_syscall)
        {
        case 0: // WriteCall
            parts = split_string(processos[i].syscall.call.writecall.path, "/");
            if (parts && parts[0] && parts[1]) {
                snprintf(dir, sizeof(dir), "%s", parts[0]);
                strcpy(file, parts[1]);
                if (processos[i].estado == 0 || processos[i].estado == 1 || processos[i].estado == 4)
                    aux = processos[i].syscall.call.writecall.offset + 16;
                else
                    aux = processos[i].syscall.call.writecall.offset;
                sprintf(pos, "%d", aux);
            }
            else {
                snprintf(dir, sizeof(dir), "%s", processos[i].syscall.call.writecall.path);
            }
            break;
        case 1: // ReadCall
            parts = split_string(processos[i].syscall.call.readcall.path, "/");
            if (parts && parts[0] && parts[1]) {
                snprintf(dir, sizeof(dir), "%s", parts[0]);
                strcpy(file, parts[1]);
                if (processos[i].estado == 0 || processos[i].estado == 1 || processos[i].estado == 4)
                    aux = processos[i].syscall.call.readcall.offset + 16;
                else
                    aux = processos[i].syscall.call.readcall.offset;
                sprintf(pos, "%d", aux);
            }
            else {
                snprintf(dir, sizeof(dir), "%s", processos[i].syscall.call.readcall.path);
            }
            break;
        case 2: // AddCall
            snprintf(dir, sizeof(dir), "%s", processos[i].syscall.call.addcall.path);
            strcpy(file, "-");
            strcpy(pos, "-");
            break;
        case 3: // RemCall
            parts = split_string(processos[i].syscall.call.remcall.path, "/");
            if (parts && parts[0]) {
                if (strlen(parts[0]) == 0 && parts[1]) snprintf(dir, sizeof(dir), "/%s", parts[1]);
                else snprintf(dir, sizeof(dir), "%s", parts[0]);
            }
            else {
                snprintf(dir, sizeof(dir), "%s", processos[i].syscall.call.remcall.path);
            }
            strcpy(file, "-");
            strcpy(pos, "-");
            break;
        case 4: // ListDirCall
            snprintf(dir, sizeof(dir), "%s", processos[i].syscall.call.listdircall.path);
            strcpy(file, "-");
            strcpy(pos, "-");
            break;
        default:
            // For uninitialized or invalid syscalls, keep default "-" values
            break;
        }

        if (parts)
        {
            free(parts[0]);
            free(parts[1]);
            free(parts);
        }

        printf("%-50s ", dir);
        printf("%-35s ", file);
        printf("%-15s\n", pos);
    }
    printf("------------------------------------------------------------------------------------------------------------------------------\n\n"); // Linha divisória

    printf("=== Arquivos Abertos por Aplicação ===\n");
    printf("%-7s %-50s\n",
           "PID", "Arquivos Abertos");
    printf("------------------------------------------------------------------------------------------------------------------------------\n"); // Linha divisória
    for (int i = 0; i < NUM_APP; i++)
    {
        printf("%-7d ", processos[i].pid);
        if (listaFiles[i][0] == NULL || strlen(listaFiles[i][0]) == 0)
        {
            printf("-\n");
        }
        else
        {
            for (int j = 0; j < 50; j++)
            {
                if (listaFiles[i][j] == NULL || strlen(listaFiles[i][j]) == 0)
                    break;
                if (j == 0)
                    printf("%-16s", listaFiles[i][j]);
                else
                    printf("%-3s %-16s", "", listaFiles[i][j]);
            }
            printf("\n");
        }
    }
    printf("------------------------------------------------------------------------------------------------------------------------------\n\n"); // Linha divisória
    sem_unlock();
}

void error(char *msg)
{
    perror(msg);
    exit(0);
}

void imprimir_lista_diretorio(ListDirCall *dl)
{
    for (int i = 0; i < dl->nrnames; i++)
    {
        int start = dl->fstlstpositions[i].start;
        int end = dl->fstlstpositions[i].end;
        int len = end - start + 1;

        char nome[256];
        if (len >= 256)
            len = 255;
        strncpy(nome, dl->alldirinfo + start, len);
        nome[len] = '\0';

        if (dl->fstlstpositions[i].type == 2)
        {
            // diretório: /nome
            printf("/%s\n", nome);
        }
        else
        {
            // arquivo: .nome
            printf(".%s\n", nome);
        }
    }
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
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
}

void enviaUdpRequest(CallRequest request)
{
    int n;
    int serverlen;
    int buflen = sizeof(CallRequest);
    char buf[buflen];

    /* serialize the request */
    bzero(buf, buflen);
    memcpy(buf, &request, sizeof(CallRequest));

    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, buf, sizeof(CallRequest), 0, (struct sockaddr *)&serveraddr, serverlen);

    if (n < 0)
        error("ERROR in sendto");
}

CallRequest recebeUdpResponse(void)
{
    int n;
    socklen_t serverlen;
    int buflen = sizeof(CallRequest);
    char buf[buflen];
    CallRequest response;

    // Initialize response structure to safe values
    memset(&response, 0, sizeof(CallRequest));
    response.tipo_syscall = -1; // Indicates no valid response
    response.owner = -1;

    /* receive the server's reply */
    serverlen = sizeof(serveraddr);
    bzero(buf, buflen);
    n = recvfrom(sockfd, buf, sizeof(CallRequest), MSG_DONTWAIT, (struct sockaddr *)&serveraddr, &serverlen);
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return response; // No response available
        }
        else
        {
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
    if (f->qtd == NUM_APP)
        return;
    if (owner_na_fila(f, request.owner))
        return; // evita duplicata
    f->lista[(f->inicio + f->qtd) % NUM_APP] = request;
    f->qtd++;
}

int owner_na_fila(FilaRequests *f, int owner)
{
    for (int i = 0, idx = f->inicio; i < f->qtd; i++, idx = (idx + 1) % NUM_APP)
    {
        if (f->lista[idx].owner == owner)
            return 1;
    }
    return 0;
}

int estaVaziaRequests(FilaRequests *fila)
{
    return fila->qtd == 0;
}

int removerDaFilaRequests(FilaRequests *fila)
{
    CallRequest valor_removido = fila->lista[fila->inicio];
    if (fila->qtd > 0)
        fila->qtd--;
    fila->inicio = (fila->inicio + 1) % NUM_APP;
    return valor_removido.owner;
}

char **split_string(const char *str, const char *delim)
{
    // Allocate memory for the result array (2 strings)
    char **result = malloc(2 * sizeof(char *));
    if (!result)
        return NULL;

    // Find the last occurrence of the delimiter
    char *last_delim = strrchr(str, delim[0]); // Assumes single character delimiter

    if (last_delim == NULL)
    {
        // No delimiter found, return the whole string as first part, empty as second
        result[0] = strdup(str);
        result[1] = strdup("");
    }
    else
    {
        // Calculate lengths for before and after the delimiter
        int first_len = last_delim - str;

        // Allocate and copy the first part (before delimiter)
        result[0] = malloc(first_len + 1);
        if (result[0])
        {
            strncpy(result[0], str, first_len);
            result[0][first_len] = '\0';
        }
        else
        {
            result[0] = strdup("");
        }

        // Copy the second part (after delimiter)
        result[1] = strdup(last_delim + 1);
        if (!result[1])
        {
            result[1] = strdup("");
        }
    }

    return result;
}

char *escape(char *buffer)
{
    int i, j;
    int l = strlen(buffer) + 1;
    char esc_char[] = {'\a', '\b', '\f', '\n', '\r', '\t', '\v', '\\'};
    char essc_str[] = {'a', 'b', 'f', 'n', 'r', 't', 'v', '\\'};
    char *dest = (char *)calloc(l * 2, sizeof(char));
    char *ptr = dest;
    for (i = 0; i < l; i++)
    {
        for (j = 0; j < 8; j++)
        {
            if (buffer[i] == esc_char[j])
            {
                *ptr++ = '\\';
                *ptr++ = essc_str[j];
                break;
            }
        }
        if (j == 8)
            *ptr++ = buffer[i];
    }
    *ptr = '\0';
    return dest;
}

void respostaParaApp(InfoProcesso *app, CallRequest resposta)
{
    app->syscall.lido = 1;
    switch (resposta.tipo_syscall)
    {
    case 0: // WriteCall
        app->syscall.tipo_syscall = 0;
        app->syscall.call.writecall = resposta.call.writecall;
        break;
    case 1: // ReadCall
        app->syscall.tipo_syscall = 1;
        app->syscall.call.readcall = resposta.call.readcall;
        break;
    case 2: // AddCall
        app->syscall.tipo_syscall = 2;
        app->syscall.call.addcall = resposta.call.addcall;
        break;
    case 3: // RemCall
        app->syscall.tipo_syscall = 3;
        app->syscall.call.remcall = resposta.call.remcall;
        break;
    case 4: // ListDirCall
        app->syscall.tipo_syscall = 4;
        app->syscall.call.listdircall = resposta.call.listdircall;
        break;
    }
}