#include <iostream>
class X;
class Z{
    static X hidden_instance1;
};

class X
{
private:
    X() {}
public:
    X(const X&) = delete;
    X& operator=(const X&) = delete;
    friend X& getInstance();    
    friend class Y;
    friend class Z;
};

X& getInstance(){
    static X inst;
    return inst;
}

int main(){
}
