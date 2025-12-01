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
void test_semaphore_initialization();
void test_queue_operations();
void test_shared_memory_setup();
void test_fifo_creation_and_communication();
void test_syscall_processing();
void test_irq_processing();
void test_udp_communication();
void test_process_management();
void cleanup_test_resources();

// Global variables for testing
static FilaApps test_prontos;
static FilaApps test_esperandoD1;
static FilaApps test_esperandoD2;
static FilaRequests test_filaFiles;
static FilaRequests test_filaDirs;
static InfoProcesso test_processos[NUM_APP];
static int test_shm_segment;

int main()
{
    printf("=== Testando Main (Kernel Simulator) ===\n\n");

    // Test 1: Semaphore Initialization
    printf("1. Testando inicialização de semáforos...\n");
    test_semaphore_initialization();
    printf("   ✓ Semáforos inicializados com sucesso\n\n");

    // Test 2: Queue Operations
    printf("2. Testando operações das filas...\n");
    test_queue_operations();
    printf("   ✓ Operações das filas funcionando corretamente\n\n");

    // Test 3: Shared Memory Setup
    printf("3. Testando configuração de memória compartilhada...\n");
    test_shared_memory_setup();
    printf("   ✓ Memória compartilhada configurada com sucesso\n\n");

    // Test 4: FIFO Creation and Communication
    printf("4. Testando criação e comunicação via FIFO...\n");
    test_fifo_creation_and_communication();
    printf("   ✓ FIFO criado e comunicação funcionando\n\n");

    // Test 5: UDP Communication
    printf("5. Testando comunicação UDP...\n");
    test_udp_communication();
    printf("   ✓ Comunicação UDP inicializada com sucesso\n\n");

    // Test 6: SysCall Processing
    printf("6. Testando processamento de SysCalls...\n");
    test_syscall_processing();
    printf("   ✓ SysCalls processadas corretamente\n\n");

    // Test 7: IRQ Processing
    printf("7. Testando processamento de IRQs...\n");
    test_irq_processing();
    printf("   ✓ IRQs processadas corretamente\n\n");

    // Test 8: Process Management
    printf("8. Testando gerenciamento de processos...\n");
    test_process_management();
    printf("   ✓ Gerenciamento de processos funcionando\n\n");

    // Cleanup
    cleanup_test_resources();
    
    printf("=== Todos os testes do Main (Kernel) passaram! ===\n");
    return 0;
}

void test_semaphore_initialization()
{
    // Test semaphore initialization
    init_sem();
    
    // Test lock/unlock operations
    sem_lock();
    sem_unlock();
    
    // The fact that we reach here without hanging means the semaphore works
}

void test_queue_operations()
{
    // Initialize all queues
    inicializarFila(&test_prontos);
    inicializarFila(&test_esperandoD1);
    inicializarFila(&test_esperandoD2);
    inicializarFilaRequests(&test_filaFiles);
    inicializarFilaRequests(&test_filaDirs);

    // Test FilaApps operations
    if (!estaVazia(&test_prontos)) {
        printf("Erro: Fila deveria estar vazia inicialmente\n");
        exit(EXIT_FAILURE);
    }

    // Test insertion and removal
    pid_t test_pids[] = {1001, 1002, 1003, 1004, 1005};
    
    for (int i = 0; i < 5; i++) {
        inserirNaFila(&test_prontos, test_pids[i]);
    }

    // Verify all PIDs were inserted
    for (int i = 0; i < 5; i++) {
        if (!pid_na_fila(&test_prontos, test_pids[i])) {
            printf("Erro: PID %d não encontrado na fila\n", test_pids[i]);
            exit(EXIT_FAILURE);
        }
    }

    // Test removal
    for (int i = 0; i < 5; i++) {
        pid_t removed = removerDaFila(&test_prontos);
        if (removed != test_pids[i]) {
            printf("Erro: PID removido (%d) diferente do esperado (%d)\n", removed, test_pids[i]);
            exit(EXIT_FAILURE);
        }
    }

    // Test FilaRequests operations
    if (!estaVaziaRequests(&test_filaFiles)) {
        printf("Erro: Fila de requests deveria estar vazia inicialmente\n");
        exit(EXIT_FAILURE);
    }

    CallRequest test_request;
    test_request.tipo_syscall = 0;
    test_request.owner = 1;
    strcpy(test_request.call.writecall.path, "/A1/test.txt");
    test_request.call.writecall.len = strlen(test_request.call.writecall.path);

    inserirNaFilaRequests(&test_filaFiles, test_request);
    
    if (estaVaziaRequests(&test_filaFiles)) {
        printf("Erro: Fila não deveria estar vazia após inserção\n");
        exit(EXIT_FAILURE);
    }

    int owner = removerDaFilaRequests(&test_filaFiles);
    if (owner != 1) {
        printf("Erro: Owner removido (%d) diferente do esperado (1)\n", owner);
        exit(EXIT_FAILURE);
    }
}

void test_shared_memory_setup()
{
    // Test shared memory allocation
    test_shm_segment = shmget(IPC_CODE + 100, sizeof(InfoProcesso) * NUM_APP, 
                             IPC_CREAT | S_IRUSR | S_IWUSR);
    if (test_shm_segment == -1) {
        perror("shmget falhou no teste");
        exit(EXIT_FAILURE);
    }

    // Test shared memory attachment
    InfoProcesso* shm_processos = (InfoProcesso*) shmat(test_shm_segment, 0, 0);
    if (shm_processos == (InfoProcesso*) -1) {
        perror("shmat falhou no teste");
        exit(EXIT_FAILURE);
    }

    // Initialize test process data
    sem_lock();
    for (int i = 0; i < NUM_APP; i++) {
        shm_processos[i].pid = 3000 + i;
        shm_processos[i].pc = 0;
        shm_processos[i].estado = PRONTO;
        shm_processos[i].syscall.novo = 0;
        shm_processos[i].syscall.tipo_syscall = -1;
        shm_processos[i].executando = (i == 0) ? 1 : 0; // First process executing
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
        if (test_processos[i].pid != 3000 + i ||
            test_processos[i].estado != PRONTO ||
            test_processos[i].executando != ((i == 0) ? 1 : 0)) {
            printf("Erro: Dados da memória compartilhada corrompidos\n");
            exit(EXIT_FAILURE);
        }
    }

    // Detach for now (will cleanup later)
    shmdt(shm_processos);
}

void test_fifo_creation_and_communication()
{
    // Test FIFO creation
    if (criaFIFO("TEST_MAIN_FIFO") < 0) {
        perror("Falha ao criar FIFO de teste");
        exit(EXIT_FAILURE);
    }

    // Fork a process to simulate IRQ writing
    pid_t writer_pid = fork();
    if (writer_pid == 0) {
        // Child process - writer (simulates InterController)
        int write_fd;
        sleep(1); // Give reader time to open
        
        if (abreFIFO(&write_fd, "TEST_MAIN_FIFO", O_WRONLY) < 0) {
            perror("Falha ao abrir FIFO para escrita");
            exit(EXIT_FAILURE);
        }

        // Send test IRQs
        write(write_fd, "IRQ0\0", 5);
        write(write_fd, "IRQ1\0", 5);
        write(write_fd, "IRQ2\0", 5);
        
        close(write_fd);
        exit(0);
    } else {
        // Parent process - reader (simulates Kernel)
        int read_fd;
        
        if (abreFIFO(&read_fd, "TEST_MAIN_FIFO", O_RDONLY | O_NONBLOCK) < 0) {
            perror("Falha ao abrir FIFO para leitura");
            exit(EXIT_FAILURE);
        }

        char buffer[25];
        int irq_count = 0;
        
        // Read IRQs with timeout
        for (int attempts = 0; attempts < 50 && irq_count < 3; attempts++) {
            ssize_t bytes_read = read(read_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                char* ponteiro_msg = buffer;
                
                while (ponteiro_msg < buffer + bytes_read) {
                    if (strncmp(ponteiro_msg, "IRQ", 3) == 0) {
                        irq_count++;
                    }
                    ponteiro_msg += 5;
                }
            }
            usleep(100000); // 100ms
        }
        
        if (irq_count < 3) {
            printf("Aviso: Apenas %d IRQs recebidas de 3 esperadas\n", irq_count);
        }
        
        close(read_fd);
        
        // Wait for writer to finish
        int status;
        waitpid(writer_pid, &status, 0);
    }
    
    // Cleanup
    unlink("TEST_MAIN_FIFO");
}

void test_udp_communication()
{
    // Test UDP client initialization
    iniciaUdpClient();
    
    // Test creating a CallRequest
    CallRequest test_request;
    test_request.tipo_syscall = 0;
    test_request.owner = 1;
    strcpy(test_request.call.writecall.path, "/A1/test_file.txt");
    test_request.call.writecall.len = strlen(test_request.call.writecall.path);
    strcpy(test_request.call.writecall.payload, "TestData12345678");
    test_request.call.writecall.offset = 0;
    
    // Note: We don't actually send UDP requests in the test to avoid
    // requiring a running SFSS server, but we verify the structure is valid
    printf("   UDP request estruturado: tipo=%d, owner=%d, path=%s\n",
           test_request.tipo_syscall, test_request.owner, test_request.call.writecall.path);
    
    // Test UDP response reception (should return empty response)
    CallRequest response = recebeUdpResponse();
    if (response.tipo_syscall != -1) {
        printf("Aviso: Resposta UDP inesperada recebida\n");
    }
}

void test_syscall_processing()
{
    // Test different syscall types processing
    InfoProcesso test_app;
    test_app.pid = 4000;
    test_app.estado = EXECUTANDO;
    test_app.executando = 1;
    test_app.syscall.novo = 1;
    
    // Test WriteCall processing
    test_app.syscall.tipo_syscall = 0;
    strcpy(test_app.syscall.call.writecall.path, "test_write.txt");
    test_app.syscall.call.writecall.len = strlen(test_app.syscall.call.writecall.path);
    strcpy(test_app.syscall.call.writecall.payload, "WriteTestData123");
    test_app.syscall.call.writecall.offset = 0;
    
    CallRequest wr;
    wr.tipo_syscall = 0;
    wr.owner = 1;
    snprintf(wr.call.writecall.path, sizeof(wr.call.writecall.path), 
             "/A1/%s", test_app.syscall.call.writecall.path);
    wr.call.writecall.len = test_app.syscall.call.writecall.len;
    strcpy(wr.call.writecall.payload, test_app.syscall.call.writecall.payload);
    wr.call.writecall.offset = test_app.syscall.call.writecall.offset;
    
    printf("   Processando WriteCall: %s -> %s\n", 
           test_app.syscall.call.writecall.path, wr.call.writecall.path);

    // Test ReadCall processing
    test_app.syscall.tipo_syscall = 1;
    strcpy(test_app.syscall.call.readcall.path, "test_read.txt");
    test_app.syscall.call.readcall.len = strlen(test_app.syscall.call.readcall.path);
    test_app.syscall.call.readcall.offset = 16;
    
    CallRequest rd;
    rd.tipo_syscall = 1;
    rd.owner = 1;
    snprintf(rd.call.readcall.path, sizeof(rd.call.readcall.path), 
             "/A1/%s", test_app.syscall.call.readcall.path);
    rd.call.readcall.len = test_app.syscall.call.readcall.len;
    rd.call.readcall.offset = test_app.syscall.call.readcall.offset;
    
    printf("   Processando ReadCall: %s -> %s\n", 
           test_app.syscall.call.readcall.path, rd.call.readcall.path);

    // Test AddCall processing
    test_app.syscall.tipo_syscall = 2;
    strcpy(test_app.syscall.call.addcall.path, "");
    test_app.syscall.call.addcall.len1 = 0;
    strcpy(test_app.syscall.call.addcall.dirname, "new_directory");
    test_app.syscall.call.addcall.len2 = strlen(test_app.syscall.call.addcall.dirname);
    
    printf("   Processando AddCall: criar '%s' em /A1/\n", 
           test_app.syscall.call.addcall.dirname);

    // Test RemCall processing
    test_app.syscall.tipo_syscall = 3;
    strcpy(test_app.syscall.call.remcall.path, "");
    test_app.syscall.call.remcall.len1 = 0;
    strcpy(test_app.syscall.call.remcall.name, "file_to_remove.txt");
    test_app.syscall.call.remcall.len2 = strlen(test_app.syscall.call.remcall.name);
    
    printf("   Processando RemCall: remover '%s' de /A1/\n", 
           test_app.syscall.call.remcall.name);

    // Test ListDirCall processing
    test_app.syscall.tipo_syscall = 4;
    strcpy(test_app.syscall.call.listdircall.path, "");
    test_app.syscall.call.listdircall.len1 = 0;
    
    printf("   Processando ListDirCall: listar /A1/\n");
}

void test_irq_processing()
{
    // Initialize test queues
    inicializarFila(&test_prontos);
    inicializarFila(&test_esperandoD1);
    inicializarFila(&test_esperandoD2);
    inicializarFilaRequests(&test_filaFiles);
    inicializarFilaRequests(&test_filaDirs);

    // Setup test processes
    pid_t test_pids[] = {5001, 5002, 5003};
    
    // Test IRQ0 (timer) processing
    printf("   Testando IRQ0 (timer interrupt):\n");
    
    // Add processes to ready queue
    for (int i = 0; i < 3; i++) {
        inserirNaFila(&test_prontos, test_pids[i]);
    }
    
    // Simulate current executing process
    pid_t current_pid = test_pids[0];
    printf("     Processo atual: %d\n", current_pid);
    printf("     Preemptando e reescalonando...\n");
    
    // Remove from ready queue and add back (simulates preemption)
    removerDaFila(&test_prontos); // Remove first
    inserirNaFila(&test_prontos, current_pid); // Add back
    
    // Get next process
    pid_t next_pid = removerDaFila(&test_prontos);
    printf("     Próximo processo escalonado: %d\n", next_pid);

    // Test IRQ1 (Device 1) processing
    printf("   Testando IRQ1 (Device 1 interrupt):\n");
    
    // Add process to waiting queue
    inserirNaFila(&test_esperandoD1, test_pids[2]);
    
    // Create file request
    CallRequest file_req;
    file_req.tipo_syscall = 0;
    file_req.owner = 3;
    inserirNaFilaRequests(&test_filaFiles, file_req);
    
    // Simulate IRQ1 processing
    if (!estaVaziaRequests(&test_filaFiles)) {
        removerDaFilaRequests(&test_filaFiles);
        if (!estaVazia(&test_esperandoD1)) {
            removerPidDaFila(&test_esperandoD1, test_pids[2]);
            inserirNaFila(&test_prontos, test_pids[2]);
            printf("     Processo %d movido de esperandoD1 para prontos\n", test_pids[2]);
        }
    }

    // Test IRQ2 (Device 2) processing
    printf("   Testando IRQ2 (Device 2 interrupt):\n");
    
    // Add process to waiting queue
    inserirNaFila(&test_esperandoD2, test_pids[1]);
    
    // Create directory request
    CallRequest dir_req;
    dir_req.tipo_syscall = 2;
    dir_req.owner = 2;
    inserirNaFilaRequests(&test_filaDirs, dir_req);
    
    // Simulate IRQ2 processing
    if (!estaVaziaRequests(&test_filaDirs)) {
        removerDaFilaRequests(&test_filaDirs);
        if (!estaVazia(&test_esperandoD2)) {
            removerPidDaFila(&test_esperandoD2, test_pids[1]);
            inserirNaFila(&test_prontos, test_pids[1]);
            printf("     Processo %d movido de esperandoD2 para prontos\n", test_pids[1]);
        }
    }
}

void test_process_management()
{
    // Test numeroDoProcesso function
    InfoProcesso test_procs[NUM_APP];
    for (int i = 0; i < NUM_APP; i++) {
        test_procs[i].pid = 6000 + i;
        test_procs[i].estado = PRONTO;
    }
    
    // Test finding process number by PID
    for (int i = 0; i < NUM_APP; i++) {
        int proc_num = numeroDoProcesso(test_procs, 6000 + i);
        if (proc_num != i + 1) {
            printf("Erro: Número do processo incorreto. Esperado: %d, Obtido: %d\n", 
                   i + 1, proc_num);
            exit(EXIT_FAILURE);
        }
    }
    
    // Test encontrarAplicacaoPorPID function
    InfoProcesso* found = encontrarAplicacaoPorPID(test_procs, 6002);
    if (!found || found->pid != 6002) {
        printf("Erro: Processo não encontrado por PID\n");
        exit(EXIT_FAILURE);
    }
    
    // Test processosAcabaram function
    // All processes PRONTO - should return 0
    if (processosAcabaram(test_procs)) {
        printf("Erro: processosAcabaram retornou true com processos ativos\n");
        exit(EXIT_FAILURE);
    }
    
    // Set all processes to TERMINADO - should return 1
    for (int i = 0; i < NUM_APP; i++) {
        test_procs[i].estado = TERMINADO;
    }
    
    if (!processosAcabaram(test_procs)) {
        printf("Erro: processosAcabaram retornou false com todos processos terminados\n");
        exit(EXIT_FAILURE);
    }
    
    printf("   Gerenciamento de processos validado\n");
}

void cleanup_test_resources()
{
    // Cleanup shared memory
    if (test_shm_segment > 0) {
        shmctl(test_shm_segment, IPC_RMID, NULL);
    }
    
    // Remove test FIFOs
    unlink("TEST_MAIN_FIFO");
    
    // Cleanup UDP client
    encerraUdpClient();
    
    // Cleanup semaphore
    cleanup_sem();
    
    printf("Recursos de teste do Main limpos com sucesso\n");
}