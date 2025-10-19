#ifndef AUX_FUNC
#define AUX_FUNC

#include <sys/types.h>  // for pid_t

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

InfoProcesso* encontrarAplicacaoPorPID(InfoProcesso *lista_processos, int tamanho, pid_t pid_desejado);

void inicializarFila(FilaApps *fila);
void inserirNaFila(FilaApps *fila, int valor);
pid_t removerDaFila(FilaApps *fila);
int estaVazia(FilaApps *fila);
pid_t procuraNaFila(FilaApps *fila, int tamanho, pid_t pid_desejado);

#endif