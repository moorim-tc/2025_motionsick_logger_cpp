#pragma once

#include <QObject> 
#include <QWidget>
#include <QLabel>
#include <QElapsedTimer>
#include <QPushButton>
#include <array>
#include <atomic>
#include <memory>
#include "shared_structs.hpp"

class ToggleWindow : public QWidget {
    Q_OBJECT

public:
    ToggleWindow(SharedToggleState shared_state, QWidget *parent = nullptr);

private:
    std::array<QPushButton*, 3> buttons;
    std::array<bool, 3> toggle_states;
    std::array<QString, 3> label_names;

    QLabel* time_label;  // ⏱️ 경과 시간 표시용
    QElapsedTimer elapsed_timer;
    QTimer* update_timer;

    SharedToggleState external_state;  // 외부와 공유되는 상태 포인터

    void printStates();
    void close_app(); 
};
