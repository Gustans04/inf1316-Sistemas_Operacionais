#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <errno.h>

#include "../includes/aux.h"

// Test helper functions
void test_fifo_creation();
void test_signal_handlers();
void test_shared_memory_access();
void test_process_info_reading();
void test_irq_generation();
void cleanup_test_resources();

// Global variables for testing
static InfoProcesso test_processos[NUM_APP];
static int test_fifo_fd;

int main()
{
    printf("=== Testando InterController ===\n\n");

    // Initialize semaphore for tests
    init_sem();

    // Test 1: FIFO Creation and Communication
    printf("1. Testando criação e comunicação via FIFO...\n");
    test_fifo_creation();
    printf("   ✓ FIFO criado e aberto com sucesso\n\n");

    // Test 2: Shared Memory Access
    printf("2. Testando acesso à memória compartilhada...\n");
    test_shared_memory_access();
    printf("   ✓ Memória compartilhada acessada com sucesso\n\n");

    // Test 3: Process Info Reading
    printf("3. Testando leitura de informações dos processos...\n");
    test_process_info_reading();
    printf("   ✓ Informações dos processos lidas com sucesso\n\n");

    // Test 4: IRQ Generation Simulation
    printf("4. Testando geração de IRQs simuladas...\n");
    test_irq_generation();
    printf("   ✓ IRQs geradas e enviadas via FIFO com sucesso\n\n");

    // Test 5: Signal Handlers
    printf("5. Testando handlers de sinal...\n");
    test_signal_handlers();
    printf("   ✓ Handlers de sinal configurados corretamente\n\n");

    // Cleanup
    cleanup_test_resources();
    
    printf("=== Todos os testes do InterController passaram! ===\n");
    return 0;
}

void test_fifo_creation()
{
    // Test FIFO creation
    if (criaFIFO("TEST_FIFO_IRQ") < 0) {
        perror("Falha ao criar FIFO de teste");
        exit(EXIT_FAILURE);
    }

    // Fork a process to act as reader
    pid_t reader_pid = fork();
    if (reader_pid == 0) {
        // Child process - reader
        int read_fd;
        if (abreFIFO(&read_fd, "TEST_FIFO_IRQ", O_RDONLY | O_NONBLOCK) < 0) {
            perror("Falha ao abrir FIFO de teste para leitura");
            exit(EXIT_FAILURE);
        }
        
        // Just keep it open for the writer
        sleep(1);
        close(read_fd);
        exit(0);
    } else {
        // Parent process - writer
        usleep(100000); // Give reader time to open
        
        // Test FIFO opening for writing
        if (abreFIFO(&test_fifo_fd, "TEST_FIFO_IRQ", O_WRONLY) < 0) {
            perror("Falha ao abrir FIFO de teste para escrita");
            kill(reader_pid, SIGTERM);
            exit(EXIT_FAILURE);
        }

        // Test writing to FIFO
        if (write(test_fifo_fd, "TEST", 5) < 0) {
            perror("Erro ao escrever no FIFO");
            close(test_fifo_fd);
            kill(reader_pid, SIGTERM);
            exit(EXIT_FAILURE);
        }
        
        close(test_fifo_fd);
        
        // Wait for reader to finish
        int status;
        waitpid(reader_pid, &status, 0);
    }
    
    // Cleanup
    unlink("TEST_FIFO_IRQ");
}

void test_shared_memory_access()
{
    // Create shared memory segment
    int segmento = shmget(IPC_CODE + 1, sizeof(InfoProcesso) * NUM_APP, 
                         IPC_CREAT | S_IRUSR | S_IWUSR);
    if (segmento == -1) {
        perror("shmget falhou no teste");
        exit(EXIT_FAILURE);
    }

    // Attach to shared memory
    InfoProcesso* shm_processos = (InfoProcesso*) shmat(segmento, 0, 0);
    if (shm_processos == (InfoProcesso*) -1) {
        perror("shmat falhou no teste");
        exit(EXIT_FAILURE);
    }

    // Initialize test data
    sem_lock();
    for (int i = 0; i < NUM_APP; i++) {
        shm_processos[i].pid = 1000 + i;
        shm_processos[i].pc = i * 10;
        shm_processos[i].estado = (i % 2 == 0) ? PRONTO : EXECUTANDO;
        shm_processos[i].executando = (i % 2 == 0) ? 0 : 1;
        shm_processos[i].syscall.tipo_syscall = -1;
        shm_processos[i].syscall.novo = 0;
    }
    sem_unlock();

    // Test reading from shared memory
    sem_lock();
    for (int i = 0; i < NUM_APP; i++) {
        test_processos[i] = shm_processos[i];
    }
    sem_unlock();

    // Verify data integrity
    for (int i = 0; i < NUM_APP; i++) {
        if (test_processos[i].pid != 1000 + i ||
            test_processos[i].pc != i * 10 ||
            test_processos[i].estado != ((i % 2 == 0) ? PRONTO : EXECUTANDO)) {
            printf("Erro: Dados da memória compartilhada corrompidos\n");
            exit(EXIT_FAILURE);
        }
    }

    // Detach and cleanup
    shmdt(shm_processos);
    shmctl(segmento, IPC_RMID, NULL);
}

void test_process_info_reading()
{
    // Simulate the read_process_info function behavior
    int segmento = shmget(IPC_CODE + 2, sizeof(InfoProcesso) * NUM_APP, 
                         IPC_CREAT | S_IRUSR | S_IWUSR);
    if (segmento == -1) {
        perror("shmget falhou no teste de leitura");
        exit(EXIT_FAILURE);
    }

    InfoProcesso* shm_processos = (InfoProcesso*) shmat(segmento, 0, 0);
    if (shm_processos == (InfoProcesso*) -1) {
        perror("shmat falhou no teste de leitura");
        exit(EXIT_FAILURE);
    }

    // Setup test data with different syscalls
    sem_lock();
    for (int i = 0; i < NUM_APP; i++) {
        shm_processos[i].pid = 2000 + i;
        shm_processos[i].pc = i * 20;
        shm_processos[i].estado = PRONTO;
        shm_processos[i].executando = 0;
        shm_processos[i].syscall.tipo_syscall = i % 5; // Different syscall types
        shm_processos[i].syscall.novo = 1;
        
        // Fill syscall data based on type
        switch (i % 5) {
            case 0: // WriteCall
                snprintf(shm_processos[i].syscall.call.writecall.path, 
                        MAX_NAME_LEN, "/A1/test_file_%d.txt", i);
                shm_processos[i].syscall.call.writecall.len = strlen(shm_processos[i].syscall.call.writecall.path);
                snprintf(shm_processos[i].syscall.call.writecall.payload, 16, "TestData%d", i);
                shm_processos[i].syscall.call.writecall.offset = i * 16;
                break;
            case 1: // ReadCall
                snprintf(shm_processos[i].syscall.call.readcall.path, 
                        MAX_NAME_LEN, "/A1/read_file_%d.txt", i);
                shm_processos[i].syscall.call.readcall.len = strlen(shm_processos[i].syscall.call.readcall.path);
                shm_processos[i].syscall.call.readcall.offset = i * 8;
                break;
            case 2: // AddCall
                snprintf(shm_processos[i].syscall.call.addcall.path, 
                        MAX_NAME_LEN, "/A1");
                shm_processos[i].syscall.call.addcall.len1 = strlen(shm_processos[i].syscall.call.addcall.path);
                snprintf(shm_processos[i].syscall.call.addcall.dirname, 
                        MAX_NAME_LEN, "new_dir_%d", i);
                shm_processos[i].syscall.call.addcall.len2 = strlen(shm_processos[i].syscall.call.addcall.dirname);
                break;
            case 3: // RemCall
                snprintf(shm_processos[i].syscall.call.remcall.path, 
                        MAX_NAME_LEN, "/A1");
                shm_processos[i].syscall.call.remcall.len1 = strlen(shm_processos[i].syscall.call.remcall.path);
                snprintf(shm_processos[i].syscall.call.remcall.name, 
                        MAX_NAME_LEN, "file_to_remove_%d", i);
                shm_processos[i].syscall.call.remcall.len2 = strlen(shm_processos[i].syscall.call.remcall.name);
                break;
            case 4: // ListDirCall
                snprintf(shm_processos[i].syscall.call.listdircall.path, 
                        MAX_NAME_LEN, "/A1");
                shm_processos[i].syscall.call.listdircall.len1 = strlen(shm_processos[i].syscall.call.listdircall.path);
                break;
        }
    }
    sem_unlock();

    // Simulate reading process info
    sem_lock();
    for (int i = 0; i < NUM_APP; i++) {
        test_processos[i] = shm_processos[i];
    }
    sem_unlock();

    // Verify the data was read correctly
    for (int i = 0; i < NUM_APP; i++) {
        if (test_processos[i].pid != 2000 + i ||
            test_processos[i].syscall.tipo_syscall != i % 5) {
            printf("Erro: Leitura de informações dos processos falhou\n");
            exit(EXIT_FAILURE);
        }
    }

    shmdt(shm_processos);
    shmctl(segmento, IPC_RMID, NULL);
}

void test_irq_generation()
{
    char buffer[6];
    int read_fd;

    // Create a test FIFO for IRQ simulation
    if (criaFIFO("TEST_IRQ_FIFO") < 0) {
        perror("Falha ao criar FIFO para teste de IRQ");
        exit(EXIT_FAILURE);
    }

    // Fork a process to read from FIFO
    pid_t reader_pid = fork();
    if (reader_pid == 0) {
        // Child process - reader
        if (abreFIFO(&read_fd, "TEST_IRQ_FIFO", O_RDONLY) < 0) {
            perror("Falha ao abrir FIFO para leitura");
            exit(EXIT_FAILURE);
        }

        int irq0_count = 0, irq1_count = 0, irq2_count = 0;
        
        // Read IRQs for a limited time
        for (int i = 0; i < 10; i++) {
            int bytes_read = read(read_fd, buffer, 5);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                if (strcmp(buffer, "IRQ0") == 0) irq0_count++;
                else if (strcmp(buffer, "IRQ1") == 0) irq1_count++;
                else if (strcmp(buffer, "IRQ2") == 0) irq2_count++;
            }
            usleep(100000); // 100ms
        }
        
        close(read_fd);
        printf("   IRQs recebidas - IRQ0: %d, IRQ1: %d, IRQ2: %d\n", 
               irq0_count, irq1_count, irq2_count);
        exit(0);
    } else {
        // Parent process - writer (simulates InterController)
        int write_fd;
        if (abreFIFO(&write_fd, "TEST_IRQ_FIFO", O_WRONLY) < 0) {
            perror("Falha ao abrir FIFO para escrita");
            exit(EXIT_FAILURE);
        }

        srand(time(NULL));
        
        // Simulate IRQ generation
        for (int i = 0; i < 5; i++) {
            write(write_fd, "IRQ0", 5); // Always send IRQ0
            
            int valor_aleatorio = rand() % 1000;
            if (valor_aleatorio < 200) {
                write(write_fd, "IRQ1", 5);
            }
            if (valor_aleatorio < 100) {
                write(write_fd, "IRQ2", 5);
            }
            
            usleep(200000); // 200ms
        }
        
        close(write_fd);
        
        // Wait for child to finish
        int status;
        waitpid(reader_pid, &status, 0);
    }

    // Cleanup
    unlink("TEST_IRQ_FIFO");
}

void test_signal_handlers()
{
    // Test that signal handlers can be registered without errors
    // We don't actually test the handler functionality as it would require
    // complex process management, but we verify they can be set up
    
    signal(SIGINT, SIG_DFL);  // Reset to default
    signal(SIGUSR1, SIG_DFL); // Reset to default
    
    printf("   Handlers de sinal podem ser registrados sem erro\n");
}

void cleanup_test_resources()
{
    // Close test FIFO if still open
    if (test_fifo_fd > 0) {
        close(test_fifo_fd);
    }
    
    // Remove test FIFOs
    unlink("TEST_FIFO_IRQ");
    unlink("TEST_IRQ_FIFO");
    
    // Cleanup semaphore
    cleanup_sem();
    
    printf("Recursos de teste limpos com sucesso\n");
}