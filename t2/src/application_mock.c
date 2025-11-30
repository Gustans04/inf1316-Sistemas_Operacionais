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
#include <signal.h>
#include <semaphore.h>

#include "aux.h"

// Número máximo de iterações
#define MAX 5

int main()
{
    signal(SIGINT, SIG_IGN);
    srand(getpid() * time(NULL)); // Para os números ficarem randômicos
    
    // Abrir o semáforo existente para uso neste processo
    process_sem = sem_open("/t2_process_sem", 0);

    int PC = 0; // Contador de iterações (Program Counter)

    int aux; // Auxiliar para decidir sobre as syscalls, os dispositivos e as operações

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
    
    InfoProcesso* appAtual = encontrarAplicacaoPorPID(shm_processos, getpid());
    
    while (appAtual == NULL) // Caso não esteja escrito ainda o PID da Application
    {
        appAtual = encontrarAplicacaoPorPID(shm_processos, getpid());
    }

    while (PC < MAX)
    {
        printf("\nApplication com PID %d rodando pela %dª vez\n", getpid(), ++PC);
        sem_lock();
        appAtual->pc = PC;
        sem_unlock();

        usleep(500000); // 500 milissegundos

        aux = rand() % 100; // Gera um número aleatório
        
        // Probabilidade de 15% para a realização de uma syscall
        if (aux <= 50)
        {
            printf("Application com PID %d preparando uma SysCall\n", getpid());
            // Decide qual syscall será realizada
            int tipo_syscall = rand() % 5; // 0 - WriteCall, 1 - ReadCall, 2 - AddCall, 3 - RemCall, 4 - ListDirCall
            printf("Application com PID %d escolheu a SysCall do tipo %d\n", getpid(), tipo_syscall);
            sem_lock();
            appAtual->syscall.tipo_syscall = tipo_syscall;
            switch (tipo_syscall)
            {
            case 0: // WriteCall
                printf("Application com PID %d realizando WriteCall\n", getpid());
                // Preencher os dados da WriteCall
                snprintf(appAtual->syscall.call.writecall.path, sizeof(appAtual->syscall.call.writecall.path), "file_write_%d.txt", getpid());
                appAtual->syscall.call.writecall.len = strlen(appAtual->syscall.call.writecall.path);
                snprintf(appAtual->syscall.call.writecall.payload, sizeof(appAtual->syscall.call.writecall.payload), "Data from %d", getpid());
                appAtual->syscall.call.writecall.offset = (rand() % 5) * 16; // Offset aleatório múltiplo de 16
                break;
            case 1: // ReadCall
                printf("Application com PID %d realizando ReadCall\n", getpid());
                // Preencher os dados da ReadCall
                snprintf(appAtual->syscall.call.readcall.path, sizeof(appAtual->syscall.call.readcall.path), "file_read_%d.txt", getpid());
                appAtual->syscall.call.readcall.len = strlen(appAtual->syscall.call.readcall.path);
                memset(appAtual->syscall.call.readcall.buffer, 0, sizeof(appAtual->syscall.call.readcall.buffer));
                appAtual->syscall.call.readcall.offset = (rand() % 5) * 16; // Offset aleatório múltiplo de 16
                break;
            case 2: // AddCall
                printf("Application com PID %d realizando AddCall\n", getpid());
                // Preencher os dados da AddCall
                snprintf(appAtual->syscall.call.addcall.path, sizeof(appAtual->syscall.call.addcall.path), "dir_add_%d", getpid());
                snprintf(appAtual->syscall.call.addcall.dirname, sizeof(appAtual->syscall.call.addcall.dirname), "subdir_%d_%d", getpid(), PC);
                appAtual->syscall.call.addcall.len1 = strlen(appAtual->syscall.call.addcall.path);
                appAtual->syscall.call.addcall.len2 = strlen(appAtual->syscall.call.addcall.dirname);
                break;
            case 3: // RemCall
                printf("Application com PID %d realizando RemCall\n", getpid());
                // Preencher os dados da RemCall
                snprintf(appAtual->syscall.call.remcall.path, sizeof(appAtual->syscall.call.remcall.path), "dir_remove_%d", getpid());
                snprintf(appAtual->syscall.call.remcall.name, sizeof(appAtual->syscall.call.remcall.name), "subdir_%d_%d", getpid(), PC - 1); // Remove o subdiretório criado na iteração anterior
                appAtual->syscall.call.remcall.len1 = strlen(appAtual->syscall.call.remcall.path);
                appAtual->syscall.call.remcall.len2 = strlen(appAtual->syscall.call.remcall.name);
                break;
            case 4: // ListDirCall
                printf("Application com PID %d realizando ListDirCall\n", getpid());
                // Preencher os dados da ListDirCall
                snprintf(appAtual->syscall.call.listdircall.path, sizeof(appAtual->syscall.call.listdircall.path), "dir_list_%d", getpid());
                appAtual->syscall.call.listdircall.len1 = strlen(appAtual->syscall.call.listdircall.path);
                appAtual->syscall.call.listdircall.nrnames = &(int){0}; // Inicializa o número de nomes encontrados como 0
                break;
            }
            sem_unlock();
        }
        usleep(500000); // 500 milissegundos
    }

    sem_lock();
    appAtual->estado = TERMINADO;
    appAtual->executando = 0;
    sem_unlock();

    printf("\nApplication com PID %d terminou sua execução\n", getpid());
    
    shmdt(shm_processos);
    sem_close(process_sem);

    return 0;
}