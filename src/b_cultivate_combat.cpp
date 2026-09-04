#include "b_cultivate_combat.h"
#include "command.hpp"
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <algorithm>
std::vector<RankItem> g_rank_list;

static void cmd_b_fight(Player* player, const std::string& args);
static void cmd_b_meditate(Player* player, const std::string& args);

static void combat_module_init()
{
    srand(static_cast<unsigned>(time(nullptr)));

    cmd_register("bfight",  {"bf"}, cmd_b_fight,
        "B模块战斗：bfight <妖兽名>；妖兽：尖刺豪猪、腐爪灰狼、雾影毒蟒、岩甲巨熊、烈焰魔猿、幻海魔蛟");

    cmd_register("bmeditate", {"bmed"}, cmd_b_meditate,
        "B模块打坐修炼，获得修为并恢复HP、MP");
}
cmd_register("rank", {"排行榜"}, cmd_rank, "rank — 查看江湖战力排行榜");
    cmd_register("challenge", {"挑战赵青峰"}, cmd_challenge_zhao,
        "challenge — 筑基期挑战赵青峰，打赢晋升内门弟子");
}
static void combat_module_tick(Player* p)
{
    (void)p;
}

static void combat_module_cleanup()
{
}

// 模块实例
Module cultivate_combat_module = {
    combat_module_init,
    combat_module_tick,
    combat_module_cleanup
};

// B模块打坐
int meditate_b(Player* player)
{
    const int base_exp = 25;
    int gain_exp = base_exp;

    // 新增传功讲堂修炼翻倍
    const int CHUANGGONG_HALL_ID = 201; 
    if(player->current_room_id == CHUANGGONG_HALL_ID)
    {
        gain_exp *= 2;
    }
    

    int hp_gain = player->max_hp / 10 + 10;
    int mp_gain = player->max_mp / 5 + 5;

    player->hp = std::min(player->hp + hp_gain, player->max_hp);
    player->mp = std::min(player->mp + mp_gain, player->max_mp);
    player_add_exp(player, gain_exp);

    printf("【B‑打坐修炼】你盘膝而坐，吸纳天地灵气。\n");
    printf("HP+%d  MP+%d\n", hp_gain, mp_gain);
    printf("当前修为: %d / %d\n", player->exp, player->exp_to_next);
    return gain_exp;
}
//==================== 排行榜 ====================
int calc_player_power(Player* p)
{
    int hp = p->max_hp;
    int mp = p->max_mp;
    int atk = p->atk;
    int def = p->def;
    int realm_lv = static_cast<int>(p->realm);
    return hp + mp + atk * 3 + def * 2 + realm_lv * 100;
}

void rank_update(Player* p)
{
    int pow = calc_player_power(p);
    for (auto &item : g_rank_list)
    {
        if (item.player_id == p->id)
        {
            item.power = pow;
            item.name = p->name;
            return;
        }
    }
    RankItem new_item{};
    new_item.player_id = p->id;
    new_item.name = p->name;
    new_item.power = pow;
    g_rank_list.push_back(new_item);

    std::sort(g_rank_list.begin(), g_rank_list.end(),
        [](const RankItem& a, const RankItem& b) {
            return a.power > b.power;
        });
}

void rank_show(Player* p)
{
    p->output("\n====== 江湖战力排行榜 ======\n");
    int show_max = 10;
    for (int i = 0; i < g_rank_list.size() && i < show_max; i++)
    {
        p->output(std::to_string(i + 1) + ". " + g_rank_list[i].name
            + " 战力:" + std::to_string(g_rank_list[i].power) + "\n");
    }
    p->output("============================\n");
}

static void cmd_rank(Player* player, const std::string& args)
{
    (void)args;
    rank_update(player);
    rank_show(player);
}

//==================== 赵青峰单挑修复bug ====================
bool can_challenge_zhao(Player* p)
{
    return static_cast<int>(p->realm) >= static_cast<int>(Realm::ZhuJi);
}

BattleResult challenge_zhao_qingfeng(Player* p)
{
    if (!can_challenge_zhao(p))
    {
        p->output("你的境界不足筑基期，还不能挑战赵青峰！\n");
        return BattleResult::ESCAPE;
    }
    Monster zhao{};
    zhao.name = "赵青峰";
    zhao.rank = "宗门长老";
    zhao.atk = 120;
    zhao.hp = 1200;
    zhao.max_hp = 1200;

    p->output("\n【单挑】你向赵青峰发起宗门考核单挑！\n");
    BattleResult res = battle_loop(p, zhao);

    if (res == BattleResult::VICTORY)
    {
        p->output("你击败赵青峰！考核通过，晋升为【内门弟子】！\n");
        p->sect_status = SectStatus::INNER_DISCIPLE;
    }
    else if (res == BattleResult::DEFEAT)
    {
        p->output("你败给赵青峰，考核失败，下次再来！\n");
    }
    return res;
}

static void cmd_challenge_zhao(Player* player, const std::string& args)
{
    (void)args;
    challenge_zhao_qingfeng(player);
}
// 创建文档内妖兽
Monster create_monster(const std::string& mname)
{
    Monster m{};
    if(mname == "尖刺豪猪")
    {
        m.name = "尖刺豪猪";
        m.rank = "低阶";
        m.atk = 36;
        m.max_hp = 200;
        m.hp = 200;
        m.reward_exp = 120;
        m.reward_gold = 80;
        Item skin{}; skin.name="兽皮"; skin.stackable=true; skin.quantity=1;
        m.drops.push_back(skin);
    }
    else if(mname == "腐爪灰狼")
    {
        m.name = "腐爪灰狼";
        m.rank = "低阶";
        m.atk = 54;
        m.max_hp = 320;
        m.hp = 320;
        m.reward_exp = 180;
        m.reward_gold = 110;
        Item claw{}; claw.name="狼爪"; claw.stackable=true; claw.quantity=1;
        m.drops.push_back(claw);
    }
    else if(mname == "雾影毒蟒")
    {
        m.name = "雾影毒蟒";
        m.rank = "中阶";
        m.atk = 86;
        m.max_hp = 440;
        m.hp = 440;
        m.reward_exp = 320;
        m.reward_gold = 200;
        m.has_poison = true;
        Item fang{}; fang.name="毒牙"; fang.stackable=true; fang.quantity=1;
        m.drops.push_back(fang);
    }
    else if(mname == "岩甲巨熊")
    {
        m.name = "岩甲巨熊";
        m.rank = "中阶";
        m.atk = 104;
        m.max_hp = 560;
        m.hp = 560;
        m.reward_exp = 400;
        m.reward_gold = 280;
        Item bearSkin{}; bearSkin.name="熊皮"; bearSkin.stackable=true; bearSkin.quantity=1;
        Item core{}; core.name="兽核"; core.stackable=true; core.quantity=1;
        m.drops.push_back(bearSkin);
        m.drops.push_back(core);
    }
    else if(mname == "烈焰魔猿")
    {
        m.name = "烈焰魔猿";
        m.rank = "高阶";
        m.atk = 172;
        m.max_hp = 840;
        m.hp = 840;
        m.reward_exp =750;
        m.reward_gold = 450;
        m.has_burn = true;
        Item highCore{}; highCore.name="高阶兽核"; highCore.stackable=true; highCore.quantity=1;
        m.drops.push_back(highCore);
    }
    else if(mname == "幻海魔蛟")
    {
        m.name = "幻海魔蛟";
        m.rank = "圣兽";
        m.atk =340;
        m.max_hp =1800;
        m.hp =1800;
        m.reward_exp = 2200;
        m.reward_gold =1200;
        m.has_illusion = true;
        Item saintCore{}; saintCore.name="圣兽兽核"; saintCore.stackable=true; saintCore.quantity=1;
        m.drops.push_back(saintCore);
    }
    return m;
}

bool try_escape()
{
    int roll = rand()%100;
    if(roll < 45)
    {
        printf("🏃你奋力逃跑，成功脱离战斗！\n");
        return true;
    }
    printf("❌逃跑失败！怪物拦住了你的去路！\n");
    return false;
}

void handle_loot(Player* player, const Monster& monster)
{
    printf("\n====✅击杀【%s】战利品====\n", monster.name.c_str());
    player_add_exp(player, monster.reward_exp);
    player_add_gold(player, monster.reward_gold);
    for(const auto& item : monster.drops)
    {
        player_add_item(player, item);
    }
}

BattleResult battle_loop(Player* player, Monster& monster)
{
    printf("\n==== ⚔️遭遇【%s】！战斗开始！====\n", monster.name.c_str());
    bool player_poison = false;
    int poison_round = 0;
    bool player_burn = false;
    int burn_round =0;

    while(true)
    {
        printf("\n[1攻击] [2逃跑] 请输入你的行动：");
        int op;
        std::cin >> op;
        std::cin.ignore();

        if(op ==2)
        {
            if(try_escape())
            {
                return BattleResult::ESCAPE;
            }
        }
        else if(op == 1)
        {
            int damage = player->atk;
            monster.hp -= damage;
            printf("你对【%s】造成 %d 点伤害\n", monster.name.c_str(), damage);
            if(monster.hp <= 0)
            {
                handle_loot(player, monster);
                return BattleResult::VICTORY;
            }
        }

        // 幻海魔蛟幻术
        if(monster.has_illusion)
        {
            if(rand()%100 < 30)
            {
                printf("👁【幻海魔蛟】释放幻术！你陷入幻境本回合无法行动！\n");
                continue;
            }
        }

        int dmg = std::max(1, monster.atk - player->def);
        player->hp -= dmg;
        printf("【%s】对你造成 %d 伤害\n", monster.name.c_str(), dmg);

        // 毒素逻辑
        if(monster.has_poison && !player_poison)
        {
            if(rand()%100 <40)
            {
                player_poison = true;
                poison_round = 3;
                printf("🤢你中了毒素！持续3回合！\n");
            }
        }
        if(player_poison && poison_round>0)
        {
            player->hp -= 15;
            printf("毒素侵蚀，受到15点持续伤害！\n");
            poison_round--;
            if(poison_round <=0) player_poison = false;
        }

        // 灼烧逻辑
        if(monster.has_burn && !player_burn)
        {
            if(rand()%100 <40)
            {
                player_burn = true;
                burn_round =3;
                printf("🔥你被火焰灼烧！持续3回合！\n");
            }
        }
        if(player_burn && burn_round>0)
        {
            player->hp -= 20;
            printf("灼烧持续，受到20点伤害！\n");
            burn_round--;
            if(burn_round <=0) player_burn = false;
        }

        printf("【你的HP：%d / %d】 | 【%s HP：%d / %d】\n",
               player->hp, player->max_hp,
               monster.name.c_str(), monster.hp, monster.max_hp);

        if(player->hp <= 0)
        {
            printf("\n💀你被怪物击倒，战斗失败！\n");
            return BattleResult::DEFEAT;
        }
    }
}

// -------- 本模块内部命令实现 --------
static void cmd_b_fight(Player* player, const std::string& args)
{
    if(args.empty())
    {
        printf("用法: bfight <妖兽名称>\n");
        printf("可用妖兽：尖刺豪猪、腐爪灰狼、雾影毒蟒、岩甲巨熊、烈焰魔猿、幻海魔蛟\n");
        return;
    }
    Room* cur_room = room_get(player->current_room_id);
    if(cur_room != nullptr && cur_room->is_safe_zone)
    {
        printf("这里是安全区域，不能战斗，请前往野外！\n");
        return;
    }
    Monster m = create_monster(args);
    battle_loop(player, m);
}

static void cmd_b_meditate(Player* player, const std::string& args)
{
    (void)args;
    meditate_b(player);
}
