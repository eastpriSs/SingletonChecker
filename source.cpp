class X
{
private:
    int ok;
    bool io;
    X() {}    
public:
    
    void y(){  }
    void z(){  }


    X(const X&) = delete;
    X& operator=(const X&) = delete;

    friend void intrestingFriend();
    friend class Y;
};

class Y {
    static X s;
    X x;
};

template <typename T>
T& single(){
    static T singleton;
    return singleton;
}

void intrestingFriend(){
}

int main(){
}
