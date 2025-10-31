#include <iostream>
#include <memory>

class Logger {
private:
    Logger() = default;  // Приватный конструктор
    static std::unique_ptr<Logger> instance;  // Умный указатель для управления

public:
    static Logger& get() {
        if (!instance) {
            instance = std::unique_ptr<Logger>(new Logger());
        }
        return *instance;
    }

    void log(const std::string& message) {
        std::cout << "[LOG] " << message << "\n";
    }

    // Запрещаем копирование
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};

std::unique_ptr<Logger> Logger::instance = nullptr;  // Инициализация статического поля

int main() {
    Logger::get().log("Hello, hidden Singleton!");
    return 0;
}
