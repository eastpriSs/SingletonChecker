class NaiveSingleton {
private:
   
    static bool isCreated;
    static NaiveSingleton* instance;
    
    NaiveSingleton() {
        isCreated = true;
    }
    NaiveSingleton(const NaiveSingleton&) = delete;
    NaiveSingleton& operator=(const NaiveSingleton&) = delete;

public:
    static const NaiveSingleton* getInstance() {
        return (isCreated) ? instance : (instance = new NaiveSingleton());
    }
};

NaiveSingleton* NaiveSingleton::instance = nullptr;
