#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // load favorites từ file (nếu có)
    loadFavorites();
}

MainWindow::~MainWindow()
{
    delete player;
}

void MainWindow::setupUI()
{
    btnFav  = new QPushButton("💖");
    controlLayout->addWidget(btnFav);

    connect(btnFav, &QPushButton::clicked, this, &MainWindow::toggleFavorite);
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