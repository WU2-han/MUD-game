#include "mud.hpp"
#include <algorithm>

// ===== 命令系统 =====
static std::vector<Command> g_cmd_table;

static std::string str_tolower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r;
}

static std::string str_trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

void cmd_init() {
    g_cmd_table.clear();
    g_cmd_table.reserve(MAX_CMD_COUNT);
}

void cmd_register(const std::string& name,
                  const std::vector<std::string>& aliases,
                  std::function<void(Player*, const std::string&)> handler,
                  const std::string& help) {
    if (g_cmd_table.size() >= MAX_CMD_COUNT) return;
    g_cmd_table.push_back({str_tolower(name), {}, help, std::move(handler)});
    for (const auto& alias : aliases) {
        if (!alias.empty()) {
            g_cmd_table.back().aliases.push_back(str_tolower(alias));
        }
    }
}

void cmd_register_module(const Module& mod) {
    for (const auto& cmd : mod.commands) {
        cmd_register(cmd.name, cmd.aliases, cmd.handler, cmd.help);
    }
}

Command* cmd_find(const std::string& name) {
    std::string lower = str_tolower(name);
    for (auto& cmd : g_cmd_table) {
        if (cmd.name == lower) return &cmd;
        for (const auto& alias : cmd.aliases) {
            if (alias == lower) return &cmd;
        }
    }
    return nullptr;
}

void cmd_execute(Player* player, const std::string& input) {
    std::string trimmed = str_trim(input);
    if (trimmed.empty()) return;

    // 分离命令名和参数
    size_t space_pos = trimmed.find(' ');
    std::string cmd_name = trimmed.substr(0, space_pos);
    std::string args = (space_pos != std::string::npos)
                       ? str_trim(trimmed.substr(space_pos + 1)) : "";

    Command* cmd = cmd_find(cmd_name);
    if (cmd && cmd->handler) {
        cmd->handler(player, args);
    } else {
        printf("未知指令: %s（输入 help 查看可用命令）\n", cmd_name.c_str());
    }
}

void cmd_show_all(Player* player) {
    (void)player;
    printf("\n========== 可用命令 ==========\n");
    for (const auto& cmd : g_cmd_table) {
        printf("  %-14s", cmd.name.c_str());
        bool has_alias = false;
        for (const auto& alias : cmd.aliases) {
            if (!alias.empty()) {
                if (!has_alias) { printf("("); has_alias = true; }
                else printf(", ");
                printf("%s", alias.c_str());
            }
        }
        if (has_alias) printf(")");
        printf(" - %s\n", cmd.help.c_str());
    }
    printf("==============================\n\n");
}

const std::vector<Command>& cmd_get_all() {
    return g_cmd_table;
}