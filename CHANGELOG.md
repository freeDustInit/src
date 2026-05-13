# Changelog

All notable changes to Dust will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial implementation of PID 1 init system
- Service management with start/stop/restart/status commands
- Target-based runlevel system (boot, multi-user, graphical, rescue)
- Auto-restart functionality with configurable policies
- Service dependency management (After, Before, Wants, Requires)
- Environment variable support for services
- Nice level and working directory configuration
- Comprehensive CLI with colored output
- YAML-based service and target configuration
- Socket activation support (framework)
- Cross-platform Makefile with debug/release builds
- One-line installer script with auto-detection

### Security
- Services run with configurable user/group
- Proper PID file management
- Secure signal handling

## [0.1.0] - 2024-01-XX

### Added
- Initial release
- Basic init system functionality
- Service management commands
- Target system implementation
- CLI interface
- Installation script
- Documentation and examples

[Unreleased]: https://github.com/yourusername/dust/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/yourusername/dust/releases/tag/v0.1.0
