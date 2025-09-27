template<typename T>
class CRTPSingleton {
protected:
    CRTPSingleton() {}
    CRTPSingleton(const CRTPSingleton&) = delete;
    CRTPSingleton& operator=(const CRTPSingleton&) = delete;

public:
    static T& getInstance() {
        static T instance;
        return instance;
    }
};

class MySingleton : public CRTPSingleton<MySingleton> {
    friend class CRTPSingleton<MySingleton>;
    
private:
    MySingleton() {}
};
