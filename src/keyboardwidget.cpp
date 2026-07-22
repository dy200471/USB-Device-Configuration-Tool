#include "keyboardwidget.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>

KeyboardWidget::KeyboardWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(560, static_cast<int>(560.0 * kbd::totalHeight() / kbd::totalWidth()));
    logo_ = new QSvgRenderer(QStringLiteral(":/logo.svg"), this);
}

QSize KeyboardWidget::sizeHint() const {
    return QSize(static_cast<int>(kbd::totalWidth()), static_cast<int>(kbd::totalHeight()));
}

int KeyboardWidget::heightForWidth(int w) const {
    return static_cast<int>(w * kbd::totalHeight() / kbd::totalWidth());
}

void KeyboardWidget::setHighlight(uint32_t usage) {
    if (highlight_ != usage) {
        highlight_ = usage;
        update();
    }
}

void KeyboardWidget::setHighlightSet(const std::set<uint32_t>& usages) {
    highlightSet_ = usages;
    update();
}

void KeyboardWidget::setAllowedSet(const std::set<uint32_t>& usages) {
    allowedSet_ = usages;
    update();
}

void KeyboardWidget::computeTransform(double& scale, double& offX, double& offY) const {
    const double tw = kbd::totalWidth();
    const double th = kbd::totalHeight();
    const double sw = width();
    const double sh = height();
    scale = std::min(sw / tw, sh / th);
    offX = (sw - tw * scale) / 2.0;
    offY = (sh - th * scale) / 2.0;
}

uint32_t KeyboardWidget::hitTest(const QPointF& pos) const {
    double scale, offX, offY;
    computeTransform(scale, offX, offY);
    // 反变换到布局坐标。
    double lx = (pos.x() - offX) / scale;
    double ly = (pos.y() - offY) / scale;
    for (const auto& k : kbd::keys()) {
        if (lx >= k.x && lx <= k.x + k.w && ly >= k.y && ly <= k.y + k.h) {
            return k.usage;
        }
    }
    return 0;
}

void KeyboardWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    double scale, offX, offY;
    computeTransform(scale, offX, offY);
    p.translate(offX, offY);
    p.scale(scale, scale);

    const double tw = kbd::totalWidth();
    const double th = kbd::totalHeight();

    // --- 外壳（对应 .kbd-case-shape：深色渐变 + 描边） ---
    QRectF caseRect(0, 0, tw, th);
    QLinearGradient bodyGrad(0, 0, 0, th);
    bodyGrad.setColorAt(0.0, QColor("#3a3f42"));
    bodyGrad.setColorAt(1.0, QColor("#25292b"));
    QPainterPath casePath;
    casePath.addRoundedRect(caseRect, 14, 14);
    p.fillPath(casePath, bodyGrad);
    p.setPen(QPen(QColor("#4a5054"), 1.5));
    p.drawPath(casePath);

    // --- 顶部高光条（对应 .kbd-case-highlight） ---
    QRectF hlRect(0, 0, tw, th * 0.4);
    QLinearGradient glossGrad(0, 0, 0, th * 0.4);
    glossGrad.setColorAt(0.0, QColor(255, 255, 255, 28));
    glossGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
    QPainterPath hlPath;
    hlPath.addRoundedRect(hlRect, 14, 14);
    p.fillPath(hlPath, glossGrad);

    // --- 右上角 Logo（随键盘等比缩放，绘制于外壳内右上角） ---
    if (logo_ && logo_->isValid()) {
        const double logoH = 40.0;                 // 布局坐标下的 Logo 高度
        QSizeF ds = logo_->defaultSize();
        double aspect = (ds.height() > 0) ? (ds.width() / ds.height()) : 1.0;
        double logoW = logoH * aspect;
        const double margin = 12.0;
        QRectF logoRect(tw - margin - logoW, margin, logoW, logoH);
        const double prevOpacity = p.opacity();
        p.setOpacity(0.35);            // 半透明，弱化为水印效果
        logo_->render(&p, logoRect);
        p.setOpacity(prevOpacity);
    }

    // --- 键帽 ---
    QFont font = p.font();
    font.setPixelSize(14);
    p.setFont(font);

    for (const auto& k : kbd::keys()) {
        QRectF r(k.x, k.y, k.w, k.h);
        QPainterPath keyPath;
        keyPath.addRoundedRect(r, 5, 5);

        bool captured = (highlight_ != 0 && k.usage == highlight_) ||
                        (highlightSet_.count(k.usage) > 0);
        bool hovered = (hover_ != 0 && k.usage == hover_);
        // 白名单非空且该键不在其中 -> 未配置键，变暗显示。
        bool disabledKey = !allowedSet_.empty() && allowedSet_.count(k.usage) == 0;

        if (disabledKey) {
            // 未配置键：深灰键帽 + 暗淡文字，不响应 hover/captured 视觉。
            QLinearGradient keyGrad(r.topLeft(), r.bottomLeft());
            keyGrad.setColorAt(0.0, QColor("#242729"));
            keyGrad.setColorAt(1.0, QColor("#1c1f21"));
            p.fillPath(keyPath, keyGrad);
            p.setPen(QPen(QColor("#303538"), 1));
            p.drawPath(keyPath);
            p.setPen(QColor("#565c60"));
            p.drawText(r, Qt::AlignCenter, k.label);
            continue;
        }

        if (captured) {
            // .kbd-key.captured：黄色渐变填充 + 深色文字。
            QLinearGradient capGrad(r.topLeft(), r.bottomLeft());
            capGrad.setColorAt(0.0, QColor("#ffd45c"));
            capGrad.setColorAt(1.0, QColor("#ffd45c"));
            p.fillPath(keyPath, capGrad);
            p.setPen(QPen(QColor("#e0a800"), 1));
            p.drawPath(keyPath);
            p.setPen(QColor("#181a1b"));
        } else {
            // 键帽渐变（上浅下深），营造立体感。
            QColor top = hovered ? QColor("#525a5f") : QColor("#3a4044");
            QColor bot = hovered ? QColor("#41484c") : QColor("#2c3134");
            QLinearGradient keyGrad(r.topLeft(), r.bottomLeft());
            keyGrad.setColorAt(0.0, top);
            keyGrad.setColorAt(1.0, bot);
            p.fillPath(keyPath, keyGrad);
            // 可用的按键（在白名单中、当前未按下）用绿色描边突出。
            bool available = !allowedSet_.empty() && allowedSet_.count(k.usage) > 0;
            if (available) {
                p.setPen(QPen(QColor("#4ade80"), 2));
            } else {
                p.setPen(QPen(hovered ? QColor("#5a6469") : QColor("#454c50"), 1));
            }
            p.drawPath(keyPath);
            p.setPen(hovered ? QColor("#eef2f3") : QColor("#c7ccce")); // .kbd-key-label
        }
        p.drawText(r, Qt::AlignCenter, k.label);
    }
}

void KeyboardWidget::mousePressEvent(QMouseEvent* e) {
    uint32_t u = hitTest(QPointF(e->pos()));
    if (u != 0) {
        emit keyClicked(u);
    }
}

void KeyboardWidget::mouseMoveEvent(QMouseEvent* e) {
    uint32_t u = hitTest(QPointF(e->pos()));
    if (u != hover_) {
        hover_ = u;
        emit keyHovered(u);
        update();
    }
}

void KeyboardWidget::leaveEvent(QEvent*) {
    if (hover_ != 0) {
        hover_ = 0;
        emit keyHovered(0);
        update();
    }
}
