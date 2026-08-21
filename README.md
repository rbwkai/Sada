# Sada (صَدى) Arabic Vocab Engine

**Sada** (صَدى) is a C++ based engine designed to process and manage Arabic vocabulary datasets using built-in datasets or custom CSV files.


## How Sada Works

Sada makes mastering Arabic vocabulary seamless and structured:

* **Clear Flashcards**: When you run Sada, it presents an Arabic word alongside its pronunciation guide and English translation.

* **Smart Spaced Repetition**: Sada tracks your practice history and schedules timely word reviews to maximize long-term retention.

* **Flexible Datasets**: Learn directly from the built-in 2,000-word Arabic dataset or load your own custom word lists.


## Requirements

- **C++ Compiler**: GCC (10+), Clang (11+), or MSVC with C++17 support.
- **CMake**: Version 3.10 or higher.
- **Make** / **Ninja** Or any CMake-compatible build system.


## Building and Running

### 1. Clone the Repository

```bash
git clone https://github.com/rbwkai/Sada.git
cd Sada
```

### 2. Build with CMake

```bash
# Generate build configuration files
cmake -B build -S .

# Compile the executable
cmake --build build
```

### 3. Run the Executable

```bash
# On Linux/macOS
./build/Sada

# On Windows (Command Prompt / PowerShell)
.\build\Debug\Sada.exe
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.



