class NaiveSingleton {
private:
    static NaiveSingleton* instance;
    
    NaiveSingleton() {}
    NaiveSingleton(const NaiveSingleton&) = delete;
    NaiveSingleton& operator=(const NaiveSingleton&) = delete;

public:
    static const NaiveSingleton* getInstance() {
        if  (!instance) 
            instance = new NaiveSingleton();
        return  instance;
    }
    
    
};

NaiveSingleton* NaiveSingleton::instance = nullptr;
