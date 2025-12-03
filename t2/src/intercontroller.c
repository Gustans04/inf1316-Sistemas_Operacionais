#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <semaphore.h>

#include "aux.h"

static InfoProcesso processos[NUM_APP];

void ctrlC_handler(int signum);
void finalizar(int signum);

sig_atomic_t rodando = 1;

int main(void) {
    int fifo;
    srand(time(NULL));
    
    // Abrir o semáforo existente para uso neste processo
    process_sem = sem_open("/t2_process_sem", 0);

    signal(SIGINT, ctrlC_handler);
    signal(SIGUSR1, finalizar);

    if (criaFIFO("FIFO_IRQ") < 0) {
        perror("Falha ao criar FIFO");
        exit(EXIT_FAILURE);
    }

    if (abreFIFO(&fifo, "FIFO_IRQ", O_WRONLY) < 0) {
        perror("Falha ao abrir FIFO");
        exit(EXIT_FAILURE);
    }

    while(rodando) {
        usleep(500000); // Sleep por 500 milissegundos
        write(fifo, "IRQ0", 5); // Simula uma IRQ0

        int valor_aleatorio = rand() % 1000; // Gera um valor aleatório entre 0 e 999

        if (valor_aleatorio < 200) { // 20% de chance de gerar uma IRQ1
            write(fifo, "IRQ1", 5); // Simula uma IRQ1
        }

        if (valor_aleatorio < 10) { // 1% de chance de gerar uma IRQ2
            write(fifo, "IRQ2", 5); // Simula uma IRQ2
        }
    }

    close(fifo);
    sem_close(process_sem);
    printf("InterController terminou sua execução\n");
    return 0;
}

void finalizar(int signum)
{
    (void)signum; // remove warning
    rodando = 0;
}

void ctrlC_handler(int signum) {
    (void)signum; // remove warning

    kill(getpid(), SIGSTOP); // Pausa o InterController até receber SIGCONT

    printf("=== InterController CONTINUADO ===\n");

    // Continua todos os processos
    for (int i = 0; i < NUM_APP; i++) {
        if (processos[i].estado == EXECUTANDO) {
            if (kill(processos[i].pid, SIGCONT) == -1) {
                perror("Falha ao enviar sinal SIGCONT");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Re-registra o handler de SIGINT
    signal(SIGINT, ctrlC_handler);
}
