# Chương trình RMI - Hệ thống đặt vé

Chương trình RMI (Remote Method Invocation) bằng C sử dụng socket và pthread để xử lý đặt vé với cơ chế đồng bộ hóa.

## Yêu cầu hệ thống

- GCC compiler
- SQLite3 development library
- pthread library (thường có sẵn)
- GTK+3.0 development library (cho GUI)

### Cài đặt trên Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install build-essential libsqlite3-dev libgtk-3-dev
```

## Biên dịch

```bash
make
```

Hoặc biên dịch thủ công:

```bash
# Biên dịch server
gcc server.c -o bin/server -lsqlite3 -lpthread

# Biên dịch client
gcc client.c -o bin/client
```

## Cách sử dụng

### Chương trình có 2 phiên bản:
- **Terminal (CLI)**: `server`, `client` - chạy từ dòng lệnh
- **Giao diện (GUI)**: `server_gui`, `client_gui` - chạy với giao diện GTK

### 1. Chạy Server

#### Server với giao diện GTK (Khuyến nghị):
```bash
./bin/server_gui
```
Sau đó nhập IP LAN và Port vào giao diện, nhấn "Bắt đầu Server".

#### Server từ terminal:
```bash
./bin/server <IP_LAN> <PORT>
```

Ví dụ:
```bash
./bin/server 192.168.1.100 9999
```

Hoặc nếu chạy trên localhost:
```bash
./bin/server 127.0.0.1 9999
```

**Lưu ý:** Để tìm IP LAN của máy, sử dụng lệnh:
```bash
ip addr show
# hoặc
ifconfig
```

### 2. Chạy Client

#### Client với giao diện GTK (Khuyến nghị):
```bash
./bin/client_gui
```
Sau đó nhập thông tin server và đặt vé vào giao diện.

#### Client từ terminal:
```bash
./bin/client <IP_SERVER> <PORT> <TRIP_ID> <SEAT_NUMBER> <Tên hành khách>
```

Ví dụ:
```bash
./bin/client 192.168.1.100 9999 TRIP01 5 "Nguyen Van A"
```

## Kiểm thử

### Test đơn giản

1. Terminal 1 - Chạy server:
```bash
./bin/server 127.0.0.1 9999
```

2. Terminal 2 - Chạy client:
```bash
./bin/client 127.0.0.1 9999 TRIP01 5 "Nguyen Van A"
```

### Test đồng thời nhiều client

Tạo file `test_concurrent.sh`:

```bash
#!/bin/bash
echo "Bắt đầu 10 người cùng đặt ghế số 5..."

./bin/client 127.0.0.1 9999 TRIP01 5 "Nguoi A" &
./bin/client 127.0.0.1 9999 TRIP01 5 "Nguoi B" &
./bin/client 127.0.0.1 9999 TRIP01 5 "Nguoi C" &
./bin/client 127.0.0.1 9999 TRIP01 5 "Nguoi D" &
./bin/client 127.0.0.1 9999 TRIP01 5 "Nguoi E" &
./bin/client 127.0.0.1 9999 TRIP01 5 "Nguoi F" &
./bin/client 127.0.0.1 9999 TRIP01 5 "Nguoi G" &
./bin/client 127.0.0.1 9999 TRIP01 5 "Nguoi H" &
./bin/client 127.0.0.1 9999 TRIP01 5 "Nguoi I" &
./bin/client 127.0.0.1 9999 TRIP01 5 "Nguoi K" &

wait # Đợi tất cả chạy xong
echo "Hoàn tất."
```

Chạy test:
```bash
chmod +x test_concurrent.sh
./test_concurrent.sh
```

## Kết quả mong đợi

Khi test đồng thời 10 client đặt cùng 1 ghế:
- **Chỉ 1 client** nhận được `OK:BOOKED`
- **9 client còn lại** nhận được `ERROR:SEAT_TAKEN`

Điều này chứng tỏ `pthread_mutex_t` đã hoạt động đúng, đảm bảo tính toàn vẹn dữ liệu.

## Cấu trúc Database

- **File:** `ticket_system.db` (tự động tạo)
- **Bảng:** `Bookings`
- **Cột:**
  - `tripId TEXT`
  - `seatNumber INTEGER`
  - `passengerName TEXT`
- **Khóa chính:** `PRIMARY KEY (tripId, seatNumber)`

## Xử lý lỗi

Tất cả các lỗi sẽ được in ra terminal ở cả 2 phía:

- **Server:** In lỗi với prefix `[ERROR]`
- **Client:** In lỗi với prefix `[ERROR]`

Các lỗi phổ biến:
- Port không hợp lệ
- IP không hợp lệ
- Không thể kết nối tới server
- Ghế đã được đặt
- Lỗi database

## Dọn dẹp

Xóa file thực thi và database:

```bash
make clean
```

