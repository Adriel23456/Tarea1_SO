CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LIBS = -luuid -lssl -lcrypto -lpthread

SRCDIR = src
SOURCES = $(SRCDIR)/main.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = image-server

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)
	@echo "Build complete! Run with: ./$(TARGET)"

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "Clean complete"

setup:
	mkdir -p assets assets/incoming
	@if [ ! -f assets/log.txt ]; then \
		echo "== Image Server Log ==" > assets/log.txt; \
		echo "Created assets/log.txt"; \
	else \
		echo "assets/log.txt already exists, skipping..."; \
	fi
	@echo "Setup complete!"

rebuild: clean setup all

help:
	@echo "Targets:"
	@echo "  make            - build"
	@echo "  make setup      - create assets dirs and log"
	@echo "  make clean      - clean"
	@echo "  make rebuild    - clean+setup+build"
.PHONY: all clean setup rebuild help