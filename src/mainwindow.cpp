#include "mainwindow.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "keyboardwidget.h"
#include "inputworker.h"
#include "protocol.h"
#include "usages.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>
// Win11 build 22000+ 支持自定义标题栏颜色；旧 SDK 未定义时补上常量。
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

// 把标题栏改成与整体深色一致（背景 #181A1B，文字 #d4d9db）。
// DWM 用 0x00BBGGRR 格式的 COLORREF。
static void applyDarkTitleBar(HWND hwnd) {
    if (!hwnd) return;
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    COLORREF caption = RGB(0x18, 0x1A, 0x1B); // #181A1B
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    COLORREF text = RGB(0xD4, 0xD9, 0xDB);    // #d4d9db
    DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &text, sizeof(text));
}
#endif

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("USB 设备配置工具"));
    resize(980, 640);

    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("central"));
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(14);

    // --- 顶部工具栏 ---
    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(12);
    connectBtn_ = new QPushButton(QStringLiteral("连接设备"));
    connectBtn_->setCursor(Qt::PointingHandCursor);
    // 连接状态圆点指示灯：位于“键盘状态同步”文字左侧，断开红色 / 连上绿色。
    connDot_ = new QLabel();
    connDot_->setFixedSize(12, 12);
    injectLabel_ = new QLabel(QStringLiteral("键盘状态同步"));
    fwLabel_ = new QLabel(QStringLiteral("固件: -"));
    fwLabel_->setStyleSheet(QStringLiteral("color:#8b9498; font-size:12px;"));
    toolbar->addWidget(connectBtn_);
    toolbar->addSpacing(4);
    toolbar->addWidget(connDot_);
    toolbar->addWidget(injectLabel_);
    toolbar->addSpacing(16);
    toolbar->addWidget(fwLabel_);
    toolbar->addStretch();
    root->addLayout(toolbar);

    auto* hint = new QLabel(QStringLiteral(
        "说明：连接设备后自动开启键盘状态同步，本机键盘按下、松开的按键状态同步至固件。"
        "固件将使用网页预先配置的「键盘+鼠标」组合规则执行输出。"
        "按键盘上的 END 键可随时停止同步（END 键本身不会同步给固件）。"
        "本程序不会模拟任何键鼠信号。"));
    hint->setObjectName(QStringLiteral("hintLabel"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    // --- 键盘控件 ---
    keyboard_ = new KeyboardWidget();
    auto* scroll = new QScrollArea();
    scroll->setWidget(keyboard_);
    scroll->setWidgetResizable(true);
    scroll->setMinimumHeight(280);
    root->addWidget(scroll, 3);

    // --- 日志 ---
    auto* logTitle = new QLabel(QStringLiteral("事件日志"));
    logTitle->setStyleSheet(QStringLiteral("color:#8b9498; font-size:12px; font-weight:600;"));
    root->addWidget(logTitle);

    logView_ = new QPlainTextEdit();
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(500);
    logView_->setMinimumHeight(140);
    root->addWidget(logView_, 1);

    statusLabel_ = new QLabel(QStringLiteral("未连接"));
    statusLabel_->setObjectName(QStringLiteral("statusLabel"));
    root->addWidget(statusLabel_);

    setCentralWidget(central);

    setConnDot(false);

#ifdef _WIN32
    // 让原生标题栏跟随深色主题（Win11 22000+ 生效；旧系统忽略，无副作用）。
    applyDarkTitleBar(reinterpret_cast<HWND>(winId()));
#endif

    worker_ = new InputWorker(this);

    connect(connectBtn_, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(worker_, &InputWorker::connected, this, &MainWindow::onConnected);
    connect(worker_, &InputWorker::allowedUsages, this, &MainWindow::onAllowedUsages);
    connect(worker_, &InputWorker::disconnected, this, &MainWindow::onDisconnected);
    connect(worker_, &InputWorker::captureStateChanged, this, &MainWindow::onCaptureStateChanged);
    connect(worker_, &InputWorker::keyStateChanged, this, &MainWindow::onKeyStateChanged);
    connect(worker_, &InputWorker::logMessage, this, &MainWindow::log);
    connect(worker_, &InputWorker::statusMessage, this,
            [this](const QString& t, bool err) { setStatus(t, err); });

    if (!worker_->start()) {
        setStatus(QStringLiteral("键盘捕获工作线程启动失败"), true);
        connectBtn_->setEnabled(false);
    }
}

MainWindow::~MainWindow() {
    if (worker_) {
        worker_->stop();
    }
}

void MainWindow::setStatus(const QString& text, bool error) {
    statusLabel_->setText(text);
    // 保留 QSS 的背景/圆角/内边距，只切换文字颜色。
    statusLabel_->setStyleSheet(
        QStringLiteral("QLabel#statusLabel { background:#222426; border:1px solid #313739;"
                       " border-radius:6px; padding:6px 12px; font-size:12px; color:%1; }")
            .arg(error ? QStringLiteral("#ff6b6b") : QStringLiteral("#4ade80")));
}

void MainWindow::setConnDot(bool connected) {
    // 圆形指示灯：连上绿色(#4ade80)，断开红色(#ff6b6b)。
    const QString color = connected ? QStringLiteral("#4ade80") : QStringLiteral("#ff6b6b");
    connDot_->setStyleSheet(
        QStringLiteral("border-radius:6px; background:%1;").arg(color));
    connDot_->setToolTip(connected ? QStringLiteral("已连接") : QStringLiteral("未连接"));
}

void MainWindow::log(const QString& line) {
    logView_->appendPlainText(
        QDateTime::currentDateTime().toString("HH:mm:ss.zzz") + "  " + line);
}

void MainWindow::refreshHighlight() {
    keyboard_->setHighlightSet(pressed_);
}

void MainWindow::onConnectClicked() {
    if (connected_) {
        // 断开：工作线程会先停止同步再断开设备（串行执行）。
        worker_->requestDisconnect();
        return;
    }
    connectBtn_->setEnabled(false);
    setStatus(QStringLiteral("正在连接…"));
    worker_->requestConnect();
}

void MainWindow::onConnected(bool ok, const QString& fwText, bool supportsInject,
                             const QString& message) {
    connectBtn_->setEnabled(true);
    fwLabel_->setText(fwText);
    if (!ok) {
        connected_ = false;
        firmwareSupportsInject_ = false;
        QMessageBox::warning(this, QStringLiteral("连接失败"), message);
        return;
    }
    connected_ = true;
    firmwareSupportsInject_ = supportsInject;
    connectBtn_->setText(QStringLiteral("断开连接"));
    setConnDot(true);

    if (!supportsInject) {
        QMessageBox::warning(this, QStringLiteral("固件不支持"),
            QStringLiteral("当前固件版本不支持按键状态同步功能。\n"
                           "请编译并刷入支持该功能的固件（版本 v1.1.0 及以上）。"));
        return;
    }
    // 连接成功后自动开启键盘状态同步（按 END 键可停止）。
    worker_->requestEnableCapture();
}

void MainWindow::onAllowedUsages(const std::set<uint32_t>& usages) {
    // 把固件已配置的可监控按键传给键盘控件，未配置的键将变暗。
    keyboard_->setAllowedSet(usages);
    if (!usages.empty()) {
        log(QStringLiteral("仅监控固件已配置的 %1 个按键，其余按键已变暗不可触发").arg(usages.size()));
    }
}

void MainWindow::onDisconnected() {
    connected_ = false;
    firmwareSupportsInject_ = false;
    connectBtn_->setText(QStringLiteral("连接设备"));
    connectBtn_->setEnabled(true);
    fwLabel_->setText(QStringLiteral("固件: -"));
    setConnDot(false);
    keyboard_->setAllowedSet(std::set<uint32_t>{});  // 断开后恢复全部按键正常显示
    pressed_.clear();
    refreshHighlight();
    setStatus(QStringLiteral("已断开"), true);  // 红色提示
}

void MainWindow::onCaptureStateChanged(bool on) {
    // 用文字颜色反映同步状态：同步中绿色，已停止灰色。
    injectLabel_->setStyleSheet(
        on ? QStringLiteral("color:#4ade80; font-weight:600;")
           : QStringLiteral("color:#8b9498;"));
    if (!on) {
        pressed_.clear();
        refreshHighlight();
    }
}

void MainWindow::onKeyStateChanged(uint32_t usage, bool pressed) {
    if (pressed) {
        pressed_.insert(usage);
    } else {
        pressed_.erase(usage);
    }
    refreshHighlight();
}
