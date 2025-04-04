# Makefile for the Windows IPC Job Dispatcher

# Set the compiler to g++ (the GNU C++ compiler)
CC = g++


CFLAGS = -Wall -Wextra -std=c++17

# The name of the final executable
TARGET = system_IPC.exe

# The source file that needs to be compiled
SOURCES = system_IPC.cpp

build: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)


clean:
	del $(TARGET)
