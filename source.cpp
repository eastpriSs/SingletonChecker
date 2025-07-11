class X
{
private:
    static X singleton2;
    X() {}    
public:
    static X& getInstance(){
        return singleton2;
    }
    void y(){}
    void z(){}

    X(const X&) = delete;
    X& operator=(const X&) = delete;

};


int main(){
}
