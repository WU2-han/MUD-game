#include "mud.hpp"

// ===== 模块系统 =====
static std::vector<Module> g_modules;

// 前向声明
void cmd_register_module(const Module& mod);

bool module_register(const Module& mod) {
    if (g_modules.size() >= MAX_MODULE_COUNT) {
        printf("[模块] 注册失败: 模块数量已达上限\n");
        return false;
    }
    g_modules.push_back(mod);
    printf("[模块] 已注册: %s\n", mod.name.c_str());
    return true;
}

void module_init_all() {
    for (auto& mod : g_modules) {
        if (mod.init) {
            printf("[模块] 初始化: %s\n", mod.name.c_str());
            mod.init();
        }
        // 注册模块命令
        if (!mod.commands.empty()) {
            cmd_register_module(mod);
        }
    }
}

void module_tick_all(Player* player) {
    for (auto& mod : g_modules) {
        if (mod.tick) {
            mod.tick(player);
        }
    }
}

void module_cleanup_all() {
    for (auto& mod : g_modules) {
        if (mod.cleanup) {
            mod.cleanup();
        }
    }
    g_modules.clear();
}

int module_count() { return static_cast<int>(g_modules.size()); }

const std::vector<Module>& module_get_all() { return g_modules; }