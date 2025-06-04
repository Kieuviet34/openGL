# 🌄 Dự Án Tạo Địa Hình Bằng OpenGL

## 📝 Giới Thiệu
Dự án này tập trung vào việc tạo và hiển thị địa hình 3D trong môi trường thời gian thực sử dụng OpenGL. Chương trình cho phép người dùng tương tác với mô hình địa hình thông qua giao diện đồ họa và điều khiển camera.

## 🚀 Tính Năng Chính
- Tạo địa hình 3D với độ cao ngẫu nhiên
- Điều khiển camera:
  - Di chuyển: W, A, S, D
  - Xoay camera: Chuột
  - Điều chỉnh tốc độ di chuyển: Scroll chuột
- Chế độ hiển thị:
  - Fill mode (F)
  - Line mode (L)
  - Point mode (P)

## 📦 Cài Đặt (Ubuntu/Linux)

### Các thư viện cần thiết
```bash
# Cập nhật package manager
sudo apt update

# Cài đặt công cụ build
sudo apt install build-essential cmake

# Cài đặt các thư viện OpenGL
sudo apt install libgl1-mesa-dev
sudo apt install libglew-dev
sudo apt install libglfw3 libglfw3-dev
sudo apt install libglm-dev
sudo apt-get update
sudo apt-get install libassimp-dev
# Cài đặt thư viện xử lý ảnh (nếu cần)
sudo apt install libsoil-dev
```

## 🛠️ Biên Dịch và Chạy

1. **Tạo thư mục build**:
```bash
mkdir build
cd build
```

2. **Biên dịch project**:
```bash
cmake ..
make
```

3. **Chạy chương trình**:
```bash
./opengl
```

## 🎮 Hướng Dẫn Sử Dụng

### Điều Khiển Camera
- `W` - Di chuyển tiến
- `S` - Di chuyển lùi
- `A` - Di chuyển sang trái
- `D` - Di chuyển sang phải
- `Mouse` - Xoay camera
- `Scroll` - Điều chỉnh fov

### Chế Độ Hiển Thị
- `Esc` - Thoát chương trình

## 🔧 Xử Lý Lỗi Thường Gặp

1. **Lỗi thư viện không tìm thấy**:
   - Kiểm tra lại việc cài đặt các thư viện
   - Chạy `sudo ldconfig` để cập nhật cache thư viện

2. **Lỗi biên dịch**:
   - Đảm bảo đã cài đặt đầy đủ các thư viện phát triển
   - Kiểm tra version của compiler (yêu cầu GCC 7.0+)

## 📚 Tài Liệu Tham Khảo
- [LearnOpenGL](https://learnopengl.com/)
- [OpenGL Documentation](https://docs.gl/)
- [GLFW Documentation](https://www.glfw.org/docs/latest/)

---

## 💡 Phát Triển Tương Lai
- Thêm texture cho địa hình
- Tối ưu hóa hiệu suất render
- Thêm các hiệu ứng môi trường

Mọi đóng góp và phản hồi đều được chào đón! 

