#ifndef MUD_H
#define MUD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ===== 全局常量 ===== */
#define MAX_NAME_LEN      64
#define MAX_DESC_LEN      512
#define MAX_INPUT_LEN     256
#define MAX_CMD_LEN       32
#define MAX_ARG_LEN       256
#define MAX_INV_SLOTS     50
#define MAX_SKILL_SLOTS   20
#define MAX_ROOM_EXITS    6
#define MAX_NPC_PER_ROOM  10
#define MAX_ITEM_PER_ROOM 20
#define MAX_CMD_COUNT     128
#define MAX_MODULE_COUNT  16
#define MAX_EVENT_COUNT   64
#define MAX_SAVE_PATH     256

/* ===== 灵根类型 ===== */
typedef enum {
    SPIRIT_NONE = 0,
    SPIRIT_GOLD,      /* 金 */
    SPIRIT_WOOD,      /* 木 */
    SPIRIT_WATER,     /* 水 */
    SPIRIT_FIRE,      /* 火 */
    SPIRIT_EARTH,     /* 土 */
    SPIRIT_WIND,      /* 风 (变异) */
    SPIRIT_THUNDER,   /* 雷 (变异) */
    SPIRIT_ICE,       /* 冰 (变异) */
    SPIRIT_MAX
} SpiritRoot;

/* ===== 境界等级 ===== */
typedef enum {
    REALM_MORTAL = 0,     /* 凡人 */
    REALM_QI_REFINE,      /* 炼气期 */
    REALM_FOUNDATION,     /* 筑基期 */
    REALM_GOLDEN_CORE,    /* 金丹期 */
    REALM_NASCENT_SOUL,   /* 元婴期 */
    REALM_SPIRIT_TRANS,   /* 化神期 */
    REALM_TRIBULATION,    /* 渡劫期 */
    REALM_MAHAYANA,       /* 大乘期 */
    REALM_MAX
} RealmLevel;

/* ===== 境界阶段 ===== */
typedef enum {
    STAGE_EARLY = 0,      /* 初期 */
    STAGE_MIDDLE,         /* 中期 */
    STAGE_LATE,           /* 后期 */
    STAGE_PEAK            /* 圆满 */
} RealmStage;

/* ===== 道具类型 ===== */
typedef enum {
    ITEM_MISC = 0,        /* 杂项 */
    ITEM_PILL,            /* 丹药 */
    ITEM_WEAPON,          /* 武器 */
    ITEM_ARMOR,           /* 防具 */
    ITEM_MANUAL,          /* 功法秘籍 */
    ITEM_MATERIAL,        /* 材料 */
    ITEM_QUEST,           /* 任务道具 */
    ITEM_MAX
} ItemType;

/* ===== NPC 类型 ===== */
typedef enum {
    NPC_MONSTER = 0,      /* 怪物 */
    NPC_MERCHANT,         /* 商人 */
    NPC_QUEST_GIVER,      /* 任务NPC */
    NPC_ELDER,            /* 宗门长老 */
    NPC_MAX
} NPCType;

/* ===== 方向 ===== */
typedef enum {
    DIR_NORTH = 0,
    DIR_SOUTH,
    DIR_EAST,
    DIR_WEST,
    DIR_UP,
    DIR_DOWN,
    DIR_MAX
} Direction;

/* ===== 道具结构体 ===== */
typedef struct {
    int         id;
    char        name[MAX_NAME_LEN];
    char        desc[MAX_DESC_LEN];
    ItemType    type;
    int         value;          /* 基础价值（灵石） */
    int         hp_bonus;       /* HP加成 */
    int         mp_bonus;       /* MP加成 */
    int         atk_bonus;      /* 攻击加成 */
    int         def_bonus;      /* 防御加成 */
    int         exp_bonus;      /* 修为加成 */
    int         quantity;       /* 堆叠数量 */
    bool        stackable;      /* 是否可堆叠 */
} Item;

/* ===== NPC / 怪物结构体 ===== */
typedef struct {
    int         id;
    char        name[MAX_NAME_LEN];
    char        desc[MAX_DESC_LEN];
    NPCType     type;
    RealmLevel  realm;          /* 境界 */
    int         hp;
    int         max_hp;
    int         mp;
    int         max_mp;
    int         atk;
    int         def;
    int         exp_reward;     /* 击杀修为奖励 */
    int         gold_reward;    /* 击杀灵石奖励 */
    int         drop_item_id;   /* 掉落道具ID，-1表示无掉落 */
    bool        is_alive;
} NPC;

/* ===== 技能结构体 ===== */
typedef struct {
    int     id;
    char    name[MAX_NAME_LEN];
    char    desc[MAX_DESC_LEN];
    int     mp_cost;        /* 消耗MP */
    int     damage;         /* 伤害 */
    int     level;          /* 技能等级 */
    int     max_level;
} Skill;

/* ===== 房间出口 ===== */
typedef struct {
    int         room_id;    /* 目标房间ID，-1表示无出口 */
    bool        locked;     /* 是否锁定 */
    RealmLevel  req_realm;  /* 境界要求 */
} RoomExit;

/* ===== 房间结构体 ===== */
typedef struct {
    int         id;
    char        name[MAX_NAME_LEN];
    char        desc[MAX_DESC_LEN];
    RoomExit    exits[DIR_MAX];             /* 六个方向出口 */
    int         npc_ids[MAX_NPC_PER_ROOM];  /* 房间内NPC的ID列表 */
    int         npc_count;
    int         item_ids[MAX_ITEM_PER_ROOM];/* 地上道具ID列表 */
    int         item_count;
    RealmLevel  min_realm;                  /* 进入最低境界 */
    bool        is_safe_zone;               /* 安全区（不可战斗） */
} Room;

/* ===== 玩家结构体 ===== */
typedef struct {
    int         id;
    char        name[MAX_NAME_LEN];
    char        password[MAX_NAME_LEN];
    SpiritRoot  spirit_root;    /* 灵根 */
    RealmLevel  realm;          /* 当前境界 */
    RealmStage  stage;          /* 当前阶段 */
    int         exp;            /* 当前修为 */
    int         exp_to_next;    /* 突破所需修为 */
    int         hp;
    int         max_hp;
    int         mp;
    int         max_mp;
    int         atk;
    int         def;
    int         gold;           /* 灵石 */
    int         current_room_id;
    Item        inventory[MAX_INV_SLOTS];   /* 背包 */
    int         inv_count;
    Skill       skills[MAX_SKILL_SLOTS];    /* 技能 */
    int         skill_count;
    int         sect_id;        /* 宗门ID，-1为无 */
    int         disciple_count; /* 徒弟数量 */
    bool        in_combat;
    int         combat_target_id; /* 战斗目标NPC ID */
} Player;

/* ===== 命令定义 ===== */
typedef struct {
    char    name[MAX_CMD_LEN];
    char    aliases[4][MAX_CMD_LEN];  /* 别名最多4个 */
    char    help[MAX_DESC_LEN];
    void    (*handler)(Player *player, const char *args);
} Command;

/* ===== 模块接口 ===== */
typedef struct {
    char    name[MAX_NAME_LEN];
    void    (*init)(void);          /* 模块初始化 */
    void    (*tick)(Player *player);/* 每帧更新 */
    void    (*cleanup)(void);       /* 模块清理 */
    Command *commands;              /* 模块注册的命令列表 */
    int     cmd_count;
} Module;

/* ===== 事件类型 ===== */
typedef enum {
    EVENT_PLAYER_ENTER_ROOM,    /* 进入房间 */
    EVENT_PLAYER_LEAVE_ROOM,    /* 离开房间 */
    EVENT_PLAYER_KILL_NPC,      /* 击杀NPC */
    EVENT_PLAYER_LEVEL_UP,      /* 突破境界 */
    EVENT_PLAYER_USE_ITEM,      /* 使用道具 */
    EVENT_PLAYER_GET_ITEM,      /* 获得道具 */
    EVENT_COMBAT_START,         /* 战斗开始 */
    EVENT_COMBAT_END,           /* 战斗结束 */
    EVENT_MAX
} EventType;

/* 事件回调 */
typedef void (*EventCallback)(EventType type, Player *player, void *data);

/* 事件监听器 */
typedef struct {
    EventType       type;
    EventCallback   callback;
    bool            active;
} EventListener;

#endif /* MUD_H */