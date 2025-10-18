#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "aux.h"
#include <time.h>

int main(void) {
    int fifo;
    srand(time(NULL));

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