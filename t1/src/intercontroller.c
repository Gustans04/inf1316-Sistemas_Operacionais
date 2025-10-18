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

#include "aux.h"

static InfoProcesso processos[NUM_APP];

void ctrlC_handler(int signum);
void print_status(void);
void read_process_info(void);

int main(void) {
    int fifo;
    srand(time(NULL));

    signal(SIGINT, ctrlC_handler);

    if (criaFIFO("FIFO_IRQ") < 0) {
        perror("Falha ao criar FIFO");
        exit(EXIT_FAILURE);
    }

    if (abreFIFO(&fifo, "FIFO_IRQ", O_WRONLY) < 0) {
        perror("Falha ao abrir FIFO");
        exit(EXIT_FAILURE);
    }

    while(1) {
        usleep(500000); // Sleep por 500 milissegundos
        write(fifo, "IRQ0", 5); // Simula uma IRQ0

        int valor_aleatorio = rand() % 1000; // Gera um valor aleatório entre 1 e 1000

        if (valor_aleatorio < 100) { // 10% de chance de gerar uma IRQ1
            write(fifo, "IRQ1", 5); // Simula uma IRQ1
        }

        if (valor_aleatorio < 5) { // 0,5% de chance de gerar uma IRQ2
            write(fifo, "IRQ2", 5); // Simula uma IRQ2
        }
    }

    close(fifo);
    return 0;
}

void ctrlC_handler(int signum) {
    printf("\n=== InterController PAUSADO ===\n");
    read_process_info();
    print_status();
    printf("=== InterController CONTINUADO ===\n");
    signal(SIGINT, ctrlC_handler);
}

void print_status(void) {
    printf("PID\tPC\tESTADO\t\tDISPOSITIVO\tOPERAÇÃO\tEXECUTANDO\tD1\tD2\n");
    for (int i = 0; i < 5; i++) {
        printf("%d\t%d\t", processos[i].pid, processos[i].pc);
        switch (processos[i].estado) {
            case PRONTO: printf("PRONTO\t\t"); break;
            case EXECUTANDO: printf("EXECUTANDO\t"); break;
            case BLOQUEADO: printf("BLOQUEADO\t"); break;
            case TERMINADO: printf("TERMINADO\t"); break;
        }
        if (processos[i].estado == BLOQUEADO) {
            printf("%d\t\t%s\t", processos[i].dispositivo, processos[i].operacao);
        } else {
            printf("-\t\t-\t");
        }
        printf("\t%d\t\t%d\t%d\n", 
               processos[i].executando,
               processos[i].qtd_acessos[0], 
               processos[i].qtd_acessos[1]);
    }
}

void read_process_info(void) {
    // aloca a memória compartilhada
    int segmento = shmget (IPC_CODE, sizeof (processos), IPC_CREAT | S_IRUSR | S_IWUSR);
    if (segmento == -1) {
        perror("shmget falhou");
        exit(EXIT_FAILURE);
    }

    // conecta a memória compartilhada
    InfoProcesso* shm_processos = (InfoProcesso*) shmat (segmento, 0, 0);
    if (shm_processos == (InfoProcesso*) -1) {
        perror("shmat falhou");
        exit(EXIT_FAILURE);
    }

    // copia os dados da memória compartilhada para o array local
    for (int i = 0; i < NUM_APP; i++) {
        processos[i] = shm_processos[i];
    }

    // desconecta a memória compartilhada
    shmdt(shm_processos);
}