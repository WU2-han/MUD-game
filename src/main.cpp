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

// 各房间可执行的主要动作提示（仅功能性指令，不剧透剧情）
static const char* room_action_hints(int room_id) {
    switch (room_id) {
        case 1: return "修炼(train) 休息(rest) 睡觉(sleep) 回家(home)";
        case 2: return "睡觉(sleep)";
        case 3: return "修炼(train)";
        case 4: return "商店(shop) 购买(buy) 出售(sell)";
        case 5: return "淬体(cuti) 考核(kaohe) 战斗(fight)";
        case 6: return "炼丹(alchemy) 炼器(forge) 画符(talisman) 合成(combine)";
        case 7: return "查看灵兽(beast) 契约(contract)";
        case 8: return "月例(monthly)";
        case 9: case 10: case 11: case 12:
        case 18: case 20: case 22: return "战斗(fight)";
        case 13: case 14: case 15: case 16: case 17:
        case 19: case 21: case 23: case 24: return "剧情(story)";
        default: return nullptr;
    }
}

static void cmd_look(Player* player, const std::string& args) {
    (void)args;
    Room* room = room_get(player->current_room_id);
    if (!room) {
        printf("你身处一片虚无之中...\n");
        return;
    }

    const int W = 34;   // 内容区显示宽度
    auto hr = [&](const char* left, const char* right, int w) {
        printf("%s%s%s\n", left, box_rep("─", w).c_str(), right);
    };
    auto field = [&](const std::string& label, const std::string& value) {
        std::string line = label + value;
        for (auto& ln : wrap_text_by_width(line, W)) {
            printf("│ %s │\n", pad_to_width(ln, W).c_str());
        }
    };

    printf("\n");
    hr("┌", "┐", W + 2);
    printf("│ %s │\n", pad_to_width(room->name, W).c_str());
    hr("├", "┤", W + 2);
    for (auto& ln : wrap_text_by_width(room->desc, W)) {
        printf("│ %s │\n", pad_to_width(ln, W).c_str());
    }
    hr("├", "┤", W + 2);

    // 出口（带方向键）
    {
        std::string exits;
        bool has_exit = false;
        for (int d = 0; d < 6; d++) {
            if (room->exits[d].room_id <= 0) continue;
            Room* dest = room_get(room->exits[d].room_id);
            if (!dest) continue;
            if (has_exit) exits += "  ";
            exits += std::string(dir_cn_name(static_cast<Direction>(d))) + "["
                   + dir_key_name(static_cast<Direction>(d)) + "] " + dest->name;
            if (room->exits[d].locked)
                exits += std::string("[需") + realm_name(room->exits[d].req_realm) + "]";
            has_exit = true;
        }
        field("出口: ", has_exit ? exits : "无");
    }

    // NPC：分为「可对话」与「可攻击」两栏（三改意见：look 页面需明确区分）
    {
        std::string talkable, attackable;
        for (int npc_id : room->npc_ids) {
            NPC* npc = npc_get(npc_id);
            if (!npc || !npc->is_alive) continue;
            std::string& target = (npc->type == NPCType::MONSTER) ? attackable : talkable;
            if (!target.empty()) target += "  ";
            target += npc->name;
        }
        if (!talkable.empty())   field("可对话: ", talkable);
        if (!attackable.empty()) field("可攻击: ", attackable);
    }

    // 道具
    if (!room->item_ids.empty()) {
        std::string items;
        for (int item_id : room->item_ids) {
            Item* it = item_get(item_id);
            if (it) {
                if (!items.empty()) items += "  ";
                items += it->name;
            }
        }
        if (!items.empty()) field("物品: ", items);
    }

    // 此处可执行的操作
    const char* hints = room_action_hints(player->current_room_id);
    if (hints) {
        hr("├", "┤", W + 2);
        field("你在此可: ", hints);
    }

    hr("└", "┘", W + 2);
    printf("\n");
}

static bool parse_dir(const std::string& s, Direction& dir) {
    // WASD 键位：W↑北  S↓南  A←西  D→东（同时保留全称 north/south/east/west/up/down）
    if      (s == "north" || s == "w") dir = Direction::NORTH;
    else if (s == "south" || s == "s") dir = Direction::SOUTH;
    else if (s == "west"  || s == "a") dir = Direction::WEST;
    else if (s == "east"  || s == "d") dir = Direction::EAST;
    else if (s == "up"    || s == "u") dir = Direction::UP;
    else if (s == "down")              dir = Direction::DOWN;
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
        printf("方向: w/s/a/d/u/down 或 north/south/east/west/up/down\n");
        return;
    }
    do_move_dir(player, dir);
}

// 单键方向命令（WASD：w/s/a/d 对应 北/南/西/东，u 上楼，down 下楼）
static void cmd_go_north(Player* p, const std::string& a) { (void)a; do_move_dir(p, Direction::NORTH); }
static void cmd_go_south(Player* p, const std::string& a) { (void)a; do_move_dir(p, Direction::SOUTH); }
static void cmd_go_east(Player* p, const std::string& a)  { (void)a; do_move_dir(p, Direction::EAST); }
static void cmd_go_west(Player* p, const std::string& a)  { (void)a; do_move_dir(p, Direction::WEST); }
static void cmd_go_up(Player* p, const std::string& a)    { (void)a; do_move_dir(p, Direction::UP); }
static void cmd_go_down(Player* p, const std::string& a)  { (void)a; do_move_dir(p, Direction::DOWN); }

static void cmd_status(Player* player, const std::string& args) {
    (void)args;
    const int W = 34;   // 内容区显示宽度
    auto line = [&](const std::string& s) { printf("║ %s ║\n", pad_to_width(s, W).c_str()); };
    auto bar = [&]() { printf("╠%s╣\n", box_rep("═", W + 2).c_str()); };

    printf("\n╔%s╗\n", box_rep("═", W + 2).c_str());
    line("修仙者信息");
    bar();
    line("姓名: " + player->name);
    line("灵根: " + std::string(spirit_name(player->spirit_root)));
    line("境界: " + std::string(realm_name(player->realm)) + stage_name(player->stage));
    line("修为: " + std::to_string(player->exp) + " / " + std::to_string(player->exp_to_next));
    line("HP: " + std::to_string(player->hp) + "  MP: " + std::to_string(player->mp) +
         "  灵石: " + std::to_string(player->gold));
    line("攻击: " + std::to_string(player->atk) + "  防御: " + std::to_string(player->def));
    line("体质: " + std::to_string(player->con) + "  灵力: " + std::to_string(player->spi) +
         "  悟性: " + std::to_string(player->wu));
    line("速度: " + std::to_string(player->spd) + "  精力: " + std::to_string(player->stam) +
         "/" + std::to_string(player->max_stam));
    line("炼丹: " + std::to_string(player->prof_alchemy) +
         "  炼器: " + std::to_string(player->prof_forge));
    line("画符: " + std::to_string(player->prof_talisman) +
         "  御兽: " + std::to_string(player->prof_beast) + "  第" + std::to_string(player->day) + "天");
    line("宗门地位: " + std::string(sect_rank_name_idx(player->sect_rank)));
    if (!player->title.empty()) line("称号: " + player->title);
    if (player->prestige > 0)   line("威望: " + std::to_string(player->prestige));
    if (player->beast_id > 0)
        line("灵兽: 品级" + std::to_string(static_cast<int>(player->beast_grade)) +
             "  攻+" + std::to_string(player->beast_atk));
    bar();

    line("背包 (" + std::to_string(player->inventory.size()) + "/" +
         std::to_string(MAX_INV_SLOTS) + ")");
    for (size_t i = 0; i < player->inventory.size(); i++) {
        auto& it = player->inventory[i];
        std::string s = "[" + std::to_string(i + 1) + "] " + it.name;
        if (it.stackable && it.quantity > 1) s += " x" + std::to_string(it.quantity);
        line(s);
    }
    if (player->inventory.empty()) line("(空)");

    bar();

    line("技能 (" + std::to_string(player->skills.size()) + "/" +
         std::to_string(MAX_SKILL_SLOTS) + ")");
    for (auto& sk : player->skills)
        line(" " + sk.name + " Lv." + std::to_string(sk.level));
    if (player->skills.empty()) line("(无)");

    printf("╚%s╝\n\n", box_rep("═", W + 2).c_str());
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

static void cmd_home(Player* player, const std::string& args) {
    (void)args;
    if (player->in_combat) {
        printf("你正在战斗中，无法脱身回府！\n");
        return;
    }
    if (player->current_room_id == 1) {
        printf("你已经在个人主页了。\n");
        return;
    }
    // 快捷键传送回个人主页（三改意见）
    player_move_to(player, 1);
    cmd_look(player, "");
}

static void cmd_train(Player* player, const std::string& args) {
    (void)args;
    // 修炼位置限制：只能在传功讲堂(3)或个人主页(1)打坐
    if (player->current_room_id != 1 && player->current_room_id != 3) {
        printf("此处不宜修炼，只有传功讲堂或个人主页才能安心打坐。\n");
        return;
    }
    // 打坐修炼消耗精力（每日精力有限，需休息/养神丹补充，防无限爆肝）
    if (player->stam < 10) {
        printf("精力不足（需10），难以入定。可回家休息(rest)、服用养神丹恢复。\n");
        return;
    }
    player->stam -= 10;
    // 修炼恢复 + 修为固定 +20（三改意见）
    int hp_gain = player->max_hp / 10 + 10;
    int mp_gain = player->max_mp / 5 + 5;
    int exp_gain = 20;

    player->hp = std::min(player->hp + hp_gain, player->max_hp);
    player->mp = std::min(player->mp + mp_gain, player->max_mp);
    player->exp += exp_gain;

    printf("你盘膝打坐，吸纳天地灵气...\n");
    printf("HP+%d  MP+%d  修为+%d  精力-10\n", hp_gain, mp_gain, exp_gain);
    printf("当前修为: %d / %d  精力: %d/%d\n",
           player->exp, player->exp_to_next, player->stam, player->max_stam);
}

// ===== 常驻功能NPC的互动提示 =====
static const char* npc_interact_hint(const NPC* npc) {
    if (!npc) return nullptr;
    const std::string& n = npc->name;
    if (n == "钱掌柜") return "可 shop 查看商店、buy 购买、sell 出售。";
    if (n == "墨长老") return "可在此 train 打坐修炼。";
    if (n == "铁武师") return "可 cuti 淬体提升体质、kaohe 发起晋升考核。";
    if (n == "苏玄")   return "可 alchemy 炼丹、forge 炼器、talisman 画符。";
    if (n == "老猎户") return "可 beast 查看灵兽、contract 契约灵兽。";
    if (n == "李执事") return "可 monthly 领取月例。";
    if (n == "林婉儿") return "她擅长炼丹，常免费炼制低阶丹药。";
    return nullptr;
}

// 主线前铺垫台词（欲抑先扬）：达到亲传弟子、且尚未推进到揭穿墨阳子的剧情时触发
static const char* npc_foreshadow_line(const NPC* npc) {
    if (!npc) return nullptr;
    const std::string& n = npc->name;
    if (n == "墨长老")
        return "当年老朽修为滞涩，是墨阳宗主无偿赠予我《纯阳悟道札记》，才得以突破瓶颈。此人心怀坦荡，毫无门户之见，实乃正道表率。";
    if (n == "铁武师")
        return "俺当年在妖兽山脉被三阶妖兽围杀，眼看就要没命，是墨阳宗主路过一剑斩了妖兽，还扔给俺一瓶上品养神丹。人家堂堂一宗之主，对俺个外门弟子都这么仗义！";
    if (n == "苏玄")
        return "上月墨阳宗主还遣人送来玄阳宗独家的《聚火丹方》，与我互补丹道心得。他与宗主是八拜之交，两宗向来亲如一家。";
    if (n == "钱掌柜")
        return "玄阳宗的商队最是公道，从不压价，墨阳宗主还特意下令，青云宗的货物一律加价一成收。要说正道里最讲情义的，非他莫属。";
    if (n == "凌沧渊")
        return "你墨阳师伯是为兄一生挚友，宅心仁厚，修为深不可测。日后若为师不在了，你遇着难处，大可去玄阳宗寻他。";
    return nullptr;
}

// ===== 新手教程（奶龙）=====
static void print_tutorial(Player* player) {
    (void)player;
    printf("\n══════════ 奶龙 · 新手教程 ══════════\n");
    printf("奶龙扑棱着翅膀，热心地给你讲起修仙入门：\n\n");

    printf("【基础指令】\n");
    printf("  help        查看所有命令\n");
    printf("  look / l    查看当前房间（出口、可对话/可攻击NPC、你能做的事）\n");
    printf("  status      查看自身状态\n");
    printf("  home / h    直接传送回个人主页（战斗中不可用）\n");
    printf("  save        保存游戏（记得经常存档！）\n\n");

    printf("【修炼与突破】\n");
    printf("  train       打坐修炼，修为+20（只能在传功讲堂或个人主页）\n");
    printf("  breakthrough/bt  修为足够时突破境界\n");
    printf("  精力不够时：先 rest 休息，再 sleep 进入下一天恢复\n\n");

    printf("【主线剧情】\n");
    printf("  story       查看当前主线进度与触发条件\n");
    printf("  达到宗门地位「亲传弟子」后，输入 story 即可开启主线\n\n");

    printf("【各境界能做什么】\n");
    printf("  炼气期  外门弟子：妖兽山脉外围历练、学习四艺\n");
    printf("  筑基期  可到演武场 kaohe 参加内门考核（挑战赵青峰）\n");
    printf("  金丹期  可到演武场 kaohe 参加亲传考核（长老会审），开启主线\n");
    printf("  元婴期  内门执事，可向长老呈报边境战事\n");
    printf("  化神期  核心长老 / 炼虚期 峰主 / 合体期 宗主候选\n");
    printf("  大乘期  宗主 / 渡劫飞升 太上长老\n\n");

    printf("【NPC 与战斗】\n");
    printf("  talk <名字> 与当前房间的NPC对话\n");
    printf("  fight <名字> 与妖兽战斗\n");
    printf("  可攻击NPC（妖兽，出没于妖兽山脉）：\n");
    printf("    尖刺豪猪、腐爪灰狼 / 雾影毒蟒、岩甲巨熊 /\n");
    printf("    烈焰魔猿 / 幻海魔蛟\n");
    printf("  不可攻击（功能NPC，只可 talk）：钱掌柜、墨长老、\n");
    printf("    铁武师、苏玄、老猎户、李执事\n\n");
    printf("祝你在青云宗修行顺利！\n");
    printf("══════════════════════════════════════\n\n");
}

static void cmd_talk(Player* player, const std::string& args) {
    if (args.empty()) {
        printf("用法: talk <NPC名字>（用 look 查看当前房间的NPC）\n");
        return;
    }
    Room* room = room_get(player->current_room_id);
    if (!room) return;

    // 只能与当前房间的NPC互动
    NPC* target = nullptr;
    for (int npc_id : room->npc_ids) {
        NPC* npc = npc_get(npc_id);
        if (npc && npc->is_alive && npc->name.find(args) != std::string::npos) {
            target = npc;
            break;
        }
    }
    if (!target) {
        printf("这里没有叫「%s」的人，只能和当前房间的NPC对话。\n", args.c_str());
        return;
    }

    if (target->name == "奶龙") {
        print_tutorial(player);
        return;
    }

    printf("\n【%s】%s\n", target->name.c_str(), target->desc.c_str());
    // 主线前铺垫墨阳子（欲抑先扬）
    const char* fore = (player->story_phase < 7)
                       ? npc_foreshadow_line(target) : nullptr;
    if (fore) {
        printf("%s道：「%s」\n", target->name.c_str(), fore);
    }
    const char* hint = npc_interact_hint(target);
    if (hint) {
        printf("它对你说道：「%s」\n", hint);
    } else if (target->type == NPCType::MONSTER) {
        printf("它恶狠狠地盯着你，看起来可以 fight 与之战斗。\n");
    }
    printf("\n");
}

static void cmd_inventory(Player* player, const std::string& args) {
    (void)args;
    const int W = 40;   // 内容区显示宽度
    auto hr = [&](const char* left, const char* right, int w) {
        printf("%s%s%s\n", left, box_rep("─", w).c_str(), right);
    };
    auto line = [&](const std::string& s) { printf("│ %s │\n", pad_to_width(s, W).c_str()); };

    printf("\n");
    hr("┌", "┐", W + 2);
    line("背包 (" + std::to_string(player->inventory.size()) + "/" +
         std::to_string(MAX_INV_SLOTS) + ")");
    hr("├", "┤", W + 2);
    if (player->inventory.empty()) {
        line("(空)");
    } else {
        for (size_t i = 0; i < player->inventory.size(); i++) {
            auto& it = player->inventory[i];
            std::string s = "[" + std::to_string(i + 1) + "] " + it.name;
            if (it.stackable && it.quantity > 1) s += " x" + std::to_string(it.quantity);
            if (!it.desc.empty()) s += " - " + it.desc;
            for (auto& ln : wrap_text_by_width(s, W)) line(ln);
        }
    }
    hr("└", "┘", W + 2);
    printf("\n");
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

static void init_item_configure(Item* it, int hp_bonus, int mp_bonus, int atk_bonus, int def_bonus,
                                int exp_bonus, int con_bonus, int spi_bonus, int wu_bonus,
                                int spd_bonus, int stam_bonus, int prof_bonus, bool is_artifact) {
    item_configure(it, hp_bonus, mp_bonus, atk_bonus, def_bonus,
                   exp_bonus, con_bonus, spi_bonus, wu_bonus,
                   spd_bonus, stam_bonus, prof_bonus, is_artifact);
}

static void init_game_data() {
    // ---- 初始化道具模板（对齐《修仙大世界MUD》V2.0 丹药/法器/材料表）----
    Item* it;

    // [通用恢复]
    it = item_create(201, "疗伤丹", "恢复100点生命值的一品丹药",
                     ItemType::PILL, 50, 100, 0, 0, 0, 0, true);
    it = item_create(202, "回灵丹", "恢复50点灵力的丹药",
                     ItemType::PILL, 40, 0, 50, 0, 0, 0, true);
    it = item_create(207, "灵草", "一株散发着灵气的药草",
                     ItemType::MATERIAL, 30, 0, 0, 0, 0, 0, true);
    it = item_create(208, "妖丹", "妖兽体内凝结的精华",
                     ItemType::MATERIAL, 100, 0, 0, 0, 0, 50, true);
    it = item_create(209, "灵石袋", "装有一些灵石的小袋子",
                     ItemType::MISC, 500, 0, 0, 0, 0, 0, false);

    // [兽核材料]
    it = item_create(220, "兽皮", "妖兽身上剥下的皮，可用于炼器",
                     ItemType::MATERIAL, 15, 0, 0, 0, 0, 0, true);
    it = item_create(221, "狼爪", "腐爪灰狼锋利的爪子",
                     ItemType::MATERIAL, 20, 0, 0, 0, 0, 0, true);
    it = item_create(222, "毒牙", "雾影毒蟒的剧毒獠牙",
                     ItemType::MATERIAL, 25, 0, 0, 0, 0, 0, true);
    it = item_create(223, "熊皮", "岩甲巨熊坚韧的皮毛",
                     ItemType::MATERIAL, 35, 0, 0, 0, 0, 0, true);
    it = item_create(224, "兽核", "妖兽体内凝聚的核心",
                     ItemType::MATERIAL, 60, 0, 0, 0, 0, 50, true);
    it = item_create(225, "高阶兽核", "高阶妖兽的强大兽核",
                     ItemType::MATERIAL, 150, 0, 0, 0, 0, 120, true);
    it = item_create(226, "圣兽兽核", "圣兽级妖兽的内丹",
                     ItemType::MATERIAL, 500, 0, 0, 0, 0, 400, true);
    it = item_create(227, "药渣", "炼丹炸炉后残留的药渣，可在藏宝阁出售或于丹房合成止血散",
                     ItemType::MATERIAL, 10, 0, 0, 0, 0, 0, true);
    it = item_create_adv(228, "止血散", "低级药散，恢复100点气血", ItemType::PILL, 60, PillGrade::LOW, true);
    init_item_configure(it, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false);

    // ---- 四艺材料 / 产出（铁渣、精铁矿石、符纸、符箓）----
    it = item_create(229, "铁渣", "炼器/画符失败残留的残渣，可在藏宝阁出售",
                      ItemType::MATERIAL, 10, 0, 0, 0, 0, 0, true);
    it = item_create(230, "精铁矿石", "千锤百炼的精铁矿石，炼器的核心材料",
                      ItemType::MATERIAL, 20, 0, 0, 0, 0, 0, true);
    it = item_create(231, "符纸", "朱砂符纸，画符的必备材料",
                      ItemType::MATERIAL, 15, 0, 0, 0, 0, 0, true);
    it = item_create_adv(232, "破障符", "战斗威力符，使用后当日攻击+30", ItemType::PILL, 120, PillGrade::NONE, true);
    init_item_configure(it, 0, 0, 30, 0, 0, 0, 0, 0, 0, 0, 0, false);
    it = item_create_adv(233, "御灵符", "护体灵符，使用后当日被攻击减伤30", ItemType::PILL, 120, PillGrade::NONE, true);
    init_item_configure(it, 0, 0, 0, 30, 0, 0, 0, 0, 0, 0, 0, false);

    // [淬体丹：体质+血量] 下/中/上/极
    it = item_create_adv(301, "下品淬体丹", "淬炼肉身，体质+2、血量上限+50", ItemType::PILL, 30, PillGrade::LOW, true);
    init_item_configure(it, 50, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, false);
    it = item_create_adv(302, "中品淬体丹", "淬炼肉身，体质+4、血量上限+100", ItemType::PILL, 60, PillGrade::MID, true);
    init_item_configure(it, 100, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, false);
    it = item_create_adv(303, "上品淬体丹", "淬炼肉身，体质+6、血量上限+150", ItemType::PILL, 120, PillGrade::HIGH, true);
    init_item_configure(it, 150, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, false);
    it = item_create_adv(304, "极品淬体丹", "淬炼肉身，体质+8、血量上限+200", ItemType::PILL, 240, PillGrade::TOP, true);
    init_item_configure(it, 200, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, false);

    // [聚气丹：恢复灵力%] 下/中/上/极
    it = item_create_adv(305, "下品聚气丹", "恢复40%灵力", ItemType::PILL, 25, PillGrade::LOW, true);
    init_item_configure(it, 0, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, false);
    it = item_create_adv(306, "中品聚气丹", "恢复60%灵力", ItemType::PILL, 50, PillGrade::MID, true);
    init_item_configure(it, 0, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, false);
    it = item_create_adv(307, "上品聚气丹", "恢复80%灵力", ItemType::PILL, 100, PillGrade::HIGH, true);
    init_item_configure(it, 0, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, false);
    it = item_create_adv(308, "极品聚气丹", "恢复100%灵力", ItemType::PILL, 200, PillGrade::TOP, true);
    init_item_configure(it, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, false);

    // [养神丹：恢复精力%] 下/中/上/极
    it = item_create_adv(309, "下品养神丹", "恢复40%精力", ItemType::PILL, 25, PillGrade::LOW, true);
    init_item_configure(it, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, 0, false);
    it = item_create_adv(310, "中品养神丹", "恢复60%精力", ItemType::PILL, 50, PillGrade::MID, true);
    init_item_configure(it, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60, 0, false);
    it = item_create_adv(311, "上品养神丹", "恢复80%精力", ItemType::PILL, 100, PillGrade::HIGH, true);
    init_item_configure(it, 0, 0, 0, 0, 0, 0, 0, 0, 0, 80, 0, false);
    it = item_create_adv(312, "极品养神丹", "恢复100%精力", ItemType::PILL, 200, PillGrade::TOP, true);
    init_item_configure(it, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100, 0, false);

    // [启悟丹：悟性+速度%] 下/中/上/极
    it = item_create_adv(313, "下品启悟丹", "悟性+2、速度+5%", ItemType::PILL, 40, PillGrade::LOW, true);
    init_item_configure(it, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, false);
    it = item_create_adv(314, "中品启悟丹", "悟性+4、速度+8%", ItemType::PILL, 80, PillGrade::MID, true);
    init_item_configure(it, 0, 0, 0, 0, 0, 0, 0, 4, 8, 0, 0, false);
    it = item_create_adv(315, "上品启悟丹", "悟性+6、速度+12%", ItemType::PILL, 160, PillGrade::HIGH, true);
    init_item_configure(it, 0, 0, 0, 0, 0, 0, 0, 6, 12, 0, 0, false);
    it = item_create_adv(316, "极品启悟丹", "悟性+8、速度+16%", ItemType::PILL, 320, PillGrade::TOP, true);
    init_item_configure(it, 0, 0, 0, 0, 0, 0, 0, 8, 16, 0, 0, false);

    // [培元丹：修为] 下/中/上/极
    it = item_create_adv(317, "下品培元丹", "修为+300", ItemType::PILL, 50, PillGrade::LOW, true);
    init_item_configure(it, 0, 0, 0, 0, 300, 0, 0, 0, 0, 0, 0, false);
    it = item_create_adv(318, "中品培元丹", "修为+600", ItemType::PILL, 100, PillGrade::MID, true);
    init_item_configure(it, 0, 0, 0, 0, 600, 0, 0, 0, 0, 0, 0, false);
    it = item_create_adv(319, "上品培元丹", "修为+1000", ItemType::PILL, 200, PillGrade::HIGH, true);
    init_item_configure(it, 0, 0, 0, 0, 1000, 0, 0, 0, 0, 0, 0, false);
    it = item_create_adv(320, "极品培元丹", "修为+1500", ItemType::PILL, 400, PillGrade::TOP, true);
    init_item_configure(it, 0, 0, 0, 0, 1500, 0, 0, 0, 0, 0, 0, false);

    // [精工丹：熟练度] 下/中/上/极
    it = item_create_adv(321, "下品精工丹", "四艺熟练度各+80", ItemType::PILL, 35, PillGrade::LOW, true);
    init_item_configure(it, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 80, false);
    it = item_create_adv(322, "中品精工丹", "四艺熟练度各+160", ItemType::PILL, 70, PillGrade::MID, true);
    init_item_configure(it, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 160, false);
    it = item_create_adv(323, "上品精工丹", "四艺熟练度各+280", ItemType::PILL, 140, PillGrade::HIGH, true);
    init_item_configure(it, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 280, false);
    it = item_create_adv(324, "极品精工丹", "四艺熟练度各+420", ItemType::PILL, 280, PillGrade::TOP, true);
    init_item_configure(it, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 420, false);

    // [法器（藏宝阁售价表）]
    it = item_create_adv(401, "青锋灵剑", "低阶法器，攻击+22", ItemType::WEAPON, 800, PillGrade::NONE, false);
    init_item_configure(it, 0, 0, 22, 0, 0, 0, 0, 0, 0, 0, 0, true);
    it = item_create_adv(402, "玄铁裂爪", "低阶法器，攻击+28", ItemType::WEAPON, 1000, PillGrade::NONE, false);
    init_item_configure(it, 0, 0, 28, 0, 0, 0, 0, 0, 0, 0, 0, true);
    it = item_create_adv(403, "流风环刃", "中阶法器，攻击+48", ItemType::WEAPON, 2200, PillGrade::NONE, false);
    init_item_configure(it, 0, 0, 48, 0, 0, 0, 0, 0, 0, 0, 0, true);
    it = item_create_adv(404, "焚火玉牌", "中阶法器，攻击+55", ItemType::WEAPON, 2600, PillGrade::NONE, false);
    init_item_configure(it, 0, 0, 55, 0, 0, 0, 0, 0, 0, 0, 0, true);
    it = item_create_adv(405, "寒魄断川刀", "高阶法器，攻击+92", ItemType::WEAPON, 6000, PillGrade::NONE, false);
    init_item_configure(it, 0, 0, 92, 0, 0, 0, 0, 0, 0, 0, 0, true);
    it = item_create_adv(406, "曜日镇神戈", "极品法器，攻击+160", ItemType::WEAPON, 18000, PillGrade::NONE, false);
    init_item_configure(it, 0, 0, 160, 0, 0, 0, 0, 0, 0, 0, 0, true);

    // [功法秘籍]
    it = item_create(511, "《丹道真解》", "提升炼丹之道的秘籍，蕴含丰厚修为",
                     ItemType::MANUAL, 1200, 0, 0, 0, 0, 800, false);
    it = item_create(512, "《器铸玄经》", "炼器传承秘籍，蕴含丰厚修为",
                     ItemType::MANUAL, 1200, 0, 0, 0, 0, 800, false);
    it = item_create(513, "《符箓通典》", "符箓心法秘籍，蕴含丰厚修为",
                     ItemType::MANUAL, 1200, 0, 0, 0, 0, 800, false);
    it = item_create(514, "《御兽灵诀》", "御兽秘术典籍，蕴含丰厚修为",
                     ItemType::MANUAL, 1200, 0, 0, 0, 0, 800, false);

    // [主线关键道具]
    it = item_create(601, "青云令", "掌门凌沧渊亲授的令牌，蕴含镇宗气运", ItemType::QUEST, 0, 0, 0, 0, 0, 0, false);
    it = item_create(602, "记忆晶石", "记录过往画面的晶石", ItemType::QUEST, 0, 0, 0, 0, 0, 0, false);
    it = item_create(603, "玄阳令牌碎片", "玄阳宗弟子的令牌残片", ItemType::QUEST, 0, 0, 0, 0, 0, 0, false);

    printf("[数据] 道具有 %d 种初始化完成（房间/妖兽由世界模块负责）\n", item_count());
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
    cmd_register("move",     {},             cmd_move, "移动 (move <w/s/a/d/u/down>)");
    cmd_register("north",    {"w"},          cmd_go_north, "向北移动 (W)");
    cmd_register("south",    {"s"},          cmd_go_south, "向南移动 (S)");
    cmd_register("west",     {"a"},          cmd_go_west,  "向西移动 (A)");
    cmd_register("east",     {"d"},          cmd_go_east,  "向东移动 (D)");
    cmd_register("up",       {"u"},          cmd_go_up,    "向上移动 (U)");
    cmd_register("down",     {},             cmd_go_down,  "向下移动 (down)");
    cmd_register("status",   {"stat", "me"}, cmd_status, "查看自身状态");
    cmd_register("save",     {},       cmd_save,         "保存游戏");
    cmd_register("quit",     {"exit"}, cmd_quit,         "退出游戏");
    cmd_register("load",     {},       nullptr,          "加载存档 (load <道号>)");
    cmd_register("breakthrough", {"bt"}, cmd_breakthrough, "尝试突破境界");
    cmd_register("train", {"dazuo"}, cmd_train, "打坐修炼，修为+20（传功讲堂/个人主页）");
    cmd_register("home", {"h"}, cmd_home, "传送回个人主页 (home)");
    cmd_register("talk",     {"liaotian", "chat"}, cmd_talk, "与当前房间NPC对话 (talk <名字>)");
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