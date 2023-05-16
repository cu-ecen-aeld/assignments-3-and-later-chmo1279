# the compiler: gcc for C program, define as g++ for C++
CC = $(CROSS_COMPILE)gcc

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -Wall
LDFLAGS = -pthread -lrt

# the build target executable:
TARGET = aesdsocket

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c $(LDFLAGS)

clean:
	$(RM) $(TARGET)
