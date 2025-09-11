# Makefile for Image Processing Server
# This is a standalone server project

CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
LIBS = -pthread

# Source files
SOURCES = main.c image_handler.c logger.c
OBJECTS = $(SOURCES:.c=.o)

# Output binary
TARGET = image-server

# Default target
all: $(TARGET)

# Build the server
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)
	@echo "Server build complete!"
	@echo "Run with: ./$(TARGET)"

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -rf received_images
	@echo "Clean complete"

# Create directories
dirs:
	@mkdir -p received_images/histogram
	@mkdir -p received_images/colors/verdes
	@mkdir -p received_images/colors/rojas
	@mkdir -p received_images/colors/azules
	@echo "Directories created"

# Run the server
run: $(TARGET) dirs
	./$(TARGET)

# Run with custom port
run-port: $(TARGET) dirs
	@read -p "Enter port number: " port; \
	./$(TARGET) $$port

# Show log
log:
	@if [ -f server.log ]; then \
		tail -f server.log; \
	else \
		echo "No log file found. Run the server first."; \
	fi

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt update
	sudo apt install -y build-essential
	@echo "Dependencies installed"

# Full setup
setup: install-deps dirs all
	@echo "========================================="
	@echo "Server setup complete!"
	@echo "Run with: ./$(TARGET) [port]"
	@echo "Default port: 1717"
	@echo "========================================="

# Help
help:
	@echo "Image Processing Server - Makefile"
	@echo "=================================="
	@echo "Available targets:"
	@echo "  make          - Build the server"
	@echo "  make run      - Build and run server (port 1717)"
	@echo "  make run-port - Run server with custom port"
	@echo "  make clean    - Remove build files"
	@echo "  make dirs     - Create required directories"
	@echo "  make log      - View server log"
	@echo "  make setup    - Complete setup"
	@echo "  make help     - Show this help"

.PHONY: all clean dirs run run-port log install-deps setup help