#include "usages.h"

#include "keyboardlayout.h"
#include "protocol.h"

namespace usages {

QString name(uint32_t usage) {
    uint32_t page = usage & 0xFFFF0000u;
    uint32_t id = usage & 0x0000FFFFu;

    if (page == proto::KEYBOARD_USAGE_PAGE) {
        QString lbl = kbd::labelForUsage(usage);
        if (!lbl.isEmpty()) {
            return QStringLiteral("键盘 ") + lbl;
        }
        return QStringLiteral("键盘 0x%1").arg(id, 2, 16, QChar('0'));
    }
    if (page == proto::BUTTON_USAGE_PAGE) {
        switch (id) {
            case 1: return QStringLiteral("鼠标左键");
            case 2: return QStringLiteral("鼠标右键");
            case 3: return QStringLiteral("鼠标中键");
            case 4: return QStringLiteral("鼠标后退");
            case 5: return QStringLiteral("鼠标前进");
            default: return QStringLiteral("按键 %1").arg(id);
        }
    }
    if (page == proto::LAYERS_USAGE_PAGE) {
        return QStringLiteral("层 %1").arg(id);
    }
    if (page == proto::EXPR_USAGE_PAGE) {
        return QStringLiteral("表达式 %1").arg(id);
    }
    if (usage == 0) {
        return QStringLiteral("(无)");
    }
    return QStringLiteral("0x%1").arg(usage, 8, 16, QChar('0'));
}

} // namespace usages
