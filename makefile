# The compiler: gcc for C program, define as g++ for C++
CC = gcc

# Compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS = -g -Wall

# The build target executables:
P1_TARGET = tictactoeServer
P2_TARGET = tictactoeClient
TARGETS = $(P1_TARGET) $(P2_TARGET)

# Process to build application
all: $(TARGETS)

$(P1_TARGET): $(P1_TARGET).c
	$(CC) $(CFLAGS) -o $@ $<

$(P2_TARGET): $(P2_TARGET).c
	$(CC) $(CFLAGS) -o $@ $<

# Target to open all lab files
openAll: openDoc openCode

# Target to open lab documentation files
openDoc: *.md makefile
	code $^

# Target to open lab source code files
openCode: makefile $(TARGETS:=.c)
	code $^

# Remove executables for clean build
clean:
	$(RM) $(TARGETS)
