#include <stdio.h>
#include <stdlib.h>

#include "sfss_ops.h" // Inclui suas funções de arquivo


int main()
{
    printf("Servidor SFSS iniciando...\n");

    // Cria root se a lógica permitir, ou use mkdir direto
    mkdir(ROOT_DIR, 0777);

    // Loop principal do servidor (futuro)
    while (1)
    {
        // 1. Receber mensagem UDP
        // 2. Parsear a mensagem (descobrir se é READ, WRITE, etc)
        // 3. Chamar a função correspondente de sfss_ops.c
        // 4. Responder ao cliente
        break; // Break para não travar o teste agora
    }

    return 0;
}