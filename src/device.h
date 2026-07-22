// ============================================================
// [注意] 通信协议已更换，本文件内容仅供参考，不可用于实际连接设备。
// device.h —— 基于 hidapi 的设备通信层。
// 封装：枚举/打开 XX 配置接口、发送命令帧、读取响应帧（含重试）、
// 以及读取全部映射、保存映射并持久化的高层流程。
// ============================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "protocol.h"

struct hid_device_;
typedef struct hid_device_ hid_device;

// 一条按键映射（对应网页端 mapping 对象 / 固件字节布局）。
struct Mapping {
    uint32_t target_usage = 0;
    uint32_t source_usage = 0;
    int32_t scaling = proto::DEFAULT_SCALING;
    uint8_t layer_mask = 0x01; // 默认 layer 0
    uint8_t flags = 0;         // STICKY/TAP/HOLD
    uint8_t source_port = 0;
    uint8_t target_port = 0;
};

// 设备当前配置的标量字段（GET_CONFIG 返回），保存时需原样回写以免破坏其它设置。
struct DeviceConfig {
    uint8_t config_version = 0;
    uint8_t flags = 0;
    uint8_t unmapped_passthrough_layer_mask = 0;
    uint32_t partial_scroll_timeout = 1000000;
    uint16_t mapping_count = 0;
    uint32_t our_usage_count = 0;
    uint32_t their_usage_count = 0;
    uint8_t interval_override = 0;
    uint32_t tap_hold_threshold = 200000;
    uint8_t gpio_debounce_time_ms = 5;
    uint8_t our_descriptor_number = 0;
    uint8_t macro_entry_duration = 1; // 已 +1 还原（设备存的是 -1）
    uint16_t quirk_count = 0;
    uint16_t builtin_mouse_macro_return_duration = 100;
};

class Device {
public:
    Device();
    ~Device();

    // 打开第一个匹配的 XX 配置接口。成功返回 true。
    bool open();
    void close();
    bool isOpen() const { return handle_ != nullptr; }

    const std::string& lastError() const { return last_error_; }

    // 读取标量配置（同时校验版本），失败返回 false。
    bool getConfig(DeviceConfig& out);

    // 读取设备上全部映射。基于 getConfig 得到的 mapping_count。
    bool getAllMappings(std::vector<Mapping>& out);

    // 读取所有 slot 的触发键 usage（GET_SLOT_TRIGGER）。
    // 收集每个 slot 的 usage1/usage2（触发键 + 组合键），是用户真正配置、
    // 需要上位机监控的按键集合。去重后返回。
    bool getSlotTriggerUsages(std::vector<uint32_t>& out);

    // 完整保存流程：SUSPEND -> SET_CONFIG(原样回写标量) -> CLEAR_MAPPING
    // -> 逐条 ADD_MAPPING -> PERSIST_CONFIG -> RESUME。
    bool saveMappings(const DeviceConfig& cfg, const std::vector<Mapping>& mappings);

    // 注入一个输入 usage 的状态到固件（SET_INPUT_STATE，需 v1.1.0+ 固件）。
    // value: 0=松开，非0=按下。此命令只发不读，开销极小，适合高频调用。
    bool injectInputState(uint32_t usage, int32_t value, uint8_t hub_port = 0);

    // 读取固件版本（GET_FIRMWARE_VERSION）。失败或老固件返回 false。
    bool getFirmwareVersion(uint8_t& major, uint8_t& minor, uint8_t& patch);

private:
    // 发送 32 字节帧（自动前置 report id）。
    bool sendCommand(uint8_t command, const std::vector<proto::Field>& fields = {},
                     uint8_t version = proto::CONFIG_VERSION);
    // 读取一帧配置响应（去掉 report id 的 32 字节），带重试与 CRC 校验。
    bool readConfig(std::vector<uint8_t>& out, int attempts = 30, int max_delay_ms = 150);

    hid_device* handle_ = nullptr;
    std::string last_error_;
};
