// ============================================================
// [注意] 通信协议已更换，本文件内容仅供参考，不再对应实际固件行为。
// protocol.h —— 与 XX 固件通信的协议常量与帧构建工具。
// 与 config-tool-web/js/constants.js、protocol.js、crc.js 保持严格一致。
// 所有多字节整数一律小端；配置帧固定 32 字节，末 4 字节为 CRC32。
// ============================================================
#pragma once

#include <cstdint>
#include <vector>

namespace proto {

// --- USB 识别 ---
// [协议已更换] 以下 VID/PID/usage 已失效，故意设为不存在的值，
// 使上位机无法枚举到任何设备，程序仅供代码参考，不可用于实际连接。
constexpr uint16_t VENDOR_ID = 0x0000;
constexpr uint16_t PRODUCT_ID = 0x0000;
// 配置接口 collection 的 usage page / usage（已失效，仅供参考）。
constexpr uint16_t CONFIG_USAGE_PAGE = 0x0000;
constexpr uint16_t CONFIG_USAGE = 0x0000;

// --- Feature Report ---
constexpr uint8_t REPORT_ID_CONFIG = 100;
constexpr uint8_t REPORT_ID_MONITOR = 101;
constexpr int CONFIG_SIZE = 32; // 不含 report id 的载荷长度

// [协议已更换] 版本号故意改为不可能匹配的值，即使误连到设备也会在
// getConfig() 的版本校验处失败退出，不会误操作真实固件。
constexpr uint8_t CONFIG_VERSION = 0xFF;

// --- mapping flags ---
constexpr uint8_t STICKY_FLAG = 1 << 0;
constexpr uint8_t TAP_FLAG = 1 << 1;
constexpr uint8_t HOLD_FLAG = 1 << 2;

// --- config flags ---
constexpr uint8_t IGNORE_AUTH_DEV_INPUTS_FLAG = 1 << 4;
constexpr uint8_t GPIO_OUTPUT_MODE_FLAG = 1 << 5;
constexpr uint8_t NORMALIZE_GAMEPAD_INPUTS_FLAG = 1 << 6;

constexpr int NLAYERS = 8;
constexpr int32_t DEFAULT_SCALING = 1000;

// 自定义 slot 数量（与网页 constants.js CUSTOM_MOUSE_MACRO_SLOT_COUNT 一致）。
constexpr int CUSTOM_MOUSE_MACRO_SLOT_COUNT = 15;
// slot 触发类型。
constexpr uint8_t SLOT_TRIGGER_NONE = 0;

// --- usage pages ---
constexpr uint32_t KEYBOARD_USAGE_PAGE = 0x00070000;
constexpr uint32_t LAYERS_USAGE_PAGE = 0xFFF10000;
constexpr uint32_t EXPR_USAGE_PAGE = 0xFFF30000;
constexpr uint32_t BUTTON_USAGE_PAGE = 0x00090000;

// --- 命令码 (ConfigCommand) ---
enum Command : uint8_t {
    RESET_INTO_BOOTSEL = 1,
    SET_CONFIG = 2,
    GET_CONFIG = 3,
    CLEAR_MAPPING = 4,
    ADD_MAPPING = 5,
    GET_MAPPING = 6,
    PERSIST_CONFIG = 7,
    GET_OUR_USAGES = 8,
    GET_THEIR_USAGES = 9,
    SUSPEND = 10,
    RESUME = 11,
    GET_BUILTIN_MOUSE_MACRO_RETURN_DURATION = 26,
    SET_MONITOR_ENABLED = 22,
    GET_SLOT_TRIGGER = 47,
    GET_FIRMWARE_VERSION = 55,
    // 注入输入状态：把某个 usage 的按下/松开状态喂给固件（需 v1.1.0+ 固件支持）。
    // 参数（小端）：U32 usage, I32 value, U8 hub_port。
    SET_INPUT_STATE = 69,
};

// --- 字段类型标记，用于打包/解包 ---
enum class FType { U8, I8, U16, U32, I32 };

struct Field {
    FType type;
    int64_t value; // 打包时用；解包时忽略
};

inline Field U8(int64_t v) { return {FType::U8, v}; }
inline Field U16(int64_t v) { return {FType::U16, v}; }
inline Field U32(int64_t v) { return {FType::U32, v}; }
inline Field I32(int64_t v) { return {FType::I32, v}; }

// CRC32（与固件 crc.cc / 网页 crc.js 同表、首尾异或 0xFFFFFFFF）。
uint32_t crc32(const uint8_t* buf, int length);

// 构建 32 字节配置帧：byte0=version, byte1=command, 之后按 fields 顺序
// 小端打包，末 4 字节写 CRC32。返回长度恰为 CONFIG_SIZE。
std::vector<uint8_t> build_frame(uint8_t command,
                                 const std::vector<Field>& fields,
                                 uint8_t version = CONFIG_VERSION);

// 校验收到的 32 字节载荷末尾 CRC32 是否正确。
bool check_crc(const uint8_t* data, int size = CONFIG_SIZE);

} // namespace proto
