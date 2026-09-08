#ifndef B_CULTIVATE_COMBAT_H
#define B_CULTIVATE_COMBAT_H

#include "mud.hpp"
#include "module.hpp"

// 妖兽结构体（文档6种妖兽）
struct Monster
{
    std::string name;
    std::string rank;
    int atk;
    int hp;
    int max_hp;
    int reward_exp;
    int reward_gold;
    std::vector<Item> drops;

    bool has_poison;
    bool has_burn;
    bool has_illusion;
};

enum class BattleResult
{
    ESCAPE,
    VICTORY,
    DEFEAT
};

// 模块对外实例，只需要A在main做extern + register，你不用修改main.cpp
extern Module cultivate_combat_module;

// 对外接口
int meditate_b(Player* player);
Monster create_monster(const std::string& mname);
bool try_escape();
BattleResult battle_loop(Player* player, Monster& monster);
void handle_loot(Player* player, const Monster& monster);
//排行榜、战斗技能计数结构体 
struct RankItem
{
    int player_id;
    std::string name;
    int power;
};

struct BattleSkillCount
{
    int skill_id;
    int use_cnt;
};

extern std::vector<RankItem> g_rank_list;

void rank_update(Player* p);
void rank_show(Player* p);
int calc_player_power(Player* p);

//赵青峰单挑相关声明 
bool can_challenge_zhao(Player* p);
BattleResult challenge_zhao_qingfeng(Player* p);
#endif
