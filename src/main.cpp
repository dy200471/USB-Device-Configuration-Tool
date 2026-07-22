#include <QApplication>
#include <QFont>
#include <QIcon>

#include "mainwindow.h"

// 现代深色主题样式表（与键盘控件的深色外壳配色协调）。
static const char* kAppStyle = R"QSS(
QMainWindow, QWidget#central {
    background: #181a1b;
}
QLabel {
    color: #d4d9db;
    font-size: 13px;
}
QLabel#hintLabel {
    color: #8b9498;
    background: #222426;
    border: 1px solid #313739;
    border-radius: 8px;
    padding: 10px 12px;
    font-size: 12px;
}
QLabel#statusLabel {
    color: #9aa3a7;
    background: #222426;
    border: 1px solid #313739;
    border-radius: 6px;
    padding: 6px 12px;
    font-size: 12px;
}
QPushButton {
    background: #ffd45c;
    color: #181a1b;
    border: none;
    border-radius: 7px;
    padding: 8px 20px;
    font-size: 13px;
    font-weight: 600;
}
QPushButton:hover {
    background: #ffd45c;
}
QPushButton:pressed {
    background: #ffd45c;
}
QPushButton:disabled {
    background: #3a3f42;
    color: #6b7276;
}
QCheckBox {
    color: #d4d9db;
    font-size: 13px;
    spacing: 8px;
    padding: 4px;
}
QCheckBox::indicator {
    width: 18px;
    height: 18px;
    border-radius: 5px;
    border: 1px solid #4a5054;
    background: #2a2f32;
}
QCheckBox::indicator:hover {
    border-color: #2f7cf6;
}
QCheckBox::indicator:checked {
    background: #2f7cf6;
    border-color: #2f7cf6;
    image: none;
}
QCheckBox:disabled {
    color: #6b7276;
}
QScrollArea {
    background: #181a1b;
    border: 1px solid #313739;
    border-radius: 10px;
}
QScrollArea > QWidget > QWidget {
    background: transparent;
}
QPlainTextEdit {
    background: #111213;
    color: #b8c0c3;
    border: 1px solid #313739;
    border-radius: 8px;
    padding: 8px;
    font-family: "Consolas", "Cascadia Mono", monospace;
    font-size: 12px;
    selection-background-color: #2f7cf6;
}
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: #3a4145;
    border-radius: 5px;
    min-height: 24px;
}
QScrollBar::handle:vertical:hover {
    background: #4a5459;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 2px;
}
QScrollBar::handle:horizontal {
    background: #3a4145;
    border-radius: 5px;
    min-width: 24px;
}
QScrollBar::handle:horizontal:hover {
    background: #4a5459;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}
QToolTip {
    background: #222426;
    color: #d4d9db;
    border: 1px solid #4a5054;
    border-radius: 4px;
    padding: 4px 8px;
}
)QSS";

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("USB 设备配置工具"));
    // 应用/窗口图标：键盘图标（标题栏左上角 + 任务栏）。
    app.setWindowIcon(QIcon(QStringLiteral(":/keyboard.ico")));

    QFont appFont(QStringLiteral("Microsoft YaHei UI"), 9);
    app.setFont(appFont);
    app.setStyleSheet(QString::fromUtf8(kAppStyle));

    MainWindow w;
    w.show();
    return app.exec();
}
