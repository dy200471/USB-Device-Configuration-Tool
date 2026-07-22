// [注意] 通信协议已更换，本文件内容仅供参考，不可用于实际连接设备。
// protocol.h 中的 VID/PID/usage/CONFIG_VERSION 已被故意改为无效值，
// open() 将无法枚举到任何设备，即使强行修复也无法通过版本校验。
#include "device.h"

#include <chrono>
#include <cstring>
#include <thread>

#include <hidapi.h>

using proto::CONFIG_SIZE;
using proto::REPORT_ID_CONFIG;

namespace {
uint32_t rd_u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
uint16_t rd_u16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
} // namespace

Device::Device() { hid_init(); }

Device::~Device() {
    close();
    hid_exit();
}

bool Device::open() {
    close();
    last_error_.clear();

    // 设备定位策略：
    //   1) 常规情况：按固定 VID/PID 枚举，命中配置接口即用；
    //   2) 当设备 VID/PID 变化、按固定 VID/PID 枚举不到时，回退为按配置接口
    //      report descriptor 里的专属 usage_page=XX + usage=XX 匹配。
    //      因该 usage page 为本产品独占，按它过滤目标性很强，
    //      不会打开任何无关的系统键鼠设备。
    std::string target_path;

    // --- 第 1 层：固定 VID/PID ---
    hid_device_info* devs = hid_enumerate(proto::VENDOR_ID, proto::PRODUCT_ID);
    for (hid_device_info* cur = devs; cur; cur = cur->next) {
        if (cur->vendor_id != proto::VENDOR_ID || cur->product_id != proto::PRODUCT_ID) {
            continue;
        }
        if (cur->usage_page == proto::CONFIG_USAGE_PAGE && cur->usage == proto::CONFIG_USAGE) {
            target_path = cur->path ? cur->path : "";
            break;
        }
        // 兜底：记住第一个匹配 VID/PID 的接口（部分平台不填 usage 字段）。
        if (target_path.empty() && cur->path) {
            target_path = cur->path;
        }
    }
    if (devs) {
        hid_free_enumeration(devs);
    }

    // --- 第 2 层：回退，按专属 usage page 定位 ---
    if (target_path.empty()) {
        hid_device_info* all = hid_enumerate(0x0, 0x0);
        for (hid_device_info* cur = all; cur; cur = cur->next) {
            // 严格匹配本产品专属配置 collection，绝不打开其它键鼠设备。
            if (cur->usage_page == proto::CONFIG_USAGE_PAGE &&
                cur->usage == proto::CONFIG_USAGE && cur->path) {
                target_path = cur->path;
                break;
            }
        }
        if (all) {
            hid_free_enumeration(all);
        }
    }

    if (target_path.empty()) {
        last_error_ = "未找到设备（请检查是否已连接、驱动是否正常，并确认配置接口未被禁用）";
        return false;
    }

    handle_ = hid_open_path(target_path.c_str());
    if (!handle_) {
        last_error_ = "无法打开设备接口（可能被占用或权限不足）";
        return false;
    }
    return true;
}

void Device::close() {
    if (handle_) {
        hid_close(handle_);
        handle_ = nullptr;
    }
}

bool Device::sendCommand(uint8_t command, const std::vector<proto::Field>& fields,
                         uint8_t version) {
    if (!handle_) {
        last_error_ = "设备未连接";
        return false;
    }
    std::vector<uint8_t> frame = proto::build_frame(command, fields, version);
    // hidapi 约定：feature report 首字节为 report id。
    std::vector<uint8_t> out;
    out.reserve(CONFIG_SIZE + 1);
    out.push_back(REPORT_ID_CONFIG);
    out.insert(out.end(), frame.begin(), frame.end());

    int res = hid_send_feature_report(handle_, out.data(), out.size());
    if (res < 0) {
        last_error_ = "发送 feature report 失败";
        return false;
    }
    return true;
}

bool Device::readConfig(std::vector<uint8_t>& out, int attempts, int max_delay_ms) {
    if (!handle_) {
        last_error_ = "设备未连接";
        return false;
    }
    int delay = 2;
    for (int i = 0; i < attempts; i++) {
        std::vector<uint8_t> buf(CONFIG_SIZE + 1, 0);
        buf[0] = REPORT_ID_CONFIG;
        int res = hid_get_feature_report(handle_, buf.data(), buf.size());
        if (res > 1) {
            // 去掉 report id。
            out.assign(buf.begin() + 1, buf.begin() + 1 + CONFIG_SIZE);
            if (!proto::check_crc(out.data(), CONFIG_SIZE)) {
                last_error_ = "CRC 校验失败";
                return false;
            }
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        delay = std::min(delay * 2, max_delay_ms);
    }
    last_error_ = "读取响应超时（重试耗尽）";
    return false;
}

bool Device::getConfig(DeviceConfig& out) {
    if (!sendCommand(proto::GET_CONFIG)) {
        return false;
    }
    std::vector<uint8_t> d;
    if (!readConfig(d)) {
        return false;
    }
    // 字段顺序与 protocol.js get_usages_from_device 一致（小端）：
    // U8 version, U8 flags, U8 unmapped_mask, U32 partial_scroll_timeout,
    // U16 mapping_count, U32 our_usage_count, U32 their_usage_count,
    // U8 interval_override, U32 tap_hold_threshold, U8 gpio_debounce,
    // U8 our_descriptor_number, U8 macro_entry_duration, U16 quirk_count
    size_t p = 0;
    out.config_version = d[p]; p += 1;
    out.flags = d[p]; p += 1;
    out.unmapped_passthrough_layer_mask = d[p]; p += 1;
    out.partial_scroll_timeout = rd_u32(&d[p]); p += 4;
    out.mapping_count = rd_u16(&d[p]); p += 2;
    out.our_usage_count = rd_u32(&d[p]); p += 4;
    out.their_usage_count = rd_u32(&d[p]); p += 4;
    out.interval_override = d[p]; p += 1;
    out.tap_hold_threshold = rd_u32(&d[p]); p += 4;
    out.gpio_debounce_time_ms = d[p]; p += 1;
    out.our_descriptor_number = d[p]; p += 1;
    out.macro_entry_duration = static_cast<uint8_t>(d[p] + 1); p += 1; // 设备存 -1
    out.quirk_count = rd_u16(&d[p]); p += 2;

    if (out.config_version != proto::CONFIG_VERSION) {
        last_error_ = "固件配置版本不兼容（期望 " +
                      std::to_string(proto::CONFIG_VERSION) + "，实际 " +
                      std::to_string(out.config_version) + "）";
        return false;
    }

    // 额外读取 builtin_mouse_macro_return_duration（保存时需回写）。
    // 老固件可能不支持，失败时保留默认值。
    if (sendCommand(proto::GET_BUILTIN_MOUSE_MACRO_RETURN_DURATION)) {
        std::vector<uint8_t> d2;
        if (readConfig(d2)) {
            uint16_t v = rd_u16(&d2[0]);
            if (v != 0xFFFF) {
                out.builtin_mouse_macro_return_duration = v;
            }
        }
    }
    return true;
}

bool Device::getAllMappings(std::vector<Mapping>& out) {
    DeviceConfig cfg;
    if (!getConfig(cfg)) {
        return false;
    }
    out.clear();
    for (uint32_t i = 0; i < cfg.mapping_count; i++) {
        if (!sendCommand(proto::GET_MAPPING, {proto::U32(i)})) {
            return false;
        }
        std::vector<uint8_t> d;
        if (!readConfig(d)) {
            return false;
        }
        // U32 target, U32 source, I32 scaling, U8 layer_mask, U8 flags, U8 hub_ports
        Mapping m;
        size_t p = 0;
        m.target_usage = rd_u32(&d[p]); p += 4;
        m.source_usage = rd_u32(&d[p]); p += 4;
        m.scaling = static_cast<int32_t>(rd_u32(&d[p])); p += 4;
        m.layer_mask = d[p]; p += 1;
        m.flags = d[p]; p += 1;
        uint8_t hub = d[p]; p += 1;
        m.source_port = hub & 0x0F;
        m.target_port = (hub >> 4) & 0x0F;
        out.push_back(m);
    }
    return true;
}

bool Device::getSlotTriggerUsages(std::vector<uint32_t>& out) {
    out.clear();
    // 遍历所有 slot，读取各自触发键。协议（与 config-io.js 一致）：
    // GET_SLOT_TRIGGER 参数低 8 位 = slot，返回 [usage1(u32)][usage2(u32)][trigger_type(u8)]。
    // 整包 0xFF 或 trigger_type=NONE 视为该 slot 未配置触发。
    for (int slot = 0; slot < proto::CUSTOM_MOUSE_MACRO_SLOT_COUNT; slot++) {
        if (!sendCommand(proto::GET_SLOT_TRIGGER, {proto::U32(static_cast<uint32_t>(slot))})) {
            return false;
        }
        std::vector<uint8_t> d;
        if (!readConfig(d)) {
            return false;
        }
        uint32_t usage1 = rd_u32(&d[0]);
        uint32_t usage2 = rd_u32(&d[4]);
        uint8_t trigger_type = d[8];
        // 老固件不识别 / 未配置。
        if (trigger_type == 0xFF || trigger_type == proto::SLOT_TRIGGER_NONE) {
            continue;
        }
        if (usage1 != 0 && usage1 != 0xFFFFFFFFu) {
            out.push_back(usage1);
        }
        if (usage2 != 0 && usage2 != 0xFFFFFFFFu) {
            out.push_back(usage2);
        }
    }
    return true;
}

bool Device::saveMappings(const DeviceConfig& cfg, const std::vector<Mapping>& mappings) {
    if (!sendCommand(proto::SUSPEND)) {
        return false;
    }

    // 原样回写标量配置（字段顺序与 config-io.js save_to_device 的 SET_CONFIG 一致）。
    if (!sendCommand(proto::SET_CONFIG, {
            proto::U8(cfg.flags),
            proto::U8(cfg.unmapped_passthrough_layer_mask),
            proto::U32(cfg.partial_scroll_timeout),
            proto::U8(cfg.interval_override),
            proto::U32(cfg.tap_hold_threshold),
            proto::U8(cfg.gpio_debounce_time_ms),
            proto::U8(cfg.our_descriptor_number),
            proto::U8(cfg.macro_entry_duration - 1),
            proto::U16(cfg.builtin_mouse_macro_return_duration),
        })) {
        return false;
    }

    if (!sendCommand(proto::CLEAR_MAPPING)) {
        return false;
    }

    for (const auto& m : mappings) {
        uint8_t hub = static_cast<uint8_t>(((m.target_port & 0x0F) << 4) | (m.source_port & 0x0F));
        if (!sendCommand(proto::ADD_MAPPING, {
                proto::U32(m.target_usage),
                proto::U32(m.source_usage),
                proto::I32(m.scaling),
                proto::U8(m.layer_mask),
                proto::U8(m.flags),
                proto::U8(hub),
            })) {
            return false;
        }
    }

    // 持久化到 flash：固件会关中断擦写，耗时较长，加大重试预算。
    if (!sendCommand(proto::PERSIST_CONFIG)) {
        return false;
    }
    std::vector<uint8_t> d;
    if (!readConfig(d, /*attempts=*/60, /*max_delay_ms=*/500)) {
        return false;
    }

    if (!sendCommand(proto::RESUME)) {
        return false;
    }
    return true;
}

bool Device::injectInputState(uint32_t usage, int32_t value, uint8_t hub_port) {
    return sendCommand(proto::SET_INPUT_STATE, {
        proto::U32(usage),
        proto::I32(value),
        proto::U8(hub_port),
    });
}

bool Device::getFirmwareVersion(uint8_t& major, uint8_t& minor, uint8_t& patch) {
    if (!sendCommand(proto::GET_FIRMWARE_VERSION)) {
        return false;
    }
    std::vector<uint8_t> d;
    if (!readConfig(d)) {
        return false;
    }
    // 老固件不支持时返回整包 0xFF。
    if (d[0] == 0xFF && d[1] == 0xFF && d[2] == 0xFF) {
        return false;
    }
    major = d[0];
    minor = d[1];
    patch = d[2];
    return true;
}
