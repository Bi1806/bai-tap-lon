#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QListWidget>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Tài khoản
    void loginUser();
    void registerUser();
    void saveUserData();
    void loadUserData();

private:
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
};

#endif // MAINWINDOW_H