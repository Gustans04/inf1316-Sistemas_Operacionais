# Compilador e Flags
CC = gcc
# -I../includes diz ao compilador para procurar headers na pasta includes acima
# -D_DEFAULT_SOURCE é necessário para o scandir/DT_DIR funcionar
# -Wno-format-truncation silencia os avisos de string do GCC
CFLAGS = -Wall -Wextra -std=c99 -I../includes -D_DEFAULT_SOURCE -Wno-format-truncation

# Diretórios (Relativos à pasta src)
OBJDIR = obj
BINDIR = ../bin
TESTDIR = ../tests

# --- Definição dos Executáveis ---

# Kernel Simulator (Main)
MAIN_TARGET = $(BINDIR)/main
MAIN_SOURCES = main.c aux.c
MAIN_OBJECTS = $(addprefix $(OBJDIR)/, $(MAIN_SOURCES:.c=.o))

# Intercontroller
INTER_TARGET = $(BINDIR)/intercontroller
INTER_SOURCES = intercontroller.c aux.c
INTER_OBJECTS = $(addprefix $(OBJDIR)/, $(INTER_SOURCES:.c=.o))

# Application Processes (A1...A5)
APP_TARGET = $(BINDIR)/application
APP_SOURCES = application.c aux.c
APP_OBJECTS = $(addprefix $(OBJDIR)/, $(APP_SOURCES:.c=.o))

# SFSS Server 
SFSS_TARGET = $(BINDIR)/sfss
SFSS_SOURCES = sfss_server.c sfss_ops.c 
SFSS_OBJECTS = $(addprefix $(OBJDIR)/, $(SFSS_SOURCES:.c=.o))

# --- Regras de Compilação ---

# Alvo padrão: compila tudo
all: directories $(MAIN_TARGET) $(INTER_TARGET) $(APP_TARGET) $(SFSS_TARGET)

# Cria os diretórios necessários (bin e obj) se não existirem
directories:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)

# Linkagem dos Executáveis
$(MAIN_TARGET): $(MAIN_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(INTER_TARGET): $(INTER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(APP_TARGET): $(APP_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(SFSS_TARGET): $(SFSS_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

# Compilação dos Objetos (.c -> .o)
$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- Comandos Úteis (Restaurados e Adaptados) ---

clean:
	# Limpa obj, bin, FIFOs e o root do servidor (em src)
	rm -rf $(OBJDIR) $(BINDIR) FIFO_IRQ FIFO_SYSCALL SFSS-root-dir 
	# Limpa também o root criado dentro da pasta de testes
	rm -rf ../tests/SFSS-root-dir

rebuild: clean all

# Instalação (Cria diretório bin e garante que executáveis estejam lá)
install: all
	@echo "Executáveis já estão instalados em $(BINDIR)"

# Rodar os programas individualmente
run-main: $(MAIN_TARGET)
	$(MAIN_TARGET)

run-inter: $(INTER_TARGET)
	$(INTER_TARGET)

run-app: $(APP_TARGET)
	$(APP_TARGET)

run-sfss: $(SFSS_TARGET)
	$(SFSS_TARGET)

# O run-all roda apenas o Main (Cliente).
# Para rodar o Cliente (Main) e o Servidor (SFSS), são necessários terminais separados ou background (&).
run-all: all
	$(MAIN_TARGET)

# Compila e roda os testes
test: directories
	@echo "Compilando testes..."
	# Compila o teste SFSS
	$(CC) $(CFLAGS) $(TESTDIR)/test_sfss_ops.c sfss_ops.c -o $(BINDIR)/test_sfss
	# Compila o teste InterController
	$(CC) $(CFLAGS) $(TESTDIR)/test_intercontroller.c aux.c -o $(BINDIR)/test_intercontroller
	# Compila o teste Main
	$(CC) $(CFLAGS) $(TESTDIR)/test_main.c aux.c -o $(BINDIR)/test_main
	
	@echo "Rodando testes dentro da pasta 'tests'..."
	cd $(TESTDIR) && ../bin/test_sfss
	@echo ""
	@echo "Rodando teste do InterController..."
	# Roda o teste InterController
	cd $(TESTDIR) && ../bin/test_intercontroller
	@echo ""
	@echo "Rodando teste do Main..."
	# Roda o teste Main
	cd $(TESTDIR) && ../bin/test_main

# Roda apenas o teste do SFSS
test-sfss: directories
	@echo "Compilando teste SFSS..."
	$(CC) $(CFLAGS) $(TESTDIR)/test_sfss_ops.c sfss_ops.c -o $(BINDIR)/test_sfss
	@echo "Rodando teste SFSS..."
	cd $(TESTDIR) && ../bin/test_sfss

# Roda apenas o teste do InterController
test-intercontroller: directories
	@echo "Compilando teste InterController..."
	$(CC) $(CFLAGS) $(TESTDIR)/test_intercontroller.c aux.c -o $(BINDIR)/test_intercontroller
	@echo "Rodando teste InterController..."
	cd $(TESTDIR) && ../bin/test_intercontroller

# Roda apenas o teste do Main
test-main: directories
	@echo "Compilando teste Main..."
	$(CC) $(CFLAGS) $(TESTDIR)/test_main.c aux.c -o $(BINDIR)/test_main
	@echo "Rodando teste Main..."
	cd $(TESTDIR) && ../bin/test_main

# Help
help:
	@echo "Available targets:"
	@echo "  all                  - Build all executables (main, inter, app, sfss)"
	@echo "  test                 - Build and run all unit tests"
	@echo "  test-sfss            - Build and run SFSS unit tests only"
	@echo "  test-intercontroller - Build and run InterController unit tests only"
	@echo "  test-main            - Build and run Main unit tests only"
	@echo "  clean                - Remove build files and directories"
	@echo "  rebuild              - Clean and rebuild everything"
	@echo "  run-main             - Run the Kernel Simulator"
	@echo "  run-inter            - Run the Intercontroller"
	@echo "  run-sfss             - Run the File System Server"
	@echo "  run-all              - Build and run Main (Kernel)"
	@echo "  help                 - Show this help message"

.PHONY: all clean rebuild directories install run-main run-inter run-app run-sfss run-all test test-sfss test-intercontroller test-main help
