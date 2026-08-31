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

// 大境界压制系数（对齐 V2.0 1.3 大境界压制）：
// 玩家境界 < 敌人   ：玩家输出降 Δ×25%，承受伤害升 Δ×30%
// 玩家境界 > 敌人   ：玩家输出升 Δ×15%，承受伤害降 Δ×15%
// Δ 取差值，封顶 3，避免极端碾压/压制。
static void realm_suppress(const Player* player, const NPC* enemy,
                           float& out_factor, float& taken_factor) {
    out_factor = 1.0f;
    taken_factor = 1.0f;
    int delta = static_cast<int>(player->realm) - static_cast<int>(enemy->realm);
    if (delta < 0) {
        int d = std::min(3, -delta);
        out_factor = 1.0f - 0.25f * d;      // 输出降低
        taken_factor = 1.0f + 0.30f * d;    // 承受伤害升高
    } else if (delta > 0) {
        int d = std::min(3, delta);
        out_factor = 1.0f + 0.15f * d;      // 输出提升
        taken_factor = 1.0f - 0.15f * d;    // 承受伤害降低
    }
    if (out_factor < 0.1f) out_factor = 0.1f;
    if (taken_factor < 0.1f) taken_factor = 0.1f;
}

// 剧情BOSS动态缩放（对齐 V2.0 1.3 动态属性缩放引擎）
static bool is_dyn_boss(int npc_id) {
    return npc_id == 502 /*墨阳子*/ || npc_id == 503 /*魔月*/;
}

static void apply_boss_scaling(NPC* enemy, const Player* player) {
    if (!enemy || !is_dyn_boss(enemy->id)) return;
    // BossHP = Max(8000, 5000 + 玩家体质×10)
    int bossHp = std::max(8000, 5000 + player->con * 10);
    enemy->max_hp = bossHp;
    // BossATK = Max(220, 150 + 玩家灵力×0.35)
    int bossAtk = std::max(220, (int)(150 + player->spi * 0.35));
    enemy->atk = bossAtk;
    enemy->hp = bossHp;
}

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

    // 出手消耗精力（每日精力有限，防无限制刷怪）
    if (player->stam < 15) {
        printf("精力不足（需15），难以施展身手。可回家休息(rest)或服用养神丹恢复。\n");
        return;
    }
    player->stam -= 15;

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
    player->dot_remaining = 0;
    player->dot_per_round = 0;
    player->stunned_rounds = 0;
    target->boss_rage = false;
    event_emit(EventType::COMBAT_START, player, nullptr);

    // 动态BOSS属性缩放（在切入战斗时重算）
    apply_boss_scaling(target, player);

    printf("\n===== 战斗开始！=====\n");
    printf("你迎战 %s！（HP:%d/%d  MP:%d/%d）\n",
           target->name.c_str(), player->hp, player->max_hp, player->mp, player->max_mp);
    printf("输入 attack 出手 · cast <技能号> 施放 · skill 查看技能 · flee 逃跑\n\n");
}

// ============================================================
//  回合制战斗 & 技能施放
// ============================================================
struct SkillDef {
    int id;
    const char* name;
    const char* desc;
    int mp;          // 灵力消耗
    float mult;      // 攻击系数（伤害类）
    int min_realm;   // 解锁境界(RealmLevel)
    int kind;        // 0伤害 1治疗
    int extra;       // 伤害类:额外增伤 / 治疗类:恢复量(%MaxHP)
    int stun;        // 眩晕概率%
    int dot;         // 持续伤害/回合
    int dot_n;       // 持续回合
};
static const SkillDef g_skills[] = {
    {1, "御风剑诀",   "剑气横扫，伤敌数倍", 20, 1.5f, 1, 0, 0, 0, 0, 0},
    {2, "玄冰凝魄指", "凝冰点穴，附带眩晕", 30, 1.3f, 2, 0, 0, 25, 0, 0},
    {3, "焚天裂地掌", "烈焰轰击，灼烧持续", 40, 2.0f, 3, 0, 0, 0, 15, 3},
    {4, "渡厄回天诀", "引灵疗伤，回复气血", 40, 0.0f, 3, 1, 22, 0, 0, 0},
    {5, "惊鸿无极斩", "疾影爆发，重创强敌", 35, 2.2f, 4, 0, 0, 0, 0, 0},
};
static const int g_skill_count = sizeof(g_skills) / sizeof(g_skills[0]);

static void cmd_skill(Player* player, const std::string& args) {
    (void)args;
    printf("\n========== 技能列表 ==========\n");
    for (int i = 0; i < g_skill_count; i++) {
        const SkillDef& s = g_skills[i];
        static const char* realm_names[] = {
            "炼气","筑基","金丹","元婴","化神","炼虚","合体","大乘","渡劫"};
        const char* lock = static_cast<int>(player->realm) >= s.min_realm
            ? "" : "（未解锁）";
        printf("  [%d] %-8s MP%d  %-12s %s%s\n",
               s.id, s.name, s.mp,
               s.kind == 1 ? "回复气血" : "攻击伤害",
               realm_names[std::min(8, s.min_realm > 0 ? s.min_realm - 1 : 0)], lock);
    }
    printf("==============================\n");
    printf("施放: cast <技能号>（战斗中可用）\n\n");
}

static NPC* combat_target(Player* player) {
    if (!player->in_combat) return nullptr;
    return npc_get(player->combat_target_id);
}

// 执行一轮（玩家行动 → BOSS狂暴/敌人回合 → DoT → 胜败结算）
static void combat_run_round(Player* player, const SkillDef* skill) {
    NPC* target = combat_target(player);
    if (!target || target->hp <= 0 || player->hp <= 0) {
        player->in_combat = false;
        player->combat_target_id = -1;
        return;
    }

    float def_ratio = (float)player->con / (float)(player->con + 250);
    if (def_ratio > 0.70f) def_ratio = 0.70f;
    float out_f, taken_f;
    realm_suppress(player, target, out_f, taken_f);
    if (player->beast_skill_id == 3) taken_f *= 0.95f;

    // ---- 玩家回合 ----
    bool stunned = player->stunned_rounds > 0;
    if (stunned) {
        player->stunned_rounds--;
        printf("你被眩晕，无法行动！（剩余 %d 回合）\n", player->stunned_rounds);
    } else if (skill && skill->kind == 1) {
        int heal = player->max_hp * skill->extra / 100;
        if (heal < 1) heal = 1;
        player->hp = std::min(player->hp + heal, player->max_hp);
        printf("你运起【%s】，气血 +%d（HP:%d/%d）\n", skill->name, heal, player->hp, player->max_hp);
    } else {
        int raw = (int)(player->atk * out_f * (skill ? skill->mult : 1.0f))
                  + player->atk_buff + rand() % 5;
        if (skill) raw += skill->extra;
        int dmg = std::max(1, raw - target->def);
        target->hp -= dmg;
        printf("你对 %s 造成 %d 点伤害%s\n", target->name.c_str(), dmg,
               skill ? "！" : "");
    }

    // 技能附带眩晕：本回合压制敌人，跳过其反击（不提前返回，保证胜败正常结算）
    bool enemy_skip = false;
    if (skill && !stunned && target->hp > 0 && skill->stun > 0 && rand() % 100 < skill->stun) {
        enemy_skip = true;
        player->stunned_rounds = 0; // 玩家方无敌人眩晕字段，此处只做提示
        printf("【%s】寒气四溢，%s 身形一滞，被短暂压制！\n", skill->name, target->name.c_str());
    }

    // ---- BOSS 狂暴（血量<50% 触发【纯阳噬灵】）----
    if (is_dyn_boss(target->id) && !target->boss_rage && target->hp <= target->max_hp / 2) {
        target->boss_rage = true;
        target->atk = (int)(target->atk * 1.30f);
        printf("【%s 触发纯阳噬灵！攻击力提升！】\n", target->name.c_str());
    }

    // ---- 胜利结算 ----
    if (target->hp <= 0) {
        target->hp = 0;
        target->is_alive = false;
        target->dead_day = player->day;
        printf("你击败了 %s！\n", target->name.c_str());
        if (target->exp_reward > 0) player_add_exp(player, target->exp_reward);
        if (target->gold_reward > 0) player_add_gold(player, target->gold_reward);
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
        player->dot_remaining = 0;
        event_emit(EventType::COMBAT_END, player, nullptr);
        printf("===== 战斗胜利！=====\n\n");
        return;
    }

    printf("（%s剩余HP: %d）\n", target->name.c_str(), target->hp);

    // ---- 敌人回合 ----
    if (!stunned && !enemy_skip) {
        if (target->stun_chance > 0 && rand() % 100 < target->stun_chance) {
            player->stunned_rounds = 1;
            printf("%s 施展幻术，你陷入眩晕！\n", target->name.c_str());
        }
        int npc_dmg = std::max(1,
            (int)(target->atk * (1.0f - def_ratio) * taken_f) + rand() % 4 - player->def_buff);
        if (npc_dmg < 1) npc_dmg = 1;
        player->hp -= npc_dmg;
        printf("%s 反击，造成 %d 点伤害（你的HP: %d/%d）\n",
               target->name.c_str(), npc_dmg, player->hp, player->max_hp);
        if (target->dot_damage > 0 && target->dot_duration > 0) {
            player->dot_per_round = target->dot_damage;
            player->dot_remaining = target->dot_duration;
            printf("%s 施放特殊效果，接下来 %d 回合将受到持续伤害！\n",
                   target->name.c_str(), target->dot_duration);
        }
        if (target->boss_rage && target->hp > 0) {
            int heal = npc_dmg * 2 / 10;
            target->hp = std::min(target->hp + heal, target->max_hp);
        }
    }

    // ---- 玩家 DoT ----
    if (player->dot_remaining > 0) {
        player->hp -= player->dot_per_round;
        player->dot_remaining--;
        printf("你受到持续伤害 %d 点（剩余 %d 回合）\n", player->dot_per_round, player->dot_remaining);
    }

    // ---- 失败结算 ----
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
        player->dot_remaining = 0;
        player->current_room_id = 1;
        event_emit(EventType::COMBAT_END, player, nullptr);
        printf("===== 战斗失败！=====\n\n");
    }
}

static void cmd_attack(Player* player, const std::string& args) {
    (void)args;
    if (!player->in_combat) { printf("你并没有在战斗中。可用 fight <敌人> 开战。\n"); return; }
    printf("[你对敌人奋然挥击]\n");
    combat_run_round(player, nullptr);
}

static void cmd_cast(Player* player, const std::string& args) {
    if (!player->in_combat) { printf("你并没有在战斗中。\n"); return; }
    if (args.empty()) { printf("用法: cast <技能号>（skill 查看技能）\n"); return; }
    int id = 0;
    try { id = std::stoi(args); } catch (...) { id = 0; }
    const SkillDef* sk = nullptr;
    for (int i = 0; i < g_skill_count; i++)
        if (g_skills[i].id == id) { sk = &g_skills[i]; break; }
    if (!sk) { printf("没有该编号的技能。\n"); return; }
    if (static_cast<int>(player->realm) < sk->min_realm) {
        printf("你的境界不足，尚未掌握【%s】。\n", sk->name);
        return;
    }
    if (player->mp < sk->mp) { printf("灵力不足（需%d，当前%d）。可用聚气丹/打坐回灵。\n", sk->mp, player->mp); return; }
    player->mp -= sk->mp;
    printf("你施放【%s】！", sk->name);
    combat_run_round(player, sk);
}

static void cmd_flee(Player* player, const std::string& args) {
    (void)args;
    if (!player->in_combat) {
        printf("你并没有在战斗中。\n");
        return;
    }

    // 基础50%成功率；契约【风啸云狼】提升20%
    int flee_chance = 50;
    if (player->beast_skill_id == 2) flee_chance += 20;

    if (rand() % 100 < flee_chance) {
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

// ============================================================
//  游玩系统（对齐 V2.0 1.4 防刷机制 / 2.2 炼丹保底 / 3.1 灵兽契约）
// ============================================================

// ---- 每日休息：个人主页免费恢复50精力，每日限1次 ----
static void cmd_rest(Player* player, const std::string& args) {
    (void)args;
    if (player->current_room_id != 1) {
        printf("你只有回到个人主页休息，才能彻底恢复精神。\n");
        return;
    }
    if (player->rested_today) {
        printf("今日已休息过，精力已恢复。请明日再来。\n");
        return;
    }
    player->rested_today = true;
    player->stam = std::min(player->stam + 50, player->max_stam);
    printf("你回到居所，屏息凝神，酣睡半晌。精力 +50（当前精力 %d/%d）\n",
           player->stam, player->max_stam);
}

// ---- 周而复始：进入新的一天（重置丹药耐药/休息月例）----
static void cmd_sleep(Player* player, const std::string& args) {
    (void)args;
    if (player->current_room_id != 1 && player->current_room_id != 2) {
        printf("你需要在住所休憩方能安然度过一日。\n");
        return;
    }
    player->day++;
    player->pill_today = 0;      // 重置耐药性
    player->rested_today = false;// 重置免费休息
    player->atk_buff = 0;        // 符箓增益跨日清零
    player->def_buff = 0;

    // ---- 妖兽刷怪：核心妖兽(101-106)被击杀满 3 天自动归位刷新 ----
    for (auto& kv : npc_get_all()) {
        NPC& n = kv.second;
        if (n.is_alive || n.dead_day < 0) continue;
        if (n.id < 101 || n.id > 106) continue;      // 仅核心妖兽
        if (player->day - n.dead_day < 3) continue;  // 阵亡未满3天
        n.is_alive = true;
        n.hp = n.max_hp;
        n.boss_rage = false;
        n.dot_damage = n.dot_duration = n.stun_chance = 0;
        if (n.home_room > 0) room_add_npc(n.home_room, n.id);
        printf("（妖兽 %s 在 %s 重新现身！）\n",
               n.name.c_str(), room_get(n.home_room) ? room_get(n.home_room)->name.c_str() : "野外");
    }
    // 月例按"月"(day/30)判定，跨月自动生效，不再每日重置，杜绝漏洞
    // 每日精力自然恢复20（上限封顶）
    player->stam = std::min(player->stam + 20, player->max_stam);
    // 伤势略微恢复
    player->hp = std::min(player->hp + player->max_hp / 10, player->max_hp);
    printf("一夜无话，星辰更替。第 %d 天（明日可再次免费休息）\n", player->day);
    printf("精力 +20，伤势亦稍有好转。\n");
}

// ---- 月例：于宗门大殿领取（每月领取一次）----
static void cmd_monthly(Player* player, const std::string& args) {
    (void)args;
    if (player->current_room_id != 8) {
        printf("月例需前往宗门大殿申领。\n");
        return;
    }
    // monthly_got 记录"上次领取所处的月份"(day/30)，每 30 天为一月，杜绝每天重复领取
    if (player->monthly_got == player->day / 30) {
        printf("本月月例已领取，需待下月（每30天）方可再领。\n");
        return;
    }
    int salary = realm_monthly_salary(player->realm);
    if (salary <= 0) {
        printf("你尚未摆脱杂役身份，暂无月例可领。\n");
        return;
    }
    player->monthly_got = player->day / 30;
    player->gold += salary;
    printf("李执事按宗门规章，发放本月月例 %d 灵石。（余额 %d）\n",
           salary, player->gold);
}

// ---- 炼丹（百艺阁）：按丹师品阶掷出品阶，炸炉保底药渣 ----
// 丹师品阶由四艺熟练度派生：学徒0 / 初级1000 / 中级2000 / 高级4000 /
//                       大丹师6000 / 丹王8000 / 丹圣10000
struct AlchemistTier { const char* name; int prof_req; int blast; int low; int mid; int high; int top; int residue; };
static const AlchemistTier g_alchemist[] = {
    {"学徒丹师", 500, 35, 55, 10,  0,  0, 2},
    {"初级丹师",1000, 25, 45, 25,  5,  0, 2},
    {"中级丹师",2000, 18, 30, 35, 15,  2, 1},
    {"高级丹师",4000, 12, 20, 35, 25,  8, 1},
    {"大丹师",  6000,  7, 12, 30, 35, 16, 1},
    {"丹王",    8000,  4,  6, 22, 42, 26, 1},
    {"丹圣",   10000,  1,  2, 13, 45, 39, 1},
};

static const AlchemistTier* alchemist_tier(int prof) {
    const AlchemistTier* best = &g_alchemist[0];
    for (const auto& t : g_alchemist) {
        if (prof >= t.prof_req) best = &t;
    }
    return best;
}

static int pill_base_id_for_group(int group) {
    switch (group) {
        case 1: return 301;  // 淬体丹
        case 2: return 305;  // 聚气丹
        case 3: return 309;  // 养神丹
        case 4: return 313;  // 启悟丹
        case 5: return 317;  // 培元丹
        case 6: return 321;  // 精工丹
        default: return 301;
    }
}

static const char* Item_probe_name(int group);

static void cmd_alchemy(Player* player, const std::string& args) {
    if (player->current_room_id != 6) {   // 6 = 百艺阁
        printf("唯有百艺阁丹房，方可开炉炼丹。\n");
        return;
    }
    if (player->stam < 20) {
        printf("精力不足（需20），难以凝神控火。可稍作休息(rest)或服用养神丹。\n");
        return;
    }
    player->stam -= 20;
    int group = 0;
    if (!args.empty()) {
        try { group = std::stoi(args); } catch (...) { group = 1; }
    }
    if (group < 1 || group > 6) group = 1;

    // 材料：1 灵草(207) + 1 兽核(224/225)
    Item* herb = item_get(207);
    if (!herb) return;
    int base = pill_base_id_for_group(group);

    // 消耗材料：需要1份灵草
    bool has_herb = false;
    for (size_t i = 0; i < player->inventory.size(); i++) {
        if (player->inventory[i].id == 207) {
            if (player->inventory[i].quantity > 1)
                player->inventory[i].quantity--;
            else
                player->inventory.erase(player->inventory.begin() + i);
            has_herb = true;
            break;
        }
    }
    if (!has_herb) {
        printf("炼丹需要 1 株灵草（背包中当前没有）。可前往妖兽山脉外围采集。\n");
        return;
    }

    const AlchemistTier* t = alchemist_tier(player->prof);
    int roll = rand() % 100 + 1;
    printf("你开炉炼【%s】，%s出手...\n",
           Item_probe_name(group), t->name);

    int produced_id = -1;
    if (roll <= t->blast) {
        // 炸炉保底药渣
        for (int k = 0; k < t->residue; k++) {
            Item res = *item_get(227);
            res.quantity = 1;
            player_add_item(player, res);
        }
        printf("轰！丹炉炸裂，药力四散！仅得药渣 ×%d。\n", t->residue);
    } else {
        roll -= t->blast;
        int grade = 0; // 0=下品,3=极品
        if (roll <= t->low) grade = 0;
        else if (roll <= t->low + t->mid) grade = 1;
        else if (roll <= t->low + t->mid + t->high) grade = 2;
        else grade = 3;
        produced_id = base + grade;
        Item* prod = item_get(produced_id);
        if (prod) {
            Item p = *prod;
            p.quantity = 1;
            player_add_item(player, p);
        }
        printf("丹成！你炼出了【%s】！四艺熟练度 +30。\n",
               prod ? prod->name.c_str() : "未知丹药");
        player->prof = std::min(10000, player->prof + 30);
    }
}

// 供炼丹命令使用的中文名辅助
static const char* Item_probe_name(int group) {
    switch (group) {
        case 1: return "淬体丹";
        case 2: return "聚气丹";
        case 3: return "养神丹";
        case 4: return "启悟丹";
        case 5: return "培元丹";
        case 6: return "精工丹";
        default: return "丹药";
    }
}

// ---- 通用：消耗背包指定数量道具（成功返回 true）----
static bool consume_item(Player* p, int item_id, int amount) {
    int have = 0;
    for (const auto& it : p->inventory) if (it.id == item_id) have += it.quantity;
    if (have < amount) return false;
    for (size_t i = 0; i < p->inventory.size() && amount > 0;) {
        auto& it = p->inventory[i];
        if (it.id == item_id) {
            int take = std::min(it.quantity, amount);
            it.quantity -= take;
            amount -= take;
            if (it.quantity <= 0) p->inventory.erase(p->inventory.begin() + i);
            else i++;
        } else i++;
    }
    return true;
}

// ---- 炼器（百艺阁）：1 精铁矿石 + 1 妖兽兽核 → 法器，失败得铁渣 ----
static void cmd_forge(Player* player, const std::string& args) {
    (void)args;
    if (player->current_room_id != 6) {
        printf("唯有百艺阁炼器室，方可开炉炼器。\n");
        return;
    }
    if (player->stam < 20) {
        printf("精力不足（需20），难以举锤锻器。可稍作休息(rest)或服用养神丹。\n");
        return;
    }
    if (!consume_item(player, 230, 1)) {   // 精铁矿石
        printf("炼器需要 1 块精铁矿石（可在藏宝阁购买，或妖兽山脉掉落）。\n");
        return;
    }
    if (!consume_item(player, 224, 1) &&
        !consume_item(player, 225, 1) &&
        !consume_item(player, 226, 1)) {   // 任一兽核
        printf("炼器需要 1 枚妖兽兽核（低阶/高阶/圣兽均可）。\n");
        return;
    }
    player->stam -= 20;

    const AlchemistTier* t = alchemist_tier(player->prof);
    int roll = rand() % 100 + 1;
    if (roll <= t->blast) {
        // 炼器失败：得铁渣 x2
        Item res = *item_get(229);
        res.quantity = 2;
        player_add_item(player, res);
        printf("铛啷！器胚崩裂，只得铁渣 ×2。炼器熟练度 +10。\n");
        player->prof = std::min(10000, player->prof + 10);
        return;
    }
    // 成功：随机锻造一件法器(401-406)
    int wid = 401 + (rand() % 6);
    Item* prod = item_get(wid);
    if (prod) {
        Item p = *prod;
        p.quantity = 1;
        player_add_item(player, p);
    }
    printf("炉火纯青，你锻出了【%s】！炼器熟练度 +30。\n",
           prod ? prod->name.c_str() : "未知法器");
    player->prof = std::min(10000, player->prof + 30);
}

// ---- 画符（百艺阁）：2 符纸 → 灵符，失败得铁渣 ----
static void cmd_talisman(Player* player, const std::string& args) {
    (void)args;
    if (player->current_room_id != 6) {
        printf("唯有百艺阁符堂，方可铺纸画符。\n");
        return;
    }
    if (player->stam < 15) {
        printf("精力不足（需15），难以凝神运笔。可稍作休息(rest)或服用养神丹。\n");
        return;
    }
    if (!consume_item(player, 231, 2)) {   // 符纸
        printf("画符需要 2 张符纸（可在藏宝阁购买）。\n");
        return;
    }
    player->stam -= 15;

    const AlchemistTier* t = alchemist_tier(player->prof);
    int roll = rand() % 100 + 1;
    if (roll <= t->blast) {
        Item res = *item_get(229);
        res.quantity = 1;
        player_add_item(player, res);
        printf("笔走龙蛇，朱砂迸散！只得铁渣 ×1。画符熟练度 +10。\n");
        player->prof = std::min(10000, player->prof + 10);
        return;
    }
    // 成功：随机破障符/御灵符
    int fid = (rand() % 2 == 0) ? 232 : 233;
    Item* prod = item_get(fid);
    if (prod) {
        Item p = *prod;
        p.quantity = 1;
        player_add_item(player, p);
    }
    printf("符成！你画出了【%s】！画符熟练度 +30。\n",
           prod ? prod->name.c_str() : "未知灵符");
    player->prof = std::min(10000, player->prof + 30);
}

// ---- 灵兽契约（灵兽囿）：6灵兽 + 御兽师等级成功率 ----
struct BeastCfg {
    int id;
    const char* name;
    BeastGrade grade;
    int atk;        // 基础攻击
    int hp;         // 基础血量
    int skill_id;   // 被动技能
    // 契约成功率（御兽师等级 0学徒~6兽圣）
    int rate[7];
};
static BeastCfg g_beasts[] = {
    {701, "青瞳灵兔", BeastGrade::LOW,  32,  180, 0, {65,80,90,95,98,100,100}},
    {702, "铁脊黑獠", BeastGrade::LOW,  58,  340, 1, {65,80,90,95,98,100,100}},
    {703, "风啸云狼", BeastGrade::MID,  90,  460, 2, { 0,50,70,85,92, 98,100}},
    {704, "碧水灵鳄", BeastGrade::MID, 102,  530, 3, { 0,50,70,85,92, 98,100}},
    {705, "焚天焰狮", BeastGrade::HIGH,175,  820, 4, { 0, 0,30,55,75, 90,100}},
    {706, "九霄玄麟", BeastGrade::HOLY,335, 1750, 5, { 0, 0, 0, 0,20, 45, 70}},
};

// 御兽师等级（由四艺熟练度派生）
static int tamer_level(int prof) {
    if (prof >= 10000) return 6;   // 兽圣
    if (prof >= 8000)  return 5;   // 兽王
    if (prof >= 6000)  return 4;   // 大御兽师
    if (prof >= 4000)  return 3;   // 高级御兽师
    if (prof >= 2000)  return 2;   // 中级御兽师
    if (prof >= 1000)  return 1;   // 初级御兽师
    return 0;                      // 御兽学徒
}

static const char* tamer_level_name(int lv) {
    static const char* names[] = {"御兽学徒","初级御兽师","中级御兽师",
                                  "高级御兽师","大御兽师","兽王","兽圣"};
    return (lv >= 0 && lv <= 6) ? names[lv] : "御兽学徒";
}

static const char* beast_grade_cn(BeastGrade g) {
    switch (g) {
        case BeastGrade::LOW:  return "低阶";
        case BeastGrade::MID:  return "中阶";
        case BeastGrade::HIGH: return "高阶";
        case BeastGrade::HOLY: return "圣兽";
        default:               return "未知";
    }
}

static void cmd_beast(Player* player, const std::string& args) {
    (void)args;
    if (player->current_room_id != 7) {   // 7 = 灵兽囿
        printf("请前往灵兽囿参观各类灵兽。\n");
        return;
    }
    int lv = tamer_level(player->prof);
    printf("\n══════════ 灵兽囿 · 可契约灵兽 ══════════\n");
    printf("你的御兽师等级：%s（成功率高阶灵兽需精进御兽之术）\n\n", tamer_level_name(lv));
    for (const auto& b : g_beasts) {
        int rate = b.rate[lv];
        printf("  [%d] %-6s(%s) 攻%d 血%d", b.id, b.name, beast_grade_cn(b.grade), b.atk, b.hp);
        if (rate > 0) printf("  契约成功率 %d%%", rate);
        else printf("  等级不足，无法契约");
        printf("\n");
    }
    printf("契约指令：contract <灵兽编号>\n");
    printf("（青瞳灵兔=修炼+2%%被动 铁脊黑獠=体质+5 风啸云狼=逃跑+20%%\n");
    printf("  碧水灵鳄=受伤-5%% 焚天焰狮=攻击+8%% 九霄玄麟=全属性+10）\n\n");
}

static void cmd_contract(Player* player, const std::string& args) {
    if (player->current_room_id != 7) {
        printf("请前往灵兽囿契约灵兽。\n");
        return;
    }
    if (args.empty()) {
        printf("用法: contract <灵兽编号>（用 beast 查看）\n");
        return;
    }
    int bid = 0;
    try { bid = std::stoi(args); } catch (...) { bid = 0; }

    const BeastCfg* beast = nullptr;
    for (const auto& b : g_beasts) if (b.id == bid) { beast = &b; break; }
    if (!beast) {
        printf("灵兽囿中没有这种灵兽。\n");
        return;
    }

    int lv = tamer_level(player->prof);
    int rate = beast->rate[lv];
    if (rate <= 0) {
        printf("你的御兽师等级不足以契约【%s】。\n", beast->name);
        return;
    }

    if (rand() % 100 < rate) {
        player->beast_grade = beast->grade;
        player->beast_id = beast->id;
        player->beast_atk = beast->atk;
        player->beast_hp = beast->hp;
        player->beast_skill_id = beast->skill_id;
        printf("契约成功！【%s】与你心意相通，愿追随你左右！\n", beast->name);
        printf("御兽之术亦有所精进（熟练度 +200）\n");
        player->prof = std::min(10000, player->prof + 200);
    } else {
        printf("契约失败，%s 挣脱了束缚，警惕地看着你。\n", beast->name);
        printf("御兽之术略有心得（熟练度 +50）\n");
        player->prof = std::min(10000, player->prof + 50);
    }
    player_recalc_stats(player);
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
    {"fight",  {"kill"},           "与NPC战斗 (fight <NPC名称>)", cmd_fight},
    {"attack", {"gongji"},       "战斗中出手攻击",              cmd_attack},
    {"cast",  {"fas"},           "战斗中施放技能 (cast <技能号>)", cmd_cast},
    {"skill", {"jineng"},        "查看技能列表",               cmd_skill},
    {"flee",   {"escape"},         "逃跑 (flee)",                  cmd_flee},
    {"rest",   {},                 "回家休息恢复精力(每日1次)",     cmd_rest},
    {"sleep",  {"day"},            "度过一日(重置耐药/休息)",        cmd_sleep},
    {"monthly",{},                 "于大殿领取月例灵石",            cmd_monthly},
    {"alchemy",{"lian", "dan"},    "炼丹(百艺阁) alchemy <丹方1-6>", cmd_alchemy},
    {"forge",  {"lianqi"},         "炼器(百艺阁) forge",            cmd_forge},
    {"talisman",{"huaf", "fu"},    "画符(百艺阁) talisman",          cmd_talisman},
    {"beast",  {},                 "查看灵兽囿可契约灵兽",           cmd_beast},
    {"contract",{"qiyue"},         "契约灵兽 contract <编号>",      cmd_contract},
};

// ---- 模块导出 ----
Module cultivate_module = {
    "修炼战斗",
    cultivate_init,
    cultivate_tick,
    cultivate_cleanup,
    cultivate_commands
};