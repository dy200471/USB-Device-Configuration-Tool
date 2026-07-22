// ============================================================
// keyboardwidget.h —— 自绘键盘控件，视觉复刻网页“捕获触发键”的键盘 SVG：
// 圆角外壳 + 顶部高光条 + 键帽（圆角矩形 + 居中标签），
// 选中键用黄色 #ffd45c 高亮（对应网页 .kbd-key.captured）。
// 点击某个键会 emit keyClicked(usage)。可通过 setHighlight 高亮指定键。
// ============================================================
#pragma once

#include <cstdint>
#include <set>

#include <QWidget>

#include "keyboardlayout.h"

class QSvgRenderer;

class KeyboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit KeyboardWidget(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    int heightForWidth(int w) const override;
    bool hasHeightForWidth() const override { return true; }

    // 高亮某个 usage（0 表示不高亮）。
    void setHighlight(uint32_t usage);
    // 高亮一组同时按下的 usage（注入模式用）。
    void setHighlightSet(const std::set<uint32_t>& usages);
    // 设置固件已配置、可监控的按键白名单。设置后不在白名单的键会变暗。
    // 传入空集合表示“不启用白名单过滤”（全部正常显示）。
    void setAllowedSet(const std::set<uint32_t>& usages);

signals:
    void keyClicked(uint32_t usage);
    void keyHovered(uint32_t usage); // 0 表示离开

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    // 计算 布局坐标 -> 控件像素 的缩放与偏移（保持等比、居中）。
    void computeTransform(double& scale, double& offX, double& offY) const;
    uint32_t hitTest(const QPointF& pos) const;

    uint32_t highlight_ = 0;
    uint32_t hover_ = 0;
    std::set<uint32_t> highlightSet_;
    std::set<uint32_t> allowedSet_;  // 固件已配置的可监控键；空表示不过滤
    QSvgRenderer* logo_ = nullptr;  // 键盘外壳右上角 Logo
};
