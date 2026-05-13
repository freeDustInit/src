#!/bin/bash
#
# Dust Init - One-line installer
# Usage: curl -fsSL https://raw.githubusercontent.com/freeDustInit/src/main/install.sh | sudo bash
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
REPO_URL="https://github.com/freeDustInit/src"
VERSION="${VERSION:-latest}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr}"

# Functions
print_banner() {
    echo -e "${BLUE}"
    echo "██████╗ ██╗   ██╗███████╗████████╗"
    echo "██╔══██╗██║   ██║██╔════╝╚══██╔══╝"
    echo "██║  ██║██║   ██║███████╗   ██║"
    echo "██║  ██║██║   ██║╚════██║   ██║"
    echo "███████╝╚██████╔╝███████║   ██║"
    echo "╚════╝  ╚═════╝ ╚══════╝   ╚═╝"
    echo -e "${NC}"
}

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

detect_arch() {
    local arch=$(uname -m)
    case $arch in
        x86_64)
            echo "amd64"
            ;;
        aarch64|arm64)
            echo "arm64"
            ;;
        armv7l)
            echo "armv7"
            ;;
        i386|i686)
            echo "386"
            ;;
        *)
            log_error "Unsupported architecture: $arch"
            exit 1
            ;;
    esac
}

detect_os() {
    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        echo $ID
    else
        echo "unknown"
    fi
}

download_release() {
    local version=$1
    local arch=$(detect_arch)
    local os=$(detect_os)
    
    log_info "Detected architecture: $arch"
    log_info "Detected OS: $os"
    
    # Warning for non-amd64 architectures
    if [[ "$arch" != "amd64" ]]; then
        log_warning "Non-amd64 architecture detected ($arch)"
        log_warning "Pre-built binaries are only available for amd64 (x86_64)"
        log_warning "The installer will attempt to build from source instead"
        log_info ""
        
        # Automatically switch to build from source
        BUILD_FROM_SOURCE=1
        build_from_source
        return
    fi
    
    # Create temporary directory
    TMP_DIR=$(mktemp -d)
    trap "rm -rf $TMP_DIR" EXIT
    
    # Only latest version is supported for binary downloads
    log_info "Downloading latest release..."
    DUST_INIT_URL="${REPO_URL}/releases/latest/download/dust-init"
    DUST_URL="${REPO_URL}/releases/latest/download/dust"
    
    # Download dust-init
    log_info "Downloading dust-init from: $DUST_INIT_URL"
    if command -v curl &> /dev/null; then
        curl -fsSL "$DUST_INIT_URL" -o "$TMP_DIR/dust-init" || {
            log_error "Failed to download dust-init"
            exit 1
        }
    elif command -v wget &> /dev/null; then
        wget -q "$DUST_INIT_URL" -O "$TMP_DIR/dust-init" || {
            log_error "Failed to download dust-init"
            exit 1
        }
    else
        log_error "Neither curl nor wget is installed"
        exit 1
    fi
    
    # Download dust
    log_info "Downloading dust from: $DUST_URL"
    if command -v curl &> /dev/null; then
        curl -fsSL "$DUST_URL" -o "$TMP_DIR/dust" || {
            log_error "Failed to download dust"
            exit 1
        }
    elif command -v wget &> /dev/null; then
        wget -q "$DUST_URL" -O "$TMP_DIR/dust" || {
            log_error "Failed to download dust"
            exit 1
        }
    else
        log_error "Neither curl nor wget is installed"
        exit 1
    fi
    
    # Install binaries
    log_info "Installing binaries..."
    cp "$TMP_DIR/dust-init" "${INSTALL_PREFIX}/sbin/"
    cp "$TMP_DIR/dust" "${INSTALL_PREFIX}/bin/"
    chmod 755 "${INSTALL_PREFIX}/sbin/dust-init"
    chmod 755 "${INSTALL_PREFIX}/bin/dust"
    
    # Create directories
    mkdir -p /etc/dust/services
    mkdir -p /etc/dust/targets
    mkdir -p /run/dust
    
    log_success "Dust v${version} has been installed successfully!"
}

build_from_source() {
    log_info "Building Dust from source..."
    
    # Check for required dependencies
    if ! command -v gcc &> /dev/null; then
        log_error "gcc is required to build from source"
        log_info "Install it with: apt-get install build-essential (Debian/Ubuntu) or yum groupinstall 'Development Tools' (RHEL/CentOS)"
        exit 1
    fi
    
    # Create temporary directory
    TMP_DIR=$(mktemp -d)
    trap "rm -rf $TMP_DIR" EXIT
    
    # Clone the repository
    log_info "Cloning repository..."
    if command -v git &> /dev/null; then
        git clone --depth 1 "${REPO_URL}.git" "$TMP_DIR/dust" || {
            log_error "Failed to clone repository"
            exit 1
        }
    else
        log_info "Git not found, downloading source archive..."
        curl -fsSL "${REPO_URL}/archive/main.tar.gz" | tar -xz -C "$TMP_DIR"
        mv "$TMP_DIR/dust-main" "$TMP_DIR/dust"
    fi
    
    # Build
    log_info "Building..."
    cd "$TMP_DIR/dust"
    make clean
    make release || {
        log_error "Build failed"
        exit 1
    }
    
    # Install
    log_info "Installing..."
    make install
    
    log_success "Dust has been built and installed successfully!"
}

confirm_installation() {
    echo ""
    echo -e "${YELLOW}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${YELLOW}║                   IMPORTANT - PLEASE READ                    ║${NC}"
    echo -e "${YELLOW}╠════════════════════════════════════════════════════════════════╣${NC}"
    echo -e "${YELLOW}║                                                                ║${NC}"
    echo -e "${YELLOW}║  You are about to install DUST init system on this machine.   ║${NC}"
    echo -e "${YELLOW}║                                                                ║${NC}"
    echo -e "${YELLOW}║  This will:                                                    ║${NC}"
    echo -e "${YELLOW}║  • Install dust-init binary to /usr/sbin/                     ║${NC}"
    echo -e "${YELLOW}║  • Install dust CLI to /usr/bin/                              ║${NC}"
    echo -e "${YELLOW}║  • Create configuration directories in /etc/dust/             ║${NC}"
    echo -e "${YELLOW}║                                                                ║${NC}"
    echo -e "${YELLOW}║  If you choose to set DUST as the default init (optional):   ║${NC}"
    echo -e "${YELLOW}║  • This will REPLACE your current init system (systemd, etc.) ║${NC}"
    echo -e "${YELLOW}║  • The change will take effect on next reboot                 ║${NC}"
    echo -e "${YELLOW}║  • A backup of the original init will be created              ║${NC}"
    echo -e "${YELLOW}║                                                                ║${NC}"
    echo -e "${YELLOW}╚════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    while true; do
        echo -e "${YELLOW}Do you want to continue with the installation? (yes/no): ${NC}"
        read -r response
        
        case "$response" in
            [Yy]|[Yy][Ee][Ss])
                echo ""
                log_info "Proceeding with installation..."
                echo ""
                return 0
                ;;
            [Nn]|[Nn][Oo])
                echo ""
                log_info "Installation cancelled by user."
                echo ""
                exit 0
                ;;
            *)
                echo ""
                log_warning "Please answer 'yes' or 'no'."
                echo ""
                ;;
        esac
    done
}

main() {
    print_banner
    
    # Ask for user confirmation before doing anything
    confirm_installation
    
    check_root
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --version)
                VERSION="$2"
                shift 2
                ;;
            --prefix)
                INSTALL_PREFIX="$2"
                shift 2
                ;;
            --build-from-source)
                BUILD_FROM_SOURCE=1
                shift
                ;;
            --help)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --version VERSION       Install specific version (default: latest)"
                echo "  --prefix PREFIX         Installation prefix (default: /usr)"
                echo "  --build-from-source     Build from source instead of downloading binary"
                echo "  --help                  Show this help message"
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    if [[ "${BUILD_FROM_SOURCE}" == "1" ]]; then
        build_from_source
    else
        download_release "${VERSION}"
    fi
    
    echo ""
    log_success "Installation complete!"
    echo ""
    echo "To get started:"
    echo "  dust --help          Show help"
    echo "  dust status          Check system status"
    echo ""
    echo "Configuration directory: /etc/dust/"
    echo "Log file: /var/log/dust.log"
    
    # Ask if user wants to set Dust as default init
    set_as_default_init
}

set_as_default_init() {
    echo ""
    echo -e "${YELLOW}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${YELLOW}║           SET DUST AS DEFAULT INIT SYSTEM                      ║${NC}"
    echo -e "${YELLOW}╠════════════════════════════════════════════════════════════════╣${NC}"
    echo -e "${YELLOW}║                                                                ║${NC}"
    echo -e "${YELLOW}║  This operation will:                                          ║${NC}"
    echo -e "${YELLOW}║                                                                ║${NC}"
    echo -e "${YELLOW}║  1. Create a backup of your current init system                  ║${NC}"
    echo -e "${YELLOW}║  2. Replace /sbin/init with dust-init                         ║${NC}"
    echo -e "${YELLOW}║  3. Configure the system to use Dust on next reboot             ║${NC}"
    echo -e "${YELLOW}║  4. Set up the Dust daemon to start automatically               ║${NC}"
    echo -e "${YELLOW}║                                                                ║${NC}"
    echo -e "${YELLOW}║  ⚠️  WARNING: This is a critical system change!               ║${NC}"
    echo -e "${YELLOW}║                                                                ║${NC}"
    echo -e "${YELLOW}║  If something goes wrong:                                      ║${NC}"
    echo -e "${YELLOW}║  • Your system may not boot                                    ║${NC}"
    echo -e "${YELLOW}║  • You may need a recovery disk/USB                             ║${NC}"
    echo -e "${YELLOW}║  • The backup can be restored from recovery mode                ║${NC}"
    echo -e "${YELLOW}║                                                                ║${NC}"
    echo -e "${YELLOW}╚════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    while true; do
        echo -e "${YELLOW}Do you want to set DUST as the default init system? (yes/no): ${NC}"
        read -r response
        
        case "$response" in
            [Yy]|[Yy][Ee][Ss])
                echo ""
                log_info "Setting DUST as default init system..."
                echo ""
                configure_dust_as_init
                return 0
                ;;
            [Nn]|[Nn][Oo])
                echo ""
                log_info "Skipping init configuration. DUST binaries are installed"
                log_info "but your current init system will remain unchanged."
                echo ""
                return 0
                ;;
            *)
                echo ""
                log_warning "Please answer 'yes' or 'no'."
                echo ""
                ;;
        esac
    done
}

configure_dust_as_init() {
    # Step 1: Create backup of current init
    log_info "Step 1/4: Creating backup of current init system..."
    
    local backup_suffix=$(date +%Y%m%d_%H%M%S)
    local init_backup="/sbin/init.backup.${backup_suffix}"
    
    if [[ -f /sbin/init ]]; then
        cp /sbin/init "$init_backup"
        log_success "Backup created at: $init_backup"
    else
        log_warning "/sbin/init not found, skipping backup"
    fi
    
    # Also backup systemd if present
    if [[ -f /lib/systemd/systemd ]]; then
        cp /lib/systemd/systemd "/lib/systemd/systemd.backup.${backup_suffix}"
        log_success "Systemd backup created"
    fi
    
    echo ""
    
    # Step 2: Replace init with dust-init
    log_info "Step 2/4: Replacing init with dust-init..."
    
    cp "${INSTALL_PREFIX}/sbin/dust-init" /sbin/init
    chmod 755 /sbin/init
    
    # Also link to /init for compatibility
    ln -sf /sbin/init /init 2>/dev/null || true
    
    log_success "dust-init is now installed as /sbin/init"
    
    echo ""
    
    # Step 3: Create dust service configuration
    log_info "Step 3/4: Configuring dust services..."
    
    # Create basic service files
    mkdir -p /etc/dust/services
    mkdir -p /etc/dust/targets
    
    # Create a basic multi-user target if not exists
    if [[ ! -f /etc/dust/targets/multi-user.yaml ]]; then
        cat > /etc/dust/targets/multi-user.yaml << 'EOF'
[Unit]
Description=Multi-User System
Requires=basic.target
After=basic.target
EOF
        log_info "Created default multi-user target"
    fi
    
    log_success "Dust service configuration created"
    
    echo ""
    
    # Step 4: Configure bootloader to use new init
    log_info "Step 4/4: Configuring bootloader..."
    
    # Detect bootloader and configure
    if [[ -f /boot/grub/grub.cfg ]] || [[ -d /boot/grub ]]; then
        # GRUB detected
        log_info "GRUB bootloader detected"
        
        # Check if init= parameter is already set
        if grep -q "init=" /etc/default/grub 2>/dev/null; then
            # Backup grub config
            cp /etc/default/grub /etc/default/grub.backup.${backup_suffix}
            
            # Update init parameter
            sed -i 's|init=[^ ]*|init=/sbin/init|g' /etc/default/grub
            
            # Update GRUB
            if command -v update-grub &> /dev/null; then
                update-grub
                log_success "GRUB configuration updated"
            elif command -v grub2-mkconfig &> /dev/null; then
                grub2-mkconfig -o /boot/grub2/grub.cfg
                log_success "GRUB2 configuration updated"
            fi
        else
            log_info "Add 'init=/sbin/init' to GRUB_CMDLINE_LINUX in /etc/default/grub"
            log_info "Then run: update-grub"
        fi
        
    elif [[ -f /boot/syslinux/syslinux.cfg ]] || [[ -f /boot/syslinux/extlinux.conf ]]; then
        # Syslinux detected
        log_info "Syslinux bootloader detected"
        log_info "Add 'init=/sbin/init' to your kernel parameters"
        
    elif [[ -d /boot/loader ]] && [[ -f /boot/loader/loader.conf ]]; then
        # systemd-boot detected
        log_info "systemd-boot detected"
        log_info "Add 'init=/sbin/init' to your kernel command line in boot entries"
        
    else
        log_warning "Could not detect bootloader automatically"
        log_info "You may need to manually add 'init=/sbin/init' to your kernel parameters"
    fi
    
    echo ""
    log_success "DUST has been configured as the default init system!"
    echo ""
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                    NEXT STEPS                                  ║${NC}"
    echo -e "${GREEN}╠════════════════════════════════════════════════════════════════╣${NC}"
    echo -e "${GREEN}║                                                                ║${NC}"
    echo -e "${GREEN}║  1. Reboot your system to start using DUST:                   ║${NC}"
    echo -e "${GREEN}║       sudo reboot                                             ║${NC}"
    echo -e "${GREEN}║                                                                ║${NC}"
    echo -e "${GREEN}║  2. After reboot, check DUST is running:                      ║${NC}"
    echo -e "${GREEN}║       dust status                                             ║${NC}"
    echo -e "${GREEN}║                                                                ║${NC}"
    echo -e "${GREEN}║  3. To rollback to your previous init system:                 ║${NC}"
    echo -e "${GREEN}║       sudo cp /sbin/init.backup.* /sbin/init                  ║${NC}"
    echo -e "${GREEN}║                                                                ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

main "$@"
