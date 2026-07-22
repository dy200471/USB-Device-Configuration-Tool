#include "inputworker.h"

#include <set>

#include <QStringList>

#include "device.h"
#include "usages.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

// ------------------------------------------------------------
// 扫描码(set1) -> HID 键盘 usage low byte 映射（与旧 keyhook 保持一致）。
// ------------------------------------------------------------
static uint8_t base_table(uint32_t sc) {
    switch (sc) {
        case 0x01: return 0x29; // Esc
        case 0x3B: return 0x3A; case 0x3C: return 0x3B; case 0x3D: return 0x3C; case 0x3E: return 0x3D; // F1-F4
        case 0x3F: return 0x3E; case 0x40: return 0x3F; case 0x41: return 0x40; case 0x42: return 0x41; // F5-F8
        case 0x43: return 0x42; case 0x44: return 0x43; case 0x57: return 0x44; case 0x58: return 0x45; // F9-F12
        case 0x29: return 0x35; // `
        case 0x02: return 0x1E; case 0x03: return 0x1F; case 0x04: return 0x20; case 0x05: return 0x21;
        case 0x06: return 0x22; case 0x07: return 0x23; case 0x08: return 0x24; case 0x09: return 0x25;
        case 0x0A: return 0x26; case 0x0B: return 0x27; // 1-0
        case 0x0C: return 0x2D; case 0x0D: return 0x2E; case 0x0E: return 0x2A; // - = Backspace
        case 0x0F: return 0x2B; // Tab
        case 0x10: return 0x14; case 0x11: return 0x1A; case 0x12: return 0x08; case 0x13: return 0x15;
        case 0x14: return 0x17; case 0x15: return 0x1C; case 0x16: return 0x18; case 0x17: return 0x0C;
        case 0x18: return 0x12; case 0x19: return 0x13; // QWERTYUIOP
        case 0x1A: return 0x2F; case 0x1B: return 0x30; case 0x2B: return 0x31; // [ ] backslash
        case 0x3A: return 0x39; // Caps
        case 0x1E: return 0x04; case 0x1F: return 0x16; case 0x20: return 0x07; case 0x21: return 0x09;
        case 0x22: return 0x0A; case 0x23: return 0x0B; case 0x24: return 0x0D; case 0x25: return 0x0E;
        case 0x26: return 0x0F; // ASDFGHJKL
        case 0x27: return 0x33; case 0x28: return 0x34; case 0x1C: return 0x28; // ; ' Enter
        case 0x2A: return 0xE1; // LShift
        case 0x2C: return 0x1D; case 0x2D: return 0x1B; case 0x2E: return 0x06; case 0x2F: return 0x19;
        case 0x30: return 0x05; case 0x31: return 0x11; case 0x32: return 0x10; // ZXCVBNM
        case 0x33: return 0x36; case 0x34: return 0x37; case 0x35: return 0x38; // , . /
        case 0x36: return 0xE5; // RShift
        case 0x1D: return 0xE0; // LCtrl
        case 0x38: return 0xE2; // LAlt
        case 0x39: return 0x2C; // Space
        case 0x46: return 0x47; // ScrollLock
        // 小键盘（非扩展）
        case 0x45: return 0x53; // NumLock
        case 0x37: return 0x55; // Numpad *
        case 0x4A: return 0x56; // Numpad -
        case 0x4E: return 0x57; // Numpad +
        case 0x4F: return 0x59; case 0x50: return 0x5A; case 0x51: return 0x5B; // Numpad 1-3
        case 0x4B: return 0x5C; case 0x4C: return 0x5D; case 0x4D: return 0x5E; // Numpad 4-6
        case 0x47: return 0x5F; case 0x48: return 0x60; case 0x49: return 0x61; // Numpad 7-9
        case 0x52: return 0x62; case 0x53: return 0x63; // Numpad 0 .
        default: return 0;
    }
}

static uint8_t ext_table(uint32_t sc) {
    switch (sc) {
        case 0x1D: return 0xE4; // RCtrl
        case 0x38: return 0xE6; // RAlt
        case 0x5B: return 0xE3; // LWin
        case 0x5C: return 0xE7; // RWin
        case 0x5D: return 0x65; // Menu
        case 0x52: return 0x49; // Insert
        case 0x47: return 0x4A; // Home
        case 0x49: return 0x4B; // PageUp
        case 0x53: return 0x4C; // Delete
        case 0x4F: return 0x4D; // End
        case 0x51: return 0x4E; // PageDown
        case 0x48: return 0x52; // Up
        case 0x4B: return 0x50; // Left
        case 0x50: return 0x51; // Down
        case 0x4D: return 0x4F; // Right
        case 0x1C: return 0x58; // Numpad Enter
        case 0x35: return 0x54; // Numpad /
        case 0x37: return 0x46; // PrtSc
        default: return 0;
    }
}

uint8_t scancode_to_hid(uint32_t scanCode, bool extended) {
    return extended ? ext_table(scanCode) : base_table(scanCode);
}

#ifdef _WIN32

namespace {
// 工作线程自定义命令消息。
constexpr UINT WM_CMD_CONNECT = WM_APP + 1;
constexpr UINT WM_CMD_DISCONNECT = WM_APP + 2;
constexpr UINT WM_CMD_ENABLE = WM_APP + 3;
constexpr UINT WM_CMD_DISABLE = WM_APP + 4;
constexpr UINT WM_CMD_QUIT = WM_APP + 5;

// 保活定时器：有按键按住时，按此间隔重发按下键状态，刷新固件端保活计时，
// 使固件在上位机异常终止（崩溃/强杀/拔线）后能超时自动释放，避免卡键。
// 必须显著小于固件超时阈值（固件 2000ms）。
constexpr UINT_PTR KEEPALIVE_TIMER_ID = 1;
constexpr UINT KEEPALIVE_INTERVAL_MS = 500;

// 窗口类名前缀（中性、无语义）。真正的类名在运行期追加随机后缀生成，
// 避免固定 magic string 被反作弊特征库收录、被 EnumWindows+GetClassName 定位。
constexpr wchar_t kWndClassPrefix[] = L"AppMsgWindow_";

// 反跳滤波窗口：同一 usage 在该时间内的“反向”状态翻转视为抖动丢弃（毫秒）。
constexpr long long kDebounceMs = 3;
} // namespace

// ------------------------------------------------------------
// 实现体：全部字段仅在工作线程内访问（除线程启动握手用的同步原语）。
// ------------------------------------------------------------
struct InputWorker::Impl {
    InputWorker* owner = nullptr;

    std::thread thread;
    HWND hwnd = nullptr;
    std::wstring wnd_class;    // 运行期随机生成的窗口类名（避免固定 magic string 被特征库收录）
    ATOM wnd_atom = 0;         // RegisterClassExW 返回的 atom，用于精确注销

    // start() 与工作线程之间的就绪握手。
    std::mutex ready_mtx;
    std::condition_variable ready_cv;
    bool ready = false;
    bool ready_ok = false;

    // 以下字段仅工作线程访问。
    Device device;
    bool capturing = false;          // Raw Input 是否已注册
    bool inject_enabled = false;     // 是否允许下发注入
    bool supports_inject = false;

    std::unordered_set<uint32_t> pressed;                 // 当前按下的 usage
    std::unordered_map<uint32_t, long long> last_change;  // usage -> 上次翻转时刻(ms)
    std::unordered_set<uint32_t> allowed;                 // 固件已配置、可监控的键盘 usage 白名单

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void threadMain();
    bool createWindow();
    void destroyWindow();

    bool registerRawInput(bool enable);
    void handleRawInput(LPARAM lParam);
    void onKeyEvent(uint32_t usage, bool pressed);
    void onKeepalive();
    void handleDeviceLost();
    void releaseAll();

    void doConnect();
    void doDisconnect();
    void doEnable();
    void doDisable();
};

LRESULT CALLBACK InputWorker::Impl::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Impl* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_INPUT:
            if (self) {
                self->handleRawInput(lp);
            }
            // 按 Raw Input 约定，WM_INPUT 需要走默认处理以便系统清理。
            return DefWindowProcW(hwnd, msg, wp, lp);
        case WM_CMD_CONNECT:
            if (self) self->doConnect();
            return 0;
        case WM_CMD_DISCONNECT:
            if (self) self->doDisconnect();
            return 0;
        case WM_CMD_ENABLE:
            if (self) self->doEnable();
            return 0;
        case WM_CMD_DISABLE:
            if (self) self->doDisable();
            return 0;
        case WM_TIMER:
            if (self && wp == KEEPALIVE_TIMER_ID) {
                self->onKeepalive();
            }
            return 0;
        case WM_CMD_QUIT:
            if (self) {
                self->doDisable();
                self->doDisconnect();
            }
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

bool InputWorker::Impl::createWindow() {
    // 运行期生成随机类名后缀（8 位十六进制），每次进程启动均不同。
    std::random_device rd;
    unsigned int suffix = (static_cast<unsigned int>(rd()) ^
                           static_cast<unsigned int>(GetCurrentProcessId()) ^
                           static_cast<unsigned int>(GetTickCount()));
    wchar_t buf[16];
    swprintf(buf, 16, L"%08X", suffix);
    wnd_class = std::wstring(kWndClassPrefix) + buf;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &Impl::wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = wnd_class.c_str();
    wnd_atom = RegisterClassExW(&wc);
    if (wnd_atom == 0) {
        return false;
    }

    hwnd = CreateWindowExW(0, wnd_class.c_str(), L"", 0, 0, 0, 0, 0,
                           HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) {
        UnregisterClassW(wnd_class.c_str(), GetModuleHandleW(nullptr));
        wnd_atom = 0;
        return false;
    }
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    return true;
}

void InputWorker::Impl::destroyWindow() {
    if (hwnd) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
    // 注销窗口类，避免类名残留（配合随机类名，重入/多开也不冲突）。
    if (wnd_atom != 0) {
        UnregisterClassW(wnd_class.c_str(), GetModuleHandleW(nullptr));
        wnd_atom = 0;
    }
}

bool InputWorker::Impl::registerRawInput(bool enable) {
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = 0x01; // Generic Desktop
    rid.usUsage = 0x06;     // Keyboard
    if (enable) {
        // RIDEV_INPUTSINK：窗口即使不在前台也能收到输入（游戏中本窗口不聚焦）。
        rid.dwFlags = RIDEV_INPUTSINK;
        rid.hwndTarget = hwnd;
    } else {
        rid.dwFlags = RIDEV_REMOVE;
        rid.hwndTarget = nullptr;
    }
    return RegisterRawInputDevices(&rid, 1, sizeof(rid)) == TRUE;
}

void InputWorker::Impl::handleRawInput(LPARAM lParam) {
    if (!inject_enabled) {
        return;
    }
    UINT size = 0;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr,
                        &size, sizeof(RAWINPUTHEADER)) != 0) {
        return;
    }
    if (size == 0 || size > 1024) {
        return;
    }
    BYTE buf[1024];
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buf,
                        &size, sizeof(RAWINPUTHEADER)) != size) {
        return;
    }
    RAWINPUT* ri = reinterpret_cast<RAWINPUT*>(buf);
    if (ri->header.dwType != RIM_TYPEKEYBOARD) {
        return;
    }
    const RAWKEYBOARD& kb = ri->data.keyboard;
    // 过滤伪键（如 Pause 的前导码）。
    if (kb.VKey == 0xFF || kb.MakeCode == 0) {
        return;
    }
    bool pressed = (kb.Flags & RI_KEY_BREAK) == 0;
    bool extended = (kb.Flags & RI_KEY_E0) != 0;

    uint32_t sc = kb.MakeCode;
    uint8_t code = scancode_to_hid(sc, extended);
    if (code == 0) {
        return;
    }
    uint32_t usage = 0x00070000u | code;

    // END 键作为“关闭键”：按下即停止键盘状态同步，且 END 本身不注入给固件。
    if (usage == 0x0007004Du) {
        if (pressed) {
            emit owner->logMessage(QStringLiteral("检测到 END 键：停止键盘状态同步"));
            doDisable();
        }
        return;
    }

    // 白名单过滤：仅监控固件中已配置的按键。未配置的键直接忽略（不注入、不高亮）。
    // 白名单为空时视为未取得配置，放行全部（保持兼容/兜底）。
    if (!allowed.empty() && allowed.count(usage) == 0) {
        return;
    }

    onKeyEvent(usage, pressed);
}

void InputWorker::Impl::onKeyEvent(uint32_t usage, bool pressed) {
    bool held = this->pressed.count(usage) > 0;

    // 去重：忽略系统自动重复的 keydown 与重复同态事件。
    if (pressed && held) {
        return;
    }
    if (!pressed && !held) {
        return;
    }

    // 反跳滤波：极短时间内的反向翻转丢弃（机械抖动），正常快速输入不受影响。
    long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();
    auto it = last_change.find(usage);
    if (it != last_change.end() && (now - it->second) < kDebounceMs) {
        return;
    }
    last_change[usage] = now;

    if (pressed) {
        this->pressed.insert(usage);
    } else {
        this->pressed.erase(usage);
    }

    bool ok = device.injectInputState(usage, pressed ? 1 : 0);
    emit owner->keyStateChanged(usage, pressed);
    if (!ok) {
        emit owner->logMessage(QStringLiteral("同步失败: %1 %2 (%3)")
                                   .arg(usages::name(usage))
                                   .arg(pressed ? QStringLiteral("按下") : QStringLiteral("松开"))
                                   .arg(QString::fromStdString(device.lastError())));
        handleDeviceLost();
    } else {
        emit owner->logMessage(QStringLiteral("%1  %2")
                                   .arg(pressed ? QStringLiteral("按下") : QStringLiteral("松开"))
                                   .arg(usages::name(usage)));
    }
}

void InputWorker::Impl::handleDeviceLost() {
    // 注入失败通常意味着设备已被物理拔出或接口失效。
    // 零额外流量地复用失败信号：停止捕获、关闭设备并通知 UI 复位，
    // 而非新增周期性在线探测（那会增加 HID 流量特征）。
    KillTimer(hwnd, KEEPALIVE_TIMER_ID);
    inject_enabled = false;
    if (capturing) {
        registerRawInput(false);
        capturing = false;
    }
    // 设备已不可用，直接清本地按下集（无法再下发松开，交由固件超时兜底释放）。
    pressed.clear();
    last_change.clear();
    if (device.isOpen()) {
        device.close();
    }
    supports_inject = false;
    emit owner->statusMessage(QStringLiteral("设备连接已丢失（可能已拔出）"), true);
    emit owner->disconnected();
}

void InputWorker::Impl::releaseAll() {
    // 松开所有仍按下的键，避免固件端残留“按住”状态。
    for (uint32_t u : pressed) {
        device.injectInputState(u, 0);
        emit owner->keyStateChanged(u, false);
    }
    pressed.clear();
    last_change.clear();
}

void InputWorker::Impl::onKeepalive() {
    // 仅当确有按键处于按下状态时才发保活：稳态无按键则完全不发包，
    // 契合“稳态不发包”的低流量特征；而“有键按住”本就是需维持的有效状态。
    if (!inject_enabled || pressed.empty()) {
        return;
    }
    for (uint32_t u : pressed) {
        // 重发按下状态：固件按状态（非边沿）处理注入 usage，重复置 1 幂等，
        // 不会产生额外按键事件，仅用于刷新固件端保活计时。
        if (!device.injectInputState(u, 1)) {
            handleDeviceLost();
            return;
        }
    }
}

void InputWorker::Impl::doConnect() {
    if (device.isOpen()) {
        return;
    }
    if (!device.open()) {
        emit owner->connected(false, QStringLiteral("固件: -"), false,
                              QString::fromStdString(device.lastError()));
        emit owner->statusMessage(QString::fromStdString(device.lastError()), true);
        return;
    }
    uint8_t maj = 0, min = 0, pat = 0;
    supports_inject = false;
    QString fwText;
    if (device.getFirmwareVersion(maj, min, pat)) {
        fwText = QStringLiteral("固件: v%1.%2.%3").arg(maj).arg(min).arg(pat);
        supports_inject = (maj > 1) || (maj == 1 && min >= 1);
    } else {
        fwText = QStringLiteral("固件: 未知(旧版)");
    }

    if (!supports_inject) {
        emit owner->connected(true, fwText, false,
                              QStringLiteral("已连接，但当前固件版本不支持状态同步（需刷入 v1.1.0+）"));
        emit owner->statusMessage(
            QStringLiteral("已连接，但当前固件版本不支持状态同步（需刷入 v1.1.0+）"), true);
        return;
    }
    // 读取用户在网页里配置的鼠标宏“触发键”（slot_triggers）作为可监控白名单。
    // 这才是用户真正配置、需要上位机监控的按键（如 前进/W、左Ctrl、H 等）。
    // 仅取键盘页(0x00070000)的 usage 用于键盘控件灰显与注入过滤。
    allowed.clear();
    std::vector<uint32_t> triggerUsages;
    if (device.getSlotTriggerUsages(triggerUsages)) {
        QStringList names;
        for (uint32_t u : triggerUsages) {
            if ((u & 0xFFFF0000u) == 0x00070000u) {
                if (allowed.insert(u).second) {
                    names << usages::name(u);
                }
            }
        }
        emit owner->logMessage(QStringLiteral("固件已配置触发键 %1 个: %2")
                                   .arg(allowed.size())
                                   .arg(names.join(QStringLiteral(", "))));
    } else {
        emit owner->logMessage(QStringLiteral("读取触发键配置失败，将监控全部按键"));
    }
    // 回传白名单给 UI（用于灰显未配置的键）。
    std::set<uint32_t> allowedSet(allowed.begin(), allowed.end());
    emit owner->allowedUsages(allowedSet);

    emit owner->connected(true, fwText, true,
                          QStringLiteral("已连接，固件支持状态同步。"));
    emit owner->statusMessage(
        QStringLiteral("已连接，固件支持状态同步。"), false);
}

void InputWorker::Impl::doDisconnect() {
    if (inject_enabled) {
        doDisable();
    }
    if (device.isOpen()) {
        device.close();
    }
    supports_inject = false;
    emit owner->disconnected();
}

void InputWorker::Impl::doEnable() {
    if (!device.isOpen() || !supports_inject) {
        emit owner->captureStateChanged(false);
        return;
    }
    if (!capturing) {
        if (!registerRawInput(true)) {
            emit owner->captureStateChanged(false);
            emit owner->statusMessage(QStringLiteral("Raw Input 注册失败，无法捕获键盘"), true);
            return;
        }
        capturing = true;
    }
    inject_enabled = true;
    // 启动保活定时器（固件端超时释放兜底的配套）。
    SetTimer(hwnd, KEEPALIVE_TIMER_ID, KEEPALIVE_INTERVAL_MS, nullptr);
    emit owner->captureStateChanged(true);
    emit owner->logMessage(QStringLiteral("== 已启用键盘状态同步（Raw Input） =="));
    emit owner->statusMessage(QStringLiteral("同步中：按键状态会实时同步到固件"), false);
}

void InputWorker::Impl::doDisable() {
    inject_enabled = false;
    KillTimer(hwnd, KEEPALIVE_TIMER_ID);
    if (capturing) {
        registerRawInput(false); // 彻底注销：不再接收任何键盘原始输入
        capturing = false;
    }
    releaseAll();
    emit owner->captureStateChanged(false);
    emit owner->logMessage(QStringLiteral("== 已停止状态同步（已释放所有按下的按键） =="));
    emit owner->statusMessage(QStringLiteral("已停止状态同步"), false);
}

void InputWorker::Impl::threadMain() {
    bool ok = createWindow();
    {
        std::lock_guard<std::mutex> lk(ready_mtx);
        ready = true;
        ready_ok = ok;
    }
    ready_cv.notify_one();
    if (!ok) {
        return;
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    destroyWindow();
}

#else // 非 Windows：空实现占位

struct InputWorker::Impl {
    InputWorker* owner = nullptr;
};

#endif // _WIN32

// ------------------------------------------------------------
// 对外接口
// ------------------------------------------------------------
InputWorker::InputWorker(QObject* parent) : QObject(parent) {
    d_ = new Impl();
    d_->owner = this;
}

InputWorker::~InputWorker() {
    stop();
    delete d_;
}

bool InputWorker::start() {
#ifdef _WIN32
    if (d_->thread.joinable()) {
        return true;
    }
    d_->ready = false;
    d_->ready_ok = false;
    d_->thread = std::thread([this] { d_->threadMain(); });
    std::unique_lock<std::mutex> lk(d_->ready_mtx);
    d_->ready_cv.wait(lk, [this] { return d_->ready; });
    if (!d_->ready_ok) {
        lk.unlock();
        if (d_->thread.joinable()) {
            d_->thread.join();
        }
        return false;
    }
    return true;
#else
    return false;
#endif
}

void InputWorker::stop() {
#ifdef _WIN32
    if (d_->thread.joinable()) {
        if (d_->hwnd) {
            PostMessageW(d_->hwnd, WM_CMD_QUIT, 0, 0);
        }
        d_->thread.join();
    }
#endif
}

void InputWorker::requestConnect() {
#ifdef _WIN32
    if (d_->hwnd) PostMessageW(d_->hwnd, WM_CMD_CONNECT, 0, 0);
#endif
}

void InputWorker::requestDisconnect() {
#ifdef _WIN32
    if (d_->hwnd) PostMessageW(d_->hwnd, WM_CMD_DISCONNECT, 0, 0);
#endif
}

void InputWorker::requestEnableCapture() {
#ifdef _WIN32
    if (d_->hwnd) PostMessageW(d_->hwnd, WM_CMD_ENABLE, 0, 0);
#endif
}

void InputWorker::requestDisableCapture() {
#ifdef _WIN32
    if (d_->hwnd) PostMessageW(d_->hwnd, WM_CMD_DISABLE, 0, 0);
#endif
}
