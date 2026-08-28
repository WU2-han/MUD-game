/* ============================================
 * 模块C：世界地图 & 场景交互
 * 负责人：成员C
 *
 * 可用接口:
 * - room_get()         获取房间
 * - room_set_exit()    设置出口
 * - room_lock_exit()   设置境界锁
 * - room_add_item()    房间添加道具
 * - room_add_npc()     房间添加NPC
 * - npc_spawn()        生成NPC
 * ============================================ */

#include "../include/mud.hpp"

// 前向声明
Room* room_get(int id);

static void cmd_map(Player* player, const std::string& args) {
    (void)args;
    printf("\n╔══════════ 修仙世界地图 ══════════╗\n");
    printf("║                                  ║\n");
    printf("║    渡劫台(11)                    ║\n");
    printf("║      ↑                           ║\n");
    printf("║  御兽园(10)← 宗门大殿(6) →藏经阁(8)║\n");
    printf("║      ↓        ↓                  ║\n");
    printf("║  炼丹房(9)  灵药山(4) →妖兽森林(5)║\n");
    printf("║      ↑        ↑        ↓   →秘境(7)║\n");
    printf("║  新手村(1) →村外树林(2) 后山山洞(12)║\n");
    printf("║      →                            ║\n");
    printf("║  修仙坊市(3)                      ║\n");
    printf("║                                  ║\n");
    printf("║ 你当前所在: %-20s ║\n",
           room_get(player->current_room_id) ? room_get(player->current_room_id)->name.c_str() : "未知");
    printf("╚══════════════════════════════════╝\n\n");
}

// ---- 模块命令列表 ----
static std::vector<Command> world_commands = {
    {"map", {}, "查看世界地图", cmd_map},
};

// ---- 模块初始化/更新/清理 ----

static void world_mod_init() {
    printf("[模块C] 世界地图初始化\n");
    // 房间数据已在 main.cpp 的 init_game_data() 中创建
}

static void world_mod_tick(Player* player) {
    (void)player;
}

static void world_mod_cleanup() {
    printf("[模块C] 世界地图清理\n");
}

// ---- 模块导出 ----
Module world_module = {
    "世界地图",
    world_mod_init,
    world_mod_tick,
    world_mod_cleanup,
    world_commands
};