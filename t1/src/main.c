#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() 
{
    int pid_list[5];
    int inter_pid;

    // Lista de processos esperando pelos dispositivos
    int esperandoD1[5];
    int esperandoD2[5];

    if ((inter_pid = fork()) < 0) {
        perror("Fork falhou!");
        exit(EXIT_FAILURE);
    } else if (inter_pid == 0) {
        // InterController 
        printf("InterController com PID %d\n", getpid());
        exit(0);
    }

    for (int i = 0; i < 5; i++) {
        pid_list[i] = fork();
        if (pid_list[i] < 0) {
            perror("Fork falhou!");
            exit(EXIT_FAILURE);
        } else if (pid_list[i] == 0) {
            // Applications
            execl("./application", "", NULL);
            fprintf(stderr, "Erro ao rodar a Application");
            exit(-1);
        }
    }

    // Kernel
    printf("Kernel com PID %d\n", getpid());

    for(int i = 0; i < 6; i++) wait(NULL);
    
    return 0;
}
