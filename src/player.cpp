#include "mud.hpp"

// ===== 玩家管理 =====

// 前向声明
Room* room_get(int id);
void room_remove_item(int room_id, int item_id);
void event_emit(EventType type, Player* player, void* data);

// ---- 属性重算 ----
void player_recalc_stats(Player* player) {
    int r = static_cast<int>(player->realm);
    int s = static_cast<int>(player->stage);

    int base_hp  = 100 + r * 200 + s * 50;
    int base_mp  = 50  + r * 100 + s * 30;
    int base_atk = 10  + r * 25  + s * 8;
    int base_def = 5   + r * 15  + s * 5;

    player->max_hp = base_hp;
    player->max_mp = base_mp;
    player->atk = base_atk;
    player->def = base_def;

    // 变异灵根额外加成
    if (static_cast<int>(player->spirit_root) >= 6) {
        player->max_hp += 50;
        player->max_mp += 30;
        player->atk += 10;
        player->def += 5;
    }

    // 装备加成
    for (auto& it : player->inventory) {
        if (it.type == ItemType::WEAPON || it.type == ItemType::ARMOR) {
            player->max_hp += it.hp_bonus;
            player->max_mp += it.mp_bonus;
            player->atk += it.atk_bonus;
            player->def += it.def_bonus;
        }
    }
}

// ---- 创建/销毁 ----
Player* player_create(int id, const std::string& name, const std::string& password) {
    Player* p = new Player();
    p->id = id;
    p->name = name;
    p->password = password;
    p->spirit_root = SpiritRoot::NONE;
    p->realm = RealmLevel::MORTAL;
    p->stage = RealmStage::EARLY;
    p->exp = 0;
    p->exp_to_next = realm_exp_required(RealmLevel::MORTAL, RealmStage::EARLY);
    p->hp = 100;
    p->max_hp = 100;
    p->mp = 50;
    p->max_mp = 50;
    p->atk = 10;
    p->def = 5;
    p->gold = 0;
    p->current_room_id = 1;
    p->sect_id = -1;
    p->disciple_count = 0;
    p->in_combat = false;
    p->combat_target_id = -1;
    return p;
}

void player_destroy(Player* player) {
    delete player;
}

// ---- 修为操作 ----
bool player_add_exp(Player* player, int amount) {
    if (amount <= 0) return false;
    player->exp += amount;
    printf("你获得了 %d 点修为！(当前: %d / %d)\n",
           amount, player->exp, player->exp_to_next);
    return true;
}

bool player_try_breakthrough(Player* player) {
    if (player->realm >= RealmLevel::MAHAYANA && player->stage >= RealmStage::PEAK) {
        printf("你已经达到修仙巅峰，无法继续突破。\n");
        return false;
    }

    if (player->exp < player->exp_to_next) {
        printf("修为不足，无法突破！还需要 %d 点修为。\n",
               player->exp_to_next - player->exp);
        return false;
    }

    // 消耗修为
    player->exp -= player->exp_to_next;

    // 进阶
    if (player->stage >= RealmStage::PEAK) {
        if (player->realm < RealmLevel::MAHAYANA) {
            int r = static_cast<int>(player->realm);
            player->realm = static_cast<RealmLevel>(r + 1);
            player->stage = RealmStage::EARLY;
            printf("\n====== 突破！======\n");
            printf("你成功突破至【%s】！\n", realm_name(player->realm));
            printf("====================\n\n");

            if (player->realm == RealmLevel::TRIBULATION) {
                printf("天劫将至，你需要做好准备迎接天劫考验！\n");
            }

            event_emit(EventType::PLAYER_LEVEL_UP, player, nullptr);
        }
    } else {
        int s = static_cast<int>(player->stage);
        player->stage = static_cast<RealmStage>(s + 1);
        printf("你成功突破至【%s%s】！\n",
               realm_name(player->realm), stage_name(player->stage));
    }

    player->exp_to_next = realm_exp_required(player->realm, player->stage);
    player_recalc_stats(player);

    // 恢复满状态
    player->hp = player->max_hp;
    player->mp = player->max_mp;

    printf("下一阶段需要修为: %d\n", player->exp_to_next);
    printf("HP: %d  MP: %d  攻击: %d  防御: %d\n",
           player->max_hp, player->max_mp, player->atk, player->def);

    return true;
}

// ---- 移动 ----
bool player_move_to(Player* player, int room_id) {
    Room* room = room_get(room_id);
    if (!room) {
        printf("那个方向没有路。\n");
        return false;
    }

    if (static_cast<int>(player->realm) < static_cast<int>(room->min_realm)) {
        printf("你的境界不足，无法进入该区域。（需要%s以上）\n",
               realm_name(room->min_realm));
        return false;
    }

    event_emit(EventType::PLAYER_LEAVE_ROOM, player, nullptr);
    player->current_room_id = room_id;
    event_emit(EventType::PLAYER_ENTER_ROOM, player, nullptr);
    printf("你来到了【%s】。\n", room->name.c_str());
    return true;
}

// ---- 背包操作 ----
bool player_add_item(Player* player, const Item& item) {
    // 可堆叠的先找同ID
    if (item.stackable) {
        for (auto& inv : player->inventory) {
            if (inv.id == item.id) {
                inv.quantity += item.quantity;
                printf("你获得了 %s x%d（共 %d）\n",
                       item.name.c_str(), item.quantity, inv.quantity);
                return true;
            }
        }
    }

    if (static_cast<int>(player->inventory.size()) >= MAX_INV_SLOTS) {
        printf("背包已满，无法拾取！\n");
        return false;
    }

    player->inventory.push_back(item);
    printf("你获得了 %s。\n", item.name.c_str());
    event_emit(EventType::PLAYER_GET_ITEM, player, nullptr);
    return true;
}

bool player_remove_item(Player* player, int slot) {
    if (slot < 0 || slot >= static_cast<int>(player->inventory.size())) return false;
    printf("你丢弃了 %s。\n", player->inventory[slot].name.c_str());
    player->inventory.erase(player->inventory.begin() + slot);
    return true;
}

bool player_use_item(Player* player, int slot) {
    if (slot < 0 || slot >= static_cast<int>(player->inventory.size())) return false;

    Item& it = player->inventory[slot];

    switch (it.type) {
    case ItemType::PILL:
        printf("你服用了 %s。\n", it.name.c_str());
        player->hp = std::min(player->hp + it.hp_bonus, player->max_hp);
        player->mp = std::min(player->mp + it.mp_bonus, player->max_mp);
        if (it.exp_bonus > 0) player_add_exp(player, it.exp_bonus);
        break;

    case ItemType::MANUAL:
        printf("你研读了秘籍《%s》。\n", it.name.c_str());
        if (it.exp_bonus > 0) player_add_exp(player, it.exp_bonus);
        break;

    default:
        printf("%s 无法直接使用。\n", it.name.c_str());
        return false;
    }

    event_emit(EventType::PLAYER_USE_ITEM, player, nullptr);

    // 消耗道具
    if (it.stackable && it.quantity > 1) {
        it.quantity--;
    } else {
        player_remove_item(player, slot);
    }
    return true;
}

Item* player_find_item(Player* player, int item_id) {
    for (auto& it : player->inventory) {
        if (it.id == item_id) return &it;
    }
    return nullptr;
}

// ---- 技能 ----
bool player_add_skill(Player* player, const Skill& skill) {
    if (static_cast<int>(player->skills.size()) >= MAX_SKILL_SLOTS) {
        printf("技能槽已满，无法学习新技能！\n");
        return false;
    }
    player->skills.push_back(skill);
    printf("你学会了新技能【%s】！\n", skill.name.c_str());
    return true;
}

// ---- 灵石 ----
void player_add_gold(Player* player, int amount) {
    player->gold += amount;
    printf("你获得了 %d 灵石。（余额: %d）\n", amount, player->gold);
}

bool player_spend_gold(Player* player, int amount) {
    if (player->gold < amount) {
        printf("灵石不足！需要 %d，当前只有 %d。\n", amount, player->gold);
        return false;
    }
    player->gold -= amount;
    printf("你花费了 %d 灵石。（余额: %d）\n", amount, player->gold);
    return true;
}