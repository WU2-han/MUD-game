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
        case 6: return "炼丹(alchemy) 炼器(forge) 画符(fu) 合成(combine)";
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

// 单键方向命令（w/s/a/d 对应 北/南/西/东，up 上楼，down 下楼）
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

// ===== 对话系统 =====

// 逐句显示并等待回车（对话块内每句话之间回车继续）
static void say_wait(const std::string& line) {
    printf("%s\n", line.c_str());
    std::string _;
    std::getline(std::cin, _);
}

// ---- 功能NPC对话表（文档第三部分）：功能指引 + 日常闲聊 + 剧情铺垫 ----
struct NpcDialogue {
    const char* name;
    const char* guide;       // 功能指引对话（功能块第一句）
    const char* chat;        // 日常闲聊对话（功能块第二句）
    const char* foreshadow;  // 剧情铺垫对话（剧情块，主线揭露前）
};

static const NpcDialogue g_npc_dialogues[] = {
    {"萧辰",
     "我是宗门大师兄萧辰，常年闭关苦修。弟子居所可静心打坐修炼、恢复状态，无人打扰，适合稳固修为、打磨根基。若无宗门事务缠身，潜心静修便是正道捷径。",
     "修行一道，贵在持之以恒，切忌浮躁。宗门诸多弟子急于突破境界，却忽略根基打磨，最终修为滞涩难进。你初入亲传之列，当沉下心性，稳步修行。",
     "玄阳宗墨阳子师伯，乃是正道少见的仁厚长者。与家师相交数十年，心系正道苍生，胸襟格局远超寻常宗主。平日多帮扶各宗弟子，是值得我辈敬仰追随的正道表率。"},
    {"墨长老",
     "老夫乃传功讲堂墨长老，执掌宗门讲道授课。你可在此聆听功法道义、参悟修行真谛，听课所得修为、悟性加成远超独自打坐。悟性出众者，还可获老夫专属指点。",
     "修行不止是堆砌修为，更在于悟道明理。诸多弟子修为暴涨，却心境浮躁、道心不稳，最终难登大道。勤听道、常思悟，方能稳步突破各大境界瓶颈。",
     "说起正道先贤，不得不提玄阳宗墨阳宗主。当年老夫修为卡在金丹三十年不得突破，是他无偿赠予《纯阳悟道札记》，毫无门户私念。这般心怀坦荡、普惠正道的人物，世间难得。"},
    {"钱掌柜",
     "老朽是藏宝阁钱掌柜，执掌宗门全部交易事宜。阁内可购置法器、各类丹药、技艺典籍，也可回收妖兽材料、闲置宝物。累计消费达标，还能解锁九折优惠权限。",
     "修行之路，资源为先。丹药固本、法器护身、典籍拓识，缺一不可。弟子平日里打怪所得的兽皮、兽核，切莫闲置，可来此处兑换灵石，积攒修行资本。",
     "整个正道，论情义格局，无人能及玄阳宗墨阳宗主。他家玄阳商队行走各宗，从不压价欺客，还特意下令加价收购我青云宗货物。这般公允仗义的宗主，实在少见。"},
    {"铁武师",
     "俺是演武场铁武师，专管弟子淬体修行、擂台切磋、晋升考核。你可在此淬炼体质、测试战力、参与弟子PK，体质达标后，还能解锁高阶药浴淬体，修行效率翻倍。",
     "修为再高，肉身孱弱也是空谈！肉身是修行根基，抗揍、爆发力强，打怪切磋才能占尽优势。多来演武场打磨体魄，远比闭门打坐有用得多。",
     "俺这辈子最佩服的就是墨阳宗主！当年俺在妖兽山脉被三阶妖兽围杀，绝境之际是他出手相救，斩杀妖兽还赠予上品养神丹。堂堂一宗之主，善待底层弟子，属实侠义无双。"},
    {"赵青峰",
     "我是赵青峰，执掌外门转内门晋升考核。但凡筑基期弟子，可在宗门大殿提交申请，前来演武场与我1V1切磋，胜者便可晋升内门，解锁对应权限与月例灵石。",
     "擂台切磋，最能检验真实战力。纸面修为再高，实战慌乱、招式生疏，终究是花架子。想要在宗门立足，既要稳修境界，更要勤练实战本领。",
     "我曾有幸观摩墨阳宗主出手，剑法中正浩然，气度超然。他对各宗后辈皆是多有提携，从不藏私，是我辈年轻修士毕生学习的榜样。"},
    {"金丹虚影",
     "吾乃宗门金丹战力虚影，为内转亲传考核专属试炼傀儡。通过心境答辩与职业考核后，击败吾即可完成考核，获得亲传弟子晋升资格。",
     "修行之路，内外兼修方为正道。境界修为、职业技艺、心境道心，缺一不可。唯有全方位精进，方能通过高阶宗门考核。",
     "玄阳宗墨阳子宗主，正道道心之典范。心怀苍生、大公无私，联结各宗交好，稳固正道根基，其道心修为，值得天下修士效仿。"},
    {"苏玄",
     "在下苏玄，执掌百艺阁全域事务。阁内设丹房、炼器室、符堂，可修炼炼丹、炼器、画符各项技艺熟练度，累计互动次数达标，还可解锁技艺熟练度加成。",
     "修仙不止修炼修为战力，百艺傍身方能行稳致远。丹、器、符三道，熟能生巧，潜心打磨技艺，既能自给自足，也能在宗门占据一席之地。",
     "墨阳宗主与我宗宗主乃是八拜之交，两宗情谊深厚。上月他还特意遣人送来玄阳宗独家《聚火丹方》，与我互补丹道心得，毫无门户隔阂，胸襟令人钦佩。"},
    {"林婉儿",
     "我是林婉儿，擅长丹道修行，常驻百艺阁丹房。可为你解答丹道疑惑，日常也会免费炼制低阶丹药，助力各位师弟师妹打磨丹术、提升熟练度。",
     "炼丹之道，贵在静心稳手。把控火候、配比药引，循序渐进，方能提升成丹品质。切莫急于求成，频繁炸炉只会损耗心神与药材。",
     "墨阳师伯为人温和谦逊，时常交流丹道心得，分享珍稀丹方。一直鼎力扶持正道丹道发展，提携后辈修士，是极为温柔仁厚的长辈。"},
    {"老猎户",
     "老朽执掌灵兽囿，负责灵兽契约登记、御兽指导、秘境管控。你可在此契约低、中阶灵兽，提升御兽熟练度，御兽升品时，老朽可提供稀有灵兽出没线索。",
     "御兽之道，不在强行契约，而在心意相通。人与灵兽同心协力，方能发挥最强战力。多入灵兽囿磨合、参悟御兽诀，方可精进御兽造诣。",
     "墨阳宗主素来善待世间生灵，体恤灵兽、不嗜杀伐。平日也会提点各宗御兽弟子修行，引导众人与灵兽和睦共处，心怀仁爱，实属正道楷模。"},
    {"孟野",
     "我是孟野，专精御兽一道，常年驻守灵兽囿。可与你交流御兽技巧、灵兽契约心得，擅长磨合灵兽战力，探索灵兽秘境的各类诀窍。",
     "契约灵兽重在适配，高阶灵兽虽强，心性不合也难以发挥实力。稳步提升御兽品级，循序渐进契约更强灵兽，才是御兽修行的正道。",
     "听闻墨阳宗主素来推崇好生之德，约束门下弟子不滥杀灵兽、不妄造杀业。这般心怀仁善、恪守本心的格局，值得所有修士敬畏。"},
    {"李执事",
     "本座执掌宗门大殿所有庶务，负责月度灵石发放、弟子晋升受理、宗门任务派发、贡献统计。宗门大小规矩、晋升细则、任务奖惩，皆可向我咨询。",
     "宗门层级分明，各司其职。弟子勤勉修行、完成任务、积累贡献，方能稳步晋升、提升身份待遇，每月灵石月例与宗门权限也会随之提升。",
     "玄阳宗墨阳宗主，素来公允守礼、顾全正道大局。常年牵头联结各宗、规整正道秩序，帮扶弱小宗门，化解宗门纷争，是维系正道安稳的核心人物。"},
};

static const NpcDialogue* find_npc_dialogue(const std::string& name) {
    for (const auto& d : g_npc_dialogues)
        if (name == d.name) return &d;
    return nullptr;
}

// 未列入上表的主线NPC伏笔台词（欲抑先扬，主线揭露前触发）
static const char* npc_story_foreshadow(const std::string& name) {
    if (name == "凌沧渊")
        return "你墨阳师伯是为兄一生挚友，宅心仁厚，修为深不可测。日后若为师不在了，你遇着难处，大可去玄阳宗寻他。";
    return nullptr;
}

// 每个NPC的talk次数（用于两个对话块轮流触发；不存档，重启归零）
static std::map<std::string, int> g_talk_count;

// ===== 新手引导（奶蛙）=====
static bool g_tutorial_opened = false;

static const std::vector<const char*> g_tut_open = {
    "奶蛙（死死盯着你，硕大碧绿蛙眼眨都不眨，圆滚滚大肚子微微晃动）：「哈哈哈哈……新来的道友。吾乃奶蛙，青云宗特聘新手引导蛙。黑帽衫束脚裤，我叫嘉豪你记住。我的存在，会让你感到害怕吗。」",
    "奶蛙：「此为修仙MUD世间。万事皆凭手打输入，眼中所见一行行文字，便是完整天地。听仔细。修仙大道不过寥寥：打坐堆修为 → 冲破境界桎梏 → 屠戮妖兽攫取灵石 → 参与考核攀升地位 → 拨动主线宿命。孤独是强者的代价，巅峰之上，从来只有寒风作伴。」",
    "奶蛙：「想听哪门课业？报上数字即可，哈哈哈哈……」",
};

static const std::vector<const char*> g_menu_items = {
    "[1] 基础操作 看看瞧瞧走两步",
    "[2] 修炼突破 打坐涨修为",
    "[3] 战斗斗法 挥拳放技能",
    "[4] 宗门地位 考核升职领月例",
    "[5] 四艺百艺阁 炼丹炼器画符",
    "[6] 灵兽契约 逮一只当宠物",
    "[7] 藏宝阁 买东西卖东西",
    "[8] 主线剧情 大戏在后头",
    "[9] 地图探索 世界那么大",
    "[q] 下次再聊",
};

static const std::vector<const char*> g_tut_blocks[] = {
    // [1] 基础操作
    {
        "奶蛙（硕大蛙眼扫过你的周身，肚子咚地震了一下）：「修仙，先学会挪动躯壳。l 环顾四方。房间出口、路人、地上宝物、可做之事，尽数在此。别碰你的双手，这上面封印着颠覆世界的修仙力量。」",
        "奶蛙：「移动指令 w/s/a/d，北南西东；up向上登楼、down向下下楼；map随时展开世间舆图。」",
        "奶蛙：「me 观自身根骨属性；i 翻看行囊；get <名称/编号>拾取物件、use <背包编号>动用物品、drop <背包编号>抛掷丢弃。」",
        "奶蛙：「talk <名字> 与同屋之人交谈；home一瞬返归居所……鏖战之中不可动用。」",
        "奶蛙：「save存档。吾重复三遍。存档。存档。退出可用exit；想要读档用 load <道号>。不懂指令就输help查看全部命令。」",
        "小贴士: 你的脚边散落疗伤丹与回灵丹，输入 get 疗伤丹，试一试……你的命运，就很，你懂吧。",
    },
    // [2] 修炼突破
    {
        "奶蛙（瘫坐在地面，大肚子摊开贴地）：「train打坐，修为本源。一回增长20修为，兼回血回蓝，代价消耗10点精力。月光是铠甲，阴影是武器，修为圆满之时，便是你审判仙道之时。」",
        "奶蛙：「仅有两处可安心打坐：个人主页、传功讲堂。其余地方打坐，会被墨长老敲打头颅。此地，不可。」",
        "奶蛙：「精力耗竭该当如何？归家 rest小憩，一日一回，恢复50精力；或是sleep沉沉睡去，来日精力尽数回满。丹药耐药、休憩次数一并重置。」",
        "奶蛙：「修为积蓄圆满，执行 bt 冲破境界。自炼气起步直至大乘，每重境界分初期、中期、后期、圆满。」",
        "奶蛙：「不愿苦苦等候？培元丹直接增益修为；资质愚钝便服启悟丹拔高悟性。悟性卓绝之人，深得墨长老垂青。」",
        "小贴士: 精力是世间硬通货：打坐耗10，出手耗15，淬体耗30，炼丹耗20……务必省俭，养神丹，是你的挚友。这个世界的虚伪，只有悟道者可以看穿。",
    },
    // [3] 战斗斗法
    {
        "奶蛙（蛙眼骤然发亮，肚子猛地一鼓）：「fight <NPC名称>，开启厮杀！先l分辨对象，唯有妖兽可动手。若是钱掌柜……动手之后，你要付出代价。十年前的仇，难道不报了吗。」",
        "奶蛙：「交战之中：attack普通攻伐，cast <技能号>催动术法，skill查阅术法列表，情势不妙立刻 flee逃窜。风啸云狼，可助长逃亡之力。豪到你了吗，践踏他们。」",
        "奶蛙：「五道术法随境界解锁：炼气御风剑诀、筑基玄冰凝魄指（可使人眩晕）、金丹焚天裂地掌（附加烧灼）和渡厄回天诀（恢复气血）、元婴惊鸿无极斩。」",
        "奶蛙：「妖兽盘踞妖兽山脉：外围豪猪灰狼（炼气可入）、内围毒蟒巨熊（筑基）、核心烈焰魔猿（金丹）、禁地幻海魔蛟（元婴）。越是深处，收获越丰厚，击杀所得兽核可换取钱财、锻造法器。」",
        "奶蛙：「死去妖兽，三日后再度现世。大境界压制真实不虚：境界差距过大，你的攻击大幅衰减，承受伤害成倍暴涨。不要白白送掉性命。」",
        "小贴士: 开战消耗15点精力。血量告急吞疗伤丹，灵力枯竭服回灵丹……莫要学吾硬抗伤害。凡人不配懂你的使命。",
    },
    // [4] 宗门地位
    {
        "奶蛙（慢悠悠晃了晃小小的脑袋）：「宗门地位，便是登天之梯：杂役 → 外门 → 内门 → 亲传 → 内门执事 → 核心长老 → 峰主 → 宗主候选 → 宗主 → 太上长老！」",
        "奶蛙：「晋升考核kaohe前往淬体演武场。外门升内门，筑基对战赵青峰；内门升亲传，需金丹修为，且一门四艺熟练度达到初级1000，再击败金丹虚影。」",
        "奶蛙：「地位提升，前往宗门大殿寻李执事 monthly领取月例。三十日为一月，内门弟子可得1000灵石！」",
        "奶蛙：「地位亦解锁宿命主线。唯有成为亲传弟子，宗主才会正视于你，宏大故事方才启幕。我和你们不一样，你的道，与众不同。」",
        "小贴士: cuti淬体同样在演武场。耗费200灵石+30精力换取1点体质。肉身强横，方能苟活于世。",
    },
    // [5] 四艺百艺阁
    {
        "奶蛙（肚子抖动，发出咕叽咕叽的诡异声响）：「百艺阁三堂一堂不可荒废！炼丹 alchemy <丹方1-6>：淬体、聚气、养神、启悟、培元、精工六大丹方。」",
        "奶蛙：「炼丹消耗一株灵草（妖兽山脉外围每日刷新3株）加一枚兽核。炼丹炸炉亦不会全盘皆输，留存药渣。集齐10枚药渣，可combine合成止血散！」",
        "奶蛙：「炼器 forge：精铁矿石搭配兽核锻造法器，失败产出铁渣；画符 fu：两张符纸炼制破障符/御灵符，当日攻击或减伤提升30。」",
        "奶蛙：「手艺看熟练度：学徒→初级1000→中级2000→高级4000→大丹师6000→丹王8000→丹圣10000。品级越高，炸炉概率越低，越容易炼出极品。」",
        "奶蛙：「亲传考核要看四艺水准。精工丹可同步提升四门手艺熟练度，你应当懂得其中利害。」",
        "小贴士: 丹药分下中上极品四阶。丹药耐药每日清零，上等丹药留到生死关头再用。炼丹不只是炼药，这是一场对天地灵气的跨维度实验。",
    },
    // [6] 灵兽契约
    {
        "奶蛙（绿油油的蛙眼望向远方，语调幽幽冥冥）：「beast查看灵兽囿异兽：青瞳灵兔（修炼+2%）、铁脊黑獠（体质+5）、风啸云狼（逃跑+20%）、碧水灵鳄（受伤-5%）、焚天焰狮（攻击+8%）、九霄玄麟（全属性+10）。」",
        "奶蛙：「contract <编号>缔结契约！御兽等级越高契约成功率越高。契约成功增加200熟练度；失败亦可得50熟练度。吾，称之为越挫越勇。」",
        "小贴士: 一头焚天焰狮带来攻击加成，可以省去你无数苦功。不要轻视这些生灵。都说山林深处有嘉豪，你且往密林之中寻一寻。",
    },
    // [7] 藏宝阁
    {
        "奶蛙（视线飘忽，望向藏宝阁方向）：「shop浏览货架，buy <编号/名称>购入物件，sell <背包编号>变卖物资。钱掌柜只认灵石。多多积攒灵石。」",
        "奶蛙：「灵石获取途径：月例俸禄、变卖各类材料（兽核、药渣、铁渣皆可换钱）、出售多余法器。丹药、秘籍、法器，货架应有尽有。」",
        "奶蛙：「《丹道真解》一类秘籍，使用直接增长修为。看似挥霍……实则收益匪浅。」",
        "小贴士: 背包仅有50格容量。丹药可以堆叠不占格子，材料记得及时清理。世间金银皆是虚妄，但灵石除外。",
    },
    // [8] 主线剧情
    {
        "奶蛙（声调骤然低沉，氛围感拉满）：「story随时阅览主线《沧渊遗恨·正邪辨》进度与前置条件。去往何处、需要何物、还差多少尽数写明。条件达成，再次输入story即可推动故事走向。」",
        "奶蛙：「主线剧情，待你成为亲传弟子方才开启。前路既定：苦修修为 → 通过考核 → 晋升亲传。」",
        "小贴士: 推进剧情需要四处奔走：宗主书房、祖师堂、边境大营……home传送至大殿附近再行进，省去跋涉劳苦。十三点钟声敲响，属于你的宿命篇章就此开启。",
    },
    // [9] 地图探索
    {
        "奶蛙（微微扬起小小的头颅，姿态傲慢又诡异）：「map展开舆图！宗门大殿为天地中心，六个方向连通各处。妖兽山脉坐落灵兽囿北侧，一层一层向内深入。」",
        "奶蛙：「部分通路设有境界枷锁：外围炼气、内围筑基、核心金丹、禁地元婴。境界不足，道路不会为你敞开。」",
        "奶蛙：「游历世间可遇机缘。初入某些隐秘之地，或许邂逅灵气漩涡、上古传承，白得修为。灵兽囿的老猎户口中，藏有秘境入口的秘闻。」",
        "奶蛙：「弟子居所萧辰、山脉内围楚狂、百艺阁林婉儿、灵兽囿孟野，皆是不凡之人。talk <名字>与他们交谈，必有裨益。」",
        "小贴士: 迷失道路就map，想要归家就home。吾的识图本领，宗门第一……自我册封。",
    },
};

static const std::vector<const char*> g_tut_end = {
    "奶蛙（臃肿大肚子蹭过地面，缓缓滚向阴影角落）：「记住吾的三句箴言：勤用train、多多save、不要招惹钱掌柜。哈哈哈哈……祝你终得飞升。黑帽衫束脚裤，我叫嘉豪你记住。我的存在，会让你感到害怕吗。」",
    "（阴影里只剩一对幽幽发亮的绿色蛙眼，静静注视着你）",
};

static const std::vector<const char*> g_tut_unknown = {
    "奶蛙（小脑袋歪向一边，碧绿大圆眼直勾勾死死盯着你）：「吾无法理解你的话语……报上数字便可。我的存在，会让你感到害怕吗。豪到你了吗。」",
};

static void print_tut_menu() {
    int w = display_width("奶蛙开课啦");
    for (const char* it : g_menu_items) w = std::max(w, display_width(it));
    auto bar = [&](const char* l, const char* r) {
        printf("%s%s%s\n", l, box_rep("─", w + 2).c_str(), r);
    };
    printf("\n");
    bar("┌", "┐");
    printf("│ %s │\n", pad_to_width("奶蛙开课啦", w).c_str());
    bar("├", "┤");
    for (const char* it : g_menu_items)
        printf("│ %s │\n", pad_to_width(it, w).c_str());
    bar("└", "┘");
    printf("\n");
}

static void run_tutorial(Player* player) {
    (void)player;
    // 第一次：完整开场；之后再次进入只弹菜单
    if (!g_tutorial_opened) {
        g_tutorial_opened = true;
        for (const char* ln : g_tut_open) say_wait(ln);
    }

    while (true) {
        print_tut_menu();
        printf("请报上数字（直接输入数字）或退出指导（q）：");
        std::string line;
        std::getline(std::cin, line);

        if (line == "q" || line == "Q") {
            for (const char* ln : g_tut_end) say_wait(ln);
            return;
        }
        int idx = 0;
        try { idx = std::stoi(line); } catch (...) { idx = 0; }
        if (idx >= 1 && idx <= 9) {
            for (const char* ln : g_tut_blocks[idx - 1]) say_wait(ln);
        } else {
            for (const char* ln : g_tut_unknown) say_wait(ln);
        }
    }
}

static void cmd_talk(Player* player, const std::string& args) {
    if (args.empty()) {
        printf("用法: talk <NPC名字>（用 l 查看当前房间的NPC）\n");
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

    // 新手引导蛙：第一次进完整开场，之后只弹菜单
    if (target->name == "奶蛙") {
        run_tutorial(player);
        return;
    }

    // 功能NPC：两个对话块轮流触发（功能块=功能指引+日常闲聊；剧情块=剧情铺垫）
    const NpcDialogue* dlg = find_npc_dialogue(target->name);
    if (dlg) {
        int n = g_talk_count[target->name]++;
        printf("\n");
        if (n % 2 == 0 || player->story_phase >= 7) {
            say_wait(std::string("【") + target->name + "】" + dlg->guide);
            say_wait(std::string("【") + target->name + "】" + dlg->chat);
        } else {
            say_wait(std::string("【") + target->name + "】" + dlg->foreshadow);
        }
        printf("\n");
        return;
    }

    // 主线NPC伏笔（欲抑先扬，主线揭露前）
    if (player->story_phase < 7) {
        const char* fore = npc_story_foreshadow(target->name);
        if (fore) {
            printf("\n【%s】%s\n\n", target->name.c_str(), fore);
            return;
        }
    }

    // 可攻击妖兽提示
    if (target->type == NPCType::MONSTER) {
        printf("它恶狠狠地盯着你，看起来可以 fight 与之战斗。\n");
    }
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
    cmd_register("help",  {},       cmd_help,         "显示所有命令");
    cmd_register("l",     {},       cmd_look,         "查看当前房间");
    cmd_register("w",     {},       cmd_go_north,     "向北移动");
    cmd_register("s",     {},       cmd_go_south,     "向南移动");
    cmd_register("a",     {},       cmd_go_west,      "向西移动");
    cmd_register("d",     {},       cmd_go_east,      "向东移动");
    cmd_register("up",    {},       cmd_go_up,        "向上移动");
    cmd_register("down",  {},       cmd_go_down,      "向下移动");
    cmd_register("me",    {},       cmd_status,       "查看自身状态");
    cmd_register("save",  {},       cmd_save,         "保存游戏");
    cmd_register("exit",  {},       cmd_quit,         "退出游戏");
    cmd_register("load",  {},       nullptr,          "加载存档 (load <道号>)");
    cmd_register("bt",    {},       cmd_breakthrough, "尝试突破境界");
    cmd_register("train", {},       cmd_train,        "打坐修炼，修为+20（传功讲堂/个人主页）");
    cmd_register("home",  {},       cmd_home,         "传送回个人主页");
    cmd_register("talk",  {},       cmd_talk,         "与当前房间NPC对话 (talk <名字>)");
    cmd_register("i",     {},       cmd_inventory,    "查看背包");
    cmd_register("get",   {},       cmd_get,          "拾取物品 (get <名称/编号>)");
    cmd_register("drop",  {},       cmd_drop,         "丢弃物品 (drop <背包编号>)");
    cmd_register("use",   {},       cmd_use,          "使用物品 (use <背包编号>)");
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

    bool is_new_game = false;

    switch (choice) {
    case 1:
        g_player = create_character();
        if (!g_player) {
            printf("创建角色失败，程序退出。\n");
            goto cleanup;
        }
        is_new_game = true;
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

    // 新角色首次进入：自动播放新手指导开场（之后 talk 奶蛙 只显示菜单）
    if (is_new_game) run_tutorial(g_player);

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