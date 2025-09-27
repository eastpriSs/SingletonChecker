#include <iostream>

class X
{
private:
    X() {}    
public:
    X(const X&) = delete;
    X& operator=(const X&) = delete;
    static X& intrestingFriend() {
        static X x;
        return x;
    }
};



int main(){
}
