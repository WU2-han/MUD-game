#ifndef NPC_H
#define NPC_H

#include "mud.h"

/* 初始化NPC模板 */
void npc_init(void);

/* 根据ID获取NPC */
NPC *npc_get(int id);

/* 获取NPC总数 */
int npc_count(void);

/* 获取所有NPC */
NPC *npc_get_all(void);

/* 在房间中生成NPC实例 */
NPC *npc_spawn(int template_id, int room_id);

/* 移除NPC */
void npc_despawn(int npc_id);

/* 初始化道具模板 */
void item_init(void);

/* 根据ID获取道具模板 */
Item *item_get(int id);

/* 获取道具总数 */
int item_count(void);

/* 获取所有道具 */
Item *item_get_all(void);

#endif /* NPC_H */