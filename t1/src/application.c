#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#include "aux.h"

// Número máximo de iterações
#define MAX 5

int main()
{
    srand(getpid() * time(NULL)); // Para os números ficarem randômicos

    int PC = 0; // Contador de iterações (Program Counter)

    int aux; // Auxiliar para decidir sobre as syscalls, os dispositivos e as operações
    Dispositivos Dx; // Equivale ao dispositivo 
    Operacoes Op; // Equivale à operação 

    printf("\nApplication com PID %d inicializado\n", getpid());

    // aloca a memória compartilhada
    int segmento = shmget (IPC_CODE, sizeof (InfoProcesso) * NUM_APP, IPC_CREAT | S_IRUSR | S_IWUSR);
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
    InfoProcesso* appAtual = encontrarAplicacaoPorPID(shm_processos, NUM_APP, getpid());

    if (criaFIFO("FIFO_SYSCALL") < 0) {
        perror("Falha ao criar FIFO");
        exit(EXIT_FAILURE);
    }

    int fifo_syscall;
    // A Application irá escrever na FIFO de SYSCALLs
    if (abreFIFO(&fifo_syscall, "FIFO_SYSCALL", O_WRONLY) < 0) {
        perror("Falha ao abrir FIFO de SYSCALLs");
        exit(EXIT_FAILURE);
    }

    while (PC < MAX)
    {
        printf("\nApplication com PID %d rodando pela %dª vez\n", getpid(), ++PC);
        appAtual->pc = PC;

        sleep(0.5);

        aux = rand() % 100 + 1; // Gera um número entre 0 ou 100
        
        // Probabilidade de 15% para a realização de uma syscall
        if (aux < 15)
        {
            if (aux % 2) Dx = D1;
            else Dx = D2;

            if (aux % 3 == 0) Op = R;
            else if (aux % 3 == 1) Op = W;
            else Op = X;

            printf("Foi realizada uma syscall ao dispositivo %d com a operação ", Dx + 1);

            switch (Op){
                case R:
                    printf("de leitura\n");
                    break;
                case W:
                    printf("de escrita\n");
                    break;
                case X:
                    printf("de escrita e leitura\n");
                    break;
            }

            // Realizar a syscall
            char msg[12];
            snprintf(msg, sizeof(msg), "%7d;%d;%d", getpid(), Dx, Op);
            write(fifo_syscall, msg, 12);
        }

        sleep(0.5);
    }
    
    appAtual->estado = TERMINADO;
    appAtual->executando = 0;

    printf("\nApplication com PID %d terminou sua execução\n", getpid());
    
    shmdt(shm_processos);

    return 0;
}