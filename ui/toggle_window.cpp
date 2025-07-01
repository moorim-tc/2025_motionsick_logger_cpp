#include "toggle_window.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QApplication>
#include <QScreen>
#include <QSizePolicy>
#include <QDebug>

ToggleWindow::ToggleWindow(SharedToggleState shared_state, QWidget *parent)
    : QWidget(parent), external_state(shared_state),
      label_names{ "멀미", "불편함", "불안감" } {

    // Remove window decorations
    setWindowFlags(Qt::FramelessWindowHint);

    // Fullscreen setup and black background
    setWindowTitle("Data Logger");
    setStyleSheet("background-color: black;");
    showFullScreen();  // Use full screen including hiding the menu bar

    setCursor(Qt::BlankCursor);  // 🔒 Hide mouse cursor

    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        setGeometry(screen->geometry());
    }

 
    // Face detection status circle
    status_circle = new QLabel();
    status_circle->setFixedSize(20, 20);
    status_circle->setStyleSheet("background-color: gray; border-radius: 10px;");

    // Time label
    time_label = new QLabel("00:00:00");
    time_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    time_label->setStyleSheet("font-size: 18px; color: white;");

    // Secret quit button
    QPushButton *quit_button = new QPushButton("");
    quit_button->setFixedSize(40, 40);
    quit_button->setStyleSheet("background-color: black; border: none;");
    connect(quit_button, &QPushButton::clicked, this, &ToggleWindow::close_app);

    // Button creation
    QHBoxLayout *btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(10);
    btn_layout->setContentsMargins(10, 10, 10, 10);

    for (int i = 0; i < 3; ++i) {
        toggle_states[i] = false;
        (*external_state)[i] = 0;

        buttons[i] = new QPushButton(label_names[i]);
        buttons[i]->setCheckable(true);
        buttons[i]->setFixedHeight(200);
        buttons[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        buttons[i]->setStyleSheet(
            "QPushButton { font-size: 40px; background-color: #333; color: white; border-radius: 8px; }"
            "QPushButton:checked { background-color: #e69500; color: black; }"
        );

        btn_layout->addWidget(buttons[i]);

        connect(buttons[i], &QPushButton::clicked, [=]() {
            toggle_states[i] = buttons[i]->isChecked();
            (*external_state)[i] = toggle_states[i] ? 1 : 0;
            printStates();
        });
    }

    // Top layout: time + invisible quit
    QHBoxLayout *top_layout = new QHBoxLayout();
    top_layout->addWidget(status_circle, 0, Qt::AlignLeft| Qt::AlignVCenter);
    top_layout->addWidget(time_label, 0, Qt::AlignLeft| Qt::AlignVCenter);
    top_layout->addStretch();
    top_layout->addWidget(quit_button, 0, Qt::AlignRight);

    // Main layout
    QVBoxLayout *main_layout = new QVBoxLayout();
    main_layout->addLayout(top_layout, 1);
    main_layout->addLayout(btn_layout, 3);
    main_layout->setSpacing(10);
    main_layout->setContentsMargins(10, 10, 10, 10);
    setLayout(main_layout);

    // Timer to update elapsed time
    elapsed_timer.start();
    update_timer = new QTimer(this);
    connect(update_timer, &QTimer::timeout, this, [=]() {
        int seconds = elapsed_timer.elapsed() / 1000;
        int hours = seconds / 3600;
        int minutes = (seconds % 3600) / 60;
        int secs = seconds % 60;
        time_label->setText(QString("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0')));
    });
    update_timer->start(1000);

    // ✅ 얼굴 감지 상태 업데이트용 Signal-Slot 연결
    connect(this, &ToggleWindow::faceDetectionChanged, this, &ToggleWindow::setFaceDetected);
}

void ToggleWindow::printStates() {
    qDebug() << "[Toggle] Current states:";
    for (int i = 0; i < 3; ++i) {
        qDebug() << " -" << label_names[i] << ":" << ((*external_state)[i] ? "ON" : "OFF");
    }
}

void ToggleWindow::close_app() {
    qDebug() << "[Toggle] Secret quit triggered";
    close();
    QApplication::quit();
}

void ToggleWindow::setFaceDetected(bool detected) {
    if (detected) {
        status_circle->setStyleSheet("background-color: green; border-radius: 10px;");
    } else {
        status_circle->setStyleSheet("background-color: gray; border-radius: 10px;");
    }
}
