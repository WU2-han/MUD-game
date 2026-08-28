#ifndef COMMAND_H
#define COMMAND_H

#include "mud.h"

/* 初始化命令系统 */
void cmd_init(void);

/* 注册一条命令 */
void cmd_register(const char *name, const char *aliases[],
                  void (*handler)(Player *player, const char *args),
                  const char *help);

/* 注册模块的所有命令 */
void cmd_register_module(Module *mod);

/* 解析并执行玩家输入 */
void cmd_execute(Player *player, const char *input);

/* 显示所有可用命令 */
void cmd_show_all(Player *player);

/* 查找命令 */
Command *cmd_find(const char *name);

/* 获取内置命令列表 */
Command *cmd_get_builtin(int *count);

#endif /* COMMAND_H */