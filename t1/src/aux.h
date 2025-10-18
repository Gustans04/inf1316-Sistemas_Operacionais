#ifndef AUX_FUNC
#define AUX_FUNC

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

int criaFIFO(const char* nomeFIFO);
int abreFIFO(int* fifo, const char* nomeFIFO, int modo);

#endif