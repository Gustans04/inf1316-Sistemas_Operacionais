# Trabalho 2 — INF1316 Sistemas Operacionais

## Visão Geral
- **Descrição:** Implementação de um simulador de sistema operacional com kernel simulado, aplicações cliente, servidor de sistema de arquivos (SFSS) e InterController para gerenciamento de IRQs.
- **Objetivos:** Simular um kernel com escalonamento de processos, comunicação via UDP, gerenciamento de sistema de arquivos simulado, e controle de interrupções com diferentes probabilidades para operações de arquivo e diretório.

## Componentes do Sistema
- **Kernel (main.c):** Simulador do kernel que gerencia processos, filas de requisições, comunicação UDP e escalonamento.
- **InterController (intercontroller.c):** Gerencia interrupções com diferentes probabilidades: IRQ0 (time slice), IRQ1 (operações de arquivo - P=0.1), IRQ2 (operações de diretório - P=0.02).
- **Application (application.c):** Aplicações cliente que geram syscalls (WriteCall, ReadCall, AddCall, RemCall, ListDirCall) e se comunicam com o kernel via memória compartilhada.
- **SFSS Server (sfss_server.c):** Servidor de sistema de arquivos simulado que processa requisições via UDP e gerencia operações de arquivo/diretório.
- **Biblioteca Auxiliar (aux.c/aux.h):** Funções de suporte para estruturas de dados, comunicação UDP, semáforos e gerenciamento de filas.
- **SFSS Operations (sfss_ops.c):** Implementação das operações do sistema de arquivos simulado.

## Pré-requisitos
- Sistema compatível com POSIX (Linux, macOS) ou WSL no Windows.
- Ferramentas básicas: gcc, make.

## Compilação e Execução
### Compilando o projeto
```bash
make
```
Este comando irá compilar todos os componentes do sistema: main (kernel), intercontroller, application e sfss (servidor de sistema de arquivos).

### Limpando arquivos compilados
```bash
make clean
```
Remove todos os arquivos objeto (*.o), executáveis e os arquivos FIFO.

### Recompilando o projeto
```bash
make rebuild
```
Limpa todos os arquivos compilados e depois recompila o projeto.

### Executando os componentes separadamente
- Para executar apenas o kernel:
```bash
make run-main
```

- Para executar apenas o intercontroller:
```bash
make run-inter
```

- Para executar apenas uma application:
```bash
make run-app
```

- Para executar apenas o servidor SFSS:
```bash
make run-sfss
```

### Executando testes
- Para rodar todos os testes:
```bash
make test
```

- Para rodar apenas testes do SFSS:
```bash
make test-sfss
```

- Para rodar apenas testes do InterController:
```bash
make test-intercontroller
```

- Para rodar apenas testes do Kernel:
```bash
make test-main
```

### Executando o sistema completo
```bash
make run-all
```
Este comando compila e executa todos os componentes do sistema. Primeiro inicia o servidor SFSS, depois executa o kernel principal que gerencia as aplicações e o InterController.

## Estrutura do Projeto
- **main.c:** Implementação do kernel simulado do sistema operacional.
- **intercontroller.c:** Implementação do controlador de interrupções com probabilidades específicas.
- **application.c:** Implementação das aplicações cliente que geram syscalls.
- **sfss_server.c:** Implementação do servidor de sistema de arquivos simulado.
- **sfss_ops.c:** Implementação das operações do sistema de arquivos (read, write, add, rem, listdir).
- **aux.c/aux.h:** Biblioteca de funções auxiliares incluindo comunicação UDP e gerenciamento de filas.
- **tests/:** Diretório contendo testes unitários para cada componente.
- **Makefile:** Script de compilação e execução com suporte a testes.

## Detalhes de Implementação
O projeto simula um kernel que gerencia 5 aplicações concorrentes (A1-A5) que fazem syscalls para operações de arquivo e diretório. O sistema utiliza:

- **Comunicação UDP:** Entre kernel e servidor SFSS para operações de sistema de arquivos
- **Memória compartilhada:** Para comunicação entre kernel e aplicações
- **FIFOs:** Para simular IRQs (interrupções) do InterController para o kernel
- **Semáforos nomeados:** Para sincronização entre processos
- **Duas filas de requisições:** File-Request-Queue (read/write) e Dir-Request-Queue (add/rem/listdir)
- **Sistema de interrupções:** IRQ0 (time slice), IRQ1 (arquivo - P=0.1), IRQ2 (diretório - P=0.02)
- **Sistema de arquivos simulado:** Gerenciado pelo servidor SFSS com operações via UDP

### Syscalls Implementadas:
- **WriteCall:** Escreve dados em arquivo
- **ReadCall:** Lê dados de arquivo  
- **AddCall:** Adiciona diretório
- **RemCall:** Remove arquivo/diretório
- **ListDirCall:** Lista conteúdo de diretório

### Estados dos Processos:
- **PRONTO:** Processo pronto para execução
- **EXECUTANDO:** Processo em execução
- **ESPERANDO:** Aguardando resposta de syscall
- **BLOQUEADO:** Bloqueado esperando recurso
- **TERMINADO:** Processo finalizado

## Ajuda
```bash
make help
```
Exibe todos os comandos disponíveis no Makefile com suas respectivas descrições.

## Contato
- Para questões sobre este repositório, abra uma issue ou entre em contato com o proprietário do trabalho.

