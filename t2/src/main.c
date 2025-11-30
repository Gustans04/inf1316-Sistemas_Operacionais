#define _DEFAULT_SOURCE

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
#include <semaphore.h>

#include "aux.h"

int main() 
{
    signal(SIGINT, SIG_IGN);
    pid_t pid_list[NUM_APP];
    pid_t inter_pid;
    
    // Initialize the semaphore for inter-process communication
    init_sem();

    // Lista de processos esperando pelos dispositivos
    FilaApps* esperandoD1 = (FilaApps* )malloc(sizeof(FilaApps));
    FilaApps* esperandoD2 = (FilaApps* )malloc(sizeof(FilaApps));
    FilaApps* prontos = (FilaApps* )malloc(sizeof(FilaApps));
    inicializarFila(esperandoD1);
    inicializarFila(esperandoD2);
    inicializarFila(prontos);

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

    if (criaFIFO("FIFO_IRQ") < 0) {
        perror("Falha ao criar FIFOs");
        exit(EXIT_FAILURE);
    }

    int fifo_irq;

    // O Kernel simplesmente lê o que for escrito na FIFO pelo InterController
    if (abreFIFO(&fifo_irq, "FIFO_IRQ", O_RDONLY | O_NONBLOCK) < 0) {
        perror("Falha ao abrir FIFO de IRQs");
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
            printf("Criando Application com PID %d\n", getpid());
            char app_name[20];
            snprintf(app_name, sizeof(app_name), "application%d", i);
            app_name[12] = '\0';

            execl("./application", app_name, NULL);
            fprintf(stderr, "Erro ao rodar a Application\n");
            exit(EXIT_FAILURE);
        }
        else {
            // Kernel continua aqui
            // Adiciona o pid da Application na fila de prontos
            inserirNaFila(prontos, pid_list[i]);

            sem_lock();
            // Adiciona o pid da Application na struct de memória compartilhada
            shm_processos[i].pid = pid_list[i];
            shm_processos[i].pc = 0;
            shm_processos[i].estado = PRONTO;
            shm_processos[i].syscall.tipo_syscall = -1;
            shm_processos[i].executando = 1;
            sem_unlock();
        }
    }

    // Kernel
    printf("Kernel com PID %d\n", getpid());

    for (int i = 1; i < NUM_APP; i++)
    {
        if (kill(pid_list[i], SIGSTOP) == -1)
        {
            perror("Falha ao enviar sinal SIGSTOP");
            exit(EXIT_FAILURE);
        }

        sem_lock();
        shm_processos[i].executando = 0;
        sem_unlock();
    }

    pid_t pidTemp = removerDaFila(prontos); // Pega o primeiro da fila de prontos
    InfoProcesso* appAtual = encontrarAplicacaoPorPID(shm_processos, pidTemp);
    if (appAtual) {
        sem_lock();
        appAtual->estado = EXECUTANDO;
        appAtual->executando = 1;
        sem_unlock();
    }

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
                    sem_lock();
                    if (appAtual->estado == EXECUTANDO)
                    {
                        printf("Kernel: Preemptando processo %d (timer)\n", appAtual->pid);
                        appAtual->estado = PRONTO;
                        appAtual->executando = 0;

                        if (kill(appAtual->pid, SIGSTOP) == -1)
                        {
                            perror("Falha ao enviar sinal SIGSTOP (IRQ0)");
                            exit(EXIT_FAILURE);
                        } 
                        inserirNaFila(prontos, appAtual->pid);
                    }
                    sem_unlock();

                    int proximo_encontrado = 0;
                    while (!estaVazia(prontos))
                    {
                        pidTemp = removerDaFila(prontos);
                        InfoProcesso* appTestado = encontrarAplicacaoPorPID(shm_processos, pidTemp); 
                        
                        printf("Kernel: Verificando PID %d da fila [Estado: %d]\n", pidTemp, appTestado->estado);

                        if (appTestado->estado == PRONTO) 
                        {
                            appAtual = appTestado; 
                            
                            printf("Kernel: Escalonando PID %d\n", appAtual->pid);
                            sem_lock();
                            if (kill(appAtual->pid, SIGCONT) == -1)
                            {
                                perror("Falha ao enviar sinal SIGCONT (IRQ0)");
                                exit(EXIT_FAILURE);
                            }
                            appAtual->estado = EXECUTANDO;
                            appAtual->executando = 1;
                            proximo_encontrado = 1;
                            sem_unlock();
                            break; 
                        }
                        printf("Kernel: Ignorando PID %d (estado nao eh PRONTO)\n", pidTemp);
                    }

                    if (!proximo_encontrado && appAtual->estado != EXECUTANDO) {
                        printf("Kernel: Fila de prontos vazia. CPU Ocioso.\n");
                    }
                }
                
                if (strncmp(ponteiro_msg, "IRQ1", 5) == 0)
                {
                    if (!estaVazia(esperandoD1))
                    {
                        pid_t pidTemp = removerDaFila(esperandoD1);
                        InfoProcesso* appPronto = encontrarAplicacaoPorPID(shm_processos, pidTemp);
                        sem_lock();
                        appPronto->estado = PRONTO;
                        inserirNaFila(prontos, appPronto->pid);
                        sem_unlock();
                    }
                }
                
                if (strncmp(ponteiro_msg, "IRQ2", 5) == 0)
                {
                    if (!estaVazia(esperandoD2))
                    {
                        pid_t pidTemp = removerDaFila(esperandoD2);
                        InfoProcesso* appPronto = encontrarAplicacaoPorPID(shm_processos, pidTemp);
                        sem_lock();
                        appPronto->estado = PRONTO;
                        inserirNaFila(prontos, appPronto->pid);
                        sem_unlock();
                    }
                }

                ponteiro_msg += 5;
            }
        }

        // Lê SYSCALLs
        sem_lock();
        int syscall = appAtual->syscall.tipo_syscall;
        sem_unlock();
        if (syscall >= 0) {
            printf("Kernel: Recebeu SysCall do PID %d: ", appAtual->pid);
            switch (syscall)
            {
            case 0: // WriteCall
                sem_lock();
                printf("write(%s, %s, %d)\n", 
                    appAtual->syscall.call.writecall.path, 
                    appAtual->syscall.call.writecall.payload, 
                    appAtual->syscall.call.writecall.offset);
                appAtual->syscall.tipo_syscall = -1; // Reseta a syscall após processar
                sem_unlock();
                // Processa a WriteCall aqui
                break;
            case 1: // ReadCall
                sem_lock();
                printf("read(%s, buffer, %d)\n", 
                    appAtual->syscall.call.readcall.path, 
                    appAtual->syscall.call.readcall.offset);
                appAtual->syscall.tipo_syscall = -1; // Reseta a syscall após processar
                sem_unlock();
                // Processa a ReadCall aqui
                break;
            case 2: // AddCall
                sem_lock();
                printf("add(%s, %d, %s, %d)\n", 
                    appAtual->syscall.call.addcall.path, 
                    appAtual->syscall.call.addcall.len1,
                    appAtual->syscall.call.addcall.dirname,
                    appAtual->syscall.call.addcall.len2);
                appAtual->syscall.tipo_syscall = -1; // Reseta a syscall após processar
                sem_unlock();
                // Processa a AddCall aqui
                break;
            case 3: // RemCall
                sem_lock();
                printf("rem(%s, %d, %s, %d)\n", 
                    appAtual->syscall.call.remcall.path, 
                    appAtual->syscall.call.remcall.len1,
                    appAtual->syscall.call.remcall.name,
                    appAtual->syscall.call.remcall.len2);
                appAtual->syscall.tipo_syscall = -1; // Reseta a syscall após processar
                sem_unlock();
                // Processa a RemCall aqui
                break;
            case 4: // ListDirCall
                sem_lock();
                printf("listdir(%s, %d, alldirInfo[], fstlstpositions[40], &nrnames)\n", 
                    appAtual->syscall.call.listdircall.path,
                    appAtual->syscall.call.listdircall.len1);
                appAtual->syscall.tipo_syscall = -1; // Reseta a syscall após processar
                sem_unlock();
                // Processa a ListDirCall aqui
                break;
            }
            
            pid_t pidTemp;
            int era_atual;

            /*
            if (itensEncontrados == 3)
            {
                InfoProcesso2* appBloqueado = encontrarAplicacaoPorPID(shm_processos, pidTemp);
                sem_lock();
                appBloqueado->estado = BLOQUEADO;
                removerTodasOcorrencias(prontos, pidTemp);

                if (appBloqueado->executando) era_atual = 1;
                else era_atual = 0;

                appBloqueado->executando = 0;

                
                sem_unlock();

                if (kill(pidTemp, SIGSTOP) == -1)
                {
                    perror("Falha ao enviar sinal SIGSTOP");
                    exit(EXIT_FAILURE);
                }

                if (era_atual)
                {
                    int proximo_encontrado = 0;
                    while (!estaVazia(prontos))
                    {
                        pidTemp = removerDaFila(prontos);
                        appAtual = encontrarAplicacaoPorPID(shm_processos, pidTemp);

                        // pula qualquer PID que não exista mais ou já tenha TERMINADO
                        if (!appAtual || appAtual->estado == TERMINADO || kill(pidTemp, 0) == -1) {
                            removerTodasOcorrencias(prontos, pidTemp); 
                            continue; 
                        }

                        printf("Kernel: Verificando PID %d da fila [Estado: %d]\n", pidTemp, appAtual->estado);

                        if (appAtual->estado == PRONTO)
                        {
                            printf("Kernel: Escalonando PID %d\n", appAtual->pid);
                            sem_lock();
                            if (kill(appAtual->pid, SIGCONT) == -1)
                            {
                                perror("Falha ao enviar sinal SIGCONT (SYSCALL)");
                                exit(EXIT_FAILURE);
                            }
                            appAtual->estado = EXECUTANDO;
                            appAtual->executando = 1;
                            proximo_encontrado = 1;
                            sem_unlock();
                            break; 
                        }
                        printf("Kernel: Ignorando PID %d (estado nao eh PRONTO)\n", pidTemp);
                    }

                    if (!proximo_encontrado) {
                        printf("Kernel: Fila de prontos ficou vazia\n");
                    }
                }
            }
            */
        }

        if (processosAcabaram(shm_processos))
        {
            printf("\nTodos os processos acabaram!\n");
            break;
        }

        // Varredura para verificar se um processo já acabou
        int status;
        pid_t app;
        while ((app = waitpid(-1, &status, WNOHANG)) > 0) 
        {
            InfoProcesso* processo = encontrarAplicacaoPorPID(shm_processos, app);
            sem_lock();
            if (processo) {
                processo->estado = TERMINADO;
                processo->executando = 0;
            }
            sem_unlock();
            removerTodasOcorrencias(prontos, app);
            removerTodasOcorrencias(esperandoD1, app);
            removerTodasOcorrencias(esperandoD2, app);
        }

    }

    if (kill(inter_pid, SIGUSR1) == -1)
    {
        perror("Falha ao enviar sinal SIGUSR1");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < NUM_APP + 1; i++) wait(NULL); // esperar todas as 5 Applications e o InterController
    
    printf("Kernel terminou sua execução\n");

    printf("\n=== Tabela Final dos Processos ===\n");
    // print_status(shm_processos);

    shmdt(shm_processos);
    close(fifo_irq);
    
    // Cleanup semaphore
    cleanup_sem();

    return 0;
}
