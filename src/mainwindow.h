// ============================================================
// mainwindow.h —— 上位机主界面（键盘注入模式）。
// 核心用途：监听本机 Windows 键盘 -> 把按下/松开的键作为标准键盘 usage
// 注入固件（SET_INPUT_STATE），让固件用它预设的“键盘+鼠标组合”规则触发输出。
// 键鼠输出逻辑全在固件内部，本程序只做状态采集与下发。
//
// 界面：顶部连接 + “启用注入”开关；中间键盘控件实时高亮当前按下的键；
// 底部日志显示注入事件。
// ============================================================
#pragma once

#include <cstdint>
#include <set>

#include <QMainWindow>

class KeyboardWidget;
class InputWorker;
class QLabel;
class QPushButton;
class QPlainTextEdit;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    // 来自工作线程的队列信号槽。
    void onConnected(bool ok, const QString& fwText, bool supportsInject, const QString& message);
    void onAllowedUsages(const std::set<uint32_t>& usages);
    void onDisconnected();
    void onCaptureStateChanged(bool on);
    void onKeyStateChanged(uint32_t usage, bool pressed);

private:
    void setStatus(const QString& text, bool error = false);
    void log(const QString& line);
    void refreshHighlight();
    // 更新连接状态圆点指示灯：connected=true 绿色，false 红色。
    void setConnDot(bool connected);

    InputWorker* worker_ = nullptr;
    bool connected_ = false;
    bool firmwareSupportsInject_ = false;

    // 当前处于“按下”状态的键（仅用于 UI 高亮，由工作线程信号驱动）。
    std::set<uint32_t> pressed_;

    KeyboardWidget* keyboard_ = nullptr;
    QLabel* connDot_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* fwLabel_ = nullptr;
    QPushButton* connectBtn_ = nullptr;
    QLabel* injectLabel_ = nullptr;
    QPlainTextEdit* logView_ = nullptr;
};
