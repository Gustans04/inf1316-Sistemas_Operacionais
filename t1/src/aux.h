#ifndef AUX_FUNC
#define AUX_FUNC

#include <sys/types.h>  // for pid_t
#include <pthread.h> // for pthread_mutex_t

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

typedef struct {
    pid_t pid;
    int pc;
    EstadoProcesso estado;
    Dispositivos dispositivo;
    Operacoes operacao; // R/W/X
    int qtd_acessos[2]; // D1, D2
    int executando;
} InfoProcesso;

typedef struct {
    pid_t lista[NUM_APP];
    int inicio;
    int fim;
    int qtd;
} FilaApps;

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

extern pthread_mutex_t mutex;

#endif
