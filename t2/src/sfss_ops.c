#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "sfss_ops.h"

// Função auxiliar para construir o caminho real no servidor
void build_real_path(char *buffer, int owner, const char *virtual_path)
{
    // Remove a barra inicial se houver, para evitar duplicação
    const char *clean_path = (virtual_path[0] == '/') ? virtual_path + 1 : virtual_path;
    snprintf(buffer, MAX_PATH_LEN, "%s/A%d/%s", ROOT_DIR, owner, clean_path);
}

// Implementa a lógica da mensagem RD-REQ/RD-REP
// Retorna: número de bytes lidos ou negativo em caso de erro
int sfss_read(int owner, char *path, int offset, char *buffer_out)
{
    char full_path[MAX_PATH_LEN];
    build_real_path(full_path, owner, path);

    // Abre o arquivo para leitura
    int fd = open(full_path, O_RDONLY);
    if (fd < 0)
        return -1; // Erro ao abrir 

    // Move o cursor para o offset desejado
    if (lseek(fd, offset, SEEK_SET) < 0)
    {
        close(fd);
        return -2; // Erro de seek 
    }

    // Lê 16 bytes (BLOCK_SIZE)
    int bytes_read = read(fd, buffer_out, BLOCK_SIZE);

    close(fd);
    return bytes_read; // Retorna quantos bytes leu (pode ser 0 se EOF)
}

// Implementa a lógica da mensagem WR-REQ/WR-REP 
// Retorna: offset confirmado ou negativo em caso de erro
int sfss_write(int owner, char *path, char *payload, int offset)
{
    char full_path[MAX_PATH_LEN];
    build_real_path(full_path, owner, path);
    struct stat st;

    // Tenta abrir R/W, cria se não existir
    int fd = open(full_path, O_RDWR | O_CREAT, 0666);
    if (fd < 0)
        return -1;

    // Verifica o tamanho atual do arquivo para preenchimento de lacunas 
    if (fstat(fd, &st) < 0)
    {
        close(fd);
        return -2;
    }

    // Se offset > tamanho atual, preencher com 0x20 (espaço)
    if (offset > st.st_size)
    {
        lseek(fd, st.st_size, SEEK_SET);
        int gap_size = offset - st.st_size;
        char *spaces = (char *)malloc(gap_size);
        memset(spaces, 0x20, gap_size); // 0x20 é o 'whitespace character'
        write(fd, spaces, gap_size);
        free(spaces);
    }

    // Posiciona e escreve o payload de 16 bytes
    lseek(fd, offset, SEEK_SET);
    int written = write(fd, payload, BLOCK_SIZE);

    close(fd);

    if (written < 0)
        return -3;
    return offset; // Retorna o offset confirmando a operação
}

// Implementa a lógica da mensagem DC-REQ/DC-REP
// Retorna: 0 sucesso, < 0 erro
int sfss_add(int owner, char *path, char *dirname)
{
    char full_path[MAX_PATH_LEN];
    char temp_path[MAX_PATH_LEN];

    // Constrói o path base
    build_real_path(temp_path, owner, path);

    // Concatena o novo diretório
    snprintf(full_path, MAX_PATH_LEN, "%s/%s", temp_path, dirname);

    if (mkdir(full_path, 0777) < 0)
    {
        return -1; // Erro 
    }
    return 0;
}

// Implementa a lógica da mensagem DR-REQ/DR-REP
// Retorna: 0 sucesso, < 0 erro
int sfss_rem(int owner, char *path, char *name)
{
    char full_path[MAX_PATH_LEN];
    char temp_path[MAX_PATH_LEN];
    struct stat st;

    build_real_path(temp_path, owner, path);
    snprintf(full_path, MAX_PATH_LEN, "%s/%s", temp_path, name);

    // Verifica se é arquivo ou diretório usando stat
    if (stat(full_path, &st) < 0)
        return -1;

    if (S_ISDIR(st.st_mode))
    {
        // É diretório: usa rmdir
        if (rmdir(full_path) < 0)
            return -2;
    }
    else
    {
        // É arquivo: usa unlink
        if (unlink(full_path) < 0)
            return -3;
    }
    return 0;
}

// Implementa a lógica da mensagem DL-REQ/DL-REP
// Retorna: número de nomes encontrados (nrnames) ou erro
// Saída: Preenche 'allfilenames' e o array 'positions' (fstlstpositions)
int sfss_listDir(int owner, char *path, char *allfilenames, struct IndexInfo *positions)
{
    char full_path[MAX_PATH_LEN];
    build_real_path(full_path, owner, path);

    DIR *d;
    struct dirent *dir;

    d = opendir(full_path); // Abre o diretório
    if (!d)
        return -1;

    int count = 0;
    int current_char_pos = 0;
    allfilenames[0] = '\0'; // Inicia string vazia

    while ((dir = readdir(d)) != NULL)
    {
        // Pula "." e ".." 
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
        {
            continue;
        }

        if (count >= 40)
            break; // Limite de nomes em um diretório

        int name_len = strlen(dir->d_name);

        // Concatena o nome na string gigante
        strcat(allfilenames, dir->d_name);

        // Preenche a estrutura de índices 
        positions[count].start = current_char_pos;
        positions[count].end = current_char_pos + name_len - 1; // inclusivo

        // Determina o tipo (F ou D)
        if (dir->d_type == DT_DIR)
        {
            positions[count].type = 2; // Diretório
        }
        else
        {
            positions[count].type = 1; // Arquivo
        }

        current_char_pos += name_len;
        count++;
    }
    closedir(d);
    return count; // Retorna nrnames
}
