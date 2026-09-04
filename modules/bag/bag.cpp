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

    // 检查是否在藏宝阁
    if (player->current_room_id != 4) {
        printf("你不在藏宝阁，无法购买物品。请前往藏宝阁找钱掌柜。\n");
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

    if (!player_add_item(player, it)) {
        player_add_gold(player, si.price);
        printf("购买失败，已退还 %d 灵石。\n", si.price);
    }
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

    if (!player_add_item(player, it)) {
        player_add_gold(player, si.price);
        printf("购买失败，已退还 %d 灵石。\n", si.price);
    }
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

    if (player->current_room_id != 4) {
        printf("你不在藏宝阁，无法出售物品。请前往藏宝阁找钱掌柜。\n");
        return;
    }

    int idx = 0;
    try { idx = std::stoi(args); } catch (...) { idx = 0; }

    if (idx < 1 || idx > static_cast<int>(player->inventory.size())) {
        printf("无效的背包编号。\n");
        return;
    }

   auto& it = player->inventory[idx - 1];

// 药渣按策划固定 10 灵石/个回收，其余半价
int sell_price = (it.id == 227 || it.id == 229) ? 10 : (it.value / 2);

player_add_gold(player, sell_price);

printf("你出售了 %s x1，获得 %d 灵石。\n",
       it.name.c_str(), sell_price);

// 可堆叠并且数量大于1，只卖掉一个
if (it.stackable && it.quantity > 1) {
    it.quantity--;
}
else {
    player->inventory.erase(player->inventory.begin() + (idx - 1));
}
}

// ---- 丹房合成：累积 10 个药渣合成 1 瓶止血散（丹房 / 百艺阁）----
static int count_item(const Player* p, int item_id) {
    int n = 0;
    for (const auto& it : p->inventory) if (it.id == item_id) n += it.quantity;
    return n;
}
static void remove_item_amount(Player* p, int item_id, int amount) {
    for (size_t i = 0; i < p->inventory.size() && amount > 0;) {
        auto& it = p->inventory[i];
        if (it.id == item_id) {
            int take = std::min(it.quantity, amount);
            it.quantity -= take;
            amount -= take;
            if (it.quantity <= 0)
                p->inventory.erase(p->inventory.begin() + i);
            else
                i++;
        } else i++;
    }
}
static void cmd_combine(Player* player, const std::string& args) {
    (void)args;
    if (player->current_room_id != 6) {   // 6 = 百艺阁（丹房）
        printf("需在百艺阁丹房开炉合成止血散。\n");
        return;
    }
    const int cost = 10;
    if (count_item(player, 227) < cost) {
        printf("药渣不足（需 %d 个，当前 %d 个）。炸炉产出或击杀妖兽可得药渣。\n",
               cost, count_item(player, 227));
        return;
    }
    remove_item_amount(player, 227, cost);
    Item* tmpl = item_get(228);
    if (tmpl) {
        Item it = *tmpl;
        it.quantity = 1;
        player_add_item(player, it);
    }
    printf("你以 10 个药渣合成了 1 瓶【止血散】（恢复100气血）。\n");
}

static void cmd_shop(Player* player, const std::string& args) {
    (void)args;
    if (player->current_room_id != 4) {
        printf("你不在藏宝阁，附近没有商店。请前往藏宝阁找钱掌柜。\n");
        return;
    }

    const int W = 34;   // 内容区显示宽度
    auto line = [&](const std::string& s) { printf("║ %s ║\n", pad_to_width(s, W).c_str()); };
    auto hr   = [&](const char* l, const char* r) { printf("%s%s%s\n", l, box_rep("═", W + 2).c_str(), r); };

    printf("\n");
    hr("╔", "╗");
    line("藏宝阁商店");
    hr("╠", "╣");
    line("你的灵石: " + std::to_string(player->gold));
    hr("╠", "╣");
    line(pad_to_width("编号  物品", 26) + "价格");
    for (size_t i = 0; i < g_shop_items.size(); i++) {
        auto& si = g_shop_items[i];
        Item* tmpl = item_get(si.item_id);
        if (!tmpl) continue;
        std::string row = "[" + std::to_string(i + 1) + "] " + tmpl->name;
        line(pad_to_width(row, 26) + std::to_string(si.price));
    }
    hr("╚", "╝");
    printf("使用 buy <编号> 购买，sell <背包编号> 出售\n");
    printf("（药渣可按10灵石/个出售给钱掌柜）\n\n");
}

// ---- 模块命令列表 ----
static std::vector<Command> bag_commands = {
    {"buy",  {},               "购买物品 (buy <编号/名称>)", cmd_buy},
    {"sell", {},               "出售物品 (sell <背包编号>)", cmd_sell},
    {"shop", {"store", "list"},"查看商店",                  cmd_shop},
    {"combine", {"hecheng"},   "丹房合成 (10药渣→止血散)",  cmd_combine},
};

// ---- 模块初始化/更新/清理 ----

static void bag_init() {
    printf("[模块D] 背包商店系统初始化\n");

    // 藏宝阁商品列表（对齐 V2.0 丹药效果价格明细 & 法器售价表）
    g_shop_items = {

        // ===== 丹药（下/中/上/极品）=====
        {301, 30},    // 下品淬体丹
        {302, 60},    // 中品淬体丹
        {303, 120},   // 上品淬体丹
        {304, 240},   // 极品淬体丹
        {305, 25},    // 下品聚气丹
        {306, 50},    // 中品聚气丹
        {307, 100},   // 上品聚气丹
        {308, 200},   // 极品聚气丹
        {309, 25},    // 下品养神丹
        {310, 50},    // 中品养神丹
        {311, 100},   // 上品养神丹
        {312, 200},   // 极品养神丹
        {313, 40},    // 下品启悟丹
        {314, 80},    // 中品启悟丹
        {315, 160},   // 上品启悟丹
        {316, 320},   // 极品启悟丹
        {317, 50},    // 下品培元丹
        {318, 100},   // 中品培元丹
        {319, 200},   // 上品培元丹
        {320, 400},   // 极品培元丹
        {321, 35},    // 下品精工丹
        {322, 70},    // 中品精工丹
        {323, 140},   // 上品精工丹
        {324, 280},   // 极品精工丹

        // ===== 法器（藏宝阁售价表）=====
        {401, 800},   // 青锋灵剑
        {402, 1000},  // 玄铁裂爪
        {403, 2200},  // 流风环刃
        {404, 2600},  // 焚火玉牌
        {405, 6000},  // 寒魄断川刀
        {406, 18000}, // 曜日镇神戈

        // ===== 四艺材料 =====
        {230, 20},    // 精铁矿石（炼器）
        {231, 15},    // 符纸（画符）

        // ===== 功法秘籍 =====
        {511, 1200},  // 《丹道真解》
        {512, 1200},  // 《器铸玄经》
        {513, 1200},  // 《符箓通典》
        {514, 1200}   // 《御兽灵诀》
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