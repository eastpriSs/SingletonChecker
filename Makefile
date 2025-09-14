LLVM_FLAGS = $(shell llvm-config --cxxflags --ldflags --system-libs --libs core)
SOURCE ?= source.cpp

all: SingletonChecker.so test

SingletonChecker.so: 
	clang++ -std=c++17 -fno-rtti -fPIC -I$(shell llvm-config --includedir) -shared SingltonCheckerMain.cpp -o SingletoneChecker.so $(LLVM_FLAGS)

test: SingletoneChecker.so $(SOURCE)
	clang++ -fsyntax-only -Xclang -load -Xclang ./SingletoneChecker.so -Xclang -plugin -Xclang class-visitor $(SOURCE)

clean:
	rm -f SingletoneChecker.so

.PHONY: all test clean
