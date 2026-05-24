FROM python:3.11-slim

WORKDIR /app

# Cài đặt thư viện paho-mqtt
RUN pip install --no-cache-dir paho-mqtt==2.1.0

# Copy mã nguồn giả lập vào container
COPY hardware_simulator.py .

# Khởi chạy chương trình Python ở chế độ Unbuffered để hiển thị logs lập tức
CMD ["python", "-u", "hardware_simulator.py"]
