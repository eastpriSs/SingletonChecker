#include <memory>
#include <mutex>

class Singleton {
private:
    
    Singleton() = default; 
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    
    static std::unique_ptr<Singleton> instance;
    static std::once_flag initFlag;

public:

    static Singleton& getInstance() {
        std::call_once(initFlag, []() {
            instance = std::unique_ptr<Singleton>(new Singleton());
        });
        return *instance;
    }

    ~Singleton() = default;

};

int main(){}
