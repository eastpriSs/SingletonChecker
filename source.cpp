class X
{
private:
    static X singleton2;
    X() {}    
public:
    static X& getInstance2(){
        return singleton2;
    }
    static X& getInstance1(){
        static X singleton1;
        return singleton1;
    }
    void y(){}
    void z(){}

    X(const X&) = delete;
    X& operator=(const X&) = delete;

};

template <typename T>
T& single(){
    static T singleton;
    return singleton;
}

int main(){
}
