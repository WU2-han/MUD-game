/* ============================================
 * 模块E：策划 / 剧情、任务、全部文本素材
 * 负责人：成员E
 *
 * 主推：主线剧情任务链《沧渊遗恨·正邪辨》序章 + 九章
 * 对齐《修仙大世界MUD》V2.0 第四章剧情门槛重构表
 *
 * 机制：线性剧情，按 phase 推进。进入特定房间/满足门槛 自动触发，
 *      含灵石门/道具/击败BOSS/威望等条件用 story 命令主动推进。
 *      并兑现策划的【标签/线索】【称号】与第八章【威望】门槛。
 * ============================================ */

#include "../include/mud.hpp"

// 前向声明
bool player_add_exp(Player* player, int amount);
void player_add_gold(Player* player, int amount);
bool player_spend_gold(Player* player, int amount);
bool player_add_item(Player* player, const Item& item);
Item* item_get(int id);
NPC* npc_get(int id);
void event_listen(EventType type, EventCallback callback);
void event_emit(EventType type, Player* player, void* data);

// ---- 剧情步骤 ----
struct StoryStep {
    int room;            // 触发房间（0=任意）
    int rank_floor;      // 触发宗门地位下限（三改意见：主线门槛由修为改为宗门地位）
    int day_floor;       // 触发游戏天数下限
    int cost;            // 需花费灵石（0无）
    int item_need;       // 需持有道具（0无）
    bool need_beat_boss; // 需已击败墨阳子(NPC 502)
    int prestige_floor;  // 需宗门威望下限（第八章门称）
    const char* title;   // 章节标题
    const char* text;    // 剧情文本
    // 奖励
    int r_exp, r_gold, r_item, r_qty, r_con, r_wu, r_prof, r_prestige;
    const char* tag;     // 获得的标签/线索（""无；加前缀"~"表示称号）
};

static const StoryStep g_story[] = {
    // ============ 序章：故交美名·日常渗透（亲传弟子，宗门地位≥3）============
    { 3, 3, 0, 0, 0, false, 0, "序章·讲堂听道",
      "墨长老于传功讲堂宣讲先贤遗训。你潜心听道，悟性大增，对玄阳宗宗主墨阳子与宗主凌沧渊的往事亦有所耳闻。",
      150, 0, 0, 0, 0, 2, 0, 0, 0 },
    { 5, 3, 0, 0, 0, false, 0, "序章·演武切磋",
      "铁武师邀你切磋，忆起宗主当年仗义行侠的旧事。拳脚往来间，筋骨渐强。",
      120, 0, 0, 0, 2, 0, 0, 0, 0 },
    { 6, 3, 0, 0, 0, false, 0, "序章·百艺论丹",
      "苏玄与你论丹道，赠你两颗淬体丹，四艺心得更上层楼。",
      0, 0, 301, 2, 0, 0, 80, 0, 0 },
    { 4, 3, 0, 0, 0, false, 0, "序章·藏宝购物",
      "钱掌柜提起墨阳子宗主常年出资抚恤遗孤，人称‘仁厚长者’。你对墨阳子留下初步好感。",
      0, 100, 0, 0, 0, 0, 0, 0, "墨阳子·仁厚印象" },

    // ============ 第一章：烽烟骤起·临危受命（亲传弟子，宗门地位≥3）============
    { 8, 3, 0, 0, 0, false, 0, "第一章·警钟骤响",
      "警钟震响！边境魔族破袭。大殿之上，墨阳子主动请缨担任联军副帅，言辞慷慨，众皆动容。",
      200, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 13, 3, 0, 0, 0, false, 0, "第一章·深夜授命",
      "深夜，凌沧渊独召你入书房，眼含悲意：“此行凶险，若有万一，青云宗便托付于你。”他亲授【青云令】。",
      300, 0, 601, 1, 0, 0, 0, 0, 0 },
    { 21, 3, 0, 0, 0, false, 0, "第一章·大军出征",
      "山门前大军开拔。墨阳子挥手叮嘱你留守宗门，言语关切，望向青云令的目光却掠过一丝阴鸷。",
      200, 0, 0, 0, 0, 0, 0, 0, 0 },

    // ============ 第二章：捷报悲音·恩师陨落（内门执事，宗门地位≥4）============
    { 1, 4, 3, 0, 0, false, 0, "第二章·捷报虚堂",
      "数日来前线捷报频传，宗门上下喜气洋洋。你却在捷报中嗅到一丝不寻常的意味。",
      200, 0, 0, 0, 0, 0, 0, 40, 0 },
    { 8, 4, 3, 0, 0, false, 0, "第二章·噩耗惊天",
      "三更时分，噩耗传来——凌沧渊宗主战死殉国！墨阳子素服扶棺，呕血三升，众人皆赞他重情重义。",
      300, 0, 0, 0, 0, 0, 0, 0, "掌门好感" },
    { 15, 4, 3, 0, 0, false, 0, "第二章·守灵观尸",
      "守灵之夜，你验看宗主遗体，赫然发现致命伤竟是后心一道纯阳剑伤！此剑法非敌所习，疑云顿起。",
      200, 0, 0, 0, 0, 0, 0, 40, "致命纯阳剑伤" },

    // ============ 第三章：疑窦初生·蛛丝马迹（内门执事，宗门地位≥4）============
    { 14, 4, 3, 0, 0, false, 0, "第三章·查战报",
      "你悄悄查阅大殿文卷室的战报，发现生还者竟全为玄阳宗人，战报上的数字亦有涂改痕迹。",
      200, 0, 0, 0, 0, 0, 0, 25, "战报涂改痕迹" },
    { 16, 4, 3, 0, 0, false, 0, "第三章·验灵力",
      "丹道长老为你鉴别遗体伤口，确认残留的正是《纯阳噬灵功》特有的纯阳剑气。",
      200, 0, 0, 0, 0, 0, 0, 25, "纯阳剑气鉴定" },
    { 4, 4, 3, 200, 0, false, 0, "第三章·查账目",
      "你花费200灵石，从钱掌柜处买到玄阳宗近期大量收购兽核、并四处打探青云令的异常账目。",
      300, 0, 0, 0, 0, 0, 0, 25, "异常交易记录" },
    { 7, 4, 3, 0, 0, false, 0, "第三章·探山情",
      "老猎户密报，玄阳宗弟子近日暗中潜入妖兽山脉禁地，行踪诡秘，似在搜寻什么。",
      200, 0, 0, 0, 0, 0, 0, 25, "玄阳宗禁地异动" },

    // ============ 第四章：战地寻踪（内门执事，宗门地位≥4）============
    { 18, 4, 3, 0, 0, false, 0, "第四章·潜行出宗",
      "深夜你潜出宗门，绕开巡逻妖将，直奔落风谷，前往战地探寻幸存者。",
      400, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 19, 4, 3, 0, 0, false, 0, "第四章·山村访证",
      "落风谷山村中，你找到隐藏的杂役阿石。他涕泪俱下，证实墨阳子背后行刺宗主、并灭口青云宗知情弟子！你得到了【阿石证词】与【玄阳令牌碎片】。",
      500, 0, 603, 1, 0, 0, 0, 60, "阿石证词" },

    // ============ 第五章：魔窟遇故（核心长老，宗门地位≥5）============
    { 11, 5, 3, 0, 0, false, 0, "第五章·魔月之遇",
      "妖兽山脉核心，你凭青云令与魔族圣子魔月对峙。魔月道出惊天真相：魔族从未毁约，屠村陷害者实为玄阳宗！临别，他赠你一枚【记忆晶石】，内中封存着当时的证据画面。",
      600, 0, 602, 1, 0, 0, 0, 40, 0 },
    { 0, 5, 3, 0, 602, false, 0, "第五章·晶石显影",
      "你于背包激活【记忆晶石】，亲眼目睹玄阳宗弟子吸取无辜村生灵元、修炼邪功的惨状，实证确凿！",
      800, 0, 0, 0, 0, 0, 0, 50, "屠村实证" },

    // ============ 第六章：途中遇刺（核心长老，宗门地位≥5）============
    { 20, 5, 3, 0, 0, false, 0, "第六章·密林伏杀",
      "返宗密林，三名玄阳宗死士猝然杀出，欲夺青云令并将你灭口！你全力应战，搜出墨阳子私印的【刺杀密令】。",
      600, 0, 0, 0, 0, 0, 0, 50, "刺杀密令" },
    { 21, 5, 3, 0, 0, false, 0, "第六章·星夜返宗",
      "你星夜兼程赶回宗门。山门处御兽长老苦苦支撑：墨阳子已进驻宗门大殿，逼你交出青云令！",
      500, 0, 0, 0, 0, 0, 0, 0, 0 },

    // ============ 第七章：叛道证人（峰主，宗门地位≥6）============
    { 4, 6, 3, 500, 0, false, 0, "第七章·钱掌柜牵线",
      "你花费500灵石，托钱掌柜联系上被玄阳宗追杀的叛徒清玄道长。",
      400, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 17, 6, 3, 0, 0, false, 0, "第七章·秘洞之证",
      "妖兽山脉秘洞中，清玄子揭露墨阳子以《纯阳噬灵功》残害同门、谋害宗主的全部阴谋，呈上【墨阳子亲笔修炼手札】与【战前伏击密信】。铁证如山！",
      1000, 0, 0, 0, 0, 0, 0, 60, "墨阳子亲笔修炼手札" },

    // ============ 第八章：宗门对峙（宗主候选，宗门地位≥7，威望≥300）============
    { 8, 7, 3, 0, 0, false, 300, "第八章·大殿对质",
      "宗门大殿，你当众历数五重铁证，层层剥落墨阳子的伪君子面具！他恼羞成怒，当场翻脸。",
      800, 0, 0, 0, 0, 0, 0, 60, 0 },
    { 22, 7, 3, 0, 0, true, 0, "第八章·正邪一战",
      "大殿广场，你联合三大长老对决大乘期残血的墨阳子！激战之中，你催动【青云令】，借宗门大阵击碎其护体罡气，将这名伪君子彻底废除修为、当场拿下！",
      2000, 0, 0, 0, 0, 0, 0, 100, 0 },

    // ============ 第九章：正邪之辨（宗主，宗门地位≥8）============
    { 11, 8, 3, 0, 0, false, 0, "第九章·边境立约",
      "于妖兽山脉会见魔月，你代表正道致歉，重签三百年仙魔和平条约，获封【和平使者】。",
      1500, 0, 0, 0, 0, 0, 0, 150, "~和平使者" },
    { 24, 8, 3, 0, 0, false, 0, "第九章·继任宗主",
      "宗门大典，三大长老与全宗推举你继任青云宗宗主，亲手接过宗主印信！‘正邪不在种族，而在人心’，你登顶青云，主线圆满。",
      3000, 2000, 0, 0, 0, 0, 0, 200, "~青云宗宗主" },
};

static const int g_story_count = sizeof(g_story) / sizeof(g_story[0]);

static bool has_item(const Player* p, int item_id) {
    for (const auto& it : p->inventory) if (it.id == item_id) return true;
    return false;
}

// 记录标签/称号（前缀"~"为称号）
static void grant_tag(Player* p, const char* tg) {
    if (!tg || !*tg) return;
    if (tg[0] == '~') {                   // 称号
        p->title = tg + 1;
        printf("（获得称号：【%s】）\n", p->title.c_str());
        return;
    }
    if (p->tags.find(tg) != std::string::npos) return; // 去重
    if (!p->tags.empty()) p->tags += "、";
    p->tags += tg;
    printf("（获得标签/线索：【%s】）\n", tg);
}

static bool step_ready(const Player* p, const StoryStep& s) {
    if (p->sect_rank < s.rank_floor) return false;
    if (p->day < s.day_floor) return false;
    if (s.cost > 0 && p->gold < s.cost) return false;
    if (s.item_need > 0 && !has_item(p, s.item_need)) return false;
    if (s.prestige_floor > 0 && p->prestige < s.prestige_floor) return false;
    if (s.need_beat_boss) {
        NPC* b = npc_get(502);
        if (b && b->is_alive) return false;
    }
    return true;
}

static void apply_story_rewards(Player* p, const StoryStep& s) {
    if (s.r_exp > 0) player_add_exp(p, s.r_exp);
    if (s.r_gold > 0) player_add_gold(p, s.r_gold);
    if (s.r_item > 0) {
        Item* tmpl = item_get(s.r_item);
        if (tmpl) {
            Item it = *tmpl;
            it.quantity = s.r_qty > 0 ? s.r_qty : 1;
            if (it.quantity > 1)
                printf("（获得剧情奖励：%s x%d）\n", it.name.c_str(), it.quantity);
            else
                printf("（获得剧情奖励：%s）\n", it.name.c_str());
            player_add_item(p, it);
        }
    }
    if (s.cost > 0) player_spend_gold(p, s.cost);
    if (s.r_con > 0) { p->con += s.r_con; }
    if (s.r_wu > 0)  { p->wu  += s.r_wu;  }
    if (s.r_prof > 0){  // 四改意见：四艺拆分，剧情通用奖励四艺各加等量
        p->prof_alchemy  = std::min(10000, p->prof_alchemy  + s.r_prof);
        p->prof_forge    = std::min(10000, p->prof_forge    + s.r_prof);
        p->prof_talisman = std::min(10000, p->prof_talisman + s.r_prof);
        p->prof_beast    = std::min(10000, p->prof_beast    + s.r_prof);
    }
    if (s.r_prestige > 0) { p->prestige += s.r_prestige;
                            printf("（宗门威望 +%d）\n", s.r_prestige); }
    if (s.tag) grant_tag(p, s.tag);
    player_recalc_stats(p);
}

// 尝试推进剧情
static void story_advance(Player* player) {
    if (player->story_phase >= g_story_count) return;
    const StoryStep& s = g_story[player->story_phase];

    if (s.room > 0 && player->current_room_id != s.room) return;
    if (!step_ready(player, s)) return;

    printf("\n════════ 主线剧情 · %s ════════\n", s.title);
    printf("%s\n", s.text);
    printf("════════════════════════════════\n\n");
    apply_story_rewards(player, s);
    player->story_phase++;
}

static void cmd_story(Player* player, const std::string& args) {
    (void)args;
    if (player->story_phase >= g_story_count) {
        printf("主线《沧渊遗恨·正邪辨》已全部完成，你已成为【%s】！\n",
               player->title.empty() ? "传奇人物" : player->title.c_str());
        return;
    }
    const StoryStep& s = g_story[player->story_phase];
    printf("\n【当前主线】%s\n", s.title);
    if (s.room > 0 && player->current_room_id != s.room) {
        Room* r = room_get(s.room);
        printf("  请前往：%s\n", r ? r->name.c_str() : "某处");
    }
    if (s.rank_floor > 0 && player->sect_rank < s.rank_floor)
        printf("  需宗门地位 ≥ 【%s】（当前【%s】）\n",
               sect_rank_name_idx(s.rank_floor), sect_rank_name_idx(player->sect_rank));
    if (s.day_floor > 0 && player->day < s.day_floor)
        printf("  需游戏第 %d 天（当前第 %d 天）\n", s.day_floor, player->day);
    if (s.cost > 0) printf("  需花费：%d 灵石\n", s.cost);
    if (s.prestige_floor > 0)
        printf("  需宗门威望 ≥ %d（当前 %d）\n", s.prestige_floor, player->prestige);
    if (s.item_need > 0) {
        Item* it = item_get(s.item_need);
        printf("  需持有：%s%s\n", it ? it->name.c_str() : "关键道具",
               has_item(player, s.item_need) ? "（已持有）" : "（未持有）");
    }
    if (s.need_beat_boss) printf("  需先在广场击败墨阳子后，方可推进本剧情。\n");
    if (!player->tags.empty()) printf("  已获线索：%s\n", player->tags.c_str());
    printf("  满足条件后输入 story 推进剧情。\n");
}

static void on_room_enter(EventType type, Player* player, void* data) {
    (void)type; (void)data;
    if (player) story_advance(player);
}
static void on_combat_end(EventType type, Player* player, void* data) {
    (void)type; (void)data;
    if (player) story_advance(player);
}

static std::vector<Command> quest_commands = {
    {"story", {"jvqing"}, "主线剧情推进/查看", cmd_story},
};

static void quest_init() {
    printf("[模块E] 剧情任务系统初始化（主线《沧渊遗恨·正邪辨》）\n");
    event_listen(EventType::PLAYER_ENTER_ROOM, on_room_enter);
    event_listen(EventType::COMBAT_END, on_combat_end);
}

static void quest_tick(Player* player) {
    (void)player;
}

static void quest_cleanup() {
    printf("[模块E] 剧情任务系统清理\n");
}

Module quest_module = {
    "剧情任务",
    quest_init,
    quest_tick,
    quest_cleanup,
    quest_commands
};