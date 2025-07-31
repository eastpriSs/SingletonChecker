class X
{
private:
    int ok;
    bool io;
    static X singleton2;
    static X singleton3;
    static X singleton4;
    X() {}    
public:
    
    void y(){ singleton2.z();  }
    void z(){ X s;  }

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
