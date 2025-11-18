#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>

class SpotifyAPI : public QObject {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Mạng / Tìm kiếm
    void onSearchReturnPressed(); // slot helper (optional)
    void handleSearchReply(QNetworkReply *reply); // optional helper

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
    // Mạng
    QNetworkAccessManager *networkManager = nullptr;

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