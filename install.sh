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
    
    # Determine download URLs
    if [[ "$version" == "latest" ]]; then
        log_info "Downloading latest release..."
        DUST_INIT_URL="${REPO_URL}/releases/latest/download/dust-init"
        DUST_URL="${REPO_URL}/releases/latest/download/dust"
    else
        log_info "Downloading version $version..."
        DUST_INIT_URL="${REPO_URL}/releases/download/${version}/dust-init"
        DUST_URL="${REPO_URL}/releases/download/${version}/dust"
    fi
    
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

main() {
    print_banner
    
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
}

main "$@"
