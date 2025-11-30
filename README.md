# Singleton Pattern Checker - Clang Plugin

Статический анализатор на основе Clang для автоматического обнаружения и анализа шаблонов проектирования Singleton в C++ коде.

## 📋 О проекте

Этот проект представляет собой плагин для Clang, который анализирует C++ код и определяет различные реализации шаблона Singleton. Плагин способен распознавать несколько популярных вариаций Singleton и предоставляет детальный отчет о найденных паттернах.

## ✨ Возможности

- **Автоматическое обнаружение Singleton** в исходном коде C++
- **Поддержка различных паттернов Singleton**:
  - Naive Singleton (статическое поле класса)
  - Meyer's Singleton (статическая локальная переменная)
  - CRTP Singleton (Curiously Recurring Template Pattern)
  - If-Naive Singleton (условная инициализация)
  - Flags-Naive Singleton (флаговая инициализация)
- **Анализ условий инициализации** в GetInstance методах
- **Проверка корректности реализации**:
  - Приватные конструкторы
  - Удаленные копирующие конструкторы
  - Удаленные операторы присваивания
- **Детальная отчетность** с визуализацией результатов

## 🛠 Установка и компиляция

### Требования

- Clang/LLVM 10.0+
- CMake 3.10+
- Компилятор C++ с поддержкой C++14

### Сборка

```bash
# Клонирование репозитория
git clone <repository-url>
cd singleton-checker

# Создание директории для сборки
mkdir build
cd build

# Конфигурация проекта
cmake -DLLVM_DIR=/path/to/llvm/cmake ..

# Компиляция
make
```

## 🚀 Использование

### Как плагин Clang

```bash
clang -Xclang -load -Xclang /path/to/libSingletonChecker.so -Xclang -plugin -Xclang class-visitor your_file.cpp
```


## 📊 Пример вывода

Плагин генерирует детализированные отчеты в формате:

```
╔══════════════════════════════════════════════════════════════════════════════════════╗
║                           SINGLETON PATTERN ANALYSIS REPORT                          ║
╠══════════════════════════════════════════════════════════════════════════════════════╣
│ 📋 CLASS INFORMATION
│   • Class Name: MySingleton
│   • Location: /path/to/file.cpp:10
╠══════════════════════════════════════════════════════════════════════════════════════╣
│ 🔍 SINGLETON PATTERN ANALYSIS
│ Core Requirements:
│   • Private Constructors: ✓ YES
│   • Deleted Copy Constructor: ✓ YES
│   • Deleted Assignment Operator: ✓ YES
│   • Static Instances Count: 1
│ GetInstance Method Analysis:
│   • GetInstance Method Found: ✓ YES
│   • Method Name: getInstance
│   • Method Access: public
│   • Method Location: /path/to/file.cpp:15
│   • Method Hidden: ✗ NO
│   • Has Method Body: ✓ YES
╚══════════════════════════════════════════════════════════════════════════════════════╝
```

## 🔍 Детектируемые паттерны

### Naive Singleton
```cpp
class NaiveSingleton {
private:
    static NaiveSingleton* instance;
    NaiveSingleton() = default;
public:
    static NaiveSingleton* getInstance() {
        if (!instance) {
            instance = new NaiveSingleton();
        }
        return instance;
    }
};
```

### Meyer's Singleton
```cpp
class MeyersSingleton {
private:
    MeyersSingleton() = default;
public:
    static MeyersSingleton& getInstance() {
        static MeyersSingleton instance;
        return instance;
    }
};
```

### CRTP Singleton
```cpp
template<typename T>
class CRTPSingleton {
protected:
    CRTPSingleton() = default;
public:
    static T& getInstance() {
        static T instance;
        return instance;
    }
};

```

## 🎯 Особенности анализа

- **Глубокий анализ AST** - полный обход абстрактного синтаксического дерева
- **Контекстно-зависимая проверка** - учет областей видимости и модификаторов доступа
- **Обнаружение анти-паттернов** - выявление некорректных реализаций Singleton
- **Поддержка дружественных функций** - анализ friend-функций и классов


---
