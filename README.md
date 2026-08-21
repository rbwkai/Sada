# Sada (صَدى) Arabic Vocab Engine

**Sada** (صَدى) is a C++ based engine designed to process and manage Arabic vocabulary datasets using built-in datasets or custom CSV files.


## 🚀 Features

- **Arabic Vocabulary Engine**: Implements core logic (`sada.h`, `engine.cpp`) to handle flashcards, datasets, and vocabulary management.

- **Database & File I/O**: Loads, parses, and manages datasets (such as `sada_arabic_2000.csv`).

- **Card & Dataset Abstractions**: Separate modules (`card.cpp`, `dataset.cpp`, `db.cpp`) to handle flashcard management and scheduling logic.

- **Modern C++ & CMake Support**: Modular structure configured via `CMakeLists.txt` for easy compilation across platforms.


## 💻 Requirements

- **C++ Compiler**: GCC (10+), Clang (11+), or MSVC with C++17 support.
- **CMake**: Version 3.10 or higher.
- **Make** / **Ninja** Or any CMake-compatible build system.


## 📦 Building and Running

### 1. Clone the Repository

```bash
git clone https://github.com/your-username/Sada.git
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

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.



