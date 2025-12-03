#ifndef AUX_FUNC
#define AUX_FUNC

#include <sys/types.h> // for pid_t
#include <semaphore.h> // for named semaphores
#include <fcntl.h>     // for O_* constants
#include <sys/stat.h>  // for mode constants

// Códigos IPC para memória compartilhada
#define IPC_CODE 1234

// Número de aplicações
#define NUM_APP 5

// Espaço máximo para nomes de arquivos e diretórios
#define MAX_NAME_LEN 1024

// Dispositivos
typedef enum
{
    D1,
    D2
} Dispositivos;

// Operações
typedef enum
{
    R,
    W,
    X
} Operacoes;

typedef enum
{
    PRONTO,
    EXECUTANDO,
    ESPERANDO,
    BLOQUEADO,
    TERMINADO
} EstadoProcesso;

// Estrutura para listagem de diretórios
typedef struct 
{
    int start;
    int end;
    int type; // 1 = Arquivo (F), 2 = Diretório (D)
} IndexInfo;

// typedef struct {
//     pid_t pid;
//     int pc;
//     EstadoProcesso estado;
//     Dispositivos dispositivo;
//     Operacoes operacao; // R/W/X
//     int qtd_acessos[2]; // D1, D2
//     int executando;
// } InfoProcesso;

typedef struct
{
    pid_t lista[NUM_APP];
    int inicio;
    int fim;
    int qtd;
} FilaApps;

typedef struct
{
    char path[MAX_NAME_LEN]; // Caminho do arquivo
    int len;                 // Tamanho do nome do caminho
    char payload[16];        // Dados a serem escritos
    int offset;              // Posição no arquivo
} WriteCall;                 // escreve o conteúdo de payload no arquivo path a partir do offset

typedef struct
{
    char path[MAX_NAME_LEN]; // Caminho do arquivo
    int len;                 // Tamanho do nome do caminho
    char buffer[16];         // Buffer para leitura
    int offset;              // Posição no arquivo
} ReadCall;                  // lê o conteúdo do arquivo path a partir do offset para o buffer

typedef struct
{
    char path[MAX_NAME_LEN];    // Caminho do arquivo
    int len1;                   // Tamanho do nome do caminho
    char dirname[MAX_NAME_LEN]; // Nome do novo diretório
    int len2;                   // Tamanho do nome do novo diretório
} AddCall;                      // cria um novo subdiretório em path

typedef struct
{
    char path[MAX_NAME_LEN]; // Caminho do arquivo ou diretório a ser removido
    int len1;                // Tamanho do nome do caminho
    char name[MAX_NAME_LEN]; // Nome do arquivo ou diretório a ser removido
    int len2;                // Tamanho do nome do arquivo ou diretório a ser removido
} RemCall;                   // remove o arquivo ou diretório em path

typedef struct
{
    char path[MAX_NAME_LEN]; // Caminho do diretório
    int len1;                // Tamanho do nome do caminho
    char alldirinfo[2048];   // Nomes de todos os arquivos e diretórios dentro de path
    IndexInfo fstlstpositions[40]; // Posições iniciais dos nomes dos arquivos e diretórios dentro de alldirinfo
    int nrnames;             // Número de nomes encontrados
} ListDirCall;               // lista o conteúdo do diretório em path em uma única string alldirinfo, e com os nomes indexados por fstlstpositions

typedef struct
{
    int novo;         // Indica se há uma nova syscall a ser processada: 0 - não, 1 - sim
    int tipo_syscall; // Tipo da syscall: 0 - WriteCall, 1 - ReadCall, 2 - AddCall, 3 - RemCall, 4 - ListDirCall, -1 - Nenhuma syscall
    union
    {
        WriteCall writecall;
        ReadCall readcall;
        AddCall addcall;
        RemCall remcall;
        ListDirCall listdircall;
    } call;
} SysCallInfo;

typedef struct
{
    pid_t pid;
    int pc;
    EstadoProcesso estado;
    SysCallInfo syscall;
    int executando;
} InfoProcesso;

typedef struct
{
    int tipo_syscall; // Tipo da syscall: 0 - WriteCall, 1 - ReadCall, 2 - AddCall, 3 - RemCall, 4 - ListDirCall
    int owner;        // Número da aplicação que fez a syscall (1 a NUM_APP)
    union
    {
        WriteCall writecall;
        ReadCall readcall;
        AddCall addcall;
        RemCall remcall;
        ListDirCall listdircall;
    } call;
} CallRequest;

typedef struct
{
    CallRequest lista[NUM_APP];
    int inicio;
    int fim;
    int qtd;
} FilaRequests;

int criaFIFO(const char *nomeFIFO);
int abreFIFO(int *fifo, const char *nomeFIFO, int modo);

InfoProcesso *encontrarAplicacaoPorPID(InfoProcesso *lista_processos, pid_t pid_desejado);
int processosAcabaram(InfoProcesso *lista_processos);

void inicializarFila(FilaApps *fila);
void inserirNaFila(FilaApps *fila, int valor);
int pid_na_fila(FilaApps *f, pid_t pid);
pid_t removerDaFila(FilaApps *fila);
void removerTodasOcorrencias(FilaApps *f, pid_t pid);
int estaVazia(FilaApps *fila);
pid_t procuraNaFila(FilaApps *fila, pid_t pid_desejado);
int numeroDoProcesso(InfoProcesso *processos, pid_t pid); // retorna o número do processo (1 a NUM_APP) dado o PID
void removerPidDaFila(FilaApps *fila, pid_t pid);

void inicializarFilaRequests(FilaRequests *fila);
void inserirNaFilaRequests(FilaRequests *fila, CallRequest request);
int owner_na_fila(FilaRequests *f, int owner);
int estaVaziaRequests(FilaRequests *fila);
int removerDaFilaRequests(FilaRequests *fila);

void error(char *msg);

// Udp Client function
void iniciaUdpClient(void);
void enviaUdpRequest(CallRequest request);
CallRequest recebeUdpResponse(void);
void encerraUdpClient(void);
void respostaParaApp(InfoProcesso *processos, CallRequest resposta);

char **split_string(const char *str, const char *delim);
void print_status(InfoProcesso *processos);
char* escape(char* buffer);

// Named semaphore functions for inter-process synchronization
void init_sem(void);
void sem_lock(void);
void sem_unlock(void);
void cleanup_sem(void);

extern sem_t *process_sem;

#endif
