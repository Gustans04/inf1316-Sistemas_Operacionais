#ifndef AUX_FUNC
#define AUX_FUNC

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
    BLOQUEADO,
    TERMINADO
} EstadoProcesso;

typedef struct {
    pid_t pid;
    int pc;
    EstadoProcesso estado;
    Dispositivos dispositivo; // 0: D1, 1: D2
    Operacoes operacao[2]; // R/W/X
    int qtd_acessos[2]; // D1, D2
    int executando;
} InfoProcesso;

int criaFIFO(const char* nomeFIFO);
int abreFIFO(int* fifo, const char* nomeFIFO, int modo);

#endif