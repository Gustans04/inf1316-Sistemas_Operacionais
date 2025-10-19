```markdown
# Trabalho 1 — INF1316 Sistemas Operacionais

## Visão Geral
- **Descrição:** Implementação de um simulador de sistema operacional que gerencia processos e dispositivos.
- **Objetivos:** Simular um kernel com escalonamento de processos, comunicação interprocessos, e gerenciamento de dispositivos através de IRQs e chamadas de sistema.

## Componentes do Sistema
- **Kernel (main.c):** Gerenciador principal do sistema, responsável por criar e escalonar processos.
- **InterController (intercontroller.c):** Gerencia as interrupções e sinais de hardware (IRQs).
- **Application (application.c):** Aplicações que simulam processos requisitando recursos do sistema.
- **Biblioteca Auxiliar (aux.c/aux.h):** Funções de suporte para estruturas de dados e comunicação.

## Pré-requisitos
- Sistema compatível com POSIX (Linux, macOS) ou WSL no Windows.
- Ferramentas básicas: gcc, make.

## Compilação e Execução
### Compilando o projeto
```bash
make
```
Este comando irá compilar todos os componentes do sistema: main (kernel), intercontroller e application.

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

- Para executar apenas a application:
```bash
make run-application
```

### Executando o sistema completo
```bash
make run-all
```
Este comando compila e executa todos os componentes do sistema. Será o utilizado para rodar o programa como um todo.

## Estrutura do Projeto
- **main.c:** Implementação do kernel do sistema operacional.
- **intercontroller.c:** Implementação do controlador de interrupções.
- **application.c:** Implementação das aplicações de usuário.
- **aux.c/aux.h:** Biblioteca de funções auxiliares.
- **Makefile:** Script de compilação e execução.

## Detalhes de Implementação
O projeto simula um kernel que gerencia 5 aplicações concorrentes que podem solicitar acesso a 2 dispositivos (D1 e D2) para operações de leitura, escrita e execução. O kernel utiliza:

- Memória compartilhada para comunicação entre processos
- FIFOs para simular IRQs (interrupções) e chamadas de sistema
- Semáforos para sincronização
- Sinais UNIX para controle de execução dos processos

## Ajuda
```bash
make help
```
Exibe todos os comandos disponíveis no Makefile com suas respectivas descrições.

## Contato
- Para questões sobre este repositório, abra uma issue ou entre em contato com o proprietário do trabalho.

