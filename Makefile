CC = gcc
CFLAGS = -Wall
TARGET = js5
SRC = js5.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
