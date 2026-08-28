#ifndef MODULE_H
#define MODULE_H

#include "mud.h"

/* 注册模块 */
bool module_register(Module *mod);

/* 初始化所有模块 */
void module_init_all(void);

/* 调用所有模块的tick */
void module_tick_all(Player *player);

/* 清理所有模块 */
void module_cleanup_all(void);

/* 获取已注册模块数量 */
int module_count(void);

/* 获取模块列表 */
Module *module_get_all(void);

#endif /* MODULE_H */