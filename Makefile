# Dust Makefile
# Lightweight and modern init system for Linux

# Version
VERSION := 0.1.0

# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -O2 -std=c99 -DVERSION=\"$(VERSION)\"
LDFLAGS := -lrt

# Directories
PREFIX := /usr
BINDIR := $(PREFIX)/bin
SBINDIR := $(PREFIX)/sbin
LIBDIR := $(PREFIX)/lib
SYSCONFDIR := /etc
RUNDIR := /run

# Project directories
SRCDIR := src
BUILDDIR := build

# Source files
INIT_SRC := $(SRCDIR)/dust-init.c
CLI_SRC := $(SRCDIR)/dust-cli.c

# Object files
INIT_OBJ := $(BUILDDIR)/dust-init.o
CLI_OBJ := $(BUILDDIR)/dust-cli.o

# Executables
INIT_BIN := dust-init
CLI_BIN := dust

# Default target
.PHONY: all clean install uninstall debug release

all: release

# Release build
release: CFLAGS := -Wall -Wextra -O2 -s -DNDEBUG -DVERSION=\"$(VERSION)\"
release: dirs $(INIT_BIN) $(CLI_BIN)
	@echo "Build completed successfully!"
	@echo "Version: $(VERSION)"
	@echo ""
	@echo "Generated executables:"
	@echo "  - $(INIT_BIN) (PID 1 init system)"
	@echo "  - $(CLI_BIN) (CLI client)"

# Debug build
debug: CFLAGS := -Wall -Wextra -g -O0 -DDEBUG -DVERSION=\"$(VERSION)\"
debug: dirs $(INIT_BIN) $(CLI_BIN)
	@echo "Debug build completed!"
	@echo "Use gdb for debugging."

# Create required directories
dirs:
	@mkdir -p $(BUILDDIR)
	@mkdir -p $(SRCDIR)

# Compile dust-init
$(INIT_BIN): $(INIT_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "  CC      $@"

# Compile dust-cli
$(CLI_BIN): $(CLI_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "  CC      $@"

# Compile object files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<
	@echo "  CC      $@"

# Clean
clean:
	@echo "Cleaning generated files..."
	@rm -rf $(BUILDDIR)
	@rm -f $(INIT_BIN) $(CLI_BIN)
	@echo "Clean completed!"

# Install
install: release
	@echo "Installing Dust $(VERSION)..."
	@echo ""
	
	# Create directories
	@mkdir -p $(DESTDIR)$(SBINDIR)
	@mkdir -p $(DESTDIR)$(BINDIR)
	@mkdir -p $(DESTDIR)$(SYSCONFDIR)/dust/services
	@mkdir -p $(DESTDIR)$(SYSCONFDIR)/dust/targets
	@mkdir -p $(DESTDIR)$(RUNDIR)/dust
	@mkdir -p $(DESTDIR)/var/log
	
	# Install binaries
	@cp $(INIT_BIN) $(DESTDIR)$(SBINDIR)/
	@cp $(CLI_BIN) $(DESTDIR)$(BINDIR)/
	@chmod 755 $(DESTDIR)$(SBINDIR)/$(INIT_BIN)
	@chmod 755 $(DESTDIR)$(BINDIR)/$(CLI_BIN)
	
	@echo "Installation completed!"
	@echo ""
	@echo "Installed executables:"
	@echo "  - $(DESTDIR)$(SBINDIR)/$(INIT_BIN)"
	@echo "  - $(DESTDIR)$(BINDIR)/$(CLI_BIN)"
	@echo ""
	@echo "Config directory: $(DESTDIR)$(SYSCONFDIR)/dust/"

# Uninstall
uninstall:
	@echo "Uninstalling Dust..."
	@rm -f $(DESTDIR)$(SBINDIR)/$(INIT_BIN)
	@rm -f $(DESTDIR)$(BINDIR)/$(CLI_BIN)
	@echo "Binaries removed."
	@echo "Note: configuration directories in $(SYSCONFDIR)/dust/"
	@echo "      were not removed for safety."

# Show variables
dumpvars:
	@echo "Build variables:"
	@echo "  VERSION: $(VERSION)"
	@echo "  CC: $(CC)"
	@echo "  CFLAGS: $(CFLAGS)"
	@echo "  LDFLAGS: $(LDFLAGS)"
	@echo "  PREFIX: $(PREFIX)"
	@echo "  BINDIR: $(BINDIR)"
	@echo "  SBINDIR: $(SBINDIR)"

# Help
help:
	@echo "Dust Makefile - Available commands:"
	@echo ""
	@echo "Build:"
	@echo "  make release    - Production optimized build (default)"
	@echo "  make debug      - Debug build with symbols"
	@echo "  make all        - Alias for 'make release'"
	@echo ""
	@echo "Installation:"
	@echo "  make install    - Install Dust to the system"
	@echo "  make uninstall  - Remove Dust from the system"
	@echo ""
	@echo "Maintenance:"
	@echo "  make clean      - Clean generated files"
	@echo "  make dumpvars   - Show build variables"
	@echo "  make help       - Show this help"
