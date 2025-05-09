# Makefile for compiling test.cpp with the given flags

# Compiler and flags
CXX = g++
CXXFLAGS = -pthread
LDFLAGS = -lglfw -lGLU -lGL -lXrandr -lXxf86vm -lXi -lXinerama -lX11 -lrt -ldl

SRC = main

all:
	$(CXX) $(CXXFLAGS) -o out $(SRC).cpp lib/glad.c $(LDFLAGS)
	./out

clean:
	$(RM) $(SRC)	

