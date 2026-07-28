# ⚡ CLI Utils

> A high-performance collection of lightweight command-line utilities built in **C** for Linux, macOS, Windows, and Android (Termux).

[![Language](https://img.shields.io/badge/Language-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platforms](https://img.shields.io/badge/Platforms-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Android-orange.svg)](#supported-platforms)
[![Build Status](https://github.com/PLGCesar/cli-utils/actions/workflows/build.yml/badge.svg)](https://github.com/PLGCesar/cli-utils/actions)

---

## 📋 Requirements

Before using or compiling any utility, ensure you have:
- **Compiler:** `gcc` or `clang` (C99 standard)
- **Build Tool:** `make`

---

## 🚀 Projects Overview

| Utility | Description | Tech Stack |
| :--- | :--- | :--- |
| 🌐 **`ip-info`** | Network inspector (WAN/LAN, Geolocation, DNS, Proxy, Latency) | `C99` |
| 📂 **`template`** | Folder structure scaffolder with ASCII tree preview & export/import | `C99` |
| 🧮 **`calculator`** | Fast cross-platform CLI calculator | `C99` |
| 🚀 **`warp/mark`** | Directory bookmarking and instant navigation tool | `C99` + `Bash/Sh` |
| 📝 **`note`** | CLI note manager with priority levels and shell startup reminders | `C99` |
| 🔐 **`genpass`** | Secure password & key generator with custom character sets | `C99` |
| 🛡️ **`shredder`** | Secure file destruction tool with multi-pass random data overwriting | `C99` |
| ⚡ **`lit`** | Ultra-fast local Git alternative for folder snapshot versioning | `C99` |

---

## 🛠️ Utilities Showcase

### 🌐 IP-Info
- **Description:** A cross-platform network inspector that detects WAN/LAN IPs, geolocation, ISP, DNS resolvers, and environment proxies. Includes an interactive **Expert Mode** with Cloudflare TCP latency testing.
- **Language:** C (gcc/clang)

### 📂 Template
- **Description:** A directory structure scaffolder that maps folder hierarchies, saves them as reusable templates, and unpacks them anywhere. Features ASCII tree previews, export to Downloads, and import capabilities.
- **Language:** C (gcc/clang)

### 🧮 Calculator
- **Description:** A universal calculator for CLI environments across Windows, Mac, Linux, and Android. Designed for lightning-fast compilation and instant execution.
- **Language:** C (gcc)

### 🚀 Warp / Mark
- **Description:** A directory bookmarking utility that lets you instantly jump between folders from anywhere using `mark` and `warp`. Stress-free folder navigation.
- **Language:** C (gcc) + Bash/Sh

### 📝 Note
- **Description:** A smart CLI note manager with priority levels and automatic reminders displayed when opening a new Bash/Zsh shell session.
- **Language:** C (gcc)

### 🔐 Genpass
- **Description:** A universal password generator that generates secure strings including special characters, uppercase/lowercase letters, and numbers (up to 16 characters).
- **Language:** C (gcc)

### 🛡️ Shredder
- **Description:** A secure file destruction tool that overwrites sensitive files multiple times with random garbage data before unlinking to prevent disk recovery.
- **Language:** C (gcc/clang)

### ⚡ Lit
- **Description:** An ultra-fast local Git alternative that creates versioned directory snapshots in `.snapshot/` with instant restore capabilities.
- **Language:** C (gcc/clang)

---

## 📦 Installation & Quickstart

To build and install any utility manually with its shell alias automatically configured:

```bash
# 1. Clone the repository
git clone https://github.com/PLGCesar/cli-utils.git

# 2. Enter the utility directory
cd cli-utils/<utility-folder>

# 3. Compile and set up alias in 1 command
make

# 4. Reload your shell configuration
source ~/.bashrc  # or source ~/.zshrc
```
# ***(Open Source Project, under MIT License, feel free to use)***
