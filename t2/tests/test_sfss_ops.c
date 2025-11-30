#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "sfss_ops.h"

int main()
{
    // Cria diretórios base para teste
    mkdir(ROOT_DIR, 0777);
    mkdir(ROOT_DIR "/A1", 0777);

    printf("--- Testando SFSS Ops ---\n");

    // Teste WRITE
    char data[] = "OlaMundo12345678"; // 16 bytes
    printf("1. Escrevendo em arquivo...\n");
    int res_wr = sfss_write(1, "teste.txt", data, 0);
    printf("Resultado Write: %d\n", res_wr);

    // Teste READ
    char buffer[17];
    printf("2. Lendo arquivo...\n");
    int res_rd = sfss_read(1, "teste.txt", 0, buffer);
    if (res_rd > 0)
    {
        buffer[res_rd] = '\0';
        printf("Lido: %s\n", buffer);
    }
    else
    {
        printf("Erro leitura: %d\n", res_rd);
    }

    // Teste ADD DIR
    printf("3. Criando subdiretorio 'SubDir'...\n");
    sfss_add(1, "", "SubDir");

    // Teste LIST DIR
    char names[1024];
    struct IndexInfo idx[40];
    printf("4. Listando diretorio raiz de A1...\n");
    int count = sfss_listDir(1, "", names, idx);
    printf("Itens encontrados: %d\n", count);

    // Mostra a listagem
    // Como 'names' é uma string contínua, precisa-se usar os índices para imprimir separado
    for (int i = 0; i < count; i++)
    {
        char tempName[256];
        int len = idx[i].end - idx[i].start + 1;
        strncpy(tempName, names + idx[i].start, len);
        tempName[len] = '\0';
        printf(" - [%s] Tipo: %s\n", tempName, (idx[i].type == 2 ? "DIR" : "ARQ"));
    }

    return 0;
}
