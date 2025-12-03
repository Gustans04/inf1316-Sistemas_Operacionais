#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "../includes/aux.h"

// Globais para controle do processo servidor
pid_t server_pid = -1;

// Protótipos
void start_server();
void stop_server();
void test_udp_write();
void test_udp_read();
void test_udp_add_dir();
void test_udp_list_dir();
void test_udp_remove();
void cleanup_resources();

// --- NOVA FUNÇÃO AUXILIAR ---
// Tenta receber resposta por até 2 segundos
CallRequest wait_for_response() {
    CallRequest res;
    int attempts = 0;
    // Tenta 20 vezes com pausa de 100ms = 2 segundos de timeout
    while (attempts < 20) {
        res = recebeUdpResponse();
        if (res.tipo_syscall != -1) {
            return res; // Chegou resposta válida!
        }
        usleep(100000); // Espera 100ms antes de tentar de novo
        attempts++;
    }
    return res; // Retorna o erro (-1) se estourar o tempo
}
// ----------------------------

int main()
{
    printf("=== Testando SFSS Server (Integração UDP) ===\n\n");

    // Limpeza prévia para garantir estado limpo
    system("rm -rf SFSS-root-dir");

    // 1. Iniciar o Servidor
    printf("0. Iniciando processo do servidor em background...\n");
    start_server();
    
    // Inicializa o cliente UDP (função do aux.c)
    iniciaUdpClient();
    printf("   ✓ Servidor rodando e cliente conectado\n\n");

    // 2. Teste WRITE
    printf("1. Testando WRITE via UDP...\n");
    test_udp_write();
    printf("   ✓ Resposta de Write recebida e arquivo verificado\n\n");

    // 3. Teste READ
    printf("2. Testando READ via UDP...\n");
    test_udp_read();
    printf("   ✓ Resposta de Read recebida com dados corretos\n\n");

    // 4. Teste ADD DIR
    printf("3. Testando ADD DIR via UDP...\n");
    test_udp_add_dir();
    printf("   ✓ Diretório criado via comando remoto\n\n");

    // 5. Teste LIST DIR
    printf("4. Testando LIST DIR via UDP...\n");
    test_udp_list_dir();
    printf("   ✓ Listagem recebida corretamente\n\n");

    // 6. Teste REMOVE (Feature Especial)
    printf("5. Testando REMOVE via UDP (Write com payload vazio)...\n");
    test_udp_remove();
    printf("   ✓ Arquivo removido remotamente\n\n");

    // Finalização
    stop_server();
    cleanup_resources();

    printf("=== Todos os testes do Server passaram! ===\n");
    return 0;
}

void start_server()
{
    server_pid = fork();
    if (server_pid < 0) {
        perror("Erro ao fazer fork do servidor");
        exit(EXIT_FAILURE);
    }

    if (server_pid == 0) {
        // Processo Filho: Executa o servidor real
        execl("../bin/sfss", "sfss", NULL);
        perror("Erro ao executar ../bin/sfss");
        exit(EXIT_FAILURE);
    }

    // Processo Pai: Espera um pouco para o servidor subir e dar bind na porta
    sleep(1); 
}

void stop_server()
{
    if (server_pid > 0) {
        kill(server_pid, SIGTERM);
        wait(NULL); 
        printf("   ✓ Servidor encerrado.\n");
    }
}

void test_udp_write()
{
    CallRequest req;
    memset(&req, 0, sizeof(CallRequest));

    // Configura requisição WRITE em /A1/remoto.txt
    req.tipo_syscall = 0; // Write
    req.owner = 1;        // User A1
    strcpy(req.call.writecall.path, "/A1/remoto.txt");
    req.call.writecall.len = strlen(req.call.writecall.path);
    strcpy(req.call.writecall.payload, "DadosUDP1234567");
    req.call.writecall.offset = 0;

    enviaUdpRequest(req);

    // MUDANÇA AQUI: Usa wait_for_response()
    CallRequest res = wait_for_response();

    // Verificações
    if (res.tipo_syscall == -1) {
        printf("Erro: Timeout - Servidor não respondeu\n");
        stop_server(); exit(EXIT_FAILURE);
    }
    if (res.tipo_syscall != 0) {
        printf("Erro: Tipo da resposta incorreto (%d)\n", res.tipo_syscall);
        stop_server(); exit(EXIT_FAILURE);
    }
    if (res.call.writecall.offset < 0) {
        printf("Erro: Servidor retornou erro no Write: %d\n", res.call.writecall.offset);
        stop_server(); exit(EXIT_FAILURE);
    }

    if (access("SFSS-root-dir/A1/remoto.txt", F_OK) == -1) {
        printf("Erro: O arquivo não foi criado fisicamente no disco!\n");
        stop_server(); exit(EXIT_FAILURE);
    }
}

void test_udp_read()
{
    CallRequest req;
    memset(&req, 0, sizeof(CallRequest));

    req.tipo_syscall = 1; // Read
    req.owner = 1;
    strcpy(req.call.readcall.path, "/A1/remoto.txt");
    req.call.readcall.len = strlen(req.call.readcall.path);
    req.call.readcall.offset = 0;

    enviaUdpRequest(req);

    // MUDANÇA AQUI
    CallRequest res = wait_for_response();

    if (res.tipo_syscall == -1) {
        printf("Erro: Timeout - Servidor não respondeu\n");
        stop_server(); exit(EXIT_FAILURE);
    }

    if (res.call.readcall.len <= 0) {
        printf("Erro: Leitura retornou 0 bytes ou erro\n");
        stop_server(); exit(EXIT_FAILURE);
    }

    if (strncmp(res.call.readcall.buffer, "DadosUDP1234567", 15) != 0) {
        printf("Erro: Conteúdo lido diferente do esperado. Recebido: %s\n", res.call.readcall.buffer);
        stop_server(); exit(EXIT_FAILURE);
    }
}

void test_udp_add_dir()
{
    CallRequest req;
    memset(&req, 0, sizeof(CallRequest));

    req.tipo_syscall = 2; // Add
    req.owner = 1;
    strcpy(req.call.addcall.path, "/A1");
    strcpy(req.call.addcall.dirname, "PastaRemota");
    
    enviaUdpRequest(req);
    
    // MUDANÇA AQUI
    CallRequest res = wait_for_response();

    if (res.tipo_syscall == -1) {
        printf("Erro: Timeout - Servidor não respondeu\n");
        stop_server(); exit(EXIT_FAILURE);
    }

    if (res.call.addcall.len1 < 0) {
        printf("Erro: Falha ao criar diretório remoto\n");
        stop_server(); exit(EXIT_FAILURE);
    }

    if (access("SFSS-root-dir/A1/PastaRemota", F_OK) == -1) {
        printf("Erro: Diretório físico não encontrado\n");
        stop_server(); exit(EXIT_FAILURE);
    }
}

void test_udp_list_dir()
{
    CallRequest req;
    memset(&req, 0, sizeof(CallRequest));

    req.tipo_syscall = 4; // List
    req.owner = 1;
    strcpy(req.call.listdircall.path, "/A1");
    
    enviaUdpRequest(req);
    
    // MUDANÇA AQUI
    CallRequest res = wait_for_response();

    if (res.tipo_syscall == -1) {
        printf("Erro: Timeout - Servidor não respondeu\n");
        stop_server(); exit(EXIT_FAILURE);
    }

    int qtd = (int)(long)res.call.listdircall.nrnames; 

    if (qtd < 2) { 
        printf("Erro: Listagem retornou menos itens que o esperado (%d)\n", qtd);
    }
    
    if (strstr(res.call.listdircall.alldirinfo, "remoto.txt") == NULL) {
        printf("Erro: Arquivo remoto.txt não apareceu na listagem\n");
        stop_server(); exit(EXIT_FAILURE);
    }
}

void test_udp_remove()
{
    CallRequest req;
    memset(&req, 0, sizeof(CallRequest));

    req.tipo_syscall = 0; // Write para remover
    req.owner = 1;
    strcpy(req.call.writecall.path, "/A1/remoto.txt");
    req.call.writecall.payload[0] = '\0'; // Vazio
    req.call.writecall.offset = 0;

    enviaUdpRequest(req);
    
    // MUDANÇA AQUI
    CallRequest res = wait_for_response();

    if (res.tipo_syscall == -1) {
        printf("Erro: Timeout - Servidor não respondeu\n");
        stop_server(); exit(EXIT_FAILURE);
    }

    if (res.call.writecall.offset != 0) {
        printf("Erro: Servidor retornou código de erro na remoção: %d\n", res.call.writecall.offset);
        stop_server(); exit(EXIT_FAILURE);
    }

    if (access("SFSS-root-dir/A1/remoto.txt", F_OK) != -1) {
        printf("Erro: Arquivo ainda existe no disco após comando de remoção!\n");
        stop_server(); exit(EXIT_FAILURE);
    }
}

void cleanup_resources()
{
    encerraUdpClient();
}
