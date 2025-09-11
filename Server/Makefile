CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LIBS = -luuid -lssl -lcrypto -lpthread -ljson-c

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
	mkdir -p assets \
	         assets/incoming \
	         assets/histogram \
	         assets/colors/red \
	         assets/colors/green \
	         assets/colors/blue \
	         assets/tls
	@if [ ! -f assets/log.txt ]; then \
		echo "== Image Server Log ==" > assets/log.txt; \
		echo "Created assets/log.txt"; \
	else \
		echo "assets/log.txt already exists, skipping..."; \
	fi
	@if [ ! -f assets/config.json ]; then \
		printf '%s\n' \
		'{' \
		'  "server": {' \
		'    "port": 1717,' \
		'    "tls_enabled": 0,' \
		'    "tls_dir": "assets/tls"' \
		'  },' \
		'  "paths": {' \
		'    "log_file": "assets/log.txt",' \
		'    "incoming_dir": "assets/incoming",' \
		'    "histogram_dir": "assets/histogram",' \
		'    "colors_dir": {' \
		'      "red": "assets/colors/red",' \
		'      "green": "assets/colors/green",' \
		'      "blue": "assets/colors/blue"' \
		'    }' \
		'  }' \
		'}' > assets/config.json; \
		echo "Created assets/config.json"; \
	else \
		echo "assets/config.json already exists, skipping..."; \
	fi
	@echo "Setup complete!"

rebuild: clean setup all

help:
	@echo "Targets:"
	@echo "  make            - build"
	@echo "  make setup      - create assets dirs, default config and log"
	@echo "  make clean      - clean"
	@echo "  make rebuild    - clean+setup+build"
.PHONY: all clean setup rebuild help