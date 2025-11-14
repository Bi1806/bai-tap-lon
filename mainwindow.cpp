#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QMessageBox>
#include <QStyle>
#include <QGraphicsOpacityEffect>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QSet>
#include <QToolButton>
#include <QPainter>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QLineEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRadialGradient>
#include <QFileDialog>
#include <QDateTime>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QTcpServer>
#include <QTcpSocket>

// Thay thế những thông tin này bằng thông tin xác thực
static const QString SPOTIFY_CLIENT_ID = "1bb921c950964cada3bf8b4a7b3c2ace";
static const QString SPOTIFY_CLIENT_SECRET = "c5cb36e4fe10419fbcf94b4d245f9bf4";
static const QString SPOTIFY_REDIRECT_URI = "http://127.0.0.1:8888/callback";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Khởi tạo player/audio
    player = new QMediaPlayer(this);
    networkManager = new QNetworkAccessManager(this);
    server = new QTcpServer(this);
    connect(server, &QTcpServer::newConnection, this, [this]() {
        QTcpSocket *socket = server->nextPendingConnection();
        if (!socket) return;
        QByteArray req = socket->readAll();
        QString reqStr = QString::fromUtf8(req);
        QRegularExpression rx("code=([^&\\s]+)");
        QRegularExpressionMatch m = rx.match(reqStr);
        if (m.hasMatch()) {
            QString code = QUrl::fromPercentEncoding(m.captured(1).toUtf8());
            exchangeSpotifyCode(code);
        }
        // trả trang web nhỏ để người dùng biết đăng nhập ok
        QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                          "<html><body><h2>Spotify login OK — you can close this tab.</h2></body></html>";
        socket->write(resp);
        socket->flush();
        socket->disconnectFromHost();
    });
    audioOutput = new QAudioOutput(this);
    player->setAudioOutput(audioOutput);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateProgress);
    rotateTimer = new QTimer(this);
    connect(rotateTimer, &QTimer::timeout, this, [this]() {
        rotationAngle += 2; // tốc độ xoay (độ mỗi khung)
        if (rotationAngle >= 360) rotationAngle = 0;

        if (!currentCover.isNull()) {
            QPixmap rotated(currentCover.size());
            rotated.fill(Qt::transparent);

            QPainter p(&rotated);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);

            // Xoay ảnh đĩa
            p.translate(rotated.width() / 2, rotated.height() / 2);
            p.rotate(rotationAngle);
            p.translate(-rotated.width() / 2, -rotated.height() / 2);
            p.drawPixmap(0, 0, currentCover);

            // Hiệu ứng phản sáng
            QRadialGradient glow(rotated.width()/2, rotated.height()/2, rotated.width()/2);
            glow.setColorAt(0.0, QColor(255,255,255,60));
            glow.setColorAt(0.5, QColor(255,255,255,20));
            glow.setColorAt(1.0, Qt::transparent);
            p.setBrush(glow);
            p.setPen(Qt::NoPen);
            p.drawEllipse(rotated.rect());

            p.end();
            coverArt->setPixmap(rotated.scaled(250, 250, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    });

    // load favorites từ file (nếu có)
    loadFavorites();
    loadUserData();

    setupUI();

    // Lấy token Spotify sẵn sàng
    fetchSpotifyToken();

    // kết nối signal quan trọng: duration và media status
    connect(player, &QMediaPlayer::durationChanged, this, &MainWindow::durationChanged);
    connect(player, &QMediaPlayer::mediaStatusChanged, this, &MainWindow::mediaStatusChanged);

}

MainWindow::~MainWindow()
{
    delete player;
}

void MainWindow::setupUI()
{
    resize(420, 640);
    setWindowTitle("🎵 Disc mp3");

    // Giao diện tối
    setStyleSheet(R"(
        QWidget { background-color: #0f1428; color: white; }
        QPushButton {
            background-color: #1b203a;
            border-radius: 8px;
            padding: 6px;
            color: white;
        }
        QPushButton:hover { background-color: #2b3370; }
        QListWidget { background-color: #181c35; border: none; }
        QProgressBar { border: 1px solid #333; border-radius: 5px; text-align: center; }
        QProgressBar::chunk { background-color: #00bfff; }
        QSlider::groove:horizontal { height: 6px; background: #333; }
        QSlider::handle:horizontal { background: #00bfff; width: 12px; border-radius: 6px; }
    )");

    // --- Trang nghe nhạc ---
    QWidget *musicPage = new QWidget;
    QVBoxLayout *musicLayout = new QVBoxLayout(musicPage);

    coverArt = new QLabel;
    coverArt->setPixmap(QPixmap("img/default.jpg").scaled(280, 280, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    coverArt->setAlignment(Qt::AlignCenter);

    songTitle = new QLabel("🎧 Chưa Phát Bài Hát");
    songTitle->setAlignment(Qt::AlignCenter);
    songTitle->setStyleSheet("font-size: 18px; color: #00bfff;");

    // ===== Thanh tìm kiếm bài hát =====
    searchBox = new QLineEdit(this);
    searchBox->setPlaceholderText("🔍 Tìm kiếm bài hát...");
    searchBox->setStyleSheet("background-color: #1b203a; color: white; padding: 6px; border-radius: 8px;");
    musicLayout->addWidget(searchBox);

    connect(searchBox, &QLineEdit::returnPressed, this, [this]() {
        QString text = searchBox->text().trimmed();
        if (!text.isEmpty()) {
            searchSpotify(text);
        }
    });

    // Khi người dùng nhập vào thanh tìm kiếm -> lọc danh sách bài hát local
    connect(searchBox, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int i = 0; i < songList->count(); ++i) {
            QListWidgetItem *item = songList->item(i);
            bool match = item->text().contains(text, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    });

    // Danh sách bài hát có bìa nhỏ
    songList = new QListWidget;
    // Thêm vào playlistFiles tương ứng
    playlistFiles.clear();
    QString p1 = "Nhac/Sorangembietanhconyeuem.mp3";
    QString p2 = "Nhac/Cuocsongemonkhong.mp3";
    QString p3 = "Nhac/Thangdien.mp3";
    QString p4 = "Nhac/Hongkong1.mp3";
    QString p5 = "Nhac/Text07.mp3";
    QString p6 = "Nhac/Ngaynaynamay.mp3";
    QString p7 = "Nhac/Divangnhatnhoa.mp3";
    QString p8 = "Nhac/Anhchieutan.mp3";
    QString p9 = "Nhac/10ngannam";
    QString p10 = "Nhac/Muonnoivoiem";
    QString p11 = "Nhac/Suytnuathi";
    QString p12 = "Nhac/Anhsaovabautroi";
    QString p13 = "Nhac/Bacphan";
    QString p14 = "Nhac/Neungayay";
    QString p15 = "Nhac/Nuocmatemlaubangtinhyeumoi";
    songList->addItem(new QListWidgetItem(QIcon("img/playing1.jpg"), "Sợ Rằng Em Biết Anh Còn Yêu Em"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing2.jpg"), "Cuộc Sống Em Ổn Không"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing3.jpg"), "Thằng Điên"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing4.jpg"), "Hongkong1"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing5.jpg"), "Text 07"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing6.jpg"), "Ngày Này Năm Ấy"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing7.jpg"), "Dĩ Vãng Nhạt Nhòa"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing8.jpg"), "Ánh Chiều Tàn"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing9.jpg"), "10 Ngàn Năm"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing10.jpg"), "Muốn Nói Với Em"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing11.jpg"), "Suýt Nữa Thì"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing12.jpg"), "Ánh Sao Và Bầu Trời"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing13.jpg"), "Bạc Phận"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing14.jpg"), "Nếu Ngày Ấy"));
    songList->addItem(new QListWidgetItem(QIcon("img/playing15.jpg"), "Nước Mắt Em Lau Bằng Tình Yêu Mới"));
    playlistFiles.push_back(p1);
    playlistFiles.push_back(p2);
    playlistFiles.push_back(p3);
    playlistFiles.push_back(p4);
    playlistFiles.push_back(p5);
    playlistFiles.push_back(p6);
    playlistFiles.push_back(p7);
    playlistFiles.push_back(p8);
    playlistFiles.push_back(p9);
    playlistFiles.push_back(p10);
    playlistFiles.push_back(p11);
    playlistFiles.push_back(p12);
    playlistFiles.push_back(p13);
    playlistFiles.push_back(p14);
    playlistFiles.push_back(p15);
    songList->setIconSize(QSize(48, 48));
    songList->setAcceptDrops(true);

    btnPlay = new QPushButton("▶ Phát");
    btnPause = new QPushButton("⏸ Tạm dừng");
    btnStop = new QPushButton("⏹ Dừng");
    btnPrev = new QPushButton("⏮");
    btnNext = new QPushButton("⏭");
    btnFav  = new QPushButton("💖");

    // Thanh tua bài hát
    progressSlider = new QSlider(Qt::Horizontal);
    progressSlider->setRange(0, 100);
    progressSlider->setStyleSheet("QSlider::groove:horizontal { height: 6px; background: #333; }"
                                  "QSlider::handle:horizontal { background: #00bfff; width: 12px; border-radius: 6px; }"
                                  "QSlider::sub-page:horizontal { background: #00bfff; }");

    // --- Thời gian ---
    timeLabel = new QLabel("00:00 / 00:00");
    timeLabel->setAlignment(Qt::AlignCenter);
    timeLabel->setStyleSheet("color: #cfcfcf; font-size: 12px;");

    QHBoxLayout *controlLayout = new QHBoxLayout;
    controlLayout->addWidget(btnPrev);
    controlLayout->addWidget(btnPlay);
    controlLayout->addWidget(btnPause);
    controlLayout->addWidget(btnStop);
    controlLayout->addWidget(btnNext);
    controlLayout->addWidget(btnFav);

    // Thanh âm lượng
    QHBoxLayout *volumeLayout = new QHBoxLayout;
    QLabel *volLabel = new QLabel("🔊 Âm lượng:");
    volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(70);
    audioOutput->setVolume(0.7);
    volumeLayout->addWidget(volLabel);
    volumeLayout->addWidget(volumeSlider);

    musicLayout->addWidget(coverArt);
    musicLayout->addWidget(songTitle);
    musicLayout->addWidget(songList);
    musicLayout->addLayout(controlLayout);
    musicLayout->addWidget(progressSlider);
    musicLayout->addWidget(timeLabel);
    musicLayout->addLayout(volumeLayout);

    connect(btnPlay, &QPushButton::clicked, this, &MainWindow::playSelectedSong);
    connect(btnPause, &QPushButton::clicked, this, &MainWindow::pauseOrResume);
    connect(btnStop, &QPushButton::clicked, this, &MainWindow::stopMusic);
    connect(btnNext, &QPushButton::clicked, this, &MainWindow::nextSong);
    connect(btnPrev, &QPushButton::clicked, this, &MainWindow::prevSong);
    connect(btnFav, &QPushButton::clicked, this, &MainWindow::toggleFavorite);

    // Khi người dùng thả tay sau khi kéo thanh tua
    connect(progressSlider, &QSlider::sliderMoved, this, [this](int position) {
        if (player->duration() > 0) {
            qint64 newPos = static_cast<qint64>(position / 100.0 * player->duration());
            player->setPosition(newPos);
        }
    });

    connect(progressSlider, &QSlider::sliderReleased, this, [this]() {
        if (player->duration() > 0) {
            int value = progressSlider->value();
            qint64 newPos = (value * player->duration()) / 100;
            player->setPosition(newPos);
        }
    });

    // Cập nhật âm lượng khi thay đổi thanh volume
    connect(volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        audioOutput->setVolume(value / 100.0);
    });

    // Cập nhật vị trí phát hiện tại lên thanh tiến trình
    connect(player, &QMediaPlayer::positionChanged, this, [this](qint64 pos) {
        if (!progressSlider->isSliderDown() && player->duration() > 0) {
            int value = static_cast<int>((pos * 100) / player->duration());
            progressSlider->setValue(value);
        }
        // cập nhật timeLabel
        int currentSec = static_cast<int>(pos / 1000);
        int totalSec = static_cast<int>(player->duration() / 1000);
        QString cur = QString("%1:%2").arg(currentSec/60, 2, 10, QChar('0')).arg(currentSec%60, 2, 10, QChar('0'));
        QString tot = QString("%1:%2").arg(totalSec/60, 2, 10, QChar('0')).arg(totalSec%60, 2, 10, QChar('0'));
        timeLabel->setText(cur + " / " + tot);
    });

    // --- Trang Khám Phá (Spotify) ---
    exploreTab = new QWidget;
    QVBoxLayout *exploreLayout = new QVBoxLayout(exploreTab);

    QLabel *exploreTitle = new QLabel("✨ Khám Phá Âm Nhạc");
    exploreTitle->setAlignment(Qt::AlignCenter);
    exploreTitle->setStyleSheet("font-size: 20px; color: #00bfff; font-weight: bold;");

    exploreList = new QListWidget;
    exploreList->setIconSize(QSize(64,64));
    exploreList->setStyleSheet("background: transparent; color: white; border: none; font-size: 14px;");

    exploreLayout->addWidget(exploreTitle);
    exploreLayout->addWidget(exploreList);

    QPushButton *btnLoginSpotify = new QPushButton("🔑 Đăng nhập Spotify");
    exploreLayout->insertWidget(0, btnLoginSpotify);
    connect(btnLoginSpotify, &QPushButton::clicked, this, &MainWindow::startSpotifyLogin);

    // Khi người dùng click 1 kết quả trên Khám Phá -> phát preview Spotify (đã thay)
    connect(exploreList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        QString songData = item->data(Qt::UserRole).toString();
        QString uri = item->data(Qt::UserRole).toString();
        if (uri.isEmpty()) {
            QMessageBox::information(this, "Spotify", "Không có URI bài hát.");
            return;
        }
        // Nếu là link spotify:track:xxx → phát bằng Spotify thật
        if (songData.startsWith("spotify:track:")) {
            if (lastDeviceId.isEmpty()) {
                getUserDevices();
                return;
            }
            transferPlaybackToDevice(lastDeviceId, true);
            playSpotifyOnDevice(songData, lastDeviceId);
            return;
        }

        // Ngược lại: phát preview 30s trong app (nếu có link .mp3)
        if (songData.startsWith("http")) {
            player->setSource(QUrl(songData));
            player->play();
            return;
        }

        QMessageBox::information(this, "Spotify", "Không tìm thấy dữ liệu phát cho bài này.");
    });

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

    // Thêm hai trang vào Stack
    profileStack->addWidget(loginPage);
    profileStack->addWidget(profileMainPage);
    QVBoxLayout *profileLayout = new QVBoxLayout(profilePage);
    profileLayout->addWidget(profileStack);

    // --- Stack chính ---
    stackedWidget = new QStackedWidget;
    stackedWidget->addWidget(musicPage);
    stackedWidget->addWidget(exploreTab);
    stackedWidget->addWidget(profilePage);

    // --- Thanh điều hướng ---
    bottomNav = new QWidget;
    bottomNav->setStyleSheet("background-color: #0b0e20;");

    btnTabMusic = new QPushButton("🎵 Nhạc");
    btnTabExplore = new QPushButton("✨ Khám Phá");
    btnTabProfile = new QPushButton("👤 Của Tui");

    QHBoxLayout *navLayout = new QHBoxLayout(bottomNav);
    navLayout->addWidget(btnTabMusic);
    navLayout->addWidget(btnTabExplore);
    navLayout->addWidget(btnTabProfile);

    connect(btnTabMusic, &QPushButton::clicked, this, [=]() { fadeToPage(0); });
    connect(btnTabExplore, &QPushButton::clicked, this, [=]() { fadeToPage(1); });
    connect(btnTabProfile, &QPushButton::clicked, this, [=]() {
        fadeToPage(2);
        if (isLoggedIn)
            profileStack->setCurrentWidget(profileMainPage);
        else
            profileStack->setCurrentWidget(loginPage);
    });

    // --- Tổng thể ---
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(stackedWidget);
    mainLayout->addWidget(bottomNav);

    central = new QWidget;
    central->setLayout(mainLayout);
    setCentralWidget(central);
} // <-- đóng setupUI()

// ---------------- Spotify token (Client Credentials) ----------------
void MainWindow::fetchSpotifyToken()
{
    if (SPOTIFY_CLIENT_ID.isEmpty() || SPOTIFY_CLIENT_SECRET.isEmpty()) {
        qWarning() << "ID/bí mật của khách hàng Spotify chưa được thiết lập";
        return;
    }

    QByteArray cred = (SPOTIFY_CLIENT_ID + ":" + SPOTIFY_CLIENT_SECRET).toUtf8();
    QByteArray auth = cred.toBase64();

    QNetworkRequest req(QUrl("https://accounts.spotify.com/api/token"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setRawHeader("Authorization", "Basic " + auth);

    QByteArray body;
    body.append("grant_type=client_credentials");

    QNetworkReply *reply = networkManager->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray resp = reply->readAll();
        reply->deleteLater();

        QJsonParseError perr;
        QJsonDocument doc = QJsonDocument::fromJson(resp, &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning() << "Failed parse spotify token:" << perr.errorString();
            return;
        }

        QJsonObject obj = doc.object();
        this->spotifyToken = obj.value("access_token").toString();
        int expires = obj.value("expires_in").toInt(3600);
        this->spotifyTokenExpiry = QDateTime::currentDateTimeUtc().addSecs(expires - 30);
        qDebug() << "Got spotify token, length =" << this->spotifyToken.size();
    });
}

void MainWindow::startSpotifyAuthServer() {
    if (server->isListening()) {
        server->close();
    }

    if (!server->listen(QHostAddress::Any, 8000)) {
        QMessageBox::critical(this, "Spotify", "Không thể mở cổng 8000: " + server->errorString());
        return;
    }
    qDebug() << "Đang nghe cổng 8000 cho Spotify callback...";
}

void MainWindow::startSpotifyLogin()
{
    QString scope = "user-read-playback-state user-modify-playback-state user-read-currently-playing";
    QString authUrl = QString(
                          "https://accounts.spotify.com/authorize" // <--- XÁC MINH URL NÀY
                          "?response_type=code"
                          "&client_id=%1"
                          "&scope=%2"
                          "&redirect_uri=%3")
                          .arg(SPOTIFY_CLIENT_ID)
                          .arg(QUrl::toPercentEncoding(scope))
                          .arg(QUrl::toPercentEncoding(SPOTIFY_REDIRECT_URI));
    QDesktopServices::openUrl(QUrl(authUrl));

    bool ok = false;
    QString code = QInputDialog::getText(
        this,
        "Spotify Đăng Nhập",
        "🎧 Sau khi đăng nhập Spotify xong,\n"
        "trình duyệt sẽ hiển thị đường dẫn dạng:\n\n"
        "http://127.0.0.1:8888/callback?code=XXXXXX\n\n"
        "👉 Hãy copy phần mã sau 'code=' và dán vào đây:",
        QLineEdit::Normal,
        "",
        &ok
        );

    if (ok && !code.isEmpty()) {
        exchangeSpotifyCode(code);
    } else {
        QMessageBox::information(this, "Spotify", "Bạn chưa nhập mã code.");
    }
}

void MainWindow::searchSpotify(const QString &query)
{
    if (spotifyToken.isEmpty()) {
        QMessageBox::warning(this, "Spotify", "Chưa có token Spotify, vui lòng đợi vài giây.");
        return;
    }

    QString encoded = QUrl::toPercentEncoding(query);
    QString apiUrl = QString("https://api.spotify.com/v1/search?q=%1&type=track&limit=10").arg(encoded);

    QNetworkRequest req{ QUrl(apiUrl) };
    req.setRawHeader("Authorization", ("Bearer " + spotifyToken).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = networkManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            QMessageBox::warning(this, "Lỗi", "Không đọc được phản hồi từ Spotify.");
            return;
        }

        exploreList->clear();
        QJsonArray items = doc["tracks"].toObject()["items"].toArray();
        if (items.isEmpty()) {
            exploreList->addItem(new QListWidgetItem("❌ Không tìm thấy bài hát nào trên Spotify."));
            return;
        }

        for (const QJsonValue &v : items) {
            QJsonObject obj = v.toObject();
            QString title = obj["name"].toString();
            QString artist = obj["artists"].toArray().first().toObject()["name"].toString();
            QString preview = obj["preview_url"].toString();
            QString uri = obj["uri"].toString();
            QString img = obj["album"].toObject()["images"].toArray().first().toObject()["url"].toString();

            QListWidgetItem *item = new QListWidgetItem(QIcon(img), title + " - " + artist);
            if (!uri.isEmpty())
                item->setData(Qt::UserRole, uri);
            else
                item->setData(Qt::UserRole, preview);
            exploreList->addItem(item);
        }

        fadeToPage(1); // Chuyển qua tab khám phá
    });
}

// ==================== CÁC CHỨC NĂNG ====================

void MainWindow::fadeToPage(int index)
{
    QWidget *current = stackedWidget->currentWidget();
    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(current);
    current->setGraphicsEffect(effect);

    QPropertyAnimation *anim = new QPropertyAnimation(effect, "opacity");
    anim->setDuration(400);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    connect(anim, &QPropertyAnimation::finished, this, [=]() {
        stackedWidget->setCurrentIndex(index);
        QGraphicsOpacityEffect *newEffect = new QGraphicsOpacityEffect(stackedWidget->currentWidget());
        stackedWidget->currentWidget()->setGraphicsEffect(newEffect);
        QPropertyAnimation *fadeIn = new QPropertyAnimation(newEffect, "opacity");
        fadeIn->setDuration(400);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void MainWindow::setVolume(int value)
{
    audioOutput->setVolume(value / 100.0);
}

// ==================== NHẠC ====================

void MainWindow::playSelectedSong()
{
    QListWidgetItem *item = songList->currentItem();
    if (!item) {
        QMessageBox::warning(this, "Thông báo", "Vui lòng chọn bài hát!");
        return;
    }

    QString song = item->text();
    QString filePath;
    QString coverPath;

    // --- Chọn bài hát ---
    if (song.contains("Sợ Rằng Em Biết Anh Còn Yêu Em")) {
        filePath = "Nhac/Sorangembietanhconyeuem.mp3";
        coverPath = "img/playing1_1.png";
        songTitle->setText("Sợ Rằng Em Biết Anh Còn Yêu Em");
    }
    else if (song.contains("Cuộc Sống Em Ổn Không")) {
        filePath = "Nhac/Cuocsongemonkhong.mp3";
        coverPath = "img/playing2_1.png";
        songTitle->setText("Cuộc Sống Em Ổn Không");
    }
    else if(song.contains("Thằng Điên")) {
        filePath = "Nhac/Thangdien.mp3";
        coverPath = "img/playing3_1.png";
        songTitle->setText("Thằng Điên");
    }
    else if(song.contains("Hongkong1")) {
        filePath = "Nhac/Hongkong1.mp3";
        coverPath = "img/playing4_1.png";
        songTitle->setText("Hongkong1");
    }
    else if(song.contains("Text 07")) {
        filePath = "Nhac/Text07.mp3";
        coverPath = "img/playing5_1.png";
        songTitle->setText("Text 07");
    }
    else if(song.contains("Ngày Này Năm Ấy")) {
        filePath = "Nhac/Ngaynaynamay.mp3";
        coverPath = "img/playing6_1.png";
        songTitle->setText("Ngày Này Năm Ấy");
    }
    else if(song.contains("Dĩ Vãng Nhạt Nhòa")) {
        filePath = "Nhac/Divangnhatnhoa.mp3";
        coverPath = "img/playing7_1.png";
        songTitle->setText("Dĩ Vãng Nhạt Nhòa");
    }
    else if(song.contains("Ánh Chiều Tàn")) {
        filePath = "Nhac/Anhchieutan.mp3";
        coverPath = "img/playing8_1.png";
        songTitle->setText("Ánh Chiều Tàn");
    }
    else if(song.contains("10 Ngàn Năm")) {
        filePath = "Nhac/10ngannam.mp3";
        coverPath = "img/playing9_1.png";
        songTitle->setText("10 Ngàn Năm");
    }
    else if(song.contains("Muốn Nói Với Em")) {
        filePath = "Nhac/Muonnoivoiem.mp3";
        coverPath = "img/playing10_1.png";
        songTitle->setText("Muốn Nói Với Em");
    }
    else if(song.contains("Suýt Nữa Thì")) {
        filePath = "Nhac/Suytnuathi.mp3";
        coverPath = "img/playing11_1.png";
        songTitle->setText("Suýt Nữa Thì");
    }
    else if(song.contains("Ánh Sao Và Bầu Trời")) {
        filePath = "Nhac/Anhsaovabautroi.mp3";
        coverPath = "img/playing12_1.png";
        songTitle->setText("Ánh Sao Và Bầu Trời");
    }
    else if(song.contains("Bạc Phận")) {
        filePath = "Nhac/Bacphan.mp3";
        coverPath = "img/playing13_1.png";
        songTitle->setText("Bạc Phận");
    }
    else if(song.contains("Nếu Ngày Ấy")) {
        filePath = "Nhac/Neungayay.mp3";
        coverPath = "img/playing14_1.png";
        songTitle->setText("Nếu Ngày Ấy");
    }
    else if(song.contains("Nước Mắt Em Lau Bằng Tình Yêu Mới")) {
        filePath = "Nhac/Nuocmatemlaubangtinhyeumoi.mp3";
        coverPath = "img/playing15_1.png";
        songTitle->setText("Nước Mắt Em Lau Bằng Tình Yêu Mới");
    }
    else {
        QMessageBox::warning(this, "Thông báo", "Không có thông tin bài hát này!");
        return;
    }

    // --- Kiểm tra file tồn tại ---
    if (!QFile::exists(filePath)) {
        QMessageBox::warning(this, "Lỗi", "Không tìm thấy file nhạc: " + filePath);
        return;
    }

    // --- Hiệu ứng mờ dần khi đổi ảnh ---
    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(coverArt);
    coverArt->setGraphicsEffect(effect);

    QPropertyAnimation *fade = new QPropertyAnimation(effect, "opacity");
    fade->setDuration(400);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);

    connect(fade, &QPropertyAnimation::finished, this, [=]() {
        currentCover = QPixmap(coverPath).scaled(250, 250, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        coverArt->setPixmap(currentCover);
        rotationAngle = 0;
        rotateTimer->start(50); // 20 fps

        songTitle->setText(song);

        QPropertyAnimation *fadeIn = new QPropertyAnimation(effect, "opacity");
        fadeIn->setDuration(400);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
    });

    fade->start(QAbstractAnimation::DeleteWhenStopped);

    // --- Đặt nguồn nhạc và phát ---
    player->setSource(QUrl::fromLocalFile(filePath));
    player->play();
    timer->start(1000);
}

void MainWindow::pauseOrResume()
{
    if (player->playbackState() == QMediaPlayer::PlayingState) {
        player->pause();
        rotateTimer->stop();
        btnPause->setText("▶ Tiếp tục");
    } else if (player->playbackState() == QMediaPlayer::PausedState) {
        player->play();
        rotateTimer->start(50);
        btnPause->setText("⏸ Tạm dừng");
    }
}

void MainWindow::stopMusic()
{
    player->stop();
    timer->stop();
    rotateTimer->stop();
    rotationAngle = 0;
    coverArt->setPixmap(QPixmap("img/default.jpg").scaled(250, 250, Qt::KeepAspectRatio));
    songTitle->setText("🎧 Chưa Phát Bài Hát");
}

void MainWindow::updateProgress()
{
    if (player->duration() > 0) {
        int pos = static_cast<int>((double)player->position() / player->duration() * 100);
        progressSlider->setValue(pos);
    }
}

void MainWindow::nextSong()
{
    if (songList->count() == 0) return;

    int index = songList->currentRow();
    index = (index + 1) % songList->count();
    songList->setCurrentRow(index);

    QListWidgetItem *item = songList->currentItem();
    if (!item) return;

    QString song = item->text();
    QString filePath, coverPath;

    if (song.contains("Sợ Rằng Em Biết Anh Còn Yêu Em")) {
        filePath = "Nhac/Sorangembietanhconyeuem.mp3";
        coverPath = "img/playing1_1.png";
    } else if (song.contains("Cuộc Sống Em Ổn Không")) {
        filePath = "Nhac/Cuocsongemonkhong.mp3";
        coverPath = "img/playing2_1.png";
    } else if (song.contains("Thằng Điên")) {
        filePath = "Nhac/Thangdien.mp3";
        coverPath = "img/playing3_1.png";
    } else if (song.contains("Hongkong1")) {
        filePath = "Nhac/Hongkong1.mp3";
        coverPath = "img/playing4_1.png";
    } else if (song.contains("Text 07")) {
        filePath = "Nhac/Text07.mp3";
        coverPath = "img/playing5_1.png";
    } else if (song.contains("Ngày Này Năm Ấy")) {
        filePath = "Nhac/Ngaynaynamay.mp3";
        coverPath = "img/playing6_1.png";
    } else if (song.contains("Dĩ Vãng Nhạt Nhòa")) {
        filePath = "Nhac/Divangnhatnhoa.mp3";
        coverPath = "img/playing7_1.png";
    } else if (song.contains("Ánh Chiều Tàn")) {
        filePath = "Nhac/Anhchieutan.mp3";
        coverPath = "img/playing8_1.png";
    } else if (song.contains("10 Ngàn Năm")) {
        filePath = "Nhac/10ngannam.mp3";
        coverPath = "img/playing9_1.png";
    } else if (song.contains("Muốn Nói Với Em")) {
        filePath = "Nhac/Muonnoivoiem.mp3";
        coverPath = "img/playing10_1.png";
    } else if (song.contains("Suýt Nữa Thì")) {
        filePath = "Nhac/Suytnuathi.mp3";
        coverPath = "img/playing11_1.png";
    } else if (song.contains("Ánh Sao Và Bầu Trời")) {
        filePath = "Nhac/Anhsaovabautroi.mp3";
        coverPath = "img/playing12_1.png";
    } else if (song.contains("Bạc Phận")) {
        filePath = "Nhac/Bacphan.mp3";
        coverPath = "img/playing13_1.png";
    } else if (song.contains("Nếu Ngày Ấy")) {
        filePath = "Nhac/Neungayay.mp3";
        coverPath = "img/playing14_1.png";
    } else if (song.contains("Nước Mắt Em Lau Bằng Tình Yêu Mới")) {
        filePath = "Nhac/Nuocmatemlaubangtinhyeumoi.mp3";
        coverPath = "img/playing15_1.png";
    } else return;

    // Cập nhật đĩa
    rotationAngle = 0;
    currentCover = QPixmap(coverPath).scaled(250, 250, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    coverArt->setPixmap(currentCover);

    // Phát nhạc
    player->setSource(QUrl::fromLocalFile(filePath));
    player->play();
    rotateTimer->start(50);

    // Cập nhật tiêu đề
    songTitle->setText(song);
}

void MainWindow::prevSong()
{
    if (songList->count() == 0) return;

    int index = songList->currentRow();
    index = (index - 1 + songList->count()) % songList->count();
    songList->setCurrentRow(index);

    QListWidgetItem *item = songList->currentItem();
    if (!item) return;

    QString song = item->text();
    QString filePath, coverPath;

    if (song.contains("Sợ Rằng Em Biết Anh Còn Yêu Em")) {
        filePath = "Nhac/Sorangembietanhconyeuem.mp3";
        coverPath = "img/playing1_1.png";
    } else if (song.contains("Cuộc Sống Em Ổn Không")) {
        filePath = "Nhac/Cuocsongemonkhong.mp3";
        coverPath = "img/playing2_1.png";
    } else if (song.contains("Thằng Điên")) {
        filePath = "Nhac/Thangdien.mp3";
        coverPath = "img/playing3_1.png";
    } else if (song.contains("Hongkong1")) {
        filePath = "Nhac/Hongkong1.mp3";
        coverPath = "img/playing4_1.png";
    } else if (song.contains("Text 07")) {
        filePath = "Nhac/Text07.mp3";
        coverPath = "img/playing5_1.png";
    } else if (song.contains("Ngày Này Năm Ấy")) {
        filePath = "Nhac/Ngaynaynamay.mp3";
        coverPath = "img/playing6_1.png";
    } else if (song.contains("Dĩ Vãng Nhạt Nhòa")) {
        filePath = "Nhac/Divangnhatnhoa.mp3";
        coverPath = "img/playing7_1.png";
    } else if (song.contains("Ánh Chiều Tàn")) {
        filePath = "Nhac/Anhchieutan.mp3";
        coverPath = "img/playing8_1.png";
    } else if (song.contains("10 Ngàn Năm")) {
        filePath = "Nhac/10ngannam.mp3";
        coverPath = "img/playing9_1.png";
    } else if (song.contains("Muốn Nói Với Em")) {
        filePath = "Nhac/Muonnoivoiem.mp3";
        coverPath = "img/playing10_1.png";
    } else if (song.contains("Suýt Nữa Thì")) {
        filePath = "Nhac/Suytnuathi.mp3";
        coverPath = "img/playing11_1.png";
    } else if (song.contains("Ánh Sao Và Bầu Trời")) {
        filePath = "Nhac/Anhsaovabautroi.mp3";
        coverPath = "img/playing12_1.png";
    } else if (song.contains("Bạc Phận")) {
        filePath = "Nhac/Bacphan.mp3";
        coverPath = "img/playing13_1.png";
    } else if (song.contains("Nếu Ngày Ấy")) {
        filePath = "Nhac/Neungayay.mp3";
        coverPath = "img/playing14_1.png";
    } else if (song.contains("Nước Mắt Em Lau Bằng Tình Yêu Mới")) {
        filePath = "Nhac/Nuocmatemlaubangtinhyeumoi.mp3";
        coverPath = "img/playing15_1.png";
    } else return;

    // Cập nhật đĩa
    rotationAngle = 0;
    currentCover = QPixmap(coverPath).scaled(250, 250, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    coverArt->setPixmap(currentCover);

    // Phát nhạc
    player->setSource(QUrl::fromLocalFile(filePath));
    player->play();
    rotateTimer->start(50);

    // Cập nhật tiêu đề
    songTitle->setText(song);
}

void MainWindow::durationChanged(qint64 duration)
{
    Q_UNUSED(duration);
}

void MainWindow::mediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    // khi bài kết thúc -> chuyển bài tiếp
    if (status == QMediaPlayer::EndOfMedia) {
        rotateTimer->stop();
        nextSong();
    }
}

// Lưu favorites ra file text (mỗi dòng 1 tên)
void MainWindow::saveFavorites()
{
    if (!currentUser.isEmpty()) {
        // lưu vào user_currentUser.json
        QString fname = QString("user_%1.json").arg(currentUser);
        QFile file(fname);
        QJsonObject obj;

        // nếu file tồn tại, đọc phần info khác (username/password) để giữ nguyên mật khẩu
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) obj = doc.object();
        }

        // update favorites
        QJsonArray favArr;
        for (const QString &p : favorites) favArr.append(p);
        obj["favorites"] = favArr;

        // đảm bảo username/password vẫn giữ nếu có
        QJsonDocument outDoc(obj);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Cannot open " << fname << " to write";
            return;
        }
        file.write(outDoc.toJson());
        file.close();
        qDebug() << "Saved favorites into" << fname;
        return;
    }

    // fallback cũ: lưu ra favorites.txt nếu chưa login
    QFile f("favorites.txt");
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Không thể mở để ghi favorites.txt";
        return;
    }
    QTextStream out(&f);
    for (const QString &path : favorites) out << path << "\n";
    f.close();
    qDebug() << "Favorites saved to favorites.txt (no user logged)";

    saveUserData();
}

void MainWindow::loadFavorites()
{
    favorites.clear();
    QFile f("favorites.txt");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString path = in.readLine().trimmed();
        if (!path.isEmpty()) favorites.insert(path);
    }
    f.close();
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

void MainWindow::updateFavoriteList()
{
    if (!myFavList) return;
    myFavList->clear();

    // Nếu rỗng thì hiển thị thông báo
    if (favorites.isEmpty()) {
        QListWidgetItem *emptyItem = new QListWidgetItem("💖 Chưa có bài hát yêu thích nào!");
        emptyItem->setFlags(Qt::NoItemFlags); // không cho chọn
        myFavList->addItem(emptyItem);
        return;
    }

    // Hiển thị mỗi bài yêu thích
    for (const QString &fileName : favorites) {
        QString title;
        QString coverPath = "img/default.jpg";

        if (fileName.contains("Sorangembietanhconyeuem")) {
            title = "Sợ Rằng Em Biết Anh Còn Yêu Em";
            coverPath = "img/playing1.jpg";
        } else if (fileName.contains("Cuocsongemonkhong")) {
            title = "Cuộc Sống Em Ổn Không";
            coverPath = "img/playing2.jpg";
        } else if (fileName.contains("Thangdien")) {
            title = "Thằng Điên";
            coverPath = "img/playing3.jpg";
        } else if (fileName.contains("Hongkong1")) {
            title = "Hongkong1";
            coverPath = "img/playing4.jpg";
        } else if (fileName.contains("Text07")) {
            title = "Text 07";
            coverPath = "img/playing5.jpg";
        } else if (fileName.contains("Ngaynaynamay")) {
            title = "Ngày Này Năm Ấy";
            coverPath = "img/playing6.jpg";
        } else if (fileName.contains("Divangnhatnhoa")) {
            title = "Dĩ Vãng Nhạt Nhòa";
            coverPath = "img/playing7.jpg";
        } else if (fileName.contains("Anhchieutan")) {
            title = "Ánh Chiều Tàn";
            coverPath = "img/playing8.jpg";
        } else if (fileName.contains("10ngannam")) {
            title = "10 Ngàn Năm";
            coverPath = "img/playing9.jpg";
        } else if (fileName.contains("Muonnoivoiem")) {
            title = "Muốn Nói Với Em";
            coverPath = "img/playing10.jpg";
        } else if (fileName.contains("Suytnuathi")) {
            title = "Suýt Nữa Thì";
            coverPath = "img/playing11.jpg";
        } else if (fileName.contains("Anhsaovabautroi")) {
            title = "Ánh Sao Và Bầu Trời";
            coverPath = "img/playing12.jpg";
        } else if (fileName.contains("Bacphan")) {
            title = "Bạc Phận";
            coverPath = "img/playing13.jpg";
        } else if (fileName.contains("Neungayay")) {
            title = "Nếu Ngày Ấy";
            coverPath = "img/playing14.jpg";
        } else if (fileName.contains("Nuocmatemlaubangtinhyeumoi")) {
            title = "Nước Mắt Em Lau Bằng Tình Yêu Mới";
            coverPath = "img/playing15.jpg";
        } else {
            QFileInfo info(fileName);
            title = info.baseName();
        }

        QListWidgetItem *item = new QListWidgetItem(QIcon(coverPath), title);
        item->setData(Qt::UserRole, fileName);
        myFavList->addItem(item);
    }

    // Khi nhấn vào bài hát trong danh sách -> phát nhạc
    connect(myFavList, &QListWidget::itemClicked, this, [=](QListWidgetItem *item) {
        QString filePath = item->data(Qt::UserRole).toString();
        if (filePath.isEmpty() || !QFile::exists(filePath)) {
            QMessageBox::warning(nullptr, "Lỗi", "Không tìm thấy file nhạc: " + filePath);
            return;
        }

        QString coverPath, songName;
        if (filePath.contains("Sorangembietanhconyeuem")) {
            coverPath = "img/playing1_1.png";
            songName = "Sợ Rằng Em Biết Anh Còn Yêu Em";
        } else if (filePath.contains("Cuocsongemonkhong")) {
            coverPath = "img/playing2_1.png";
            songName = "Cuộc Sống Em Ổn Không";
        } else if (filePath.contains("Thangdien")) {
            coverPath = "img/playing3_1.png";
            songName = "Thằng Điên";
        } else if (filePath.contains("Hongkong1")) {
            coverPath = "img/playing4_1.png";
            songName = "Hongkong1";
        } else if (filePath.contains("Text07")) {
            coverPath = "img/playing5_1.png";
            songName = "Text 07";
        } else if (filePath.contains("Ngaynaynamay")) {
            coverPath = "img/playing6_1.png";
            songName = "Ngày Này Năm Ấy";
        } else if (filePath.contains("Divangnhatnhoa")) {
            coverPath = "img/playing7_1.png";
            songName = "Dĩ Vãng Nhạt Nhòa";
        } else if (filePath.contains("Anhchieutan")) {
            coverPath = "img/playing8_1.png";
            songName = "Ánh Chiều Tàn";
        } else if (filePath.contains("10ngannam")) {
            coverPath = "img/playing9_1.png";
            songName = "10 Ngàn Năm";
        } else if (filePath.contains("Muonnoivoiem")) {
            coverPath = "img/playing10_1.png";
            songName = "Muốn Nói Với Em";
        } else if (filePath.contains("Suytnuathi")) {
            coverPath = "img/playing11_1.png";
            songName = "Suýt Nữa Thì";
        } else if (filePath.contains("Anhsaovabautroi")) {
            coverPath = "img/playing12_1.png";
            songName = "Ánh Sao Và Bầu Trời";
        } else if (filePath.contains("Bacphan")) {
            coverPath = "img/playing13_1.png";
            songName = "Bạc Phận";
        } else if (filePath.contains("Neungayay")) {
            coverPath = "img/playing14_1.png";
            songName = "Nếu Ngày Ấy";
        } else if (filePath.contains("Nuocmatemlaubangtinhyeumoi")) {
            coverPath = "img/playing15_1.png";
            songName = "Nước Mắt Em Lau Bằng Tình Yêu Mới";
        } else {
            coverPath = "img/default_1.png";
            songName = QFileInfo(filePath).baseName();
        }

        // Ngắt xoay hoàn toàn
        rotateTimer->stop();
        rotationAngle = 0;
        // Phát bài
        player->setSource(QUrl::fromLocalFile(filePath));
        player->play();
        fadeToPage(0); // chuyển sang tab nhạc

        currentCover = QPixmap("img/default_1.png").scaled(250,250,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        coverArt->setPixmap(currentCover);

        player->setSource(QUrl::fromLocalFile(filePath));
        player->play();
        fadeToPage(0);
        songTitle->setText(QFileInfo(filePath).baseName());
    });
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

void MainWindow::toggleFavorite()
{
    QListWidgetItem *item = songList->currentItem();
    if (!item) return;

    QString filePath;
    QString song = item->text();

    if (song.contains("Sợ Rằng Em Biết Anh Còn Yêu Em"))
        filePath = "Nhac/Sorangembietanhconyeuem.mp3";
    else if (song.contains("Cuộc Sống Em Ổn Không"))
        filePath = "Nhac/Cuocsongemonkhong.mp3";
    else if (song.contains("Thằng Điên"))
        filePath = "Nhac/Thangdien.mp3";
    else if (song.contains("Hongkong1"))
        filePath = "Nhac/Hongkong1.mp3";
    else if (song.contains("Text 07"))
        filePath = "Nhac/Text07.mp3";
    else if (song.contains("Ngày Này Năm Ấy"))
        filePath = "Nhac/Ngaynaynamay.mp3";
    else if (song.contains("Dĩ Vãng Nhạt Nhòa"))
        filePath = "Nhac/Divangnhatnhoa.mp3";
    else if (song.contains("Ánh Chiều Tàn"))
        filePath = "Nhac/Anhchieutan.mp3";
    else if (song.contains("10 Ngàn Năm"))
        filePath = "Nhac/10ngannam.mp3";
    else if (song.contains("Muốn Nói Với Em"))
        filePath = "Nhac/Muonnoivoiem.mp3";
    else if (song.contains("Suýt Nữa Thì"))
        filePath = "Nhac/Suytnuathi.mp3";
    else if (song.contains("Ánh Sao Và Bầu Trời"))
        filePath = "Nhac/Anhsaovabautroi.mp3";
    else if (song.contains("Bạc Phận"))
        filePath = "Nhac/Bacphan.mp3";
    else if (song.contains("Nếu Ngày Ấy"))
        filePath = "Nhac/Neungayay.mp3";
    else if (song.contains("Nước Mắt Em Lau Bằng Tình Yêu Mới"))
        filePath = "Nhac/Nuocmatemlaubangtinhyeumoi.mp3";
    else{
        QMessageBox::warning(this, "Lỗi", "Không nhận diện được bài hát!");
        return;
    }

    // Thêm / Bỏ yêu thích
    if (favorites.contains(filePath)) {
        favorites.remove(filePath);
        btnFav->setText("💖");
    } else {
        favorites.insert(filePath);
        btnFav->setText("💖✔");
    }

    saveFavorites();
    saveUserData();

    updateFavoriteList();
    updateProfilePlaylist();

    saveUserData();
}

void MainWindow::updateProfilePlaylist()
{
    if (!myFavList) return;
    myFavList->clear();

    // Nếu rỗng hiển thị label rỗng trong profile
    if (favorites.isEmpty()) {

    }

    // Tạo item cho mỗi favorites (lưu đường dẫn file trong UserRole)
    for (const QString &fileName : favorites) {
        QString title;
        QString iconPath = "img/default.jpg";

        if (fileName.contains("Sorangembietanhconyeuem")) {
            title = "Sợ Rằng Em Biết Anh Còn Yêu Em";
            iconPath = "img/playing1.jpg";
        } else if (fileName.contains("Cuocsongemonkhong")) {
            title = "Cuộc Sống Em Ổn Không";
            iconPath = "img/playing2.jpg";
        } else if (fileName.contains("Thangdien")) {
            title = "Thằng Điên";
            iconPath = "img/playing3.jpg";
        } else if (fileName.contains("Hongkong1")) {
            title = "Hongkong1";
            iconPath = "img/playing4.jpg";
        } else if (fileName.contains("Text07")) {
            title = "Text 07";
            iconPath = "img/playing5.jpg";
        } else if (fileName.contains("Ngaynaynamay")) {
            title = "Ngày Này Năm Ấy";
            iconPath = "img/playing6.jpg";
        } else if (fileName.contains("Divangnhatnhoa")) {
            title = "Dĩ Vãng Nhạt Nhòa";
            iconPath = "img/playing7.jpg";
        } else if (fileName.contains("Anhchieutan")) {
            title = "Ánh Chiều Tàn";
            iconPath = "img/playing8.jpg";
        } else if (fileName.contains("10ngannam")) {
            title = "10 Ngàn Năm";
            iconPath = "img/playing9.jpg";
        } else if (fileName.contains("Muonnoivoiem")) {
            title = "Muốn Nói Với Em";
            iconPath = "img/playing10.jpg";
        } else if (fileName.contains("Suytnuathi")) {
            title = "Suýt Nữa Thì";
            iconPath = "img/playing11.jpg";
        } else if (fileName.contains("Anhsaovabautroi")) {
            title = "Ánh Sao Và Bầu Trời";
            iconPath = "img/playing12.jpg";
        } else if (fileName.contains("Bacphan")) {
            title = "Bạc Phận";
            iconPath = "img/playing13.jpg";
        } else if (fileName.contains("Neungayay")) {
            title = "Nếu Ngày Ấy";
            iconPath = "img/playing14.jpg";
        } else if (fileName.contains("Nuocmatemlaubangtinhyeumoi")) {
            title = "Nước Mắt Em Lau Bằng Tình Yêu Mới";
            iconPath = "img/playing15.jpg";
        } else {
            title = QFileInfo(fileName).baseName();
        }

        QListWidgetItem *item = new QListWidgetItem(QIcon(iconPath), title);
        item->setData(Qt::UserRole, fileName);
        myFavList->addItem(item);
    }

    // Ngắt tất cả kết nối cũ để tránh tạo connect nhiều lần khi gọi update nhiều lần
    myFavList->disconnect(this);

    // Khi nhấn vào bài hát yêu thích -> phát nhạc và quay lại tab Nhạc
    connect(myFavList, &QListWidget::itemClicked, this, [this](QListWidgetItem *itm){
        QString filePath = itm->data(Qt::UserRole).toString();
        if (filePath.isEmpty() || !QFile::exists(filePath)) {
            QMessageBox::warning(this, "Lỗi", "Không tìm thấy file: " + filePath);
            return;
        }

        // cập nhật cover/tiêu đề tương ứng (có thể refactor thành hàm)
        QString coverPath;
        QString text = itm->text();
        if (filePath.contains("Sorangembietanhconyeuem")) {
            coverPath = "img/playing1_1.png";
        } else if (filePath.contains("Cuocsongemonkhong")) {
            coverPath = "img/playing2_1.png";
        } else if (filePath.contains("Thangdien")) {
            coverPath = "img/playing3_1.png";
        } else if (filePath.contains("Hongkong1")) {
            coverPath = "img/playing4_1.png";
        } else if (filePath.contains("Text07")) {
            coverPath = "img/playing5_1.png";
        } else if (filePath.contains("Ngaynaynamay")) {
            coverPath = "img/playing6_1.png";
        } else if (filePath.contains("Divangnhatnhoa")) {
            coverPath = "img/playing7_1.png";
        } else if (filePath.contains("Anhchieutan")) {
            coverPath = "img/playing8_1.png";
        } else if (filePath.contains("10ngannam")) {
            coverPath = "img/playing9_1.png";
        } else if (filePath.contains("Muonnoivoiem")) {
            coverPath = "img/playing10_1.png";
        } else if (filePath.contains("Suytnuathi")) {
            coverPath = "img/playing11_1.png";
        } else if (filePath.contains("Anhsaovabautroi")) {
            coverPath = "img/playing12_1.png";
        } else if (filePath.contains("Bacphan")) {
            coverPath = "img/playing13_1.png";
        } else if (filePath.contains("Neungayay")) {
            coverPath = "img/playing14_1.png";
        } else if (filePath.contains("Nuocmatemlaubangtinhyeumoi")) {
            coverPath = "img/playing15_1.png";
        } else {
            coverPath = "img/default_1.png";
        }

        // set player và UI
        player->setSource(QUrl::fromLocalFile(filePath));
        currentCover = QPixmap(coverPath).scaled(250,250,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        coverArt->setPixmap(currentCover);
        songTitle->setText(itm->text());
        player->play();
        rotateTimer->start(50);

        // chuyển về trang Nhạc
        fadeToPage(0);
    });
}

void MainWindow::onSearchReturnPressed() {
    // chưa dùng, để trống
}

void MainWindow::handleSearchReply(QNetworkReply *reply) {
    Q_UNUSED(reply);
    // chưa dùng, để trống
}
void MainWindow::exchangeSpotifyCode(const QString &code)
{
    QNetworkRequest req(QUrl("https://accounts.spotify.com/api/token"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QByteArray body;
    body.append("grant_type=authorization_code");
    body.append("&code=" + QUrl::toPercentEncoding(code));
    body.append("&redirect_uri=" + QUrl::toPercentEncoding(SPOTIFY_REDIRECT_URI));
    body.append("&client_id=" + SPOTIFY_CLIENT_ID.toUtf8());
    body.append("&client_secret=" + SPOTIFY_CLIENT_SECRET.toUtf8());

    QNetworkReply *reply = networkManager->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray resp = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject()) {
            QMessageBox::warning(this, "Spotify", "Không nhận được token (parse error).");
            return;
        }
        QJsonObject obj = doc.object();
        spotifyUserToken = obj.value("access_token").toString();
        spotifyRefreshToken = obj.value("refresh_token").toString();
        int expires = obj.value("expires_in").toInt(3600);
        qDebug() << "Got user access token len=" << spotifyUserToken.size() << " expires_in=" << expires;

        getUserDevices();

        QMessageBox::information(this, "Spotify", "Đăng nhập Spotify thành công!");
    });
}

void MainWindow::refreshUserAccessTokenIfNeeded()
{
    if (spotifyRefreshToken.isEmpty()) return;
    if (!spotifyUserToken.isEmpty() && QDateTime::currentDateTimeUtc() < spotifyUserTokenExpiry.addSecs(-30)) {
        return; // token còn hạn
    }

    QNetworkRequest req(QUrl("https://accounts.spotify.com/api/token"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QByteArray body;
    body.append("grant_type=refresh_token");
    body.append("&refresh_token=" + QUrl::toPercentEncoding(spotifyRefreshToken));
    body.append("&client_id=" + SPOTIFY_CLIENT_ID.toUtf8());
    body.append("&client_secret=" + SPOTIFY_CLIENT_SECRET.toUtf8());

    QNetworkReply *reply = networkManager->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray resp = reply->readAll();
        reply->deleteLater();
        QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject()) return;
        QJsonObject obj = doc.object();
        QString token = obj.value("access_token").toString();
        int expires = obj.value("expires_in").toInt(3600);
        if (!token.isEmpty()) {
            spotifyUserToken = token;
            spotifyUserTokenExpiry = QDateTime::currentDateTimeUtc().addSecs(expires);
            qDebug() << "Refreshed spotify user token";
        } else {
            qWarning() << "Failed to refresh token:" << resp;
        }
    });
}

void MainWindow::getUserDevices()
{
    refreshUserAccessTokenIfNeeded();

    if (spotifyUserToken.isEmpty()) {
        QMessageBox::information(this, "Spotify", "Vui lòng đăng nhập Spotify trước.");
        return;
    }

    QNetworkRequest req(QUrl("https://api.spotify.com/v1/me/player/devices"));
    req.setRawHeader("Authorization", ("Bearer " + spotifyUserToken).toUtf8());
    QNetworkReply *reply = networkManager->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray resp = reply->readAll();
        reply->deleteLater();
        QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject()) {
            qWarning() << "Device parse error:" << resp;
            return;
        }

        QJsonArray devices = doc["devices"].toArray();
        if (devices.isEmpty()) {
            QMessageBox::information(this, "Spotify",
                                     "Không tìm thấy thiết bị Spotify đang online.\n"
                                     "👉 Mở Spotify trên máy tính hoặc điện thoại rồi thử lại.");
            return;
        }

        QString chosen;
        for (const QJsonValue &v : devices) {
            QJsonObject d = v.toObject();
            QString id = d["id"].toString();
            QString name = d["name"].toString();
            bool isActive = d["is_active"].toBool();
            qDebug() << "Found device:" << name << "id=" << id;
            if (isActive) chosen = id;
        }

        if (chosen.isEmpty())
            chosen = devices.first().toObject()["id"].toString();

        lastDeviceId = chosen;
        QMessageBox::information(this, "Spotify",
                                 "🎧 Đã phát hiện thiết bị Spotify:\n" + lastDeviceId +
                                     "\nApp sẽ phát nhạc trên thiết bị đó.");
    });
}

void MainWindow::transferPlaybackToDevice(const QString &deviceId, bool play)
{
    if (spotifyUserToken.isEmpty()) return;

    QNetworkRequest req(QUrl("https://api.spotify.com/v1/me/player"));
    req.setRawHeader("Authorization", ("Bearer " + spotifyUserToken).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    QJsonArray arr; arr.append(deviceId);
    obj["device_ids"] = arr;
    obj["play"] = play;

    QNetworkReply *reply = networkManager->put(req, QJsonDocument(obj).toJson());
    connect(reply, &QNetworkReply::finished, this, [reply]() {
        qDebug() << "Transfer reply:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
    });
}

void MainWindow::playSpotifyOnDevice(const QString &trackUri, const QString &deviceId)
{
    if (spotifyUserToken.isEmpty()) return;

    QString url = "https://api.spotify.com/v1/me/player/play";
    if (!deviceId.isEmpty())
        url += "?device_id=" + QUrl::toPercentEncoding(deviceId);

    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("Authorization", ("Bearer " + spotifyUserToken).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    QJsonArray arr; arr.append(trackUri);
    obj["uris"] = arr;

    QNetworkReply *reply = networkManager->put(req, QJsonDocument(obj).toJson());
    connect(reply, &QNetworkReply::finished, this, [reply]() {
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray resp = reply->readAll();
        reply->deleteLater();
        qDebug() << "Play reply:" << code << resp;
        if (code == 403) QMessageBox::warning(nullptr, "Spotify", "Bạn cần Spotify Premium để phát nhạc trực tiếp.");
        if (code == 404) QMessageBox::information(nullptr, "Spotify", "Không tìm thấy thiết bị Spotify đang hoạt động.");
    });
}

void MainWindow::playSpotifyTrack(const QString &trackUri)
{
    if (spotifyUserToken.isEmpty()) {
        QMessageBox::information(this, "Spotify", "Vui lòng đăng nhập Spotify trước.");
        return;
    }

    QNetworkRequest req(QUrl("https://api.spotify.com/v1/me/player/play"));
    req.setRawHeader("Authorization", ("Bearer " + spotifyUserToken).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QByteArray body = QString(R"({"uris":["%1"]})").arg(trackUri).toUtf8();
    QNetworkReply *reply = networkManager->put(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray resp = reply->readAll();
        qDebug() << "Play response:" << resp;
        reply->deleteLater();
        // optionally parse response and show error
    });
}
