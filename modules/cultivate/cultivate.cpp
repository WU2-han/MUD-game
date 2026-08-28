/* ============================================
 * 模块B：修炼晋升 + 战斗模块
 * 负责人：成员B
 *
 * 你需要实现:
 * - cultivate_init()   : 初始化修炼系统
 * - cultivate_tick()   : 每帧更新
 * - cultivate_cleanup(): 清理
 * - 注册命令（在 commands 列表中定义）
 *
 * 可用接口:
 * - player_add_exp()           增加修为
 * - player_try_breakthrough()  尝试突破
 * - player_recalc_stats()      重算属性
 * - event_emit()               触发事件
 * - room_get()                 获取房间
 * - npc_get()                  获取NPC
 * ============================================ */

#include "../include/mud.hpp"

// 前向声明外部接口
bool player_add_exp(Player* player, int amount);
bool player_try_breakthrough(Player* player);
void player_recalc_stats(Player* player);
void player_add_gold(Player* player, int amount);
bool player_add_item(Player* player, const Item& item);
Room* room_get(int id);
NPC* npc_get(int id);
Item* item_get(int id);
void room_remove_npc(int room_id, int npc_id);
void event_emit(EventType type, Player* player, void* data);

// ---- 模块命令处理函数声明 ----
static void cmd_fight(Player* player, const std::string& args);
static void cmd_flee(Player* player, const std::string& args);

// ---- 命令处理函数 ----

static void cmd_fight(Player* player, const std::string& args) {
    if (args.empty()) {
        printf("用法: fight <NPC名称>\n");
        return;
    }

    if (player->in_combat) {
        printf("你已经在战斗中！\n");
        return;
    }

    Room* room = room_get(player->current_room_id);
    if (!room) return;

    if (room->is_safe_zone) {
        printf("这里不能战斗。\n");
        return;
    }

    // 查找NPC
    NPC* target = nullptr;
    for (int npc_id : room->npc_ids) {
        NPC* npc = npc_get(npc_id);
        if (npc && npc->is_alive && npc->name.find(args) != std::string::npos) {
            target = npc;
            break;
        }
    }

    if (!target) {
        printf("这里没有叫 %s 的敌人。\n", args.c_str());
        return;
    }

    if (target->type != NPCType::MONSTER) {
        printf("%s 不是敌人，不能攻击。\n", target->name.c_str());
        return;
    }

    // 开始战斗
    player->in_combat = true;
    player->combat_target_id = target->id;
    event_emit(EventType::COMBAT_START, player, nullptr);

    printf("\n===== 战斗开始！=====\n");
    printf("你向 %s 发起了攻击！\n", target->name.c_str());

    // 战斗循环（简化版：自动战斗一轮）
    while (player->hp > 0 && target->hp > 0 && player->in_combat) {
        // 玩家攻击
        int player_dmg = std::max(1, player->atk - target->def + rand() % 10);
        target->hp -= player_dmg;
        printf("你对 %s 造成了 %d 点伤害", target->name.c_str(), player_dmg);

        if (target->hp <= 0) {
            target->hp = 0;
            target->is_alive = false;
            printf("，你击败了 %s！\n", target->name.c_str());

            // 奖励
            if (target->exp_reward > 0) player_add_exp(player, target->exp_reward);
            if (target->gold_reward > 0) player_add_gold(player, target->gold_reward);

            // 掉落
            if (target->drop_item_id > 0) {
                Item* drop = item_get(target->drop_item_id);
                if (drop) {
                    printf("%s 掉落了 %s！\n", target->name.c_str(), drop->name.c_str());
                    player_add_item(player, *drop);
                }
            }

            room_remove_npc(player->current_room_id, target->id);
            player->in_combat = false;
            player->combat_target_id = -1;
            event_emit(EventType::COMBAT_END, player, nullptr);
            printf("===== 战斗胜利！=====\n\n");
            break;
        }

        printf("（%s剩余HP: %d）\n", target->name.c_str(), target->hp);

        // 怪物反击
        int npc_dmg = std::max(1, target->atk - player->def + rand() % 8);
        player->hp -= npc_dmg;
        printf("%s 反击，对你造成了 %d 点伤害（你的HP: %d/%d）\n",
               target->name.c_str(), npc_dmg, player->hp, player->max_hp);

        if (player->hp <= 0) {
            player->hp = 0;
            printf("\n你被 %s 击败了...\n", target->name.c_str());
            printf("你昏迷了过去，醒来时发现自己损失了一些修为和灵石。\n");
            player->exp = std::max(0, player->exp - player->exp / 10);
            player->gold = std::max(0, player->gold - player->gold / 5);
            player->hp = player->max_hp / 2;
            player->mp = player->max_mp / 2;
            player->in_combat = false;
            player->combat_target_id = -1;
            player->current_room_id = 1; // 回到新手村
            event_emit(EventType::COMBAT_END, player, nullptr);
            printf("===== 战斗失败！=====\n\n");
            break;
        }
    }
}

static void cmd_flee(Player* player, const std::string& args) {
    (void)args;
    if (!player->in_combat) {
        printf("你并没有在战斗中。\n");
        return;
    }

    // 50% 逃跑成功率
    if (rand() % 2 == 0) {
        printf("你成功逃跑了！\n");
        player->in_combat = false;
        player->combat_target_id = -1;
        event_emit(EventType::COMBAT_END, player, nullptr);
    } else {
        printf("逃跑失败！\n");
        NPC* target = npc_get(player->combat_target_id);
        if (target) {
            int dmg = std::max(1, target->atk / 2 - player->def / 2);
            player->hp -= dmg;
            printf("%s 趁机攻击，造成 %d 点伤害！\n", target->name.c_str(), dmg);
        }
    }
}

// ---- 模块初始化/更新/清理 ----

static void cultivate_init() {
    printf("[模块B] 修炼战斗系统初始化\n");
    srand((unsigned int)time(nullptr));
}

static void cultivate_tick(Player* player) {
    // 持续伤害等逻辑可在此实现
    (void)player;
}

static void cultivate_cleanup() {
    printf("[模块B] 修炼战斗系统清理\n");
}

// ---- 模块命令列表 ----
static std::vector<Command> cultivate_commands = {
    {"fight",  {"attack", "kill"}, "与NPC战斗 (fight <NPC名称>)", cmd_fight},
    {"flee",   {"escape"},         "逃跑 (flee)",                  cmd_flee},
};

// ---- 模块导出 ----
Module cultivate_module = {
    "修炼战斗",
    cultivate_init,
    cultivate_tick,
    cultivate_cleanup,
    cultivate_commands
};