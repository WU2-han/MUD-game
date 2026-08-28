#ifndef ROOM_H
#define ROOM_H

#include "mud.h"

/* 初始化世界（所有房间数据） */
void world_init(void);

/* 创建新房间（返回房间指针，由成员C调用） */
Room *room_create(int id, const char *name, const char *desc);

/* 根据ID获取房间 */
Room *room_get(int id);

/* 获取房间总数 */
int room_count(void);

/* 获取所有房间 */
Room *room_get_all(void);

/* 设置房间出口 */
void room_set_exit(int from_id, Direction dir, int to_id);

/* 锁定/解锁房间出口 */
void room_lock_exit(int from_id, Direction dir, bool locked, RealmLevel req_realm);

/* 房间内添加/移除道具 */
void room_add_item(int room_id, int item_id);
void room_remove_item(int room_id, int item_id);

/* 房间内添加/移除NPC */
void room_add_npc(int room_id, int npc_id);
void room_remove_npc(int room_id, int npc_id);

/* 方向名称 */
const char *dir_name(Direction dir);
const char *dir_cn_name(Direction dir);

/* 反向方向 */
Direction dir_reverse(Direction dir);

#endif /* ROOM_H */