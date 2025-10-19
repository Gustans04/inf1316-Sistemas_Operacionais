#include "aux.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

sem_t *process_sem;

void init_sem(void) {
    // Remove any existing semaphore
    sem_unlink("/t1_process_sem");
    
    // Create a named semaphore with value 1 (unlocked)
    process_sem = sem_open("/t1_process_sem", O_CREAT, 0666, 1);
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
    sem_unlink("/t1_process_sem");
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

void inserirNaFila(FilaApps *fila, pid_t valor) 
{
    fila->lista[fila->fim] = valor;
    fila->qtd++;
    fila->fim = (fila->fim + 1) % NUM_APP;
}

pid_t removerDaFila(FilaApps *fila) 
{
    int valor_removido = fila->lista[fila->inicio];
    if (fila->qtd > 0)
        fila->qtd--;
    fila->inicio = (fila->inicio + 1) % NUM_APP;
    return valor_removido;
}

int estaVazia(FilaApps *fila) 
{
    return fila->qtd == 0;
}

pid_t procuraNaFila(FilaApps *fila, pid_t pid_desejado)
{
    for (int i = 0; i < NUM_APP; i++)
    {
        if (fila->lista[i] == pid_desejado)
        {
            return fila->lista[i];
        }
    }
    return -1;
}

void print_status(InfoProcesso* processos) 
{
    printf("%-7s %-4s %-12s %-12s %-10s %-12s %-4s %-4s\n", 
           "PID", "PC", "ESTADO", "DISPOSITIVO", "OPERAÇÃO", "EXECUTANDO", "D1", "D2");
    printf("---------------------------------------------------"
           "-----------------------------------------\n"); // Linha divisória

    sem_lock();
    for (int i = 0; i < 5; i++) {
        char *estado_str = "-";
        char *dispositivo_str = "-";
        char *operacao_str = "-";
        char *executando_str = processos[i].executando ? "SIM" : "NAO"; // Não sem acento para evitar problemas de padding

        switch (processos[i].estado) {
            case PRONTO:     estado_str = "PRONTO";     break;
            case EXECUTANDO: estado_str = "EXECUTANDO"; break;
            case ESPERANDO: estado_str = "ESPERANDO"; break;
            case BLOQUEADO:  estado_str = "BLOQUEADO";  break;
            case TERMINADO:  estado_str = "TERMINADO";  break;
        }

        if (processos[i].estado == BLOQUEADO) {
            switch (processos[i].dispositivo) {
                case D1: dispositivo_str = "D1"; break;
                case D2: dispositivo_str = "D2"; break;
            }
            switch (processos[i].operacao) {
                case R: operacao_str = "R"; break;
                case W: operacao_str = "W"; break;
                case X: operacao_str = "X"; break;
            }
        }

        printf("%-7d ",   processos[i].pid);
        printf("%-4d ",   processos[i].pc);
        printf("%-12s ",  estado_str);
        printf("%-12s ",  dispositivo_str);
        printf("%-10s ",  operacao_str);
        printf("%-10s ",  executando_str);
        printf("%-4d ",   processos[i].qtd_acessos[0]);
        printf("%-4d \n",  processos[i].qtd_acessos[1]);
    }
    sem_unlock();
}
