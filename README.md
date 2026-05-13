# 💨 freeDustInit

<img src="assets/banner.png" width="2048" height="1024">

[![Version](https://img.shields.io/badge/version-0.1.0-blue.svg)](https://github.com/freeDustInit/src)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

**Dust** is a modern, lightweight and innovative init system for Linux, designed as an alternative to s(hittys)ystemd.

## ✨ Features

- 🚀 **Ultra-light**: Binary < 1MB, minimal RAM usage
- ⚡ **Fast startup**: Optimized for the fastest possible boot
- 🔄 **Socket activation**: On-demand service activation
- 🛡️ **Auto-restart**: Automatic recovery of crashed services
- 📊 **Dependency management**: Intelligent dependency handling
- 🎯 **Target system**: Service organization by target (boot, multi-user, graphical, etc.)
- 🔧 **Simple configuration**: Easy-to-understand YAML configuration files
- 🐚 **Intuitive CLI**: `dust` command to manage the whole system

## 🚀 Quick Installation

```bash
curl -fsSL https://raw.githubusercontent.com/freeDustInit/src/main/install.sh | sudo sh
```

For a specific release version:

```bash
curl -fsSL https://raw.githubusercontent.com/freeDustInit/src/main/install.sh | sudo sh -s -- --version 0.1.0
```

## 📖 How to Use

### Manage services

```bash
# Start service
dust start nginx

# Stop service
dust stop nginx

# Restart service
dust restart nginx

# Service status
dust status nginx

# Enable service at boot
dust enable nginx

# Disable service at boot
dust disable nginx
```

### Manage the system

```bash
# Change target (runlevel)
dust isolate multi-user.target

# Current default target
dust get-default

# Set default target
dust set-default graphical.target

# Shutdown the system
dust poweroff

# Restart the system
dust reboot
```

### List of services and targets

```bash
# All services
dust list-units

# Only active services
dust list-units --state=active

# Failed services only
dust list-units --state=failed
```

## ⚙️ Configuration

Configuration files are in `/etc/dust/services/` and `/etc/dust/targets/`.

### Service example (`/etc/dust/services/nginx.yaml`):

```yaml
[Unit]
Description=NGINX web server
After=network.target
Wants=network.target

[Service]
Type=forking
ExecStart=/usr/sbin/nginx
ExecReload=/usr/sbin/nginx -s reload
ExecStop=/usr/sbin/nginx -s stop
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

### Service types

- **simple**: Command stays in foreground foreground (default)
- **forking**: Command forks and exits
- **oneshot**: Executes once and exits
- **notify**: Like simple but notifies when ready
- **dbus**: Waits for DBus

### Restart options

- **no**: Never restart the service ever (default)
- **always**: Restart always
- **on-success**: Restart only if exit 0
- **on-failure**: Restart only if exit != 0
- **on-abnormal**: Restart if killed by signal o timeout
- **on-abort**: Restart if killed by SIGABRT
- **on-watchdog**: Restart if watchdog timeout

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        PID 1 (dust-init)                    │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐     │
│  │  Target  │  │  Target  │  │  Target  │  │  Target  │     │
│  │ Manager  │  │   Boot   │  │Multi-User│  │Graphical │     │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘     │
│       │             │             │             │           │
│       └─────────────┴─────────────┴─────────────┘           │
│                         │                                   │
│                  ┌──────┴──────┐                            │
│                  │   Service   │                            │
│                  │   Manager   │                            │
│                  └──────┬──────┘                            │
│                         │                                   │
│       ┌─────────────────┼─────────────────┐                 │
│       ▼                 ▼                 ▼                 │
│  ┌─────────┐      ┌─────────┐      ┌─────────┐              │
│  │ Service │      │ Service │      │ Service │              │
│  │   A     │      │   B     │      │   C     │              │
│  └─────────┘      └─────────┘      └─────────┘              │
└─────────────────────────────────────────────────────────────┘
```

## 🤝 How to contribute

Contributions are welcome! Please follow these steps:

1. Fork the repository
2. Create a branch for your feature (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📝 License

MIT License - check [LICENSE](LICENSE) for details.

## 🙏 Special Thanks

- Kimi-K2.5 LLM - Bufixes, Initial development, README.md stucture + some ideas
- tsryzen - Project Founder
- Heavly inspired by runit, s6, e openrc
- Brought to you thanks to our deep hate for systemd
- Thanks to the Linux/Open Source community

---

<p align="center">
  Made with ❤️ by freeDust Team, in Italy 🇮🇹
</p>
