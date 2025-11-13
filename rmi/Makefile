CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lsqlite3 -lpthread
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)

# Thư mục chứa file thực thi
BINDIR = bin

# Tên file thực thi
SERVER = $(BINDIR)/server
CLIENT = $(BINDIR)/client
SERVER_GUI = $(BINDIR)/server_gui
CLIENT_GUI = $(BINDIR)/client_gui

.PHONY: all clean server client server_gui client_gui

all: server client server_gui client_gui

server: $(SERVER)

client: $(CLIENT)

server_gui: $(SERVER_GUI)

client_gui: $(CLIENT_GUI)

$(SERVER): server.c
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) server.c -o $(SERVER) $(LIBS)
	@echo "✓ Đã biên dịch server thành công"

$(CLIENT): client.c
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) client.c -o $(CLIENT)
	@echo "✓ Đã biên dịch client thành công"

$(SERVER_GUI): server_gui.c
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) server_gui.c -o $(SERVER_GUI) $(LIBS) $(GTK_LIBS)
	@echo "✓ Đã biên dịch server_gui thành công"

$(CLIENT_GUI): client_gui.c
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) client_gui.c -o $(CLIENT_GUI) $(GTK_LIBS) -lpthread
	@echo "✓ Đã biên dịch client_gui thành công"

clean:
	rm -rf $(BINDIR)
	rm -f ticket_system.db
	@echo "✓ Đã xóa file thực thi và database"

