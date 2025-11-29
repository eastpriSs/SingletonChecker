template<typename T>
static T& single(){
  static T instance;
  return instance;
}


template<typename T>
static T single(){
  static T instance;
  return instance;
}


int f(){ return 2; };
