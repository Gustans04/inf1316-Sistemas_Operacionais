#include "aux.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

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
    for (int i = 0; i < NUM_APP; i++)
    {
        if (lista_processos[i].pid == pid_desejado)
        {
            return &lista_processos[i];
        }
    }
    return NULL;
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
