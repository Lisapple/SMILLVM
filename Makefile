CC=clang++
CFLAGS=-O3 -Wall -g
SRCS=Parser.cpp Token.cpp ObjectType.cpp Expr.cpp CodeGen.cpp HashTable.cpp Utilities.cpp SMIL\ Parser.cpp
TARGET=SMIL
CONFIG=`llvm-config --cxxflags --ldflags --libs core mcjit interpreter native nativecodegen bitwriter`

all: build

build: SMIL\ Parser.cpp
	$(CC) $(CFLAGS) $(SRCS) $(CONFIG) -o $(TARGET)

run:
	./$(TARGET) test.sl
