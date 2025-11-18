#include "mainwindow.h"
#include <QUrlQuery>
#include <QDebug>

// Thay thế những thông tin này bằng thông tin xác thực
static const QString SPOTIFY_CLIENT_ID = "1bb921c950964cada3bf8b4a7b3c2ace";
static const QString SPOTIFY_CLIENT_SECRET = "c5cb36e4fe10419fbcf94b4d245f9bf4";
static const QString SPOTIFY_REDIRECT_URI = "http://127.0.0.1:8888/callback";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
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

    // Lấy token Spotify sẵn sàng
    fetchSpotifyToken();
}
void MainWindow::setupUI()
{
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
}
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