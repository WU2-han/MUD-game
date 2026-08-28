#ifndef MUD_HPP
#define MUD_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <iostream>

// ===== 全局常量 =====
constexpr int MAX_NAME_LEN   = 64;
constexpr int MAX_DESC_LEN   = 512;
constexpr int MAX_INPUT_LEN  = 256;
constexpr int MAX_CMD_LEN    = 32;
constexpr int MAX_ARG_LEN    = 256;
constexpr int MAX_INV_SLOTS  = 50;
constexpr int MAX_SKILL_SLOTS = 20;
constexpr int MAX_ROOM_EXITS = 6;
constexpr int MAX_NPC_PER_ROOM = 10;
constexpr int MAX_ITEM_PER_ROOM = 20;
constexpr int MAX_CMD_COUNT  = 128;
constexpr int MAX_MODULE_COUNT = 16;
constexpr int MAX_EVENT_COUNT = 64;
constexpr int MAX_SAVE_PATH  = 256;

// ===== 灵根类型 =====
enum class SpiritRoot {
    NONE = 0,
    GOLD,       // 金
    WOOD,       // 木
    WATER,      // 水
    FIRE,       // 火
    EARTH,      // 土
    WIND,       // 风 (变异)
    THUNDER,    // 雷 (变异)
    ICE,        // 冰 (变异)
    MAX
};

// ===== 境界等级 =====
enum class RealmLevel {
    MORTAL = 0,          // 凡人
    QI_REFINE,           // 炼气期
    FOUNDATION,          // 筑基期
    GOLDEN_CORE,         // 金丹期
    NASCENT_SOUL,        // 元婴期
    SPIRIT_TRANS,        // 化神期
    TRIBULATION,         // 渡劫期
    MAHAYANA,            // 大乘期
    MAX
};

// ===== 境界阶段 =====
enum class RealmStage {
    EARLY = 0,           // 初期
    MIDDLE,              // 中期
    LATE,                // 后期
    PEAK                 // 圆满
};

// ===== 道具类型 =====
enum class ItemType {
    MISC = 0,            // 杂项
    PILL,                // 丹药
    WEAPON,              // 武器
    ARMOR,               // 防具
    MANUAL,              // 功法秘籍
    MATERIAL,            // 材料
    QUEST,               // 任务道具
    MAX
};

// ===== NPC 类型 =====
enum class NPCType {
    MONSTER = 0,         // 怪物
    MERCHANT,            // 商人
    QUEST_GIVER,         // 任务NPC
    ELDER,               // 宗门长老
    MAX
};

// ===== 方向 =====
enum class Direction {
    NORTH = 0,
    SOUTH,
    EAST,
    WEST,
    UP,
    DOWN,
    MAX
};

// ===== 事件类型 =====
enum class EventType {
    PLAYER_ENTER_ROOM,
    PLAYER_LEAVE_ROOM,
    PLAYER_KILL_NPC,
    PLAYER_LEVEL_UP,
    PLAYER_USE_ITEM,
    PLAYER_GET_ITEM,
    COMBAT_START,
    COMBAT_END,
    MAX
};

// ===== 道具结构体 =====
struct Item {
    int id = 0;
    std::string name;
    std::string desc;
    ItemType type = ItemType::MISC;
    int value = 0;           // 基础价值（灵石）
    int hp_bonus = 0;
    int mp_bonus = 0;
    int atk_bonus = 0;
    int def_bonus = 0;
    int exp_bonus = 0;       // 修为加成
    int quantity = 1;
    bool stackable = false;
};

// ===== 技能结构体 =====
struct Skill {
    int id = 0;
    std::string name;
    std::string desc;
    int mp_cost = 0;
    int damage = 0;
    int level = 1;
    int max_level = 10;
};

// ===== 房间出口 =====
struct RoomExit {
    int room_id = -1;           // 目标房间ID，-1表示无出口
    bool locked = false;
    RealmLevel req_realm = RealmLevel::MORTAL;
};

// ===== NPC / 怪物结构体 =====
struct NPC {
    int id = 0;
    std::string name;
    std::string desc;
    NPCType type = NPCType::MONSTER;
    RealmLevel realm = RealmLevel::MORTAL;
    int hp = 100;
    int max_hp = 100;
    int mp = 50;
    int max_mp = 50;
    int atk = 10;
    int def = 5;
    int exp_reward = 0;
    int gold_reward = 0;
    int drop_item_id = -1;
    bool is_alive = true;
};

// ===== 房间结构体 =====
struct Room {
    int id = 0;
    std::string name;
    std::string desc;
    RoomExit exits[6];                     // 六个方向出口
    std::vector<int> npc_ids;              // 当前房间内NPC
    std::vector<int> item_ids;             // 地上道具ID列表
    RealmLevel min_realm = RealmLevel::MORTAL;
    bool is_safe_zone = true;
};

// ===== 玩家结构体 =====
struct Player {
    int id = 0;
    std::string name;
    std::string password;
    SpiritRoot spirit_root = SpiritRoot::NONE;
    RealmLevel realm = RealmLevel::MORTAL;
    RealmStage stage = RealmStage::EARLY;
    int exp = 0;
    int exp_to_next = 100;
    int hp = 100;
    int max_hp = 100;
    int mp = 50;
    int max_mp = 50;
    int atk = 10;
    int def = 5;
    int gold = 0;                         // 灵石
    int current_room_id = 1;
    std::vector<Item> inventory;
    std::vector<Skill> skills;
    int sect_id = -1;                     // 宗门ID
    int disciple_count = 0;               // 徒弟数量
    bool in_combat = false;
    int combat_target_id = -1;
};

// ===== 命令定义 =====
struct Command {
    std::string name;
    std::vector<std::string> aliases;     // 别名列表
    std::string help;
    std::function<void(Player*, const std::string&)> handler;
};

// ===== 模块接口 =====
struct Module {
    std::string name;
    std::function<void()> init;
    std::function<void(Player*)> tick;
    std::function<void()> cleanup;
    std::vector<Command> commands;
};

// ===== 事件回调类型 =====
using EventCallback = std::function<void(EventType, Player*, void*)>;

// ===== 事件监听器 =====
struct EventListener {
    EventType type;
    EventCallback callback;
    bool active = true;
};

// ===== 名称映射函数 =====
inline const char* realm_name(RealmLevel realm) {
    static const char* names[] = {
        "凡人", "炼气期", "筑基期", "金丹期",
        "元婴期", "化神期", "渡劫期", "大乘期"
    };
    int idx = static_cast<int>(realm);
    return (idx >= 0 && idx < 8) ? names[idx] : "未知";
}

inline const char* stage_name(RealmStage stage) {
    static const char* names[] = { "初期", "中期", "后期", "圆满" };
    int idx = static_cast<int>(stage);
    return (idx >= 0 && idx <= 3) ? names[idx] : "未知";
}

inline const char* spirit_name(SpiritRoot root) {
    static const char* names[] = {
        "无", "金灵根", "木灵根", "水灵根", "火灵根",
        "土灵根", "风灵根", "雷灵根", "冰灵根"
    };
    int idx = static_cast<int>(root);
    return (idx >= 0 && idx < 9) ? names[idx] : "未知";
}

inline const char* dir_name(Direction dir) {
    static const char* names[] = { "north", "south", "east", "west", "up", "down" };
    int idx = static_cast<int>(dir);
    return (idx >= 0 && idx < 6) ? names[idx] : "unknown";
}

inline const char* dir_cn_name(Direction dir) {
    static const char* names[] = { "北", "南", "东", "西", "上", "下" };
    int idx = static_cast<int>(dir);
    return (idx >= 0 && idx < 6) ? names[idx] : "未知";
}

inline Direction dir_reverse(Direction dir) {
    static const Direction rev[] = {
        Direction::SOUTH, Direction::NORTH, Direction::WEST,
        Direction::EAST,  Direction::DOWN,  Direction::UP
    };
    int idx = static_cast<int>(dir);
    return (idx >= 0 && idx < 6) ? rev[idx] : dir;
}

// ===== 修为计算 =====
inline int realm_exp_required(RealmLevel realm, RealmStage stage) {
    static const int base[] = { 100, 100, 500, 2000, 8000, 30000, 100000, 500000 };
    static const float stage_mult[] = { 1.0f, 1.5f, 2.5f, 4.0f };
    int r = static_cast<int>(realm);
    int s = static_cast<int>(stage);
    if (r < 0 || r >= 8) return 999999;
    return static_cast<int>(base[r] * stage_mult[s]);
}

// ===== 引擎层 API（详见 api.hpp）=====
#include "api.hpp"

#endif // MUD_HPP