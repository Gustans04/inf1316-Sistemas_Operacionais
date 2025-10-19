#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#include "aux.h"

int main() 
{
    pid_t pid_list[NUM_APP];
    pid_t inter_pid;

    // Lista de processos esperando pelos dispositivos
    FilaApps* esperandoD1 = (FilaApps* )malloc(sizeof(InfoProcesso));
    FilaApps* esperandoD2 = (FilaApps* )malloc(sizeof(InfoProcesso));
    inicializarFila(esperandoD1);
    inicializarFila(esperandoD2);
    
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

    if ((inter_pid = fork()) < 0) {
        perror("Fork falhou!");
        exit(EXIT_FAILURE);
    } else if (inter_pid == 0) {
        // InterController 
        printf("InterController com PID %d\n", getpid());
        execl("./intercontroller", "inter", NULL);
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
            char app_name[20];
            snprintf(app_name, sizeof(app_name), "application%d", i);
            app_name[12] = '\0';

            // Adiciona o pid da Application na struct de memória compartilhada
            shm_processos[i].pid = getpid();
            shm_processos[i].estado = PRONTO;
            shm_processos[i].dispositivo = -1;
            shm_processos[i].operacao = -1;
            shm_processos[i].executando = 1;
            memset(shm_processos[i].qtd_acessos, 0, sizeof(shm_processos[i].qtd_acessos));

            execl("./application", app_name, NULL);
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

    for (int i = 1; i < NUM_APP; i++)
    {
        if (kill(pid_list[i], SIGSTOP) == -1)
            {
                perror("Falha ao enviar sinal SIGSTOP");
                exit(EXIT_FAILURE);
            }
    }

    InfoProcesso* appAtual; 
    InfoProcesso* appProx; 
    int atual = 0;
    int proximo_indice = 1;
    while(1)
    {
        char buffer[25];

        // Lê IRQs
        ssize_t bytes_read = read(fifo_irq, buffer, sizeof(buffer) - 1); 
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; 

            char* ponteiro_msg = buffer; 

            // Itera sobre o buffer ENQUANTO houver bytes para processar
            // (Cada mensagem "IRQx\0" tem 5 bytes)
            while (ponteiro_msg < buffer + bytes_read)
            {
                printf("Kernel recebeu: %s\n", ponteiro_msg);

                if (strncmp(ponteiro_msg, "IRQ0", 5) == 0)
                {
                    appAtual = &shm_processos[atual];
                    // ... (resto da sua lógica IRQ0) ...
                    // ...
                    appProx->estado = EXECUTANDO;
                    appProx->executando = 1;
                }
                else if (strncmp(ponteiro_msg, "IRQ1", 5) == 0)
                {
                    if (!estaVazia(esperandoD1))
                    {
                        pid_t pidTemp = removerDaFila(esperandoD1);
                        appAtual = encontrarAplicacaoPorPID(shm_processos, NUM_APP, pidTemp);
                        appAtual->estado = ESPERANDO;
                    }
                }
                else if (strncmp(ponteiro_msg, "IRQ2", 5) == 0)
                {
                    if (!estaVazia(esperandoD2))
                    {
                        pid_t pidTemp = removerDaFila(esperandoD2);
                        appAtual = encontrarAplicacaoPorPID(shm_processos, NUM_APP, pidTemp);
                        appAtual->estado = ESPERANDO;
                    }
                }

                ponteiro_msg += 5;
            }
        }

        // Lê SYSCALLs
        bytes_read = read(fifo_syscall, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Kernel recebeu: %s\n", buffer);
            
            pid_t pidTemp;
            int dxTemp;
            int opTemp;

            int itensEncontrados = sscanf(buffer, "%d;%d;%d", &pidTemp, &dxTemp, &opTemp);

            if (itensEncontrados == 3)
            {
                appAtual = encontrarAplicacaoPorPID(shm_processos, NUM_APP, pidTemp);
                appAtual->estado = BLOQUEADO;
                appAtual->dispositivo = dxTemp;
                appAtual->operacao = opTemp;
                appAtual->executando = 0;

                switch (dxTemp)
                {
                    case D1: 
                        inserirNaFila(esperandoD1, pidTemp); 
                        appAtual->qtd_acessos[0]++; 
                        break;
                    case D2: 
                        inserirNaFila(esperandoD2, pidTemp); 
                        appAtual->qtd_acessos[1]++; 
                        break;
                }

                if (kill(pidTemp, SIGSTOP) == -1)
                {
                    perror("Falha ao enviar sinal SIGSTOP");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    for(int i = 0; i < NUM_APP + 1; i++) wait(NULL);

    shmdt(shm_processos);
    
    return 0;
}
