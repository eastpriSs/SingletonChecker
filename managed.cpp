class ManagedSingleton {
private:
    static ManagedSingleton* instance;
    
    ManagedSingleton() {}
    ManagedSingleton(const ManagedSingleton&) = delete;
    ManagedSingleton& operator=(const ManagedSingleton&) = delete;

public:
    static void create() {
        if (!instance) {
            instance = new ManagedSingleton();
        }
    }
    
    static ManagedSingleton* getInstance() {
        return instance;
    }
    
    static void destroy() {
        delete instance;
        instance = nullptr;
    }
};

ManagedSingleton* ManagedSingleton::instance = nullptr;
