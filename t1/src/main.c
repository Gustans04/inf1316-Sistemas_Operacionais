#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "aux.h"

#define NUM_APP 5

int main() 
{
    int pid_list[NUM_APP];
    int inter_pid;

    // Lista de processos esperando pelos dispositivos
    int esperandoD1[NUM_APP];
    int esperandoD2[NUM_APP];

    if ((inter_pid = fork()) < 0) {
        perror("Fork falhou!");
        exit(EXIT_FAILURE);
    } else if (inter_pid == 0) {
        // InterController 
        printf("InterController com PID %d\n", getpid());
        execl("./intercontroller", "", NULL);
        fprintf(stderr, "Erro ao rodar o InterController\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_APP; i++) {
        pid_list[i] = fork();
        if (pid_list[i] < 0) {
            perror("Fork falhou!");
            exit(EXIT_FAILURE);
        } else if (pid_list[i] == 0) {
            // Applications
            execl("./application", "", NULL);
            fprintf(stderr, "Erro ao rodar a Application\n");
            exit(EXIT_FAILURE);
        }
    }

    // Kernel
    printf("Kernel com PID %d\n", getpid());

    if (criaFIFO("FIFO_IRQ") < 0 || criaFIFO("FIFO_SYSCALL") < 0) {
        perror("Falha ao criar FIFOs");
        exit(EXIT_FAILURE);
    }

    int fifo_irq, fifo_syscall;

    // O Kernel simplesmente lê o que for escrito na FIFO pelo InterController
    if (abreFIFO(&fifo_irq, "FIFO_IRQ", O_RDONLY | O_NONBLOCK) < 0) {
        perror("Falha ao abrir FIFO de IRQs");
        exit(EXIT_FAILURE);
    }

    // O Kernel simplesmente lê o que for escrito na FIFO pelas Applications
    if (abreFIFO(&fifo_syscall, "FIFO_SYSCALL", O_RDONLY | O_NONBLOCK) < 0) {
        perror("Falha ao abrir FIFO de SYSCALLs");
        exit(EXIT_FAILURE);
    }

    while(1)
    {
        
    }

    for(int i = 0; i < NUM_APP + 1; i++) wait(NULL);
    
    return 0;
}
