CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS =

TARGET = webserver
SRC = webserver.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

.PHONY: clean run

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)


