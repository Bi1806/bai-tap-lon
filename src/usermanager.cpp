#include "mainwindow.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // load favorites từ file (nếu có)
    loadUserData();
}

void MainWindow::setupUI()
{
    // --- Trang đăng nhập ---
    profilePage = new QWidget;
    profileStack = new QStackedWidget(profilePage);
    loginPage = new QWidget;
    QVBoxLayout *loginLayout = new QVBoxLayout(loginPage);
    loginLayout->setAlignment(Qt::AlignCenter); // căn giữa tất cả theo chiều dọc

    QLabel *loginTitle = new QLabel("👤 Đăng Nhập");
    loginTitle->setAlignment(Qt::AlignCenter);
    loginTitle->setStyleSheet("font-size: 22px; font-weight: bold; color: #00bfff;");

    QLineEdit *usernameInput = new QLineEdit;
    usernameInput->setPlaceholderText("Tên đăng nhập");

    QLineEdit *passwordInput = new QLineEdit;
    passwordInput->setPlaceholderText("Mật khẩu");
    passwordInput->setEchoMode(QLineEdit::Password);

    QPushButton *btnLogin = new QPushButton("Đăng Nhập");
    QPushButton *btnRegister = new QPushButton("Đăng Ký");

    connect(btnLogin, &QPushButton::clicked, this, &MainWindow::loginUser);
    connect(btnRegister, &QPushButton::clicked, this, &MainWindow::registerUser);

    // Gán cho các biến thành viên để loginUser() có thể truy cập
    this->usernameInput = usernameInput;
    this->passwordInput = passwordInput;

    // Tăng padding và bo góc cho đẹp
    usernameInput->setStyleSheet("background-color: #1b203a; color: white; padding: 8px; border-radius: 8px;");
    passwordInput->setStyleSheet("background-color: #1b203a; color: white; padding: 8px; border-radius: 8px;");
    btnLogin->setStyleSheet("background-color: #2b3370; color: white; border-radius: 8px; padding: 8px;");
    btnRegister->setStyleSheet("background-color: #2b3370; color: white; border-radius: 8px; padding: 8px;");

    // Thêm khoảng cách giữa các phần tử
    loginLayout->addStretch(2);
    loginLayout->addWidget(loginTitle);
    loginLayout->addSpacing(10);
    loginLayout->addWidget(usernameInput);
    loginLayout->addWidget(passwordInput);
    loginLayout->addSpacing(10);
    loginLayout->addWidget(btnLogin);
    loginLayout->addWidget(btnRegister);
    loginLayout->addStretch(2);

    // ================== XỬ LÝ SỰ KIỆN ĐĂNG NHẬP ==================
    // --- Giao diện hồ sơ sau khi đăng nhập ---
    profileMainPage = new QWidget;
    QVBoxLayout *profileMainLayout = new QVBoxLayout(profileMainPage);
    ClickableLabel *avatar = new ClickableLabel;
    avatar->setPixmap(QPixmap("img/avatar.png").scaled(80,80,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setCursor(Qt::PointingHandCursor);

    // Khi nhấn ảnh đại diện -> đổi ảnh
    connect(avatar, &ClickableLabel::clicked, this, [=]() {
        QString path = QFileDialog::getOpenFileName(nullptr, "Chọn ảnh đại diện", "", "Ảnh (*.png *.jpg *.jpeg)");
        if (!path.isEmpty()) {
            avatar->setPixmap(QPixmap(path).scaled(80,80,Qt::KeepAspectRatio,Qt::SmoothTransformation));
            QFile::copy(path, "user_" + currentUser + "_avatar.png");
        }
    });

    QLabel *nameLabel = new QLabel;
    nameLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: white;");
    nameLabel->setAlignment(Qt::AlignCenter);
    QLabel *vipLabel = new QLabel("🌟 Tài khoản VIP (Hết hạn 27.07.2026)");
    vipLabel->setStyleSheet("color: #ffcc00;");
    vipLabel->setAlignment(Qt::AlignCenter);

    QLabel *playlistLabel = new QLabel("🎶 Playlist Yêu Thích:");
    playlistLabel->setStyleSheet("font-size: 16px; color: #00bfff;");
    myFavList = new QListWidget;
    myFavList->setStyleSheet("background: transparent; border: none; color: white;");

    QPushButton *btnLogout = new QPushButton("Đăng Xuất");
    btnLogout->setStyleSheet("background-color: #b22222; color: white; border-radius: 8px; padding: 8px;");

    profileMainLayout->addWidget(avatar);
    profileMainLayout->addWidget(nameLabel);
    profileMainLayout->addWidget(vipLabel);
    profileMainLayout->addSpacing(10);
    profileMainLayout->addWidget(playlistLabel);
    profileMainLayout->addWidget(myFavList);
    profileMainLayout->addSpacing(10);
    profileMainLayout->addWidget(btnLogout, 0, Qt::AlignCenter);

    connect(btnLogout, &QPushButton::clicked, this, [=]() {
        int confirm = QMessageBox::question(this, "Đăng xuất", "Bạn có chắc muốn đăng xuất không?");
        if (confirm == QMessageBox::Yes) {
            isLoggedIn = false;
            currentUser.clear();

            // Xoá label hiển thị tên
            nameLabel->setText("");

            // Quay lại trang đăng nhập
            profileStack->setCurrentWidget(loginPage);

            QMessageBox::information(this, "Đăng xuất", "Đã đăng xuất thành công!");
        }
    });
}
void MainWindow::saveUserData()
{
    if (!isLoggedIn || currentUser.isEmpty())
        return;

    QString filename = "user_" + currentUser + ".json";

    // Đọc file cũ (để giữ lại username + password)
    QFile f(filename);
    QJsonObject obj;
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) obj = doc.object();
        f.close();
    }

    // Giữ username và password nếu có
    obj["username"] = currentUser;
    if (!obj.contains("password")) {
        obj["password"] = passwordInput->text().trimmed();
    }

    // Lưu danh sách yêu thích
    QJsonArray favArray;
    for (const QString &p : favorites)
        favArray.append(p);
    obj["favorites"] = favArray;

    // Ghi lại file JSON
    QJsonDocument outDoc(obj);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(outDoc.toJson());
        f.close();
    }
}

void MainWindow::loadUserData()
{
    if (currentUser.isEmpty())
        return;

    QString filename = "user_" + currentUser + ".json";
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QByteArray data = f.readAll();
    f.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    favorites.clear();
    if (obj.contains("favorites")) {
        QJsonArray favArray = obj["favorites"].toArray();
        for (const QJsonValue &v : favArray)
            favorites.insert(v.toString());
    }
}
void MainWindow::loginUser() {
    // Lấy dữ liệu đăng nhập
    QString user = usernameInput->text().trimmed();
    QString pass = passwordInput->text().trimmed();

    if (user.isEmpty() || pass.isEmpty()) {
        QMessageBox::warning(this, "Lỗi", "Vui lòng nhập đủ tên đăng nhập và mật khẩu!");
        return;
    }

    // Kiểm tra tồn tại file user
    QFile file("user_" + user + ".json");
    if (!file.exists()) {
        QMessageBox::warning(this, "Lỗi", "Tài khoản không tồn tại. Hãy đăng ký!");
        return;
    }

    // Đọc file JSON
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Lỗi", "Không thể đọc dữ liệu người dùng!");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    // Kiểm tra mật khẩu
    if (!obj.contains("password") || obj["password"].toString() != pass) {
        QMessageBox::warning(this, "Sai mật khẩu", "Mật khẩu không đúng!");
        return;
    }

    // Đăng nhập thành công
    isLoggedIn = true;
    currentUser = user;

    // Tải danh sách yêu thích từ JSON
    favorites.clear();
    if (obj.contains("favorites")) {
        QJsonArray favArray = obj["favorites"].toArray();
        for (const auto &v : favArray)
            favorites.insert(v.toString());
    }

    // Cập nhật lại giao diện và dữ liệu
    saveFavorites();
    updateFavoriteList();
    updateProfilePlaylist();

    QMessageBox::information(this, "Thành công", "Đăng nhập thành công!");
    fadeToPage(2);
    profileStack->setCurrentWidget(profileMainPage);

    // Bật tính năng đặc biệt sau đăng nhập
    btnFav->setEnabled(true);
    btnNext->setEnabled(true);
    btnPrev->setEnabled(true);
    volumeSlider->setEnabled(true);

    // Gỡ các connect cũ (tránh bị lặp)
    disconnect(btnNext, nullptr, this, nullptr);
    disconnect(btnPrev, nullptr, this, nullptr);
    disconnect(btnFav, nullptr, this, nullptr);

    // Kết nối lại hành vi sau đăng nhập
    connect(btnNext, &QPushButton::clicked, this, &MainWindow::nextSong);
    connect(btnPrev, &QPushButton::clicked, this, &MainWindow::prevSong);
    connect(btnFav, &QPushButton::clicked, this, &MainWindow::toggleFavorite);

    // Thêm nút "Thêm Bài Hát Riêng" nếu chưa có
    if (!profileMainPage->findChild<QPushButton*>("btnAddSong")) {
        QPushButton *btnAddSong = new QPushButton("➕ Thêm Bài Hát Riêng", profileMainPage);
        btnAddSong->setObjectName("btnAddSong");
        btnAddSong->setStyleSheet("background-color:#2b3370;color:white;border-radius:8px;padding:6px;");
        profileMainPage->layout()->addWidget(btnAddSong);

        connect(btnAddSong, &QPushButton::clicked, this, [this]() {
            QString file = QFileDialog::getOpenFileName(nullptr, "Chọn file nhạc", "", "Nhạc (*.mp3)");
            if (!file.isEmpty()) {
                QFileInfo info(file);
                // Tạo thư mục Nhac nếu chưa có
                QDir dir("Nhac");
                if (!dir.exists()) dir.mkpath(".");

                // Copy file vào thư mục Nhac
                QString dest = "Nhac/" + info.fileName();
                if (QFile::exists(dest))
                    QFile::remove(dest);  // xoá nếu trùng tên cũ
                QFile::copy(file, dest);

                // Lưu đường dẫn đầy đủ
                favorites.insert(dest);
                saveFavorites();
                updateFavoriteList();
                updateProfilePlaylist();

                QMessageBox::information(nullptr, "Đã thêm", "🎵 Đã thêm \"" + info.baseName() + "\" vào playlist cá nhân!");
            }
        });
    }
}

void MainWindow::registerUser() {
    QString user = usernameInput->text().trimmed();
    QString pass = passwordInput->text().trimmed();

    if (user.isEmpty() || pass.isEmpty()) {
        QMessageBox::warning(this, "Lỗi", "Vui lòng nhập đầy đủ!");
        return;
    }

    QFile file("user_" + user + ".json");
    if (file.exists()) {
        QMessageBox::warning(this, "Thông báo", "Tài khoản đã tồn tại!");
        return;
    }

    QJsonObject obj;
    obj["username"] = user;
    obj["password"] = pass;
    obj["favorites"] = QJsonArray();

    QJsonDocument doc(obj);
    file.open(QIODevice::WriteOnly);
    file.write(doc.toJson());
    file.close();

    QMessageBox::information(this, "Thành công", "Đăng ký thành công! Bây giờ hãy đăng nhập.");
}