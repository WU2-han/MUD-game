#include "mud.hpp"

// ===== 房间/世界管理 =====
static std::map<int, Room> g_rooms;

void world_init() {
    g_rooms.clear();
}

Room* room_create(int id, const std::string& name, const std::string& desc) {
    Room room;
    room.id = id;
    room.name = name;
    room.desc = desc;
    for (int d = 0; d < 6; d++) {
        room.exits[d].room_id = -1;
        room.exits[d].locked = false;
        room.exits[d].req_realm = RealmLevel::MORTAL;
    }
    room.min_realm = RealmLevel::MORTAL;
    room.is_safe_zone = true;
    g_rooms[id] = room;
    return &g_rooms[id];
}

Room* room_get(int id) {
    auto it = g_rooms.find(id);
    return (it != g_rooms.end()) ? &it->second : nullptr;
}

int room_count() { return static_cast<int>(g_rooms.size()); }

std::map<int, Room>& room_get_all() { return g_rooms; }

void room_set_exit(int from_id, Direction dir, int to_id) {
    Room* room = room_get(from_id);
    if (!room) return;
    room->exits[static_cast<int>(dir)].room_id = to_id;
}

void room_lock_exit(int from_id, Direction dir, bool locked, RealmLevel req_realm) {
    Room* room = room_get(from_id);
    if (!room) return;
    int d = static_cast<int>(dir);
    room->exits[d].locked = locked;
    room->exits[d].req_realm = req_realm;
}

void room_add_item(int room_id, int item_id) {
    Room* room = room_get(room_id);
    if (!room || room->item_ids.size() >= MAX_ITEM_PER_ROOM) return;
    room->item_ids.push_back(item_id);
}

void room_remove_item(int room_id, int item_id) {
    Room* room = room_get(room_id);
    if (!room) return;
    auto& items = room->item_ids;
    auto it = std::find(items.begin(), items.end(), item_id);
    if (it != items.end()) items.erase(it);
}

void room_add_npc(int room_id, int npc_id) {
    Room* room = room_get(room_id);
    if (!room || room->npc_ids.size() >= MAX_NPC_PER_ROOM) return;
    // 记录归属房间，供妖兽击杀后 3 天刷新归位
    NPC* n = npc_get(npc_id);
    if (n) n->home_room = room_id;
    room->npc_ids.push_back(npc_id);
}

void room_remove_npc(int room_id, int npc_id) {
    Room* room = room_get(room_id);
    if (!room) return;
    auto& npcs = room->npc_ids;
    auto it = std::find(npcs.begin(), npcs.end(), npc_id);
    if (it != npcs.end()) npcs.erase(it);
}