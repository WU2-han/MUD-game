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
// 对齐《修仙大世界MUD》V2.0 境界体系与宗门职位重构表
enum class RealmLevel {
    MORTAL = 0,          // 凡人（新手）
    QI_REFINE,           // 炼气期    500
    FOUNDATION,          // 筑基期    1,500
    GOLDEN_CORE,         // 金丹期    3,000
    NASCENT_SOUL,        // 元婴期    4,500
    SPIRIT_TRANS,        // 化神期    7,500
    VOID_REFINE,         // 炼虚期    10,500
    MERGE_ALIGN,         // 合体期    16,000
    MAHAYANA,            // 大乘期    25,000
    TRANSCENDENT,        // 渡劫飞升  35,000+
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

// ===== 丹药品阶 =====
enum class PillGrade { NONE=0, LOW, MID, HIGH, TOP };

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

    // ---- V2.0 扩展 ----
    PillGrade grade = PillGrade::NONE;   // 丹药品阶（下品/中品/上品/极品）
    int con_bonus = 0;        // 体质加成
    int spi_bonus = 0;        // 灵力加成
    int wu_bonus = 0;         // 悟性加成
    int spd_bonus = 0;        // 速度加成(%)
    int stam_bonus = 0;       // 精力加成(%)
    int prof_bonus = 0;       // 熟练度加成
    bool is_artifact = false; // 是否为法器
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

// ===== 灵兽品级 =====
enum class BeastGrade { NONE=0, LOW, MID, HIGH, HOLY };

inline int beast_grade_index(BeastGrade g) {
    return static_cast<int>(g); // NONE=0,LOW=1,MID=2,HIGH=3,HOLY=4
}

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

    // 大境界差值锚定的境界级别（用于境界压制）
    int realm_index = 0;
    bool is_alive = true;

    // 动态特性：毒/灼烧/眩晕
    int dot_damage = 0;     // 每回合持续伤害
    int dot_duration = 0;   // 持续回合
    int stun_chance = 0;    // 眩晕概率(%)
    bool boss_rage = false; // BOSS狂暴状态（血量<50%触发）

    // 妖兽刷新
    int home_room = -1;     // 出生房间（用于击杀后刷新归位）
    int dead_day = -1;      // 阵亡当天的游戏日（-1=从没死，用于3天刷新）
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

    // ---- V2.0 核心属性 ----
    int con = 10;             // 体质
    int spi = 10;             // 灵力
    int wu = 0;               // 悟性
    int spd = 10;             // 速度
    int stam = 100;           // 精力
    int max_stam = 100;       // 精力上限
    int prof_alchemy = 0;     // 四艺熟练度·炼丹
    int prof_forge = 0;       // 四艺熟练度·炼器
    int prof_talisman = 0;    // 四艺熟练度·画符
    int prof_beast = 0;       // 四艺熟练度·御兽
    int day = 1;              // 游戏天数

    // ---- V2.0 每日/防御性 ----
    int pill_today = 0;       // 今日已服用养神丹次数（耐药性）
    bool rested_today = false;// 今日是否已免费休息恢复精力
    int monthly_got = 0;      // 本月是否已领取月例

    // ---- 灵兽 ----
    BeastGrade beast_grade = BeastGrade::NONE; // 当前契约灵兽品级
    int beast_id = -1;        // 契约灵兽ID(-1无)
    int beast_atk = 0;
    int beast_hp = 0;
    int beast_skill_id = 0;   // 被动技能加成类型

    // ---- 宗门 ----
    int sect_id = -1;         // 宗门ID
    int sect_rank = 0;        // 宗门地位（0杂役~9太上长老，独立于境界，靠考核晋升）
    int prestige = 0;         // 宗门威望（第八章门宣称达标）

    // ---- 主线剧情 ----
    int story_phase = 0;      // 主线《沧渊遗恨·正邪辨》推进阶段（0未开始）
    std::string title = "";   // 称号（如：和平使者）
    std::string tags = "";    // 剧情标签/线索（逗号分隔）

    std::vector<Item> inventory;
    std::vector<Skill> skills;
    int disciple_count = 0;   // 徒弟数量
    bool in_combat = false;
    int atk_buff = 0;      // 当日符箓攻击加成（御灵符/破障符，跨日清零）
    int def_buff = 0;      // 当日符箓减伤加成
    int combat_target_id = -1;

    // 战斗期间状态
    int dot_remaining = 0;    // 剩余持续伤害回合
    int dot_per_round = 0;    // 每回合持续伤害
    int stunned_rounds = 0;   // 剩余眩晕回合
    bool boss_rage = false;   // Boss狂暴状态
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
        "元婴期", "化神期", "炼虚期", "合体期",
        "大乘期", "渡劫飞升"
    };
    int idx = static_cast<int>(realm);
    return (idx >= 0 && idx < 10) ? names[idx] : "未知";
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

// 方向对应的移动键位（WASD 布局：W↑北  S↓南  A←西  D→东，U上楼，down下楼）
inline const char* dir_key_name(Direction dir) {
    static const char* names[] = { "W", "S", "D", "A", "U", "down" };
    int idx = static_cast<int>(dir);
    return (idx >= 0 && idx < 6) ? names[idx] : "?";
}

// ===== 终端显示宽度工具 =====
// 中文/全角字符占 2 列，ASCII 占 1 列；用于对齐带中文的框线（printf %-Ns 按字节对齐会错位）。

inline int display_width(const std::string& s) {
    int w = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            w += 1;
            i += 1;
        } else {
            int len = 1;
            if ((c & 0xE0) == 0xC0) len = 2;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xF8) == 0xF0) len = 4;
            w += 2;
            i += len;
        }
    }
    return w;
}

// 把字符串补齐到指定显示宽度（不足用空格补）
inline std::string pad_to_width(const std::string& s, int width) {
    int w = display_width(s);
    if (w >= width) return s;
    return s + std::string(static_cast<size_t>(width - w), ' ');
}

// 重复拼接一个 UTF-8 字符串（用于绘制框线，避免多字节字符被 char 截断）
inline std::string box_rep(const char* s, int n) {
    std::string r;
    for (int i = 0; i < n; i++) r += s;
    return r;
}

// 按显示宽度把文本切分成多行，不会从 UTF-8 字符中间截断
inline std::vector<std::string> wrap_text_by_width(const std::string& s, int width) {
    std::vector<std::string> lines;
    std::string cur;
    int cur_w = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        int len = 1;
        int cw = 1;
        if (c >= 0x80) {
            cw = 2;
            if ((c & 0xE0) == 0xC0) len = 2;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xF8) == 0xF0) len = 4;
        }
        std::string ch = s.substr(i, len);
        if (cur_w + cw > width && !cur.empty()) {
            lines.push_back(cur);
            cur.clear();
            cur_w = 0;
        }
        cur += ch;
        cur_w += cw;
        i += len;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
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
// 境界突破修为下限：炼气500/筑基1500/金丹3000/元婴4500/化神7500/炼虚10500/合体16000/大乘25000/渡劫35000
inline int realm_exp_required(RealmLevel realm, RealmStage stage) {
    static const int base[] = { 100, 500, 1500, 3000, 4500, 7500, 10500, 16000, 25000, 35000 };
    // 阶段系数：初期为下限的0.25、中期0.5、后期0.75、圆满1.0（对齐主线门槛）
    static const float stage_mult[] = { 0.25f, 0.5f, 0.75f, 1.0f };
    int r = static_cast<int>(realm);
    int s = static_cast<int>(stage);
    if (r < 0 || r >= 10) return 999999;
    if (r == 0) return 100; // 凡人初始
    return static_cast<int>(base[r] * stage_mult[s]);
}

// ===== 宗门地位 / 月例灵石 / 每日精力上限 =====
inline const char* sect_rank_name(RealmLevel realm) {
    static const char* names[] = {
        "杂役", "外门弟子", "内门弟子", "亲传弟子", "内门执事",
        "核心长老", "峰主", "宗主候选", "宗主", "太上长老"
    };
    int idx = static_cast<int>(realm);
    return (idx >= 0 && idx < 10) ? names[idx] : "未知";
}

// 宗门地位独立档位（0杂役~9太上长老）的名称/月例，供考核晋升后的显示与发放使用
inline const char* sect_rank_name_idx(int rank) {
    static const char* names[] = {
        "杂役", "外门弟子", "内门弟子", "亲传弟子", "内门执事",
        "核心长老", "峰主", "宗主候选", "宗主", "太上长老"
    };
    return (rank >= 0 && rank < 10) ? names[rank] : "未知";
}

inline int sect_rank_salary(int rank) {
    static const int salary[] = { 0, 500, 1000, 1500, 2000, 3000, 4000, 4500, 5000, 10000 };
    return (rank >= 0 && rank < 10) ? salary[rank] : 0;
}

// 四艺熟练度最高值（用于亲传考核「任一四艺≥1000」等门槛）
inline int prof_max(const Player* p) {
    return std::max(std::max(p->prof_alchemy, p->prof_forge),
                    std::max(p->prof_talisman, p->prof_beast));
}

inline int realm_monthly_salary(RealmLevel realm) {
    static const int salary[] = { 0, 500, 1000, 1500, 2000, 3000, 4000, 4500, 5000, 10000 };
    int idx = static_cast<int>(realm);
    return (idx >= 0 && idx < 10) ? salary[idx] : 0;
}

inline int realm_stamina_cap(RealmLevel realm) {
    static const int cap[] = { 100, 100, 120, 140, 160, 180, 200, 220, 250, 300 };
    int idx = static_cast<int>(realm);
    return (idx >= 0 && idx < 10) ? cap[idx] : 0;
}

// ===== 引擎层 API（详见 api.hpp）=====
#include "api.hpp"

#endif // MUD_HPP