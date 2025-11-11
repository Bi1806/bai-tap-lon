#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QListWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVector>
#include <QMouseEvent>

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
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 🎵 Các chức năng chính của trình phát nhạc local
    void playSelectedSong();   // Chọn và phát nhạc từ file local
    void pauseOrResume();      // Tạm dừng / tiếp tục phát
    void stopMusic();          // Dừng phát nhạc
    void nextSong();           // Phát bài kế tiếp
    void prevSong();           // Phát bài trước đó
    void updateProgress();     // Cập nhật thanh tiến trình
    void durationChanged(qint64 duration); // Khi độ dài bài hát thay đổi
    void setVolume(int value); // Thay đổi âm lượng
    void setupUI(); // Giao diện chính

private:

    QLabel *songTitle;          // Tên bài hát đang phát
    QSlider *progressSlider;    // Thanh tiến trình
    QLabel *timeLabel;          // Hiển thị thời gian phát
    QSlider *volumeSlider;      // Thanh chỉnh âm lượng

    QPushButton *btnPlay;       // Nút phát
    QPushButton *btnPause;      // Nút tạm dừng
    QPushButton *btnStop;       // Nút dừng
    QPushButton *btnNext;       // Nút bài kế tiếp
    QPushButton *btnPrev;       // Nút bài trước

    QMediaPlayer *player;       // Trình phát nhạc
    QAudioOutput *audioOutput;  // Thiết bị âm thanh
    QTimer *timer;              // Đồng hồ cập nhật tiến trình

    QVector<QString> playlistFiles; // Danh sách file đang phát
    int currentIndex = -1;          // Vị trí bài hát hiện tại
};

#endif // MAINWINDOW_H