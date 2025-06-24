#include "toggle_window.hpp"
#include <QVBoxLayout>
#include <QDebug>

ToggleWindow::ToggleWindow(QWidget *parent) : QWidget(parent), label_names{ "멀미", "불편함", "불안감" } {
    QVBoxLayout *layout = new QVBoxLayout(this);

    for (int i = 0; i < 3; ++i) {
        toggle_states[i] = false;
        buttons[i] = new QPushButton(label_names[i] + " OFF");
        layout->addWidget(buttons[i]);

        connect(buttons[i], &QPushButton::clicked, [=]() {
            toggle_states[i] = !toggle_states[i];
            buttons[i]->setText(label_names[i] + (toggle_states[i] ? " ON" : " OFF"));
            printStates();
        });
    }

    setLayout(layout);
    setWindowTitle("감정 상태 토글");
    resize(200, 150);
}

void ToggleWindow::printStates() {
    qDebug() << "현재 상태:";
    for (int i = 0; i < 3; ++i) {
        qDebug() << " -" << label_names[i] << ":" << (toggle_states[i] ? "ON" : "OFF");
    }
}
