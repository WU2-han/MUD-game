#include "mud.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

// ===== 前向声明 =====
extern Module cultivate_module;
extern Module world_module;
extern Module bag_module;
extern Module quest_module;

// ===== 游戏全局状态 =====
static Player* g_player = nullptr;
static bool g_running = true;

// 前向声明
void cmd_register(const std::string& name, const std::vector<std::string>& aliases,
                  std::function<void(Player*, const std::string&)> handler,
                  const std::string& help);
void cmd_show_all(Player* player);
void cmd_execute(Player* player, const std::string& input);

// ===== 内置命令实现 =====

static void cmd_help(Player* player, const std::string& args) {
    (void)args;
    cmd_show_all(player);
}

// ===== UTF-8 显示宽度辅助（中文等全角按 2 列，ASCII 按 1 列）=====

// 返回从首字节 c 开始的 UTF-8 字符占用的字节数
static int utf8_char_bytes(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

// 计算字符串显示宽度（多字节字符按 2 列）
static int display_width(const std::string& s) {
    int w = 0;
    for (size_t i = 0; i < s.size(); ) {
        int n = utf8_char_bytes((unsigned char)s[i]);
        w += (n == 1) ? 1 : 2;
        i += n;
    }
    return w;
}

// 按显示宽度把字符串切成若干行（每行 <= maxw 列，绝不切断多字节字符）
static std::vector<std::string> wrap_utf8(const std::string& s, int maxw) {
    std::vector<std::string> lines;
    std::string cur;
    int curw = 0;
    for (size_t i = 0; i < s.size(); ) {
        int n = utf8_char_bytes((unsigned char)s[i]);
        int w = (n == 1) ? 1 : 2;
        if (curw + w > maxw && !cur.empty()) {
            lines.push_back(cur);
            cur.clear();
            curw = 0;
        }
        cur += s.substr(i, n);
        curw += w;
        i += n;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

// 打印一行，右侧按显示宽度补齐空格，保证边框对齐
static void box_line(const std::string& text, int width) {
    int w = display_width(text);
    printf("│ %s", text.c_str());
    for (int k = w; k < width; k++) printf(" ");
    printf(" │\n");
}

static void cmd_look(Player* player, const std::string& args) {
    (void)args;
    Room* room = room_get(player->current_room_id);
    if (!room) {
        printf("你身处一片虚无之中...\n");
        return;
    }

    printf("\n┌──────────────────────────────────┐\n");
    box_line(room->name, 32);
    printf("├──────────────────────────────────┤\n");

    // 描述按显示宽度自动换行（中文不截断）
    std::vector<std::string> lines = wrap_utf8(room->desc, 32);
    for (const auto& ln : lines) box_line(ln, 32);

    printf("├──────────────────────────────────┤\n");

    // 出口
    printf("│ 出口: ");
    bool has_exit = false;
    for (int d = 0; d < 6; d++) {
        if (room->exits[d].room_id > 0) {
            Room* dest = room_get(room->exits[d].room_id);
            if (dest) {
                if (has_exit) printf(", ");
                printf("%s(%s)", dir_cn_name(static_cast<Direction>(d)), dest->name.c_str());
                if (room->exits[d].locked)
                    printf("[需%s]", realm_name(room->exits[d].req_realm));
                has_exit = true;
            }
        }
    }
    if (!has_exit) printf("无");
    printf("\n");

    // NPC
    if (!room->npc_ids.empty()) {
        printf("│ NPC: ");
        for (int npc_id : room->npc_ids) {
            NPC* npc = npc_get(npc_id);
            if (npc && npc->is_alive) {
                printf("%s  ", npc->name.c_str());
            }
        }
        printf("\n");
    }

    // 道具
    if (!room->item_ids.empty()) {
        printf("│ 物品: ");
        for (int item_id : room->item_ids) {
            Item* it = item_get(item_id);
            if (it) printf("%s  ", it->name.c_str());
        }
        printf("\n");
    }

    printf("└──────────────────────────────────┘\n\n");
}

static bool parse_dir(const std::string& s, Direction& dir) {
    if      (s == "north" || s == "n") dir = Direction::NORTH;
    else if (s == "south" || s == "s") dir = Direction::SOUTH;
    else if (s == "east"  || s == "e") dir = Direction::EAST;
    else if (s == "west"  || s == "w") dir = Direction::WEST;
    else if (s == "up"    || s == "u") dir = Direction::UP;
    else if (s == "down"  || s == "d") dir = Direction::DOWN;
    else return false;
    return true;
}

static void do_move_dir(Player* player, Direction dir) {
    Room* room = room_get(player->current_room_id);
    if (!room) return;

    RoomExit& exit = room->exits[static_cast<int>(dir)];
    if (exit.room_id <= 0) {
        printf("那个方向没有路。\n");
        return;
    }
    if (exit.locked && static_cast<int>(player->realm) < static_cast<int>(exit.req_realm)) {
        printf("你的境界不足，无法通过。（需要%s以上）\n", realm_name(exit.req_realm));
        return;
    }

    player_move_to(player, exit.room_id);
    cmd_look(player, "");
}

static void cmd_move(Player* player, const std::string& args) {
    Direction dir;
    if (!parse_dir(args, dir)) {
        printf("方向: n/s/e/w/u/d 或 north/south/east/west/up/down\n");
        return;
    }
    do_move_dir(player, dir);
}

// 单键方向命令：直接输入 n/s/e/w/u/d 即可移动
static void cmd_go_n(Player* p, const std::string& a) { (void)a; do_move_dir(p, Direction::NORTH); }
static void cmd_go_s(Player* p, const std::string& a) { (void)a; do_move_dir(p, Direction::SOUTH); }
static void cmd_go_e(Player* p, const std::string& a) { (void)a; do_move_dir(p, Direction::EAST); }
static void cmd_go_w(Player* p, const std::string& a) { (void)a; do_move_dir(p, Direction::WEST); }
static void cmd_go_u(Player* p, const std::string& a) { (void)a; do_move_dir(p, Direction::UP); }
static void cmd_go_d(Player* p, const std::string& a) { (void)a; do_move_dir(p, Direction::DOWN); }

static void cmd_status(Player* player, const std::string& args) {
    (void)args;
    printf("\n╔══════════ 修仙者信息 ══════════╗\n");
    printf("║ 姓名: %-24s ║\n", player->name.c_str());
    printf("║ 灵根: %-24s ║\n", spirit_name(player->spirit_root));
    printf("║ 境界: %s%-22s ║\n",
           realm_name(player->realm), stage_name(player->stage));
    printf("║ 修为: %d / %-17d ║\n", player->exp, player->exp_to_next);
    printf("║ HP: %-4d  MP: %-4d  灵石: %-6d ║\n",
           player->hp, player->mp, player->gold);
    printf("║ 攻击: %-4d  防御: %-4d         ║\n",
           player->atk, player->def);
    printf("╠════════════════════════════════╣\n");

    // 背包
    printf("║ 背包 (%d/%d):\n", (int)player->inventory.size(), MAX_INV_SLOTS);
    for (size_t i = 0; i < player->inventory.size(); i++) {
        auto& it = player->inventory[i];
        printf("║  [%d] %s", (int)(i + 1), it.name.c_str());
        if (it.stackable && it.quantity > 1)
            printf(" x%d", it.quantity);
        printf("\n");
    }
    if (player->inventory.empty()) printf("║  (空)\n");

    printf("╠════════════════════════════════╣\n");

    // 技能
    printf("║ 技能 (%d/%d):\n", (int)player->skills.size(), MAX_SKILL_SLOTS);
    for (size_t i = 0; i < player->skills.size(); i++) {
        printf("║  %s Lv.%d\n", player->skills[i].name.c_str(), player->skills[i].level);
    }
    if (player->skills.empty()) printf("║  (无)\n");

    printf("╚════════════════════════════════╝\n\n");
}

static void cmd_save(Player* player, const std::string& args) {
    (void)args;
    save_player(player);
}

static void cmd_quit(Player* player, const std::string& args) {
    (void)args;
    (void)player;
    printf("再会，修仙之路漫漫，后会有期！\n");
    g_running = false;
}

static void cmd_breakthrough(Player* player, const std::string& args) {
    (void)args;
    player_try_breakthrough(player);
}

static void cmd_meditate(Player* player, const std::string& args) {
    (void)args;
    // 打坐恢复 + 获得少量修为
    int hp_gain = player->max_hp / 10 + 10;
    int mp_gain = player->max_mp / 5 + 5;
    int exp_gain = 10 + static_cast<int>(player->realm) * 5;

    player->hp = std::min(player->hp + hp_gain, player->max_hp);
    player->mp = std::min(player->mp + mp_gain, player->max_mp);
    player->exp += exp_gain;

    printf("你盘膝打坐，吸纳天地灵气...\n");
    printf("HP+%d  MP+%d  修为+%d\n", hp_gain, mp_gain, exp_gain);
    printf("当前修为: %d / %d\n", player->exp, player->exp_to_next);
}

static void cmd_inventory(Player* player, const std::string& args) {
    (void)args;
    printf("\n========== 背包 (%d/%d) ==========\n",
           (int)player->inventory.size(), MAX_INV_SLOTS);
    for (size_t i = 0; i < player->inventory.size(); i++) {
        auto& it = player->inventory[i];
        printf("  [%d] %-20s", (int)(i + 1), it.name.c_str());
        if (it.stackable && it.quantity > 1)
            printf(" x%d", it.quantity);
        printf(" - %s\n", it.desc.c_str());
    }
    if (player->inventory.empty()) printf("  (空)\n");
    printf("==================================\n\n");
}

static void cmd_get(Player* player, const std::string& args) {
    if (args.empty()) {
        printf("用法: get <物品名称或编号>\n");
        return;
    }

    Room* room = room_get(player->current_room_id);
    if (!room) return;

    // 尝试按数字索引
    int idx = 0;
    try { idx = std::stoi(args); } catch (...) { idx = 0; }

    if (idx > 0 && idx <= static_cast<int>(room->item_ids.size())) {
        Item* tmpl = item_get(room->item_ids[idx - 1]);
        if (tmpl) {
            Item it = *tmpl;
            it.quantity = 1;
            if (player_add_item(player, it)) {
                room_remove_item(player->current_room_id, tmpl->id);
            }
            return;
        }
    }

    // 按名称查找
    for (int item_id : room->item_ids) {
        Item* tmpl = item_get(item_id);
        if (tmpl && tmpl->name.find(args) != std::string::npos) {
            Item it = *tmpl;
            it.quantity = 1;
            if (player_add_item(player, it)) {
                room_remove_item(player->current_room_id, tmpl->id);
            }
            return;
        }
    }
    printf("这里没有 %s。\n", args.c_str());
}

static void cmd_drop(Player* player, const std::string& args) {
    if (args.empty()) {
        printf("用法: drop <背包编号>\n");
        return;
    }
    int idx = 0;
    try { idx = std::stoi(args); } catch (...) { idx = 0; }

    if (idx < 1 || idx > static_cast<int>(player->inventory.size())) {
        printf("无效的背包编号。\n");
        return;
    }
    Room* room = room_get(player->current_room_id);
    if (room) {
        room_add_item(player->current_room_id, player->inventory[idx - 1].id);
    }
    player_remove_item(player, idx - 1);
}

static void cmd_use(Player* player, const std::string& args) {
    if (args.empty()) {
        printf("用法: use <背包编号>\n");
        return;
    }
    int idx = 0;
    try { idx = std::stoi(args); } catch (...) { idx = 0; }

    if (idx < 1 || idx > static_cast<int>(player->inventory.size())) {
        printf("无效的背包编号。\n");
        return;
    }
    player_use_item(player, idx - 1);
}

// ===== 创建角色 =====

static SpiritRoot choose_spirit() {
    printf("\n请选择你的灵根:\n");
    printf("  [1] 金灵根 - 攻击见长\n");
    printf("  [2] 木灵根 - 生命见长\n");
    printf("  [3] 水灵根 - 灵力见长\n");
    printf("  [4] 火灵根 - 攻击见长\n");
    printf("  [5] 土灵根 - 防御见长\n");
    printf("  [6] 风灵根 - 变异灵根(稀有)\n");
    printf("  [7] 雷灵根 - 变异灵根(稀有)\n");
    printf("  [8] 冰灵根 - 变异灵根(稀有)\n");

    int choice = 0;
    while (choice < 1 || choice >= static_cast<int>(SpiritRoot::MAX)) {
        printf("请输入选择 (1-8): ");
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            choice = 0;
        }
        getchar();
    }
    return static_cast<SpiritRoot>(choice);
}

static Player* create_character() {
    std::string name;
    std::string password;

    printf("\n====== 创建修仙者 ======\n");
    printf("请输入道号: ");
    std::getline(std::cin, name);

    if (name.empty()) {
        printf("道号不能为空。\n");
        return nullptr;
    }

    if (save_player_exists(name)) {
        printf("道号已存在，请使用 load 命令加载存档。\n");
        return nullptr;
    }

    printf("请输入密码: ");
    std::getline(std::cin, password);

    SpiritRoot root = choose_spirit();

    Player* p = player_create(1, name, password);
    if (!p) return nullptr;

    p->spirit_root = root;

    // 根据灵根调整初始属性
    switch (root) {
    case SpiritRoot::GOLD:    p->atk += 5; break;
    case SpiritRoot::WOOD:    p->max_hp += 30; p->hp += 30; break;
    case SpiritRoot::WATER:   p->max_mp += 20; p->mp += 20; break;
    case SpiritRoot::FIRE:    p->atk += 5; break;
    case SpiritRoot::EARTH:   p->def += 5; break;
    case SpiritRoot::WIND:    p->atk += 8; p->max_hp += 20; p->hp += 20; break;
    case SpiritRoot::THUNDER: p->atk += 10; p->max_mp += 10; p->mp += 10; break;
    case SpiritRoot::ICE:     p->def += 5; p->max_mp += 15; p->mp += 15; break;
    default: break;
    }

    printf("\n创建成功！\n");
    printf("道号: %s  灵根: %s\n", p->name.c_str(), spirit_name(p->spirit_root));
    printf("HP: %d  MP: %d  攻击: %d  防御: %d\n",
           p->max_hp, p->max_mp, p->atk, p->def);

    return p;
}

static void cmd_load(Player** player_ptr, const std::string& args) {
    if (args.empty()) {
        save_list_players();
        printf("用法: load <道号>\n");
        return;
    }

    if (!save_player_exists(args)) {
        printf("存档不存在: %s\n", args.c_str());
        return;
    }

    // 验证密码
    std::string password;
    printf("请输入密码: ");
    std::getline(std::cin, password);

    Player* p = load_player(args);
    if (p && p->password == password) {
        if (*player_ptr) player_destroy(*player_ptr);
        *player_ptr = p;
        g_player = p;
        printf("登录成功！\n");
    } else {
        printf("密码错误！\n");
        if (p) player_destroy(p);
    }
}

// ===== 初始化世界数据 =====

static void init_game_data() {
    // ---- 初始化道具模板 ----
    item_create(201, "疗伤丹", "恢复100点生命值的一品丹药",
                ItemType::PILL, 50, 100, 0, 0, 0, 0, true);
    item_create(202, "回灵丹", "恢复50点灵力的丹药",
                ItemType::PILL, 40, 0, 50, 0, 0, 0, true);
    item_create(203, "聚气丹", "服用后获得大量修为",
                ItemType::PILL, 200, 0, 0, 0, 0, 200, true);
    item_create(204, "铁剑", "一把普通的铁剑，略有锋芒",
                ItemType::WEAPON, 100, 0, 0, 15, 0, 0, false);
    item_create(205, "布甲", "粗布缝制的护甲",
                ItemType::ARMOR, 80, 20, 0, 0, 10, 0, false);
    item_create(206, "筑基功法", "记载了筑基期修炼法门的秘籍",
                ItemType::MANUAL, 300, 0, 0, 0, 0, 500, false);
    item_create(207, "灵草", "一株散发着灵气的药草",
                ItemType::MATERIAL, 30, 0, 0, 0, 0, 0, true);
    item_create(208, "妖丹", "妖兽体内凝结的精华",
                ItemType::MATERIAL, 100, 0, 0, 0, 0, 50, true);
    item_create(209, "灵石袋", "装有一些灵石的小袋子",
                ItemType::MISC, 500, 0, 0, 0, 0, 0, false);
    item_create(210, "筑基丹", "大幅提升突破筑基期成功率的丹药",
                ItemType::PILL, 500, 200, 100, 0, 0, 1000, false);
    item_create(211, "灵剑", "蕴含灵力的宝剑",
                ItemType::WEAPON, 500, 0, 0, 30, 0, 0, false);
    item_create(212, "灵甲", "以灵力编织的护甲",
                ItemType::ARMOR, 400, 50, 20, 0, 20, 0, false);
    item_create(213, "金丹功法", "记载了金丹大道的高深秘籍",
                ItemType::MANUAL, 1000, 0, 0, 0, 0, 2000, false);

    // 注：NPC 模板与房间/出口/境界锁/NPC摆放/机缘触发，已全部迁移到
    // modules/world/world.cpp（成员C 的世界地图模块）中实现，
    // 本处只保留道具模板（成员D 负责，后续按《游戏基础设定》丹药/法器表完善）。

    printf("[数据] 游戏世界初始化完成\n");
}

// ===== 主循环 =====

static void game_loop() {
    std::string input;

    while (g_running && g_player) {
        // 模块tick
        module_tick_all(g_player);

        // 提示符
        printf("\n[%s%s] > ",
               realm_name(g_player->realm),
               stage_name(g_player->stage));

        if (!std::getline(std::cin, input)) break;

        if (input.empty()) continue;

        cmd_execute(g_player, input);
    }
}

// ===== 注册内置命令 =====

static void register_builtin_commands() {
    cmd_register("help",     {},       cmd_help,         "显示所有命令");
    cmd_register("look",     {"l"},    cmd_look,         "查看当前房间");
    cmd_register("move",     {},             cmd_move, "移动 (move <n/s/e/w/u/d>)");
    cmd_register("north",    {"n"},          cmd_go_n, "向北移动");
    cmd_register("south",    {"s"},          cmd_go_s, "向南移动");
    cmd_register("east",     {"e"},          cmd_go_e, "向东移动");
    cmd_register("west",     {"w"},          cmd_go_w, "向西移动");
    cmd_register("up",       {"u"},          cmd_go_u, "向上移动");
    cmd_register("down",     {"d"},          cmd_go_d, "向下移动");
    cmd_register("status",   {"stat", "me"}, cmd_status, "查看自身状态");
    cmd_register("save",     {},       cmd_save,         "保存游戏");
    cmd_register("quit",     {"exit"}, cmd_quit,         "退出游戏");
    cmd_register("load",     {},       nullptr,          "加载存档 (load <道号>)");
    cmd_register("breakthrough", {"bt"}, cmd_breakthrough, "尝试突破境界");
    cmd_register("meditate", {"rest", "train"}, cmd_meditate, "打坐修炼");
    cmd_register("inventory", {"i", "bag"}, cmd_inventory, "查看背包");
    cmd_register("get",      {"pick"}, cmd_get,          "拾取物品 (get <名称/编号>)");
    cmd_register("drop",     {},       cmd_drop,         "丢弃物品 (drop <背包编号>)");
    cmd_register("use",      {},       cmd_use,          "使用物品 (use <背包编号>)");
}

// ===== 入口 =====

int main() {
#ifdef _WIN32
    // 让控制台正确显示 UTF-8 中文
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║       修 仙 传 奇  MUD               ║\n");
    printf("║       XiuXian Legend                  ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    // 初始化各子系统
    cmd_init();
    event_init();
    npc_init();
    item_init();
    world_init();

    // 初始化游戏数据
    init_game_data();

    // 注册内置命令
    register_builtin_commands();

    // 注册模块
    module_register(cultivate_module);
    module_register(world_module);
    module_register(bag_module);
    module_register(quest_module);

    // 初始化模块
    module_init_all();

    // 主菜单
    printf("欢迎来到修仙世界！\n");
    printf("  [1] 创建角色\n");
    printf("  [2] 读取存档\n");
    printf("  [3] 查看存档列表\n");
    printf("  [4] 退出\n");

    int choice = 0;
    while (choice < 1 || choice > 4) {
        printf("请选择: ");
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            choice = 0;
        }
        getchar(); // 吃掉换行
    }

    switch (choice) {
    case 1:
        g_player = create_character();
        if (!g_player) {
            printf("创建角色失败，程序退出。\n");
            goto cleanup;
        }
        break;
    case 2: {
        std::string name;
        printf("请输入道号: ");
        std::getline(std::cin, name);
        cmd_load(&g_player, name);
        if (!g_player) {
            printf("登录失败，程序退出。\n");
            goto cleanup;
        }
        break;
    }
    case 3:
        save_list_players();
        printf("按回车键退出...");
        getchar();
        goto cleanup;
    case 4:
        printf("再会！\n");
        goto cleanup;
    }

    // 进入游戏世界
    printf("\n你睁开双眼，发现自己身处一个陌生的世界...\n");
    {
        Room* start_room = room_get(g_player->current_room_id);
        if (start_room) {
            printf("当前所在: %s\n", start_room->name.c_str());
        }
    }

    // 主循环
    game_loop();

cleanup:
    // 清理
    if (g_player) {
        printf("是否保存游戏？(y/n): ");
        char c = (char)getchar();
        if (c == 'y' || c == 'Y') save_player(g_player);
        player_destroy(g_player);
    }

    module_cleanup_all();
    event_cleanup();

    printf("感谢游玩！\n");
    return 0;
}