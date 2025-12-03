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

// Valores aleatórios para offsets (múltiplos de 16)
int offsets[] = {0, 16, 32, 48, 64, 80, 96};
#define NUM_OFFSETS 7

// Nomes aleatórios para arquivos e diretórios
char *nomes[] = {"relatorio", "notas", "foto", "rato", "dados", "backup", "projeto", "log", "teste",
                 "banana", "gato", "sorvete", "trabalho", "pedro", "endler", "cachorro", "flamengo"};
#define NUM_NOMES 16

// Função auxiliar para obter um nome aleatório
char *nome_aleatorio()
{
    return nomes[rand() % NUM_NOMES];
}

// Função auxiliar para obter um offset aleatório
int offset_aleatorio()
{
    return offsets[rand() % NUM_OFFSETS];
}

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
    int segmento = shmget(IPC_CODE, sizeof(InfoProcesso) * NUM_APP, IPC_CREAT | S_IRUSR | S_IWUSR);
    if (segmento == -1)
    {
        perror("shmget falhou");
        exit(EXIT_FAILURE);
    }

    // conecta a memória compartilhada
    InfoProcesso *shm_processos = (InfoProcesso *)shmat(segmento, 0, 0);
    if (shm_processos == (InfoProcesso *)-1)
    {
        perror("shmat falhou");
        exit(EXIT_FAILURE);
    }

    InfoProcesso *appAtual = encontrarAplicacaoPorPID(shm_processos, getpid());

    while (appAtual == NULL) // Caso não esteja escrito ainda o PID da Application
    {
        appAtual = encontrarAplicacaoPorPID(shm_processos, getpid());
    }

    // Descobre o número do processo (A1, A2, etc) baseado na ordem na SHM
    int my_id = -1;
    for (int i = 0; i < NUM_APP; i++)
    {
        if (shm_processos[i].pid == getpid())
        {
            my_id = i + 1; // IDs de 1 a 5
            break;
        }
    }

    while (PC < MAX)
    {
        printf("\nApplication com PID %d rodando pela %dª vez\n", getpid(), ++PC);

        sem_lock();
        appAtual->pc = PC;
        sem_unlock();

        usleep(500000); // 500 milissegundos

        aux = rand() % 100; // Gera um número aleatório

        if (appAtual->syscall.lido == 1) // Há resposta para ser lida
        {
            printf("\n====== Resposta recebida por %d [A%d] ======\n", getpid(), my_id);
            switch (appAtual->syscall.tipo_syscall)
            {
            case 0: // WriteCall
                if (appAtual->syscall.call.writecall.offset < 0)
                {
                    printf("Ocorreu um erro na WRITE!\n");
                }
                else
                {
                    printf("WRITE(%s, %s, %d) --> Sucesso!\n", appAtual->syscall.call.writecall.path,
                           appAtual->syscall.call.writecall.payload,
                           appAtual->syscall.call.writecall.offset);
                }

                break;

            case 1: // ReadCall
                if (appAtual->syscall.call.readcall.offset < 0)
                {
                    printf("Ocorreu um erro na READ!\n");
                }
                else
                {
                    printf("READ(%s, %d) --> %s!\n", appAtual->syscall.call.readcall.path,
                           appAtual->syscall.call.readcall.offset,
                           appAtual->syscall.call.readcall.buffer);
                }

                break;

            case 2: // AddCall
                if (appAtual->syscall.call.addcall.len1 < 0)
                {
                    printf("Ocorreu um erro na ADD!\n");
                }
                else
                {
                    printf("ADD(%s, %s) --> Sucesso!\n", appAtual->syscall.call.addcall.path,
                           appAtual->syscall.call.addcall.dirname);
                }

                break;

            case 3: // RemCall
                if (appAtual->syscall.call.remcall.len1 < 0)
                {
                    printf("Ocorreu um erro na REMOVE!\n");
                }
                else
                {
                    printf("REM(%s, %s) --> Sucesso!\n", appAtual->syscall.call.remcall.path,
                           appAtual->syscall.call.remcall.name);
                }

                break;

            case 4: // ListDirCall
                if (appAtual->syscall.call.listdircall.nrnames < 0)
                {
                    printf("Ocorreu um erro na LIST DIR!\n");
                }
                else
                {
                    printf("LIST(%s) --> Sucesso!\n", appAtual->syscall.call.listdircall.path);
                    imprimir_lista_diretorio(&(appAtual->syscall.call.listdircall));
                }

                break;
            }
            printf("\n======================================\n");
            appAtual->syscall.lido = 0;
        }

        // Probabilidade de 15% para a realização de uma syscall
        if (aux <= 15)
        {
            printf("Application com PID %d preparando uma SysCall\n", getpid());

            // Decide qual syscall será realizada
            aux = rand() % 100;
            int tipo_syscall = rand() % 5; // 0 - WriteCall, 1 - ReadCall, 2 - AddCall, 3 - RemCall, 4 - ListDirCall

            // Decide se vai usar o diretório Home (/Ax) ou o Público (/A0)
            int diretorio_usado = ((rand() % 100) < 20) ? 0 : my_id; // 20% de chance de usar A0
            char base_path[20];
            sprintf(base_path, "/A%d", diretorio_usado);

            printf("[A%d] Application com PID %d escolheu a SysCall do tipo %d no home /A%d\n", my_id, getpid(), tipo_syscall, diretorio_usado);

            sem_lock();
            appAtual->syscall.tipo_syscall = tipo_syscall;
            appAtual->syscall.novo = 1; // Indica que há uma nova syscall a ser processada

            // Variáveis auxiliares para construção dos caminhos
            char dir_name[50];
            char full_path[128];

            switch (tipo_syscall)
            {
            case 0: // WriteCall
                printf("Application com PID %d realizando WriteCall\n", getpid());

                // Preencher os dados da WriteCall
                snprintf(full_path, sizeof(full_path), "%s/%s.txt", base_path, nome_aleatorio());

                strncpy(appAtual->syscall.call.writecall.path, full_path, 128);
                appAtual->syscall.call.writecall.len = strlen(full_path);

                // Payload aleatório
                snprintf(appAtual->syscall.call.writecall.payload, 16, "DADO-A%d-%04d\n", my_id, rand() % 1000);
                appAtual->syscall.call.writecall.offset = offset_aleatorio();

                printf(" -> WRITE: %s (Path: %s) (Off: %d)\n", appAtual->syscall.call.writecall.payload, full_path, appAtual->syscall.call.writecall.offset);

                break;

            case 1: // ReadCall
                printf("Application com PID %d realizando ReadCall\n", getpid());

                // Preencher os dados da ReadCall
                snprintf(full_path, sizeof(full_path), "%s/%s.txt", base_path, nome_aleatorio());

                strncpy(appAtual->syscall.call.readcall.path, full_path, 128);
                appAtual->syscall.call.readcall.len = strlen(full_path);
                memset(appAtual->syscall.call.readcall.buffer, 0, sizeof(appAtual->syscall.call.readcall.buffer));
                appAtual->syscall.call.readcall.offset = offset_aleatorio();

                printf(" -> READ: %s (Off: %d)\n", full_path, appAtual->syscall.call.readcall.offset);

                break;

            case 2: // AddCall
                printf("Application com PID %d realizando AddCall\n", getpid());

                // Preencher os dados da AddCall
                strcpy(dir_name, nome_aleatorio());

                strncpy(appAtual->syscall.call.addcall.path, base_path, 128);
                strncpy(appAtual->syscall.call.addcall.dirname, dir_name, 64);

                appAtual->syscall.call.addcall.len1 = strlen(base_path);
                appAtual->syscall.call.addcall.len2 = strlen(dir_name);

                printf(" -> ADD DIR: %s/%s\n", base_path, dir_name);

                break;

            case 3: // RemCall
                printf("Application com PID %d realizando RemCall\n", getpid());

                // Preencher os dados da RemCall
                strcpy(dir_name, nome_aleatorio());

                strncpy(appAtual->syscall.call.remcall.path, base_path, 128);
                strncpy(appAtual->syscall.call.remcall.name, dir_name, 64);

                appAtual->syscall.call.remcall.len1 = strlen(base_path);
                appAtual->syscall.call.remcall.len2 = strlen(dir_name);

                printf(" -> REM: %s/%s\n", base_path, dir_name);

                break;

            case 4: // ListDirCall
                printf("Application com PID %d realizando ListDirCall\n", getpid());

                // Preencher os dados da ListDirCall
                snprintf(full_path, sizeof(full_path), "%s", base_path);

                strncpy(appAtual->syscall.call.listdircall.path, full_path, 128);
                appAtual->syscall.call.listdircall.len1 = strlen(full_path);
                appAtual->syscall.call.listdircall.nrnames = 0;

                printf(" -> LIST: %s\n", full_path);

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
