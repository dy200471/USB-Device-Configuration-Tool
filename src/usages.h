// ============================================================
// usages.h —— usage 数值 <-> 可读名称。用于映射列表展示。
// 键盘键复用 keyboardlayout 的标签；其余给出常见 usage 的中文/英文名。
// ============================================================
#pragma once

#include <cstdint>
#include <QString>

namespace usages {

// 返回 usage 的可读名称（键盘键用键帽标签，其它给通用描述）。
QString name(uint32_t usage);

} // namespace usages
