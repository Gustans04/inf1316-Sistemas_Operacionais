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
#include "aux.h"

// Função auxiliar para construir o caminho real no servidor
void build_real_path(char *buffer, int owner, const char *virtual_path)
{
    // Acesso ao diretório público /A0
    if (strncmp(virtual_path, "/A0", 3) == 0) 
    {
        // Mapeia direto para SFSS-root-dir/A0/...
        snprintf(buffer, MAX_PATH_LEN, "%s%s", ROOT_DIR, virtual_path);
        return;
    }

    // Monta o prefixo do dono atual (ex: "/A1")
    char owner_prefix[16];
    snprintf(owner_prefix, sizeof(owner_prefix), "/A%d", owner);
    int prefix_len = strlen(owner_prefix);

    // Verifica se o path JÁ começa com o home do dono (ex: /A1/arquivo.txt) para evitar a duplicação (/A1/A1/arquivo.txt)
    if (strncmp(virtual_path, owner_prefix, prefix_len) == 0)
    {
        snprintf(buffer, MAX_PATH_LEN, "%s%s", ROOT_DIR, virtual_path);
        return;
    }

    // Caminho relativo (ex: "teste.txt")
    // Adiciona o prefixo do dono automaticamente
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

    // Se offset é 0 e payload é vazio (primeiro byte nulo), o arquivo é removido
    if (offset == 0 && (payload == NULL || payload[0] == '\0')) 
    {
        printf("SFSS: Removendo arquivo via WriteCall (Payload vazio e Offset 0)\n");
        if (unlink(full_path) < 0) 
        {
            return -4; // Erro ao remover
        }
        return 0; // Sucesso
    }

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
        if (spaces) 
        {
            memset(spaces, 0x20, gap_size);
            write(fd, spaces, gap_size);
            free(spaces);
        }
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

    // Constrói o path base onde o diretório será criado
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
        if (rmdir(full_path) < 0)
            return -2;
    }
    else
    {
        if (unlink(full_path) < 0)
            return -3;
    }
    return 0;
}

// Implementa a lógica da mensagem DL-REQ/DL-REP
// Retorna: número de nomes encontrados (nrnames) ou erro
int sfss_listDir(int owner, char *path, char *allfilenames, IndexInfo *positions)
{
    char full_path[MAX_PATH_LEN];
    build_real_path(full_path, owner, path);

    DIR *d;
    struct dirent *dir;

    d = opendir(full_path); 
    if (!d)
        return -1;

    int count = 0;
    int current_char_pos = 0;
    allfilenames[0] = '\0'; 

    while ((dir = readdir(d)) != NULL)
    {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
        {
            continue;
        }

        if (count >= 40)
            break; 

        int name_len = strlen(dir->d_name);

        strcat(allfilenames, dir->d_name);

        positions[count].start = current_char_pos;
        positions[count].end = current_char_pos + name_len - 1; 

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
    return count; 
}
