# Dự án OpenGL

## Giới thiệu
Đây là một dự án sử dụng OpenGL để tạo và hiển thị cửa sổ đồ họa. Dự án này được thiết kế để giúp bạn làm quen với OpenGL và các thư viện liên quan.

## Thẻ (Tags)
- `OpenGL`
- `GLFW`
- `GLM`
- `Linux`
- `C++`

## Cài đặt các thư viện cần thiết
Để chạy dự án này, bạn cần cài đặt các thư viện sau:

1. **Cài đặt g++**:
    ```bash
    sudo apt update
    sudo apt install g++
    ```

2. **Cài đặt GLFW**:
    ```bash
    sudo apt install libglfw3 libglfw3-dev
    ```

3. **Cài đặt GLM**:
    ```bash
    sudo apt install libglm-dev
    ```

4. **Cài đặt các thư viện bổ sung**:
    ```bash
    sudo apt install libglew-dev libgl1-mesa-dev
    ```

## Hướng dẫn thiết lập
Để tìm hiểu thêm về cách thiết lập OpenGL, bạn có thể tham khảo tài liệu tại [LearnOpenGL](https://learnopengl.com/).

## Cách chạy dự án
1. **Biên dịch mã nguồn**:
    Chạy lệnh sau trong thư mục dự án:
    ```bash
    g++ -o main main.cpp -lglfw -lGL -lGLEW
    ```

2. **Chạy chương trình**:
    ```bash
    ./main
    ```

## Ghi chú
- Đảm bảo rằng bạn đã cài đặt đầy đủ các thư viện trước khi biên dịch.
- Nếu gặp lỗi, hãy kiểm tra lại các bước cài đặt hoặc tham khảo tài liệu tại [LearnOpenGL](https://learnopengl.com/).

