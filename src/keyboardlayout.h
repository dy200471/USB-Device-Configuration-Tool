// ============================================================
// keyboardlayout.h —— 108 键键盘布局数据，移植自
// config-tool-web/js/keyboard-diagram.js 的 KBD_ROWS 与尺寸常量。
// 供自绘键盘控件按同样的三区块（main/nav/num）对齐算法绘制。
// ============================================================
#pragma once

#include <cstdint>
#include <vector>

#include <QString>

namespace kbd {

// 单位尺寸常量（像素/单位 u），与网页 keyboard-diagram.js 完全一致。
constexpr double UNIT = 42.0;
constexpr double GAP = 4.0;
constexpr double CASE_PAD = 16.0;
constexpr double CASE_PAD_TOP = 44.0;

constexpr double MAIN_WIDTH = 15.0;
constexpr double NAV_WIDTH = 3.0;
constexpr double NUM_WIDTH = 4.0;
constexpr double ZONE_GAP = 0.5;
constexpr double NAV_START = MAIN_WIDTH + ZONE_GAP;             // 15.5
constexpr double NUM_START = NAV_START + NAV_WIDTH + ZONE_GAP;  // 19.0

constexpr uint32_t KB_PAGE = 0x00070000;

// 一个已计算好绝对像素坐标（含 CASE_PAD 偏移）的可点击键。
struct KeyRect {
    uint32_t usage = 0; // 完整 32 位 usage (KB_PAGE | code)
    QString label;
    double x = 0, y = 0, w = 0, h = 0;
};

// 返回全部按键的绝对坐标（用于绘制与命中测试）。
const std::vector<KeyRect>& keys();

// 整块键盘（含外壳）的总尺寸（像素）。
double totalWidth();
double totalHeight();

// 通过完整 usage 找显示标签；找不到返回空串。
QString labelForUsage(uint32_t usage);

} // namespace kbd
