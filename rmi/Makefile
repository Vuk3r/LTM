CC = gcc
CFLAGS = -Wall -Wextra -std=c99
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null || pkg-config --cflags gtk+-2.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0 2>/dev/null || pkg-config --libs gtk+-2.0)
BINDIR = bin
TARGET_SERVER = $(BINDIR)/server
TARGET_CLIENT = $(BINDIR)/client
TARGET_CLIENT_GUI = $(BINDIR)/client_gui
SERVER_SRC = server.c
CLIENT_SRC = client.c
CLIENT_GUI_SRC = client_gui.c
HEADER = rmi.h

all: $(BINDIR) $(TARGET_SERVER) $(TARGET_CLIENT) $(TARGET_CLIENT_GUI)

$(BINDIR):
	mkdir -p $(BINDIR)

$(TARGET_SERVER): $(SERVER_SRC) $(HEADER) | $(BINDIR)
	$(CC) $(CFLAGS) -o $(TARGET_SERVER) $(SERVER_SRC)

$(TARGET_CLIENT): $(CLIENT_SRC) $(HEADER) | $(BINDIR)
	$(CC) $(CFLAGS) -o $(TARGET_CLIENT) $(CLIENT_SRC)

$(TARGET_CLIENT_GUI): $(CLIENT_GUI_SRC) $(HEADER) | $(BINDIR)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -o $(TARGET_CLIENT_GUI) $(CLIENT_GUI_SRC) $(GTK_LIBS)

clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT) $(TARGET_CLIENT_GUI)
	rmdir $(BINDIR) 2>/dev/null || true

.PHONY: all clean

