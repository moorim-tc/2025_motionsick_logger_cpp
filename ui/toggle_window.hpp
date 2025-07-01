#pragma once

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QElapsedTimer>
#include <QPushButton>
#include <QTimer>
#include <array>
#include <atomic>
#include <memory>
#include "shared_structs.hpp"

class ToggleWindow : public QWidget {
    Q_OBJECT

public:
    explicit ToggleWindow(SharedToggleState shared_state, QWidget *parent = nullptr);

signals:
    void faceDetectionChanged(bool detected);  // ✅ 외부 스레드에서 emit할 수 있는 신호

public slots:
    void setFaceDetected(bool detected);       // ✅ 시그널을 받아 UI를 업데이트하는 슬롯

private:
    std::array<QPushButton*, 3> buttons;
    std::array<bool, 3> toggle_states;
    std::array<QString, 3> label_names;

    QLabel* time_label;       // ⏱️ 경과 시간 표시용
    QLabel* status_circle;    // 🟢/⚫️ 얼굴 감지 상태 원
    QElapsedTimer elapsed_timer;
    QTimer* update_timer;

    SharedToggleState external_state;  // 외부와 공유되는 상태 포인터

    void printStates();
    void close_app();
};
