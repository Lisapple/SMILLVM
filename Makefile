CC=clang++
CFLAGS=-O3 -Wall -g
# -std=c++11 -stdlib=libc++
SRCS=Parser.cpp Token.cpp ObjectType.cpp Expr.cpp CodeGen.cpp HashTable.cpp Utilities.cpp SMIL\ Parser.cpp
TARGET=SMIL
CONFIG=`llvm-config --cxxflags --ldflags --libs core mcjit interpreter native nativecodegen bitwriter`

all: build

build: SMIL\ Parser.cpp
	$(CC) $(CFLAGS) $(SRCS) $(CONFIG) -o $(TARGET)

run:
	./$(TARGET) test.sl

test:
	./$(TARGET) test3golf.sl 2 + 2 2 12 3 6 hello 3 el
