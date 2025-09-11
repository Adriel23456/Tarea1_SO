# Makefile for refactored image server
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LIBS = -luuid -lssl -lcrypto -lpthread -ljson-c -lm

SRCDIR = src
OBJDIR = obj

# Source files
SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/config.c \
          $(SRCDIR)/logging.c \
          $(SRCDIR)/connection.c \
          $(SRCDIR)/utils.c \
          $(SRCDIR)/image_processing.c \
          $(SRCDIR)/gif_processing.c \
          $(SRCDIR)/server.c

# Object files
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

TARGET = image-server

all: check-stb check-gif $(OBJDIR) $(TARGET)

# Create object directory
$(OBJDIR):
	mkdir -p $(OBJDIR)

check-stb:
	@if [ ! -f $(SRCDIR)/stb_image.h ] || [ ! -f $(SRCDIR)/stb_image_write.h ]; then \
		echo "STB headers not found. Running download script..."; \
		chmod +x download_stb.sh; \
		./download_stb.sh; \
	fi

check-gif:
	@if [ ! -f $(SRCDIR)/gif.h ]; then \
		echo "gif.h not found. Running download script..."; \
		chmod +x download_gif.sh; \
		./download_gif.sh; \
	fi

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)
	@echo "Build complete! Run with: ./$(TARGET)"

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)
	@echo "Clean complete"

clean-all: clean
	rm -f $(SRCDIR)/stb_*.h $(SRCDIR)/gif.h
	@echo "Clean complete (including header libraries)"

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
	@echo "  make            - download headers and build"
	@echo "  make setup      - create assets dirs, default config and log"
	@echo "  make clean      - clean object files and binary"
	@echo "  make clean-all  - clean everything including headers"
	@echo "  make rebuild    - clean+setup+build"
	@echo ""
	@echo "Image Processing Features:"
	@echo "  - Color classification (dominant color detection)"
	@echo "  - Histogram equalization (contrast enhancement)"
	@echo "  - GIF animated support: per-frame processing + writing"

.PHONY: all clean clean-all setup rebuild help check-stb check-gif