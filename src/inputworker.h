// ============================================================
// inputworker.h —— 键盘捕获 + HID 通信独立工作线程。
//
// 设计要点（反作弊友好）：
//  - 键盘捕获使用 Windows Raw Input（RegisterRawInputDevices，RIDEV_INPUTSINK），
//    **不使用** WH_KEYBOARD_LL 低级全局钩子。Raw Input 是罗技 GHub / 雷蛇 Synapse
//    等正规外设软件采用的方案，检测特征远低于 LL 钩子。
//  - Raw Input 需要一个有效窗口句柄承载 WM_INPUT 消息。本类在独立 std::thread 内
//    创建一个 message-only 窗口（HWND_MESSAGE）并跑自己的 GetMessage 循环，
//    与 Qt UI 线程严格分离。
//  - HID 设备（Device）由本工作线程独占持有：连接、版本查询、状态注入全部在
//    工作线程完成，UI 线程只通过 PostMessage 下发命令、通过 Qt 队列信号接收结果。
//  - 仅在按键状态发生“按下/松开”变化时才下发 SET_INPUT_STATE，维持稳态不发包；
//    对系统自动重复(held-down repeat)与极短反跳做去重/滤波。
//  - 本程序只同步键盘原始状态给固件，不做任何组合键判断、不生成任何模拟输入。
// ============================================================
#pragma once

#include <cstdint>
#include <set>

#include <QObject>
#include <QString>

class InputWorker : public QObject {
    Q_OBJECT
public:
    explicit InputWorker(QObject* parent = nullptr);
    ~InputWorker();

    // 启动工作线程并等待其消息窗口就绪。返回 false 表示线程/窗口创建失败。
    bool start();
    // 停止工作线程（停止捕获、断开设备、退出消息循环并 join）。
    void stop();

    // 以下均为线程安全的“请求”接口：仅向工作线程 PostMessage，立即返回，
    // 实际动作在工作线程执行，结果通过下方信号异步回传。
    void requestConnect();
    void requestDisconnect();
    void requestEnableCapture();
    void requestDisableCapture();

signals:
    // 连接结果。ok=是否成功；fwText=固件版本展示串；supportsInject=固件是否支持注入。
    void connected(bool ok, const QString& fwText, bool supportsInject, const QString& message);
    // 固件中已配置（可监控）的键盘 usage 白名单（连接成功后回传，用于 UI 灰显）。
    void allowedUsages(const std::set<uint32_t>& usages);
    void disconnected();
    // 捕获开关状态变化（真正生效后回传）。
    void captureStateChanged(bool on);
    // 单个键盘 usage 的按下/松开状态变化（用于 UI 高亮与日志）。
    void keyStateChanged(uint32_t usage, bool pressed);
    void logMessage(const QString& line);
    void statusMessage(const QString& text, bool error);

private:
    struct Impl;
    Impl* d_ = nullptr;
};

// 由 Windows 扫描码 + 扩展位映射到 HID 键盘 usage code（低字节，0 表示未知）。
uint8_t scancode_to_hid(uint32_t scanCode, bool extended);
