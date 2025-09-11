# Makefile for Image Processing Client
# Compiler and flags configuration

CC = gcc
CFLAGS = `pkg-config --cflags gtk4` -Wall -Wextra -g
LIBS = `pkg-config --libs gtk4`

# Source files
SRCDIR = src
SOURCES = $(SRCDIR)/main.c $(SRCDIR)/gui.c $(SRCDIR)/dialogs.c
OBJECTS = $(SOURCES:.c=.o)

# Output binary
TARGET = image-client

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)
	@echo "Build complete! Run with: ./$(TARGET)"

# Compile source files
$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Create necessary directories
dirs:
	@mkdir -p assets
	@echo "Created assets directory"

# Clean build files
clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "Clean complete"

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt update
	sudo apt install -y build-essential pkg-config libgtk-4-dev
	@echo "Dependencies installed"

# Run the application
run: $(TARGET)
	./$(TARGET)

# Create default configuration files
setup: dirs
	@echo '{\n  "server": {\n    "host": "localhost",\n    "port": 1717,\n    "protocol": "http"\n  },\n  "client": {\n    "timeout": 30,\n    "max_retries": 3\n  }\n}' > assets/connection.json
	@echo 'IMAGE PROCESSING CLIENT\n=======================\n\nVersion 1.0.0\n\nDeveloped for Systems Operations Course\n\n© 2024' > assets/credits.txt
	@echo "Setup complete! Configuration files created in assets/"

# Full build: clean, setup, and compile
rebuild: clean setup all

# Help target
help:
	@echo "Available targets:"
	@echo "  make              - Build the application"
	@echo "  make run          - Build and run the application"
	@echo "  make clean        - Remove build files"
	@echo "  make setup        - Create assets directory and default files"
	@echo "  make rebuild      - Clean, setup, and build"
	@echo "  make install-deps - Install GTK4 dependencies (Ubuntu/Debian)"
	@echo "  make help         - Show this help message"

.PHONY: all clean dirs run setup rebuild install-deps help