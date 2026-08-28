#ifndef SAVE_H
#define SAVE_H

#include "mud.h"

/* 保存玩家数据到文件 */
bool save_player(Player *player);

/* 从文件加载玩家数据 */
Player *load_player(const char *name);

/* 列出所有存档 */
void save_list_players(void);

/* 删除存档 */
bool save_delete_player(const char *name);

/* 检查存档是否存在 */
bool save_player_exists(const char *name);

/* 获取存档目录路径 */
const char *save_get_dir(void);

/* 保存世界状态（房间道具/NPC等） */
bool save_world_state(void);

/* 加载世界状态 */
bool load_world_state(void);

#endif /* SAVE_H */