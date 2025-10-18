#ifndef AUX_FUNC
#define AUX_FUNC

// Dispositivos
#define D1 0
#define D2 1

// Operações
#define R 0
#define W 1
#define X 2

int criaFIFO(const char* nomeFIFO);
int abreFIFO(int* fifo, const char* nomeFIFO, int modo);

#endif