// Minimal single-file C web server with embedded UI
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define SERVER_PORT 8080
#define BACKLOG 16
#define RECV_BUF 8192

static volatile sig_atomic_t keep_running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

static void url_decode(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if ((src[0] == '%') && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static const char *html_page =
    "<!doctype html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "  <meta charset=\"utf-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
    "  <title>C Web Server</title>\n"
    "  <style>\n"
    "    :root{--bg:#0f172a;--fg:#e2e8f0;--muted:#94a3b8;--accent:#22d3ee;--card:#111827;--ok:#10b981;}\n"
    "    *{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.5 system-ui,Segoe UI,Roboto,Helvetica,Arial,sans-serif}\n"
    "    .wrap{max-width:900px;margin:0 auto;padding:32px}\n"
    "    header{display:flex;align-items:center;gap:12px;margin-bottom:20px}\n"
    "    header h1{margin:0;font-size:22px}\n"
    "    header .pill{padding:4px 8px;border-radius:999px;background:#0b1324;color:var(--muted);font-size:12px}\n"
    "    .card{background:var(--card);border:1px solid #1f2937;border-radius:12px;padding:20px;margin:16px 0}\n"
    "    .row{display:flex;gap:12px;flex-wrap:wrap}\n"
    "    input[type=text]{flex:1;min-width:200px;background:#0b1324;border:1px solid #1f2937;border-radius:8px;color:var(--fg);padding:10px}\n"
    "    button{background:var(--accent);color:#001018;border:0;border-radius:8px;padding:10px 14px;font-weight:600;cursor:pointer}\n"
    "    button:hover{filter:brightness(1.05)}\n"
    "    code{background:#0b1324;border:1px solid #1f2937;border-radius:6px;padding:2px 6px}\n"
    "    .muted{color:var(--muted)}\n"
    "    .ok{color:var(--ok)}\n"
    "  </style>\n"
    "</head>\n"
    "<body>\n"
    "  <div class=\"wrap\">\n"
    "    <header>\n"
    "      <h1>Minimal C Web Server</h1>\n"
    "      <span class=\"pill\">single-file</span>\n"
    "      <span class=\"pill\">port 8080</span>\n"
    "    </header>\n"
    "    <div class=\"card\">\n"
    "      <p class=\"muted\">This server is a tiny C program with an embedded UI. Try actions below.</p>\n"
    "      <div class=\"row\">\n"
    "        <input id=\"msg\" type=\"text\" placeholder=\"Type a message...\">\n"
    "        <button onclick=\"echoMsg()\">Echo</button>\n"
    "        <button onclick=\"getTime()\">Get Server Time</button>\n"
    "      </div>\n"
    "      <p id=\"out\" class=\"muted\" style=\"margin-top:12px\">Ready.</p>\n"
    "    </div>\n"
    "    <div class=\"card\">\n"
    "      <b>Endpoints</b>\n"
    "      <ul>\n"
    "        <li><code>GET /</code>: UI</li>\n"
    "        <li><code>GET /echo?msg=...</code>: returns your message</li>\n"
    "        <li><code>GET /time</code>: returns ISO time</li>\n"
    "      </ul>\n"
    "    </div>\n"
    "  </div>\n"
    "  <script>\n"
    "  async function echoMsg(){\n"
    "    const v=document.getElementById('msg').value;\n"
    "    const r=await fetch('/echo?msg='+encodeURIComponent(v));\n"
    "    const t=await r.text();\n"
    "    document.getElementById('out').textContent=t;\n"
    "  }\n"
    "  async function getTime(){\n"
    "    const r=await fetch('/time');\n"
    "    const t=await r.text();\n"
    "    document.getElementById('out').textContent='Server time: '+t;\n"
    "  }\n"
    "  </script>\n"
    "</body>\n"
    "</html>\n";

static void send_response(int client_fd, const char *status, const char *content_type, const char *body) {
    char header[512];
    size_t body_len = body ? strlen(body) : 0;
    int n = snprintf(header, sizeof(header),
                     "HTTP/1.1 %s\r\n"
                     "Server: c-min-web/1.0\r\n"
                     "Connection: close\r\n"
                     "Content-Type: %s; charset=utf-8\r\n"
                     "Content-Length: %zu\r\n\r\n",
                     status, content_type, body_len);
    if (n < 0) return;
    send(client_fd, header, (size_t)n, 0);
    if (body_len) {
        send(client_fd, body, body_len, 0);
    }
}

static void handle_client(int client_fd) {
    char buf[RECV_BUF];
    ssize_t r = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (r <= 0) {
        return;
    }
    buf[r] = '\0';

    // Parse request line
    char method[8] = {0};
    char path[1024] = {0};
    if (sscanf(buf, "%7s %1023s", method, path) != 2) {
        send_response(client_fd, "400 Bad Request", "text/plain", "Bad Request\n");
        return;
    }

    // Only handle GET
    if (strcmp(method, "GET") != 0) {
        send_response(client_fd, "405 Method Not Allowed", "text/plain", "Only GET supported\n");
        return;
    }

    // Route handling
    if (strcmp(path, "/") == 0) {
        send_response(client_fd, "200 OK", "text/html", html_page);
        return;
    }

    if (strncmp(path, "/echo", 5) == 0) {
        // Find query param msg
        const char *q = strchr(path, '?');
        char msg[1024] = {0};
        if (q) {
            // naive parsing for msg=...
            const char *p = strstr(q + 1, "msg=");
            if (p) {
                p += 4;
                size_t i = 0;
                while (*p && *p != '&' && i < sizeof(msg) - 1) {
                    msg[i++] = *p++;
                }
                msg[i] = '\0';
                url_decode(msg);
            }
        }
        if (msg[0] == '\0') {
            strcpy(msg, "(empty)");
        }
        send_response(client_fd, "200 OK", "text/plain", msg);
        return;
    }

    if (strcmp(path, "/time") == 0) {
        char iso[64];
        time_t now = time(NULL);
        struct tm t;
        gmtime_r(&now, &t);
        strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &t);
        send_response(client_fd, "200 OK", "text/plain", iso);
        return;
    }

    send_response(client_fd, "404 Not Found", "text/plain", "Not Found\n");
}

int main(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int yes = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt");
        // continue anyway
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    const char *bind_ip = "192.168.1.20";
    if (inet_pton(AF_INET, bind_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid bind IP: %s\n", bind_ip);
        close(server_fd);
        return 1;
    }
    addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server listening on http://%s:%d\n", bind_ip, SERVER_PORT);

    while (keep_running) {
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        int client_fd = accept(server_fd, (struct sockaddr*)&cli, &clilen);
        if (client_fd < 0) {
            if (errno == EINTR && !keep_running) break;
            perror("accept");
            continue;
        }
        handle_client(client_fd);
        close(client_fd);
    }

    close(server_fd);
    printf("Shutting down.\n");
    return 0;
}


