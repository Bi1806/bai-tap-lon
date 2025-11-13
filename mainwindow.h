#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QListWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include <QLabel>
#include <QStackedWidget>
#include <QSlider>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QAudioOutput>
#include <QSet>
#include <QVector>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDesktopServices>
#include <QProcess>
#include <QInputDialog>

class ClickableLabel : public QLabel {
    Q_OBJECT

public:
    explicit ClickableLabel(QWidget *parent = nullptr) : QLabel(parent) {}
signals:
    void clicked(); // phát tín hiệu khi người dùng click
protected:
    void mousePressEvent(QMouseEvent *event) override {
        emit clicked(); // phát tín hiệu click
        QLabel::mousePressEvent(event); // giữ hành vi QLabel gốc
    }
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Nhạc cơ bản
    void playSelectedSong(); // Phát bài hát local
    void pauseOrResume(); // Tạm dừng / Tiếp tục

    void stopMusic(); // Dừng phát
    void updateProgress(); // Cập nhật thanh tiến trình
    void setVolume(int value);

    void nextSong(); // Bài kế
    void prevSong(); // Bài trước
    void durationChanged(qint64 duration);  // Khi độ dài bài hát thay đổi
    void mediaStatusChanged(QMediaPlayer::MediaStatus status); // Thay đổi âm lượng
    void toggleFavorite();
    void updateFavoriteList();

    // Tài khoản
    void loginUser();
    void registerUser();

    // Mạng / Tìm kiếm
    void onSearchReturnPressed(); // slot helper (optional)
    void handleSearchReply(QNetworkReply *reply); // optional helper

    void setupUI();
    void fadeToPage(int index); // Hiệu ứng chuyển trang

    void loadFavorites(); // Đọc danh sách yêu thích từ file
    void saveFavorites(); // Lưu danh sách yêu thích vào file

    void saveUserData();
    void loadUserData();

    void searchSpotify(const QString &query); // Tìm kiếm bài hát

    void startSpotifyLogin(); // Đăng nhập người dùng Spotify
    void exchangeSpotifyCode(const QString &code); // Nhận mã trả về và đổi lấy token
    void fetchSpotifyToken();   // Lấy token từ file (nếu có)
    void playSpotifyTrack(const QString &trackUri);
    void startSpotifyAuthServer();

    void refreshUserAccessTokenIfNeeded();
    void getUserDevices(); // Lấy danh sách thiết bị phát nhạc
    void transferPlaybackToDevice(const QString &deviceId, bool play = false);
    void playSpotifyOnDevice(const QString &trackUri, const QString &deviceId = QString()); // Phát bài hát

private:
    QWidget *central;
    QStackedWidget *stackedWidget;

    // Player UI
    QLabel *coverArt;
    QLabel *songTitle; // Tên bài hát đang phát
    QListWidget *songList;
    QListWidget *exploreList; // danh sách kết quả khám phá / search
    QPushButton *btnPlay, *btnPause, *btnStop;
    QPushButton *btnNext, *btnPrev, *btnFav;
    QSlider *progressSlider; // Thanh tiến trình
    QSlider *volumeSlider; // Thanh chỉnh âm lượng
    QLabel *timeLabel; // Hiển thị thời gian phát
    QMediaPlayer *player; // Trình phát nhạc
    QAudioOutput *audioOutput; // Thiết bị âm thanh
    QTimer *timer; // Đồng hồ cập nhật tiến trình
    QWidget *profilePage;

    QWidget *bottomNav;
    QPushButton *btnTabMusic, *btnTabExplore, *btnTabProfile;

    // playlist and favorites
    QVector<QString> playlistFiles;   // Danh sách file đang phát
    int currentIndex = -1;            // Vị trí bài hát hiện tại
    QSet<QString> favorites;          // lưu tên file hoặc path
    QListWidget *favoriteList = nullptr;
    QWidget *exploreTab = nullptr;

    // Mạng
    QNetworkAccessManager *networkManager = nullptr;

    // đĩa xoay
    QTimer *rotateTimer;
    qreal rotationAngle = 0;
    QPixmap currentCover;

    QLineEdit *searchBox;

    // ==== tài khoản ====
    QString currentUser;
    QWidget *loginPage = nullptr;
    QLineEdit *usernameInput = nullptr;
    QLineEdit *passwordInput = nullptr;
    QPushButton *btnLogin = nullptr;
    QPushButton *btnRegister = nullptr;

    bool isLoggedIn = false;  // trạng thái đăng nhập
    QWidget *profileMainPage; // trang hồ sơ chính sau đăng nhập

    QStackedWidget *profileStack;
    QLabel *userNameLabel;

    QListWidget *myFavList = nullptr; // playlist hiển thị trong trang cá nhân
    void updateProfilePlaylist();

    // --- Spotify API ---
    QString spotifyToken;
    QDateTime spotifyTokenExpiry;

    QTcpServer *server;
    QString spotifyUserToken;
    QString spotifyRefreshToken;

    QString lastDeviceId;
    QDateTime spotifyUserTokenExpiry;
};

#endif // MAINWINDOW_H
