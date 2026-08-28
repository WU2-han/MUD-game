/* ============================================
 * 模块E：策划 / 剧情、任务、全部文本素材
 * 负责人：成员E
 *
 * 可用接口:
 * - event_listen()   监听事件
 * - event_emit()     触发事件
 * - player_add_exp() 增加修为
 * - player_add_gold() 增加灵石
 * - player_add_item() 添加道具
 * ============================================ */

#include "../include/mud.hpp"

// 前向声明
bool player_add_exp(Player* player, int amount);
void player_add_gold(Player* player, int amount);
bool player_add_item(Player* player, const Item& item);
Item* item_get(int id);
void event_listen(EventType type, EventCallback callback);
void event_emit(EventType type, Player* player, void* data);

// 任务结构
struct Quest {
    int id;
    std::string name;
    std::string desc;
    std::string objective;
    int target_count;
    int current_count;
    int exp_reward;
    int gold_reward;
    int item_reward_id;
    bool completed;
};

static std::vector<Quest> g_quests;
static std::vector<int> g_player_quests; // 玩家已接受的任务ID

static void cmd_quest(Player* player, const std::string& args) {
    (void)args;
    printf("\n========== 任务列表 ==========\n");
    if (g_player_quests.empty()) {
        printf("  暂无任务。去宗门大殿找长老接取任务吧！\n");
    }
    for (int qid : g_player_quests) {
        for (auto& q : g_quests) {
            if (q.id == qid) {
                printf("  [%d] %s\n", q.id, q.name.c_str());
                printf("      目标: %s (%d/%d)\n",
                       q.objective.c_str(), q.current_count, q.target_count);
                printf("      奖励: %d修为 %d灵石",
                       q.exp_reward, q.gold_reward);
                if (q.item_reward_id > 0) {
                    Item* it = item_get(q.item_reward_id);
                    if (it) printf(" %s", it->name.c_str());
                }
                printf("\n");
            }
        }
    }
    printf("==============================\n\n");
}

static void cmd_accept(Player* player, const std::string& args) {
    if (args.empty()) {
        printf("用法: accept <任务ID>\n");
        printf("可接任务:\n");
        for (auto& q : g_quests) {
            bool already = false;
            for (int qid : g_player_quests) {
                if (qid == q.id) { already = true; break; }
            }
            if (!already && !q.completed) {
                printf("  [%d] %s - %s\n", q.id, q.name.c_str(), q.desc.c_str());
            }
        }
        return;
    }

    int qid = 0;
    try { qid = std::stoi(args); } catch (...) { qid = 0; }

    for (auto& q : g_quests) {
        if (q.id == qid) {
            bool already = false;
            for (int pq : g_player_quests) {
                if (pq == qid) { already = true; break; }
            }
            if (already) {
                printf("你已经接受了这个任务。\n");
                return;
            }
            g_player_quests.push_back(qid);
            printf("接受了任务: %s\n", q.name.c_str());
            printf("目标: %s\n", q.objective.c_str());
            return;
        }
    }
    printf("找不到任务ID: %d\n", qid);
}

static void cmd_complete(Player* player, const std::string& args) {
    if (args.empty()) {
        printf("用法: complete <任务ID>\n");
        return;
    }

    int qid = 0;
    try { qid = std::stoi(args); } catch (...) { qid = 0; }

    for (auto& q : g_quests) {
        if (q.id == qid) {
            // 检查是否接受
            bool accepted = false;
            for (int pq : g_player_quests) {
                if (pq == qid) { accepted = true; break; }
            }
            if (!accepted) {
                printf("你还没有接受这个任务。\n");
                return;
            }
            if (q.completed) {
                printf("任务已完成。\n");
                return;
            }
            if (q.current_count < q.target_count) {
                printf("任务尚未完成。进度: %d/%d\n", q.current_count, q.target_count);
                return;
            }

            printf("恭喜！任务完成: %s\n", q.name.c_str());
            player_add_exp(player, q.exp_reward);
            player_add_gold(player, q.gold_reward);
            if (q.item_reward_id > 0) {
                Item* it = item_get(q.item_reward_id);
                if (it) player_add_item(player, *it);
            }
            q.completed = true;

            // 从玩家任务列表移除
            auto it = std::find(g_player_quests.begin(), g_player_quests.end(), qid);
            if (it != g_player_quests.end()) g_player_quests.erase(it);
            return;
        }
    }
    printf("找不到任务ID: %d\n", qid);
}

// 事件回调：击杀怪物时更新任务进度
static void on_player_kill_npc(EventType type, Player* player, void* data) {
    (void)type;
    (void)player;
    (void)data;
    for (auto& q : g_quests) {
        bool accepted = false;
        for (int pq : g_player_quests) {
            if (pq == q.id) { accepted = true; break; }
        }
        if (accepted && !q.completed && q.objective.find("击杀") != std::string::npos) {
            q.current_count++;
        }
    }
}

// ---- 模块命令列表 ----
static std::vector<Command> quest_commands = {
    {"quest",    {"task"},   "查看任务",                    cmd_quest},
    {"accept",   {},         "接受任务 (accept <任务ID>)",   cmd_accept},
    {"complete", {"finish"}, "提交任务 (complete <任务ID>)", cmd_complete},
};

// ---- 模块初始化/更新/清理 ----

static void quest_init() {
    printf("[模块E] 任务剧情系统初始化\n");

    // 初始化主线任务
    g_quests = {
        {1, "初入修仙", "踏上修仙之路的第一步",
         "击杀2只野狼", 2, 0, 200, 100, 201, false},
        {2, "灵药采集", "为宗门采集灵草",
         "收集3株灵草", 3, 0, 300, 150, 203, false},
        {3, "妖兽猎人", "清除妖兽森林的威胁",
         "击杀1只妖兽虎", 1, 0, 500, 300, 204, false},
    };

    // 监听击杀事件，自动更新任务进度
    event_listen(EventType::COMBAT_END, on_player_kill_npc);
}

static void quest_tick(Player* player) {
    (void)player;
}

static void quest_cleanup() {
    printf("[模块E] 任务剧情系统清理\n");
}

// ---- 模块导出 ----
Module quest_module = {
    "任务剧情",
    quest_init,
    quest_tick,
    quest_cleanup,
    quest_commands
};