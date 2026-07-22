#include "keyboardlayout.h"

namespace kbd {

namespace {

struct Cell {
    double w;
    double h;      // 0 表示默认 1
    int code;      // -1 表示 gap（占位空隙，不画键）
    const char* label;
};

// 移植自 keyboard-diagram.js 的 KBD_ROWS。每行三个区块 main/nav/num。
// code == -1 表示 gap。方向键标签用 Unicode 箭头。
struct Row {
    std::vector<Cell> main;
    std::vector<Cell> nav;
    std::vector<Cell> num;
};

const std::vector<Row>& rows() {
    static const std::vector<Row> R = {
        // Row 0: Esc / F1-F12 / PrtSc ScrLk Pause
        {
            {
                {1, 0, 0x29, "Esc"},
                {2.0 / 3, 0, -1, nullptr},
                {1, 0, 0x3a, "F1"}, {1, 0, 0x3b, "F2"}, {1, 0, 0x3c, "F3"}, {1, 0, 0x3d, "F4"},
                {2.0 / 3, 0, -1, nullptr},
                {1, 0, 0x3e, "F5"}, {1, 0, 0x3f, "F6"}, {1, 0, 0x40, "F7"}, {1, 0, 0x41, "F8"},
                {2.0 / 3, 0, -1, nullptr},
                {1, 0, 0x42, "F9"}, {1, 0, 0x43, "F10"}, {1, 0, 0x44, "F11"}, {1, 0, 0x45, "F12"},
            },
            {
                {1, 0, 0x46, "PrtSc"}, {1, 0, 0x47, "ScrLk"}, {1, 0, 0x48, "Pause"},
            },
            {},
        },
        // Row 1: ` 1-0 - = Backspace / Ins Home PgUp / Num / * -
        {
            {
                {1, 0, 0x35, "`"},
                {1, 0, 0x1e, "1"}, {1, 0, 0x1f, "2"}, {1, 0, 0x20, "3"}, {1, 0, 0x21, "4"},
                {1, 0, 0x22, "5"}, {1, 0, 0x23, "6"}, {1, 0, 0x24, "7"}, {1, 0, 0x25, "8"},
                {1, 0, 0x26, "9"}, {1, 0, 0x27, "0"}, {1, 0, 0x2d, "-"}, {1, 0, 0x2e, "="},
                {2, 0, 0x2a, "Backspace"},
            },
            {
                {1, 0, 0x49, "Ins"}, {1, 0, 0x4a, "Home"}, {1, 0, 0x4b, "PgUp"},
            },
            {
                {1, 0, 0x53, "Num"}, {1, 0, 0x54, "/"}, {1, 0, 0x55, "*"}, {1, 0, 0x56, "-"},
            },
        },
        // Row 2: Tab QWERTY ... \ / Del End PgDn / 7 8 9 +(h2)
        {
            {
                {1.5, 0, 0x2b, "Tab"},
                {1, 0, 0x14, "Q"}, {1, 0, 0x1a, "W"}, {1, 0, 0x08, "E"}, {1, 0, 0x15, "R"},
                {1, 0, 0x17, "T"}, {1, 0, 0x1c, "Y"}, {1, 0, 0x18, "U"}, {1, 0, 0x0c, "I"},
                {1, 0, 0x12, "O"}, {1, 0, 0x13, "P"}, {1, 0, 0x2f, "["}, {1, 0, 0x30, "]"},
                {1.5, 0, 0x31, "\\"},
            },
            {
                {1, 0, 0x4c, "Del"}, {1, 0, 0x4d, "End"}, {1, 0, 0x4e, "PgDn"},
            },
            {
                {1, 0, 0x5f, "7"}, {1, 0, 0x60, "8"}, {1, 0, 0x61, "9"}, {1, 2, 0x57, "+"},
            },
        },
        // Row 3: Caps ASDF ... Enter / (nav 空) / 4 5 6
        {
            {
                {1.75, 0, 0x39, "Caps"},
                {1, 0, 0x04, "A"}, {1, 0, 0x16, "S"}, {1, 0, 0x07, "D"}, {1, 0, 0x09, "F"},
                {1, 0, 0x0a, "G"}, {1, 0, 0x0b, "H"}, {1, 0, 0x0d, "J"}, {1, 0, 0x0e, "K"},
                {1, 0, 0x0f, "L"}, {1, 0, 0x33, ";"}, {1, 0, 0x34, "'"},
                {2.25, 0, 0x28, "Enter"},
            },
            {},
            {
                {1, 0, 0x5c, "4"}, {1, 0, 0x5d, "5"}, {1, 0, 0x5e, "6"},
            },
        },
        // Row 4: Shift ZXCV ... Shift / (空 ↑ 空) / 1 2 3 Enter(h2)
        {
            {
                {2.25, 0, 0xe1, "Shift"},
                {1, 0, 0x1d, "Z"}, {1, 0, 0x1b, "X"}, {1, 0, 0x06, "C"}, {1, 0, 0x19, "V"},
                {1, 0, 0x05, "B"}, {1, 0, 0x11, "N"}, {1, 0, 0x10, "M"}, {1, 0, 0x36, ","},
                {1, 0, 0x37, "."}, {1, 0, 0x38, "/"},
                {2.75, 0, 0xe5, "Shift"},
            },
            {
                {1, 0, -1, nullptr}, {1, 0, 0x52, "\u2191"}, {1, 0, -1, nullptr},
            },
            {
                {1, 0, 0x59, "1"}, {1, 0, 0x5a, "2"}, {1, 0, 0x5b, "3"}, {1, 2, 0x58, "Enter"},
            },
        },
        // Row 5: Ctrl Win Alt Space Alt Win Menu Ctrl / ← ↓ → / 0(w2) .
        {
            {
                {1.25, 0, 0xe0, "Ctrl"}, {1.25, 0, 0xe3, "Win"}, {1.25, 0, 0xe2, "Alt"},
                {6.25, 0, 0x2c, "Space"},
                {1.25, 0, 0xe6, "Alt"}, {1.25, 0, 0xe7, "Win"}, {1.25, 0, 0x65, "Menu"},
                {1.25, 0, 0xe4, "Ctrl"},
            },
            {
                {1, 0, 0x50, "\u2190"}, {1, 0, 0x51, "\u2193"}, {1, 0, 0x4f, "\u2192"},
            },
            {
                {2, 0, 0x62, "0"}, {1, 0, 0x63, "."},
            },
        },
    };
    return R;
}

std::vector<KeyRect> compute() {
    std::vector<KeyRect> out;
    const auto& R = rows();
    auto renderZone = [&](const std::vector<Cell>& cells, double startX, double y) {
        double x = startX;
        for (const auto& c : cells) {
            double w = c.w * UNIT - GAP;
            double h = (c.h != 0 ? c.h : 1) * UNIT - GAP;
            if (c.code >= 0) {
                KeyRect k;
                k.usage = (KB_PAGE | static_cast<uint32_t>(c.code));
                k.label = QString::fromUtf8(c.label);
                k.x = x;
                k.y = y;
                k.w = w;
                k.h = h;
                out.push_back(k);
            }
            x += c.w * UNIT;
        }
    };
    for (size_t i = 0; i < R.size(); i++) {
        double y = CASE_PAD_TOP + static_cast<double>(i) * UNIT;
        renderZone(R[i].main, CASE_PAD, y);
        renderZone(R[i].nav, CASE_PAD + NAV_START * UNIT, y);
        renderZone(R[i].num, CASE_PAD + NUM_START * UNIT, y);
    }
    return out;
}

} // namespace

const std::vector<KeyRect>& keys() {
    static const std::vector<KeyRect> K = compute();
    return K;
}

double totalWidth() {
    double keysWidth = (NUM_START + NUM_WIDTH) * UNIT - GAP;
    return keysWidth + CASE_PAD * 2;
}

double totalHeight() {
    double keysHeight = static_cast<double>(rows().size()) * UNIT - GAP;
    return keysHeight + CASE_PAD_TOP + CASE_PAD;
}

QString labelForUsage(uint32_t usage) {
    for (const auto& k : keys()) {
        if (k.usage == usage) {
            return k.label;
        }
    }
    return QString();
}

} // namespace kbd
