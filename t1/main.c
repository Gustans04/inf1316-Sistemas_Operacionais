#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    int pid_list[5];
    int inter_pid;

    if ((inter_pid = fork()) < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else if (inter_pid == 0) {
        // InterController process
        printf("InterController process with PID %d\n", getpid());
        exit(0);
    }

    for (int i = 0; i < 5; i++) {
        pid_list[i] = fork();
        if (pid_list[i] < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else if (pid_list[i] == 0) {
            // Child process
            printf("Child process %d with PID %d\n", i + 1, getpid());
            exit(0);
        }
    }

    // Kernel process
    printf("Kernel process with PID %d\n", getpid());
    
    return 0;
}
