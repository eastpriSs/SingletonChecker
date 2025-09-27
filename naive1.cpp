class NaiveSingleton {
private:
    static NaiveSingleton* instance;
    
    NaiveSingleton() {}
    NaiveSingleton(const NaiveSingleton&) = delete;
    NaiveSingleton& operator=(const NaiveSingleton&) = delete;

public:
    static const NaiveSingleton* getInstance() {
        if (instance == nullptr) {
            instance = new NaiveSingleton();
        }
        return instance;
    }
    
    static void destroy() {
        delete instance;
        instance = nullptr;
    }
};

NaiveSingleton* NaiveSingleton::instance = nullptr;
