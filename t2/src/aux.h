#ifndef AUX_FUNC
#define AUX_FUNC

#include <sys/types.h>  // for pid_t
#include <semaphore.h>  // for named semaphores
#include <fcntl.h>      // for O_* constants
#include <sys/stat.h>   // for mode constants

// Códigos IPC para memória compartilhada
#define IPC_CODE  1234

// Número de aplicações
#define NUM_APP 5

// Dispositivos
typedef enum{
    D1,
    D2
} Dispositivos;

// Operações
typedef enum{
    R,
    W,
    X
} Operacoes;

typedef enum {
    PRONTO,
    EXECUTANDO,
    ESPERANDO,
    BLOQUEADO,
    TERMINADO
} EstadoProcesso;

// typedef struct {
//     pid_t pid;
//     int pc;
//     EstadoProcesso estado;
//     Dispositivos dispositivo;
//     Operacoes operacao; // R/W/X
//     int qtd_acessos[2]; // D1, D2
//     int executando;
// } InfoProcesso;

typedef struct {
    pid_t lista[NUM_APP];
    int inicio;
    int fim;
    int qtd;
} FilaApps;

typedef struct {
    char path[256]; // Caminho do arquivo
    char payload[16]; // Dados a serem escritos
    int offset; // Posição no arquivo
} WriteCall; // escreve o conteúdo de payload no arquivo path a partir do offset

typedef struct {
    char path[256]; // Caminho do arquivo
    char buffer[16]; // Buffer para leitura
    int offset; // Posição no arquivo
} ReadCall; // lê o conteúdo do arquivo path a partir do offset para o buffer

typedef struct {
    char path[256]; // Caminho do arquivo
    int len1; // Tamanho do nome do caminho
    char dirname[265]; // Nome do novo diretório
    int len2; // Tamanho do nome do novo diretório
} AddCall; // cria um novo subdiretório em path

typedef struct {
    char path[256]; // Caminho do arquivo ou diretório a ser removido
    int len1; // Tamanho do nome do caminho
    char name[256]; // Nome do arquivo ou diretório a ser removido
    int len2; // Tamanho do nome do arquivo ou diretório a ser removido
} RemCall; // remove o arquivo ou diretório em path

typedef struct {
    char path[256]; // Caminho do diretório
    int len1; // Tamanho do nome do caminho
    char alldirinfo[2048]; // Nomes de todos os arquivos e diretórios dentro de path
    int fstlstpositions[40]; // Posições iniciais dos nomes dos arquivos e diretórios dentro de alldirinfo
    int* nrnames; // Número de nomes encontrados
} ListDirCall; // lista o conteúdo do diretório em path em uma única string alldirinfo, e com os nomes indexados por fstlstpositions

typedef struct {
    int tipo_syscall; // Tipo da syscall: 0 - WriteCall, 1 - ReadCall, 2 - AddCall, 3 - RemCall, 4 - ListDirCall, -1 - Nenhuma syscall
    union {
        WriteCall writecall;
        ReadCall readcall;
        AddCall addcall;
        RemCall remcall;
        ListDirCall listdircall;
    } call;
} SysCallInfo;

typedef struct {
    pid_t pid;
    int pc;
    EstadoProcesso estado;
    SysCallInfo syscall;
    int executando;
} InfoProcesso;

int criaFIFO(const char* nomeFIFO);
int abreFIFO(int* fifo, const char* nomeFIFO, int modo);

InfoProcesso* encontrarAplicacaoPorPID(InfoProcesso *lista_processos, pid_t pid_desejado);
int processosAcabaram(InfoProcesso *lista_processos);

void inicializarFila(FilaApps *fila);
void inserirNaFila(FilaApps *fila, int valor);
int pid_na_fila(FilaApps *f, pid_t pid);
pid_t removerDaFila(FilaApps *fila);
void removerTodasOcorrencias(FilaApps *f, pid_t pid);
int estaVazia(FilaApps *fila);
pid_t procuraNaFila(FilaApps *fila, pid_t pid_desejado);

void print_status(InfoProcesso* processos);

// Named semaphore functions for inter-process synchronization
void init_sem(void);
void sem_lock(void);
void sem_unlock(void);
void cleanup_sem(void);

extern sem_t *process_sem;

#endif
