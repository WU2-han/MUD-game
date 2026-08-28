#include "mud.hpp"

// ===== 事件系统 =====
static std::vector<EventListener> g_listeners;

void event_init() {
    g_listeners.clear();
    g_listeners.reserve(MAX_EVENT_COUNT);
}

void event_listen(EventType type, EventCallback callback) {
    if (g_listeners.size() >= MAX_EVENT_COUNT) return;
    g_listeners.push_back({type, std::move(callback), true});
}

void event_unlisten(EventType type, const EventCallback& callback) {
    // 通过地址比较来取消监听
    for (auto& l : g_listeners) {
        if (l.type == type && l.active) {
            l.active = false;
        }
    }
}

void event_emit(EventType type, Player* player, void* data) {
    for (auto& l : g_listeners) {
        if (l.active && l.type == type && l.callback) {
            l.callback(type, player, data);
        }
    }
}

void event_cleanup() {
    g_listeners.clear();
}