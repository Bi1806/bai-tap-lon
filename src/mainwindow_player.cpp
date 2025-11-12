#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    player = new QMediaPlayer(this);
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

    setupUI();
    
    connect(player, &QMediaPlayer::durationChanged, this, &MainWindow::durationChanged);
    connect(player, &QMediaPlayer::mediaStatusChanged, this, &MainWindow::mediaStatusChanged);
}

MainWindow::~MainWindow()
{
    delete player;
}

void MainWindow::setupUI() {
    // --- Trang nghe nhạc ---
    QWidget *musicPage = new QWidget;
    QVBoxLayout *musicLayout = new QVBoxLayout(musicPage);

    coverArt = new QLabel;
    coverArt->setPixmap(QPixmap("img/default.jpg").scaled(280, 280, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    coverArt->setAlignment(Qt::AlignCenter);

    // --- Tiêu đề bài hát ---
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

    // Thanh âm lượng
    QHBoxLayout *volumeLayout = new QHBoxLayout;
    QLabel *volLabel = new QLabel("🔊 Âm lượng:");
    volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(70);
    audioOutput->setVolume(0.7);
    volumeLayout->addWidget(volLabel);
    volumeLayout->addWidget(volumeSlider);

    musicLayout->addWidget(songTitle);
    musicLayout->addWidget(progressSlider);
    musicLayout->addWidget(timeLabel);
    musicLayout->addLayout(controlLayout);
    musicLayout->addLayout(volumeLayout);

    connect(btnPlay, &QPushButton::clicked, this, &MainWindow::playSelectedSong);
    connect(btnPause, &QPushButton::clicked, this, &MainWindow::pauseOrResume);
    connect(btnStop, &QPushButton::clicked, this, &MainWindow::stopMusic);
    connect(btnNext, &QPushButton::clicked, this, &MainWindow::nextSong);
    connect(btnPrev, &QPushButton::clicked, this, &MainWindow::prevSong);

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
