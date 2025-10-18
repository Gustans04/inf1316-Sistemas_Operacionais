CC = gcc
CFLAGS = -Wall -Wextra -std=c99
OBJDIR = obj
BINDIR = bin

# Source files
MAIN_SOURCES = main.c aux.c
INTER_SOURCES = intercontroller.c aux.c

# Object files
MAIN_OBJECTS = $(MAIN_SOURCES:.c=.o)
INTER_OBJECTS = $(INTER_SOURCES:.c=.o)

# Executables
MAIN_TARGET = main
INTER_TARGET = intercontroller

# Default target
all: $(MAIN_TARGET) $(INTER_TARGET)

# Build main executable
$(MAIN_TARGET): $(MAIN_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

# Build intercontroller executable
$(INTER_TARGET): $(INTER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f *.o $(MAIN_TARGET) $(INTER_TARGET) FIFO_IRQ

# Clean and rebuild
rebuild: clean all

# Install targets (optional)
install: all
	mkdir -p $(BINDIR)
	cp $(MAIN_TARGET) $(INTER_TARGET) $(BINDIR)/

# Run main
run-main: $(MAIN_TARGET)
	./$(MAIN_TARGET)

# Run intercontroller
run-inter: $(INTER_TARGET)
	./$(INTER_TARGET)

# Help
help:
	@echo "Available targets:"
	@echo "  all          - Build both executables"
	@echo "  main         - Build main executable"
	@echo "  intercontroller - Build intercontroller executable"
	@echo "  clean        - Remove build files"
	@echo "  rebuild      - Clean and rebuild"
	@echo "  install      - Install binaries to bin directory"
	@echo "  run-main     - Build and run main"
	@echo "  run-inter    - Build and run intercontroller"
	@echo "  help         - Show this help"

.PHONY: all clean rebuild install run-main run-inter help