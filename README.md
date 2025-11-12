# 🎵 Bài Tập Lớn - Ứng Dụng Nghe Nhạc (Qt + C++)

## 📘 Giới thiệu
Dự án **Music Player** được xây dựng bằng **C++ và Qt Framework**, cho phép người dùng:
- Phát nhạc từ file local (.mp3)
- Quản lý danh sách bài hát yêu thích
- Đăng nhập / đăng ký người dùng
- Tìm kiếm và phát nhạc qua **Spotify API**
- Lưu dữ liệu tài khoản và playlist bằng JSON

---

## 👥 Thành viên nhóm (4 người)

| STT | Họ và tên | Vai trò | Công việc chính | Tỷ lệ đóng góp |
|------|------------|----------|-----------------|----------------|
| 1️⃣ | **[Nguyễn Trường Giang]** | Lập trình viên backend (người dùng) | Đăng nhập, đăng ký, lưu thông tin người dùng JSON, cập nhật playlist cá nhân.
| 2️⃣ | **Huỳnh Nguyên Bổn** | Lập trình chính | Giao diện chính (`MainWindow`), phát nhạc local, xử lý Play/Pause/Next, thiết kế UI, quản lý cấu trúc project.
| 3️⃣ | **Nguyễn Lê Quốc Anh** | Lập trình viên API | Tích hợp Spotify API, gọi API tìm kiếm nhạc, phát bài trên Spotify, xử lý token.
| 4️⃣ | **Phan Nhất Duy** | Lập trình viên UI phụ | Danh sách yêu thích, hiệu ứng giao diện, lưu yêu thích JSON, hiệu ứng chuyển trang.

---

## 📂 Cấu trúc thư mục

```
bai-tap-lon/
│
├── src/
│   ├── main.cpp
│   ├── mainwindow.h / .cpp              ← Bổn
│   ├── clickablelabel.h / .cpp          ← Bổn
│   ├── usermanager.h / .cpp             ← Giang
│   ├── spotifyapi.h / .cpp              ← Quốc Anh
│   ├── favoritesystem.h / .cpp          ← Duy
│
├── data/
│   ├── users.json                       ← Giang
│   ├── favorites.json                   ← Duy
│
├── img/
├── Nhac/ 
│
├── UML_Class_Diagram.drawio
└── README.md
```

---

## 🧱 Các chức năng chính

### 🎧 Phát nhạc local
- Chọn bài hát `.mp3` từ thư mục.
- Điều khiển phát/tạm dừng, tua, tăng giảm âm lượng.
- Hiển thị tiến trình và thời gian phát.

### 💖 Danh sách yêu thích
- Thêm / xóa bài hát yêu thích.
- Lưu và tải danh sách yêu thích bằng file JSON.
- Hiển thị danh sách trong giao diện cá nhân.

### 👤 Quản lý người dùng
- Đăng nhập, đăng ký, lưu thông tin người dùng.
- Tự động hiển thị playlist của người dùng.

### 🌐 Spotify API
- Tích hợp tìm kiếm bài hát từ Spotify.
- Lấy token truy cập, phát nhạc qua thiết bị Spotify.
- Hỗ trợ đăng nhập Spotify OAuth.

---

## 🖼️ Sơ đồ UML
File sơ đồ lớp được đính kèm:  
➡️ [UML_Class_Diagram.drawio](https://drive.google.com/file/d/12BhsImYVL2ycC6Ez86XTtt8mfUmEGUJT/view?usp=sharing)

---

## 🚀 Cách chạy chương trình

### Yêu cầu:
- **Qt 6.5 trở lên**
- **C++17**
- Có Internet (để truy cập Spotify API)

### Cách build:
1. Mở project trong **Qt Creator**
2. Chọn kit build (MinGW hoặc MSVC)
3. Chạy (Ctrl + R)

---

## 📌 Ghi chú
- Project được chia rõ module để dễ mở rộng.
- Mỗi thành viên làm việc trên branch riêng:
  - `feature/mainwindow`
  - `feature/user`
  - `feature/spotify`
  - `feature/favorite`
- Leader chịu trách nhiệm merge và kiểm thử cuối.

---

## 🏁 Kết quả
Ứng dụng hoàn chỉnh, có giao diện thân thiện, tính năng hoạt động ổn định.
Project được lưu trữ trên GitHub tại:  
🔗 [https://github.com/Bi1806/bai-tap-lon](https://github.com/Bi1806/bai-tap-lon)
