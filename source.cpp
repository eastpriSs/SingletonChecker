class X
{
private:
    X() {}    
public:
    static X& getInstance(){
        static X singleton = X();
        static X singleton2 = X();
        return singleton2;
    }
    void y(){}
    void z(){}

};


int main(){
}
