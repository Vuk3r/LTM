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

