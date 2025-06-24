#include "toggle_window.hpp"
#include <QHBoxLayout>
#include <QDebug>
#include <QVBoxLayout>
#include <QTimer>
#include <QElapsedTimer>
#include <QLabel>

ToggleWindow::ToggleWindow(SharedToggleState shared_state, QWidget *parent)
    : QWidget(parent), external_state(shared_state),
      label_names{ "멀미", "불편함", "불안감" } {

    QVBoxLayout *main_layout = new QVBoxLayout(this);
    time_label = new QLabel("00:00:00");
    main_layout->addWidget(time_label);  // ⏱️ 시간 표시 라벨 추가

    QHBoxLayout *button_layout = new QHBoxLayout();

    for (int i = 0; i < 3; ++i) {
        toggle_states[i] = false;
        (*external_state)[i] = 0;
        buttons[i] = new QPushButton(label_names[i] + " OFF");
        button_layout->addWidget(buttons[i]);

        connect(buttons[i], &QPushButton::clicked, [=]() {
            toggle_states[i] = !toggle_states[i];
            buttons[i]->setText(label_names[i] + (toggle_states[i] ? " ON" : " OFF"));
            (*external_state)[i] = toggle_states[i] ? 1 : 0;
        });
    }

    main_layout->addLayout(button_layout);
    setLayout(main_layout);
    setWindowTitle("감정 상태 토글");
    resize(300, 150);

    // ⏱️ 경과 시간 타이머 시작
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
}

void ToggleWindow::printStates() {
    qDebug() << "현재 상태:";
    for (int i = 0; i < 3; ++i) {
        qDebug() << " -" << label_names[i] << ":" << ((*external_state)[i] ? "ON" : "OFF");
    }
}
