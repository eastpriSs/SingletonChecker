#include <iostream>

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

    friend X& intrestingFriend();
};

X& intrestingFriend() {
    static X x;
    return x;
}

int main(){
}
