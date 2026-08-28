/* ============================================
 * 模块D：背包、道具、坊市商店
 * 负责人：成员D
 *
 * 可用接口:
 * - player_add_item()    添加道具
 * - player_remove_item() 移除道具
 * - player_use_item()    使用道具
 * - player_find_item()   查找道具
 * - player_add_gold()    增加灵石
 * - player_spend_gold()  花费灵石
 * - item_get()           获取道具模板
 * ============================================ */

#include "../include/mud.hpp"

// 前向声明
void player_add_gold(Player* player, int amount);
bool player_spend_gold(Player* player, int amount);
bool player_add_item(Player* player, const Item& item);
bool player_remove_item(Player* player, int slot);
Item* item_get(int id);
Room* room_get(int id);

// 商店道具列表（在坊市房间可用）
struct ShopItem {
    int item_id;
    int price;
};

static std::vector<ShopItem> g_shop_items;

static void cmd_buy(Player* player, const std::string& args) {
    if (args.empty()) {
        printf("用法: buy <物品名称或编号>\n");
        return;
    }

    // 检查是否在坊市
    if (player->current_room_id != 3) {
        printf("你不在坊市，无法购买物品。请前往修仙坊市。\n");
        return;
    }

    // 尝试按编号
    int idx = 0;
    try { idx = std::stoi(args); } catch (...) { idx = 0; }

    if (idx > 0 && idx <= static_cast<int>(g_shop_items.size())) {
        auto& si = g_shop_items[idx - 1];
        Item* tmpl = item_get(si.item_id);
        if (!tmpl) return;

        if (player_spend_gold(player, si.price)) {
            Item it = *tmpl;
            it.quantity = 1;
            player_add_item(player, it);
        }
        return;
    }

    // 按名称查找
    for (auto& si : g_shop_items) {
        Item* tmpl = item_get(si.item_id);
        if (tmpl && tmpl->name.find(args) != std::string::npos) {
            if (player_spend_gold(player, si.price)) {
                Item it = *tmpl;
                it.quantity = 1;
                player_add_item(player, it);
            }
            return;
        }
    }
    printf("商店没有 %s。\n", args.c_str());
}

static void cmd_sell(Player* player, const std::string& args) {
    if (args.empty()) {
        printf("用法: sell <背包编号>\n");
        return;
    }

    if (player->current_room_id != 3) {
        printf("你不在坊市，无法出售物品。请前往修仙坊市。\n");
        return;
    }

    int idx = 0;
    try { idx = std::stoi(args); } catch (...) { idx = 0; }

    if (idx < 1 || idx > static_cast<int>(player->inventory.size())) {
        printf("无效的背包编号。\n");
        return;
    }

    auto& it = player->inventory[idx - 1];
    int sell_price = it.value / 2; // 半价回收
    player_add_gold(player, sell_price);
    printf("你出售了 %s，获得 %d 灵石。\n", it.name.c_str(), sell_price);
    player_remove_item(player, idx - 1);
}

static void cmd_shop(Player* player, const std::string& args) {
    (void)args;
    if (player->current_room_id != 3) {
        printf("你不在坊市，附近没有商店。请前往修仙坊市。\n");
        return;
    }

    printf("\n╔══════════ 坊市商店 ══════════╗\n");
    printf("║ 你的灵石: %-18d ║\n", player->gold);
    printf("╠══════════════════════════════╣\n");
    printf("║ 编号  物品         价格     ║\n");
    for (size_t i = 0; i < g_shop_items.size(); i++) {
        auto& si = g_shop_items[i];
        Item* tmpl = item_get(si.item_id);
        if (tmpl) {
            printf("║ [%d]  %-12s %-8d ║\n",
                   (int)(i + 1), tmpl->name.c_str(), si.price);
        }
    }
    printf("╚══════════════════════════════╝\n");
    printf("使用 buy <编号> 购买，sell <背包编号> 出售\n\n");
}

// ---- 模块命令列表 ----
static std::vector<Command> bag_commands = {
    {"buy",  {},               "购买物品 (buy <编号/名称>)", cmd_buy},
    {"sell", {},               "出售物品 (sell <背包编号>)", cmd_sell},
    {"shop", {"store", "list"},"查看商店",                  cmd_shop},
};

// ---- 模块初始化/更新/清理 ----

static void bag_init() {
    printf("[模块D] 背包商店系统初始化\n");

    // 初始化商店道具
    g_shop_items = {
        {201, 50},   // 疗伤丹
        {202, 40},   // 回灵丹
        {203, 200},  // 聚气丹
        {204, 100},  // 铁剑
        {205, 80},   // 布甲
        {206, 300},  // 筑基功法
        {207, 30},   // 灵草
        {210, 500},  // 筑基丹
    };
}

static void bag_tick(Player* player) {
    (void)player;
}

static void bag_cleanup() {
    printf("[模块D] 背包商店系统清理\n");
}

// ---- 模块导出 ----
Module bag_module = {
    "背包商店",
    bag_init,
    bag_tick,
    bag_cleanup,
    bag_commands
};