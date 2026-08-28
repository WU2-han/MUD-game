#ifndef MUD_API_HPP
#define MUD_API_HPP

// ============================================
// 引擎层共享 API 声明
// 所有跨文件调用的函数都集中在这里声明，
// 由 mud.hpp 引入，供 main 与各模块统一使用。
// ============================================

#include "mud.hpp"

// ---- 命令系统（command.cpp）----
void cmd_init();
void cmd_register(const std::string& name,
                  const std::vector<std::string>& aliases,
                  std::function<void(Player*, const std::string&)> handler,
                  const std::string& help);
void cmd_register_module(const Module& mod);
Command* cmd_find(const std::string& name);
void cmd_execute(Player* player, const std::string& input);
void cmd_show_all(Player* player);
const std::vector<Command>& cmd_get_all();

// ---- 模块系统（module.cpp）----
bool module_register(const Module& mod);
void module_init_all();
void module_tick_all(Player* player);
void module_cleanup_all();
int module_count();
const std::vector<Module>& module_get_all();

// ---- 事件系统（event.cpp）----
void event_init();
void event_listen(EventType type, EventCallback callback);
void event_unlisten(EventType type, const EventCallback& callback);
void event_emit(EventType type, Player* player, void* data);
void event_cleanup();

// ---- NPC / 道具模板（npc.cpp）----
void npc_init();
NPC* npc_create(int id, const std::string& name, const std::string& desc,
                NPCType type, RealmLevel realm, int hp, int atk, int def,
                int exp_reward, int gold_reward, int drop_item_id);
void npc_despawn(int npc_id);
NPC* npc_get(int id);
int npc_count();
std::map<int, NPC>& npc_get_all();

void item_init();
Item* item_create(int id, const std::string& name, const std::string& desc,
                  ItemType type, int value, int hp_bonus, int mp_bonus,
                  int atk_bonus, int def_bonus, int exp_bonus, bool stackable);
Item* item_get(int id);
int item_count();
std::map<int, Item>& item_get_all();

// ---- 房间 / 世界（room.cpp）----
void world_init();
Room* room_create(int id, const std::string& name, const std::string& desc);
Room* room_get(int id);
int room_count();
std::map<int, Room>& room_get_all();
void room_set_exit(int from_id, Direction dir, int to_id);
void room_lock_exit(int from_id, Direction dir, bool locked, RealmLevel req_realm);
void room_add_item(int room_id, int item_id);
void room_remove_item(int room_id, int item_id);
void room_add_npc(int room_id, int npc_id);
void room_remove_npc(int room_id, int npc_id);

// ---- 玩家（player.cpp）----
Player* player_create(int id, const std::string& name, const std::string& password);
void player_destroy(Player* player);
void player_recalc_stats(Player* player);
bool player_add_exp(Player* player, int amount);
bool player_try_breakthrough(Player* player);
bool player_move_to(Player* player, int room_id);
bool player_add_item(Player* player, const Item& item);
bool player_remove_item(Player* player, int slot);
bool player_use_item(Player* player, int slot);
Item* player_find_item(Player* player, int item_id);
bool player_add_skill(Player* player, const Skill& skill);
void player_add_gold(Player* player, int amount);
bool player_spend_gold(Player* player, int amount);

// ---- 存档（save.cpp）----
bool save_player(Player* player);
Player* load_player(const std::string& name);
void save_list_players();
bool save_delete_player(const std::string& name);
bool save_player_exists(const std::string& name);
const char* save_get_dir();
bool save_world_state();
bool load_world_state();

#endif // MUD_API_HPP