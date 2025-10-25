LLVM_FLAGS = $(shell llvm-config --cxxflags --ldflags --system-libs --libs core)
SOURCE ?= source.cpp

DEV_FLAGS = -std=c++17 -fno-rtti -fPIC -shared -g -O1 -ferror-limit=3

all: clean SingletonChecker.so test

SingletonChecker.so: 
	clang++ $(DEV_FLAGS) -I$(shell llvm-config --includedir) SingltonCheckerMain.cpp -o SingletonChecker.so $(LLVM_FLAGS)

test: SingletonChecker.so $(SOURCE)
	clang++ -fsyntax-only -Xclang -load -Xclang ./SingletonChecker.so -Xclang -plugin -Xclang class-visitor $(SOURCE)

clean:
	rm -f SingletonChecker.so

.PHONY: all test clean

