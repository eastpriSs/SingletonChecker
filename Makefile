LLVM_FLAGS = $(shell llvm-config --cxxflags --ldflags --system-libs --libs core)

all: SingletonChecker.so test

SingletonChecker.so: 
	clang++ -std=c++17 -fno-rtti -fPIC -I$(shell llvm-config --includedir) -shared SingltonCheckerMain.cpp -o SingletoneChecker.so $(LLVM_FLAGS)

test: SingletoneChecker.so source.cpp
	clang++ -fsyntax-only -Xclang -load -Xclang ./SingletoneChecker.so -Xclang -plugin -Xclang class-visitor source.cpp

clean:
	rm -f SingletoneChecker.so

.PHONY: all test clean
