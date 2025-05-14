# Makefile for compiling test.cpp with the given flags

# Compiler and flags
CXX = g++
CXXFLAGS = -pthread
LDFLAGS = -lglfw -lGLU -lGL -lXrandr -lXxf86vm -lXi -lXinerama -lX11 -lrt -ldl

SRC = main
IMGUI_SRC = imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_tables.cpp imgui/imgui_impl_glfw.cpp imgui/imgui_impl_opengl3.cpp

all:
	$(CXX) $(CXXFLAGS) -o out $(SRC).cpp lib/glad.c $(IMGUI_SRC) $(LDFLAGS)
	./out

clean:
	$(RM) out

