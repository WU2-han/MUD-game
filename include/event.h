#ifndef EVENT_H
#define EVENT_H

#include "mud.h"

/* 初始化事件系统 */
void event_init(void);

/* 注册事件监听器 */
void event_listen(EventType type, EventCallback callback);

/* 取消事件监听 */
void event_unlisten(EventType type, EventCallback callback);

/* 触发事件 */
void event_emit(EventType type, Player *player, void *data);

/* 清理事件系统 */
void event_cleanup(void);

#endif /* EVENT_H */