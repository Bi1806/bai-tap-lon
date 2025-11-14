#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSet>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QString>
#include <QListWidget>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void toggleFavorite();
    void updateFavoriteList();
    void loadFavorites(); // Đọc danh sách yêu thích từ file
    void saveFavorites(); // Lưu danh sách yêu thích vào file

private:

    QPushButton *btnFav;
    QSet<QString> favorites; // lưu tên file hoặc path
    QListWidget *favoriteList = nullptr;
    QWidget *exploreTab = nullptr;
    QListWidget *myFavList = nullptr; // playlist hiển thị trong trang cá nhân
    void updateProfilePlaylist();
};

#endif // MAINWINDOW_H