class X
{
private:
    static X singleton2;
    X() {}    
public:
    
    void y(){}
    void z(){}
    static X& singletonxd(){
        static X s;
        static X s2;
        return s;
    }
    static X& singleton(){
        static X s;
        static X s2;
        return s;
    }

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
