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
    FilaApps *esperandoD1 = (FilaApps *)malloc(sizeof(FilaApps));
    FilaApps *esperandoD2 = (FilaApps *)malloc(sizeof(FilaApps));
    FilaApps *prontos = (FilaApps *)malloc(sizeof(FilaApps));
    FilaRequests *filaFiles = (FilaRequests *)malloc(sizeof(FilaRequests));
    FilaRequests *filaDirs = (FilaRequests *)malloc(sizeof(FilaRequests));
    inicializarFila(esperandoD1);
    inicializarFila(esperandoD2);
    inicializarFila(prontos);
    inicializarFilaRequests(filaFiles);
    inicializarFilaRequests(filaDirs);
    iniciaUdpClient();

    // Aloca a memória compartilhada
    int segmento = shmget(IPC_CODE, sizeof(InfoProcesso) * NUM_APP, IPC_CREAT | S_IRUSR | S_IWUSR);
    if (segmento == -1)
    {
        perror("shmget falhou");
        exit(EXIT_FAILURE);
    }

    // Conecta a memória compartilhada
    InfoProcesso *shm_processos = (InfoProcesso *)shmat(segmento, 0, 0);
    if (shm_processos == (InfoProcesso *)-1)
    {
        perror("shmat falhou");
        exit(EXIT_FAILURE);
    }

    if (criaFIFO("FIFO_IRQ") < 0)
    {
        perror("Falha ao criar FIFOs");
        exit(EXIT_FAILURE);
    }

    int fifo_irq;

    // O Kernel simplesmente lê o que for escrito na FIFO pelo InterController
    if (abreFIFO(&fifo_irq, "FIFO_IRQ", O_RDONLY | O_NONBLOCK) < 0)
    {
        perror("Falha ao abrir FIFO de IRQs");
        exit(EXIT_FAILURE);
    }

    if ((inter_pid = fork()) < 0)
    {
        perror("Fork falhou!");
        exit(EXIT_FAILURE);
    }
    else if (inter_pid == 0)
    {
        // InterController
        printf("InterController com PID %d\n", getpid());
        execl("../bin/intercontroller", "inter", NULL);
        fprintf(stderr, "Erro ao rodar o InterController\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_APP; i++)
    {
        pid_list[i] = fork();
        if (pid_list[i] < 0)
        {
            perror("Fork falhou!");
            exit(EXIT_FAILURE);
        }
        else if (pid_list[i] == 0)
        {
            // Applications
            printf("Criando Application com PID %d\n", getpid());
            char app_name[20];
            snprintf(app_name, sizeof(app_name), "application%d", i);
            app_name[12] = '\0';

            execl("../bin/application", app_name, NULL);
            fprintf(stderr, "Erro ao rodar a Application\n");
            exit(EXIT_FAILURE);
        }
        else
        {
            // Kernel continua aqui
            // Adiciona o pid da Application na fila de prontos
            inserirNaFila(prontos, pid_list[i]);

            sem_lock();
            // Adiciona o pid da Application na struct de memória compartilhada
            shm_processos[i].pid = pid_list[i];
            shm_processos[i].pc = 0;
            shm_processos[i].estado = PRONTO;
            shm_processos[i].syscall.novo = 0;
            shm_processos[i].syscall.lido = 0;
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
    InfoProcesso *appAtual = encontrarAplicacaoPorPID(shm_processos, pidTemp);
    if (appAtual)
    {
        sem_lock();
        appAtual->estado = EXECUTANDO;
        appAtual->executando = 1;
        sem_unlock();
    }

    while (1)
    {
        char buffer[25];

        // Lê IRQs
        ssize_t bytes_read = read(fifo_irq, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';

            char *ponteiro_msg = buffer;

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
                        InfoProcesso *appTestado = encontrarAplicacaoPorPID(shm_processos, pidTemp);

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

                    if (!proximo_encontrado && appAtual->estado != EXECUTANDO)
                    {
                        printf("Kernel: Fila de prontos vazia. CPU Ocioso.\n");
                    }
                }

                if (strncmp(ponteiro_msg, "IRQ1", 5) == 0)
                {
                    if (!estaVaziaRequests(filaFiles))
                    {
                        int owner = removerDaFilaRequests(filaFiles);
                        if (!estaVazia(esperandoD1))
                        {
                            pid_t pidTemp = shm_processos[owner - 1].pid;
                            removerPidDaFila(esperandoD1, pidTemp);
                            InfoProcesso *appPronto = encontrarAplicacaoPorPID(shm_processos, pidTemp);
                            sem_lock();
                            appPronto->estado = PRONTO;
                            inserirNaFila(prontos, appPronto->pid);
                            sem_unlock();
                        }
                    }
                }

                if (strncmp(ponteiro_msg, "IRQ2", 5) == 0)
                {
                    if (!estaVaziaRequests(filaDirs))
                    {
                        int owner = removerDaFilaRequests(filaDirs);
                        if (!estaVazia(esperandoD2))
                        {
                            pid_t pidTemp = shm_processos[owner - 1].pid;
                            removerPidDaFila(esperandoD2, pidTemp);
                            InfoProcesso *appPronto = encontrarAplicacaoPorPID(shm_processos, pidTemp);
                            sem_lock();
                            appPronto->estado = PRONTO;
                            inserirNaFila(prontos, appPronto->pid);
                            sem_unlock();
                        }
                    }
                }

                ponteiro_msg += 5;
            }
        }

        // Lê SYSCALLs
        sem_lock();
        int syscall = appAtual->syscall.novo;
        sem_unlock();
        if (syscall > 0)
        {
            pid_t pidTemp = appAtual->pid;
            int era_atual;
            int owner = numeroDoProcesso(shm_processos, pidTemp);

            InfoProcesso *appBloqueado = encontrarAplicacaoPorPID(shm_processos, pidTemp);
            sem_lock();
            appBloqueado->syscall.novo = 0;
            appBloqueado->estado = BLOQUEADO;
            removerTodasOcorrencias(prontos, pidTemp);

            if (appBloqueado->executando)
                era_atual = 1;
            else
                era_atual = 0;

            appBloqueado->executando = 0;

            switch (appBloqueado->syscall.tipo_syscall)
            {
            case 0: // WriteCall
                // Processa a WriteCall aqui
                CallRequest wr;
                wr.tipo_syscall = 0;
                wr.owner = owner;
                snprintf(wr.call.writecall.path, sizeof(wr.call.writecall.path), "%s", appAtual->syscall.call.writecall.path);
                wr.call.writecall.len = appAtual->syscall.call.writecall.len;
                snprintf(wr.call.writecall.payload, sizeof(wr.call.writecall.payload), "%s", appAtual->syscall.call.writecall.payload);
                wr.call.writecall.offset = appAtual->syscall.call.writecall.offset;
                // Simula a escrita no arquivo
                printf("Kernel: WR-REQ, %d, %s, %d, %s, %d\n",
                       wr.owner,
                       wr.call.writecall.path,
                       wr.call.writecall.len,
                       wr.call.writecall.payload,
                       wr.call.writecall.offset);
                inserirNaFila(esperandoD1, pidTemp); // Simula espera pelo dispositivo D1
                enviaUdpRequest(wr);
                break;
            case 1: // ReadCall
                // Processa a ReadCall aqui
                CallRequest rd;
                rd.tipo_syscall = 1;
                rd.owner = owner;
                snprintf(rd.call.readcall.path, sizeof(rd.call.readcall.path), "%s", appAtual->syscall.call.readcall.path);
                rd.call.readcall.len = appAtual->syscall.call.readcall.len;
                memset(rd.call.readcall.buffer, 0, sizeof(rd.call.readcall.buffer));
                rd.call.readcall.offset = appAtual->syscall.call.readcall.offset;
                // Simula a leitura no arquivo
                printf("Kernel: RD-REQ, %d, %s, %d, buffer[], %d\n",
                       rd.owner,
                       rd.call.readcall.path,
                       rd.call.readcall.len,
                       rd.call.readcall.offset);
                inserirNaFila(esperandoD1, pidTemp); // Simula espera pelo dispositivo D1
                enviaUdpRequest(rd);
                break;
            case 2: // AddCall
                // Processa a AddCall aqui
                CallRequest dc;
                dc.tipo_syscall = 2;
                dc.owner = owner;
                snprintf(dc.call.addcall.path, sizeof(dc.call.addcall.path), "%s", appAtual->syscall.call.addcall.path);
                dc.call.addcall.len1 = appAtual->syscall.call.addcall.len1;
                snprintf(dc.call.addcall.dirname, sizeof(dc.call.addcall.dirname), "%s", appAtual->syscall.call.addcall.dirname);
                dc.call.addcall.len2 = appAtual->syscall.call.addcall.len2;
                // Simula a criação do diretório
                printf("Kernel: DC-REQ, %d, %s, %d, %s, %d\n",
                       dc.owner,
                       dc.call.addcall.path,
                       dc.call.addcall.len1,
                       dc.call.addcall.dirname,
                       dc.call.addcall.len2);
                inserirNaFila(esperandoD2, pidTemp); // Simula espera pelo dispositivo D2
                enviaUdpRequest(dc);
                break;
            case 3: // RemCall
                // Processa a RemCall aqui
                CallRequest dr;
                dr.tipo_syscall = 3;
                dr.owner = owner;
                snprintf(dr.call.remcall.path, sizeof(dr.call.remcall.path), "%s", appAtual->syscall.call.remcall.path);
                dr.call.remcall.len1 = appAtual->syscall.call.remcall.len1;
                snprintf(dr.call.remcall.name, sizeof(dr.call.remcall.name), "%s", appAtual->syscall.call.remcall.name);
                dr.call.remcall.len2 = appAtual->syscall.call.remcall.len2;
                // Simula a remoção do arquivo ou diretório
                printf("Kernel: DR-REQ, %d, %s, %d, %s, %d\n",
                       dr.owner,
                       dr.call.remcall.path,
                       dr.call.remcall.len1,
                       dr.call.remcall.name,
                       dr.call.remcall.len2);
                inserirNaFila(esperandoD2, pidTemp); // Simula espera pelo dispositivo D2
                enviaUdpRequest(dr);
                break;
            case 4: // ListDirCall
                // Processa a ListDirCall aqui
                CallRequest dl;
                dl.tipo_syscall = 4;
                dl.owner = owner;
                snprintf(dl.call.listdircall.path, sizeof(dl.call.listdircall.path), "%s", appAtual->syscall.call.listdircall.path);
                dl.call.listdircall.len1 = appAtual->syscall.call.listdircall.len1;
                // Simula a listagem do diretório
                printf("Kernel: DL-REQ, %d, %s, %d\n",
                       dl.owner,
                       dl.call.listdircall.path,
                       dl.call.listdircall.len1);
                inserirNaFila(esperandoD2, pidTemp); // Simula espera pelo dispositivo D2
                enviaUdpRequest(dl);
                break;
            }

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
                    if (!appAtual || appAtual->estado == TERMINADO || kill(pidTemp, 0) == -1)
                    {
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

                if (!proximo_encontrado)
                {
                    printf("Kernel: Fila de prontos ficou vazia\n");
                }
            }
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
            InfoProcesso *processo = encontrarAplicacaoPorPID(shm_processos, app);
            sem_lock();
            if (processo)
            {
                processo->estado = TERMINADO;
                processo->executando = 0;
            }
            sem_unlock();
            removerTodasOcorrencias(prontos, app);
            removerTodasOcorrencias(esperandoD1, app);
            removerTodasOcorrencias(esperandoD2, app);
        }

        // Lê resposta do UDP (se houver)
        CallRequest response;
        while ((response = recebeUdpResponse()).tipo_syscall != -1)
        {
            printf("Kernel: recebeu resposta UDP para o processo %d\n", response.owner);
            sem_lock();
            respostaParaApp(&shm_processos[response.owner - 1], response);
            if (response.tipo_syscall == 0 || response.tipo_syscall == 1)
                inserirNaFilaRequests(filaFiles, response);
            else if (response.tipo_syscall == 2 || response.tipo_syscall == 3 || response.tipo_syscall == 4)
                inserirNaFilaRequests(filaDirs, response);
            sem_unlock();
            printf("Kernel: resposta UDP enfileirada para o processo %d\n", response.owner);
        }
    }

    if (kill(inter_pid, SIGUSR1) == -1)
    {
        perror("Falha ao enviar sinal SIGUSR1");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_APP + 1; i++)
        wait(NULL); // esperar todas as 5 Applications e o InterController

    printf("Kernel terminou sua execução\n");

    printf("\n=== Tabela Final dos Processos ===\n");
    print_status(shm_processos);

    shmdt(shm_processos);
    close(fifo_irq);
    encerraUdpClient();

    // Cleanup semaphore
    cleanup_sem();

    return 0;
}
