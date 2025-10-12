class NaiveSingleton {
private:
    static NaiveSingleton* instance;
    
    NaiveSingleton() {}
    NaiveSingleton(const NaiveSingleton&) = delete;
    NaiveSingleton& operator=(const NaiveSingleton&) = delete;

public:
    static const NaiveSingleton* getInstance() {
        return (nullptr != instance) ? instance : instance = new NaiveSingleton();
    }
    
    
};

NaiveSingleton* NaiveSingleton::instance = nullptr;
