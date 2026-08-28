#include "mud.hpp"

// ===== NPC 和 Item 模板管理 =====
static std::map<int, NPC> g_npc_templates;
static std::map<int, Item> g_item_templates;

// ---- NPC ----

void npc_init() {
    g_npc_templates.clear();
}

NPC* npc_get(int id) {
    auto it = g_npc_templates.find(id);
    return (it != g_npc_templates.end()) ? &it->second : nullptr;
}

int npc_count() { return static_cast<int>(g_npc_templates.size()); }

std::map<int, NPC>& npc_get_all() { return g_npc_templates; }

// 创建一个NPC模板并返回指针
NPC* npc_create(int id, const std::string& name, const std::string& desc,
                NPCType type, RealmLevel realm, int hp, int atk, int def,
                int exp_reward, int gold_reward, int drop_item_id) {
    NPC npc;
    npc.id = id;
    npc.name = name;
    npc.desc = desc;
    npc.type = type;
    npc.realm = realm;
    npc.hp = npc.max_hp = hp;
    npc.atk = atk;
    npc.def = def;
    npc.exp_reward = exp_reward;
    npc.gold_reward = gold_reward;
    npc.drop_item_id = drop_item_id;
    npc.is_alive = true;
    g_npc_templates[id] = npc;
    return &g_npc_templates[id];
}

void npc_despawn(int npc_id) {
    auto it = g_npc_templates.find(npc_id);
    if (it != g_npc_templates.end()) it->second.is_alive = false;
}

// ---- Item ----

void item_init() {
    g_item_templates.clear();
}

Item* item_get(int id) {
    auto it = g_item_templates.find(id);
    return (it != g_item_templates.end()) ? &it->second : nullptr;
}

int item_count() { return static_cast<int>(g_item_templates.size()); }

std::map<int, Item>& item_get_all() { return g_item_templates; }

// 创建一个道具模板
Item* item_create(int id, const std::string& name, const std::string& desc,
                  ItemType type, int value, int hp_bonus, int mp_bonus,
                  int atk_bonus, int def_bonus, int exp_bonus,
                  bool stackable) {
    Item item;
    item.id = id;
    item.name = name;
    item.desc = desc;
    item.type = type;
    item.value = value;
    item.hp_bonus = hp_bonus;
    item.mp_bonus = mp_bonus;
    item.atk_bonus = atk_bonus;
    item.def_bonus = def_bonus;
    item.exp_bonus = exp_bonus;
    item.quantity = 1;
    item.stackable = stackable;
    g_item_templates[id] = item;
    return &g_item_templates[id];
}