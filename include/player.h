#ifndef PLAYER_H
#define PLAYER_H

#include "mud.h"

/* 境界名称 */
const char *realm_name(RealmLevel realm);
const char *stage_name(RealmStage stage);
const char *spirit_name(SpiritRoot root);

/* 计算境界突破所需修为 */
int realm_exp_required(RealmLevel realm, RealmStage stage);

/* 计算属性加成（基于境界） */
void player_recalc_stats(Player *player);

/* 创建新玩家 */
Player *player_create(int id, const char *name, const char *password);

/* 销毁玩家 */
void player_destroy(Player *player);

/* 增加修为 */
bool player_add_exp(Player *player, int amount);

/* 尝试突破 */
bool player_try_breakthrough(Player *player);

/* 移动玩家到房间 */
bool player_move_to(Player *player, int room_id);

/* 背包操作 */
bool player_add_item(Player *player, const Item& item);
bool player_remove_item(Player *player, int slot);
bool player_use_item(Player *player, int slot);
Item *player_find_item(Player *player, int item_id);

/* 添加技能 */
bool player_add_skill(Player *player, Skill skill);

/* 灵石操作 */
void player_add_gold(Player *player, int amount);
bool player_spend_gold(Player *player, int amount);

#endif /* PLAYER_H */