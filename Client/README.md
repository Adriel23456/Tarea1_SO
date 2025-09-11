# Image Processing Client

A modern GTK4-based client application for the Image Processing Server daemon. This client provides a clean, efficient interface for loading, managing, and sending images to the server for processing.

## Features

- **Modern GTK4 Interface**: Clean and responsive UI with modern design elements
- **Image Management**: Load and manage multiple images (jpg, jpeg, png, gif)
- **Server Configuration**: Easy-to-edit JSON configuration for server connection
- **Batch Processing**: Queue multiple images for processing
- **Scrollable Lists**: Efficient handling of large image collections
- **File Size Display**: Shows file sizes for loaded images

## Prerequisites

- Linux-based operating system (Ubuntu, Debian, Fedora, etc.)
- GTK4 development libraries
- GCC compiler
- pkg-config

## Installation

### Install Dependencies

For Ubuntu/Debian-based systems:

```bash
sudo apt update
sudo apt install build-essential pkg-config libgtk-4-dev
```

For Fedora:

```bash
sudo dnf install gcc gtk4-devel pkg-config
```

For Arch Linux:

```bash
sudo pacman -S base-devel gtk4 pkg-config
```

## Build Instructions

### Quick Build

1. Clone or download the project
2. Navigate to the project directory
3. Run the following commands:

```bash
# Create necessary directories and default files
make setup

# Build the application
make

# Run the application
./image-client
```

### Alternative Build (without Makefile)

If you prefer to compile manually:

```bash
# Create assets directory
mkdir -p assets

# Compile all source files
gcc src/main.c src/gui.c src/dialogs.c `pkg-config --cflags --libs gtk4` -o image-client

# Run the application
./image-client
```

## Project Structure

```
image-client/
├── src/
│   ├── main.c           # Application entry point
│   ├── gui.c            # Main GUI implementation
│   ├── gui.h            # GUI header file
│   ├── dialogs.c        # Dialog implementations
│   └── dialogs.h        # Dialog headers
├── assets/
│   ├── connection.json  # Server configuration
│   └── credits.txt      # Application credits
├── Makefile             # Build configuration
└── README.md            # This file
```

## Usage

### Loading Images

1. Click the **Load** button to open the file selector
2. Choose an image file (jpg, jpeg, png, or gif)
3. The image will appear in the list with its file size
4. Repeat to add multiple images

### Configuring Server Connection

1. Click the **Configuration** button
2. Edit the JSON configuration in the text editor
3. Click **Save** to apply changes
4. Configuration is stored in `assets/connection.json`

Default configuration:
```json
{
  "server": {
    "host": "localhost",
    "port": 1717,
    "protocol": "http"
  },
  "client": {
    "timeout": 30,
    "max_retries": 3
  }
}
```

### Sending Images

1. Load one or more images
2. Click **Send Images** to process the queue
3. Images will be sent to the configured server

### Viewing Credits

Click the **Credits** button to view application information and credits.

### Exiting the Application

Click the **Exit** button or close the window to quit the application.

## Makefile Targets

The project includes a comprehensive Makefile with the following targets:

- `make` - Build the application
- `make run` - Build and run the application
- `make clean` - Remove build files
- `make setup` - Create assets directory and default configuration files
- `make rebuild` - Clean, setup, and build from scratch
- `make install-deps` - Install GTK4 dependencies (Ubuntu/Debian)
- `make help` - Show all available targets

## Development Notes

### Adding New Features

1. GUI components are in `gui.c` and `gui.h`
2. Dialog windows are in `dialogs.c` and `dialogs.h`
3. The main window layout is created in `create_main_window()`
4. Button callbacks follow the pattern `on_[button]_button_clicked()`

### Styling

The application uses GTK4's CSS styling capabilities. Custom CSS is applied in `gui.c` to enhance the visual appearance with:
- Rounded corners on lists
- Hover effects on list items
- Modern button styling with suggested and destructive actions

### Image Processing Flow

1. Images are loaded into memory with metadata
2. File paths are stored in a linked list (`GSList`)
3. When sending, images are processed in order of file size (smallest first)
4. Binary data extraction and reconstruction will be handled by the server communication module

## Troubleshooting

### GTK4 Not Found

If you get an error about GTK4 not being found:
```bash
# Check if GTK4 is installed
pkg-config --modversion gtk4

# If not installed, run:
make install-deps
```

### Permission Denied

If you get permission errors:
```bash
# Make the binary executable
chmod +x image-client
```

### Missing Assets Directory

If the assets directory is missing:
```bash
make setup
```

## Future Enhancements

- [ ] Network communication module for server interaction
- [ ] Image preview thumbnails in the list
- [ ] Drag-and-drop support for adding images
- [ ] Progress indicators for image uploads
- [ ] Result viewing after server processing
- [ ] Multi-select for batch operations
- [ ] Image format validation before sending
- [ ] Connection status indicator

## License

This project is developed for the Systems Operations course. All rights reserved.

## Support

For issues, questions, or contributions, please refer to the course documentation or contact the development team.