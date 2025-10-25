#include <iostream>

class X
{
private:
    X() {}    
public:
    X(const X&) = delete;
    X& operator=(const X&) = delete;
    static X& getInstance() {
        static X* instance = new X();
        return *instance;
    }
};
int main(){
}
