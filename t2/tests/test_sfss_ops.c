#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "sfss_ops.h"
#include "aux.h"

// Protótipos das funções de teste
void setup_test_environment();
void test_write_operation();
void test_read_operation();
void test_add_dir_operation();
void test_rem_dir_operation();
void test_list_dir_operation();
void test_special_remove_feature();
void cleanup_test_resources();

// Variáveis globais de apoio
static char buffer_leitura[BLOCK_SIZE + 1];
static char listagem_nomes[1024];
static IndexInfo listagem_idx[40];

int main()
{
    printf("=== Testando SFSS Ops (File System Server) ===\n\n");

    // Configuração Inicial
    printf("0. Configurando ambiente de teste...\n");
    setup_test_environment();
    printf("   ✓ Diretórios base criados com sucesso\n\n");

    // Teste 1: Escrita de Arquivo
    printf("1. Testando operação de escrita (WRITE)...\n");
    test_write_operation();
    printf("   ✓ Arquivo escrito com sucesso\n\n");

    // Teste 2: Leitura de Arquivo
    printf("2. Testando operação de leitura (READ)...\n");
    test_read_operation();
    printf("   ✓ Conteúdo lido e verificado com sucesso\n\n");

    // Teste 3: Criação de Diretório
    printf("3. Testando criação de subdiretório (ADD)...\n");
    test_add_dir_operation();
    printf("   ✓ Subdiretório criado com sucesso\n\n");

    // Teste 4: Remoção de Diretório
    printf("4. Testando remoção de subdiretório (REM)...\n");
    test_rem_dir_operation();
    printf("   ✓ Subdiretório removido com sucesso\n\n");

    // Teste 5: Listagem de Diretório
    printf("5. Testando listagem de diretório (LIST)...\n");
    test_list_dir_operation();
    printf("   ✓ Listagem retornou os itens esperados\n\n");

    // Teste 6: Remoção de Arquivo com Write)
    printf("6. Testando remoção de arquivo com Write... \n");
    test_special_remove_feature();
    printf("   ✓ Arquivo removido via WriteCall com sucesso\n\n");

    // Limpeza Final
    cleanup_test_resources();

    printf("=== Todos os testes do SFSS passaram! ===\n");
    return 0;
}

// --- Implementação das Funções de Teste ---

void setup_test_environment()
{
    // Cria a raiz
    mkdir(ROOT_DIR, 0777);
    mkdir(ROOT_DIR "/A1", 0777);
}

void test_write_operation()
{
    char data[] = "OlaMundo12345678"; // 16 bytes exatos

    // Tenta escrever "teste.txt" na raiz do usuário 1 (/A1)
    int res = sfss_write(1, "teste.txt", data, 0);

    if (res < 0)
    {
        printf("Erro: Falha na escrita. Código: %d\n", res);
        exit(EXIT_FAILURE);
    }

    // Verifica se o offset retornado é o esperado (0 neste caso)
    if (res != 0)
    {
        printf("Erro: Offset de retorno incorreto. Esperado: 0, Recebido: %d\n", res);
        exit(EXIT_FAILURE);
    }
}

void test_read_operation()
{
    char expected[] = "OlaMundo12345678";

    // Tenta ler o arquivo que acabamos de escrever
    int res = sfss_read(1, "teste.txt", 0, buffer_leitura);

    if (res < 0)
    {
        printf("Erro: Falha na leitura. Código: %d\n", res);
        exit(EXIT_FAILURE);
    }

    // Adiciona terminador nulo para comparação de string segura
    buffer_leitura[res] = '\0';

    if (strcmp(buffer_leitura, expected) != 0)
    {
        printf("Erro: Conteúdo lido incorreto.\n   Esperado: '%s'\n   Recebido: '%s'\n", expected, buffer_leitura);
        exit(EXIT_FAILURE);
    }
}

void test_add_dir_operation()
{
    // Cria um subdiretório "MeusDocs"
    int res = sfss_add(1, "", "MeusDocs");

    if (res < 0)
    {
        printf("Erro: Falha ao criar diretório. Código: %d\n", res);
        exit(EXIT_FAILURE);
    }

    // Cria outro para testar listagem depois
    sfss_add(1, "", "Fotos");
}

void test_rem_dir_operation()
{
    // Remove o diretório "Fotos" criado acima
    int res = sfss_rem(1, "", "Fotos");

    if (res < 0)
    {
        printf("Erro: Falha ao remover diretório. Código: %d\n", res);
        exit(EXIT_FAILURE);
    }

    // Tenta remover algo que não existe (deve falhar)
    int res_fail = sfss_rem(1, "", "Inexistente");
    if (res_fail == 0)
    {
        printf("Erro: Removeu um diretório inexistente sem acusar erro.\n");
        exit(EXIT_FAILURE);
    }
}

void test_list_dir_operation()
{
    // Lista a raiz do A1. Deve conter: "teste.txt" e "MeusDocs"
    int count = sfss_listDir(1, "", listagem_nomes, listagem_idx);

    if (count < 2)
    {
        printf("Erro: Número de itens listados (%d) menor que o esperado (>=2).\n", count);
        exit(EXIT_FAILURE);
    }

    int found_file = 0;
    int found_dir = 0;

    // Varre os resultados procurando nossos arquivos
    for (int i = 0; i < count; i++)
    {
        char tempName[256];
        int len = listagem_idx[i].end - listagem_idx[i].start + 1;

        strncpy(tempName, listagem_nomes + listagem_idx[i].start, len);
        tempName[len] = '\0';

        // Verifica se achou os arquivos criados nos testes anteriores
        if (strcmp(tempName, "teste.txt") == 0 && listagem_idx[i].type == 1)
            found_file = 1;
        if (strcmp(tempName, "MeusDocs") == 0 && listagem_idx[i].type == 2)
            found_dir = 1;

        
    }

    // Cria a struct temporária só para imprimir
    ListDirCall temp_list;
    memcpy(temp_list.alldirinfo, listagem_nomes, 1024);
    memcpy(temp_list.fstlstpositions, listagem_idx, sizeof(listagem_idx));
    temp_list.nrnames = count;

    imprimir_lista_diretorio(&temp_list);

    if (!found_file || !found_dir)
    {
        printf("Erro: Listagem incompleta. Arquivo encontrado? %s. Diretório encontrado? %s.\n",
               found_file ? "Sim" : "Não", found_dir ? "Sim" : "Não");
        exit(EXIT_FAILURE);
    }
}

void test_special_remove_feature()
{
    // Tenta remover "teste.txt" enviando payload vazio e offset 0
    int res = sfss_write(1, "teste.txt", "", 0);

    if (res != 0)
    {
        printf("Erro: Falha na remoção especial via Write. Código: %d\n", res);
        exit(EXIT_FAILURE);
    }

    // Tenta ler o arquivo (deve falhar)
    int check = sfss_read(1, "teste.txt", 0, buffer_leitura);

    if (check >= 0)
    {
        printf("Erro: O arquivo 'teste.txt' ainda existe após remoção!\n");
        exit(EXIT_FAILURE);
    }
}

void cleanup_test_resources()
{
    printf("Recursos de teste do SFSS marcados para limpeza (via make clean)\n");
}
