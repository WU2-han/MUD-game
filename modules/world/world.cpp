/* ============================================
 * 模块C：世界地图 & 场景交互
 * 负责人：成员C
 *
 * 依据《游戏基础设定》"二、地图总表" 实现：
 *  - 所有房间初始化数据
 *  - 双向出口跳转
 *  - 境界锁区域
 *  - NPC / 道具摆放
 *  - 场景机缘触发
 *  - map 地图命令
 *
 * 可用接口:
 * - room_get()         获取房间
 * - room_set_exit()    设置出口
 * - room_lock_exit()   设置境界锁
 * - room_add_item()    房间添加道具
 * - room_add_npc()     房间添加NPC
 * - event_listen()     注册机缘监听（PLAYER_ENTER_ROOM）
 * ============================================ */

#include "../include/mud.hpp"
#include <ctime>
#include <set>

// ===== 房间 ID 常量 =====
enum RoomId {
    ROOM_HOME       = 1,   // 个人主页（user住所，出生点）
    ROOM_DISCIPLE   = 2,   // 弟子居所
    ROOM_LECTURE    = 3,   // 传功讲堂
    ROOM_TREASURY   = 4,   // 藏宝阁
    ROOM_ARENA      = 5,   // 淬体演武场
    ROOM_ART        = 6,   // 百艺阁
    ROOM_BEAST      = 7,   // 灵兽囿
    ROOM_HALL       = 8,   // 宗门大殿（中央枢纽）
    ROOM_MOUNT_OUT  = 9,   // 妖兽山脉·外围
    ROOM_MOUNT_IN   = 10,  // 妖兽山脉·内围
    ROOM_MOUNT_CORE = 11,  // 妖兽山脉·核心
    ROOM_MOUNT_FORB = 12,  // 妖兽山脉·禁地

    // ==== 主线剧情新增场景 ====
    ROOM_STUDY      = 13,  // 宗主书房（深夜授青云令）
    ROOM_WENJUAN    = 14,  // 大殿文卷室（查战报）
    ROOM_ZUSHI      = 15,  // 祖师堂（守灵/验尸）
    ROOM_DANDAO     = 16,  // 丹道长老居所（验灵力）
    ROOM_MOUNT_SECRET=17,  // 妖兽山脉·秘洞（见清玄子）
    ROOM_FALONG     = 18,  // 落风谷
    ROOM_FALONG_CUN = 19,  // 落风谷山村（见阿石）
    ROOM_MILIN      = 20,  // 返宗密林（黑衣客伏击）
    ROOM_SHANMEN    = 21,  // 宗门山门（御兽长老）
    ROOM_PLAZA      = 22,  // 大殿广场（BOSS战）
    ROOM_BIANJING   = 23,  // 边境大营（魔月）
    ROOM_DAVIDIAN   = 24,  // 宗门大典（继任宗主场景）
};

// ===== NPC ID 常量 =====
enum NpcId {
    // 妖兽（依据文档"三、妖/灵兽 ④具体妖兽"）
    NPC_HOG       = 101,   // 尖刺豪猪（低阶）
    NPC_WOLF      = 102,   // 腐爪灰狼（低阶）
    NPC_SNAKE     = 103,   // 雾影毒蟒（中阶）
    NPC_BEAR      = 104,   // 岩甲巨熊（中阶）
    NPC_APE       = 105,   // 烈焰魔猿（高阶）
    NPC_DRAGON    = 106,   // 幻海魔蛟（圣兽）

    // 常驻功能 NPC
    NPC_QIAN      = 201,   // 钱掌柜（藏宝阁）
    NPC_MO        = 202,   // 墨长老（传功讲堂）
    NPC_TIE       = 203,   // 铁武师（淬体演武场）
    NPC_SU        = 204,   // 苏玄（百艺阁）
    NPC_HUNTER    = 205,   // 老猎户（灵兽囿）
    NPC_LI        = 206,   // 李执事（宗门大殿）

    // 榜单常驻 NPC
    NPC_XIAO      = 301,   // 萧辰（弟子居所闭关）
    NPC_CHU       = 302,   // 楚狂（妖兽山脉）
    NPC_LIN       = 303,   // 林婉儿（百艺阁）
    NPC_MENG      = 304,   // 孟野（灵兽囿）
};

// ===== 双向连接两个房间 =====
static void link_rooms(int a, Direction dir, int b) {
    room_set_exit(a, dir, b);
    room_set_exit(b, dir_reverse(dir), a);
}

// ===== 场景机缘（进入房间时触发）=====
// 机制由成员C实现，机缘文字 TODO 交由成员E润色扩充。
struct FortuneEvent {
    int room_id;        // 触发房间
    const char* text;   // 机缘文字
    bool one_time;      // 是否仅触发一次
    int chance;         // 触发概率（百分比 0~100）
    int exp_reward;     // 修为奖励
};

static const FortuneEvent g_fortunes[] = {
    // 灵兽囿：老猎户透露高阶秘境入口线索
    { ROOM_BEAST,
      "老猎户眯起眼睛，压低声音道：灵兽囿深处藏着一处高阶灵兽秘境的入口，只待御兽之术精进便可开启。",
      true, 60, 0 },
    // 妖兽山脉·核心：偶遇灵气漩涡
    { ROOM_MOUNT_CORE,
      "你在山脉核心处撞见一团灵气漩涡，隐隐有上古遗物埋藏于此，你凝神感悟，修为略有精进。",
      true, 50, 300 },
    // 妖兽山脉·禁地：上古传承光影
    { ROOM_MOUNT_FORB,
      "禁地深处浮现出上古传承的光影，你屏息参悟，只觉丹田一暖，修为大增。",
      true, 70, 800 },
};

static std::set<int> g_triggered;   // 已触发的一次性机缘（按索引）

static void on_room_enter(EventType type, Player* player, void* data) {
    (void)type; (void)data;
    if (!player) return;

    int room_id = player->current_room_id;
    for (size_t i = 0; i < sizeof(g_fortunes) / sizeof(g_fortunes[0]); i++) {
        const FortuneEvent& f = g_fortunes[i];
        if (f.room_id != room_id) continue;
        if (f.one_time && g_triggered.count((int)i)) continue;

        if (rand() % 100 < f.chance) {
            printf("\n★ 场景机缘 ★\n%s\n", f.text);
            if (f.exp_reward > 0) {
                player->exp += f.exp_reward;
                printf("（修为 +%d）\n", f.exp_reward);
            }
            printf("\n");
            if (f.one_time) g_triggered.insert((int)i);
        }
    }
}

// ===== 初始化 NPC 模板 =====
static void init_npcs() {
    // ---- 妖兽（对齐《修仙大世界MUD》V2.0 妖兽刷怪与指标修正表）----
    // 攻击/血量依据文档，掉落绑定兽核材料
    npc_create(NPC_HOG, "尖刺豪猪", "浑身尖刺的低阶妖兽，惯用冲撞。",
               NPCType::MONSTER, RealmLevel::QI_REFINE,
               200, 36, 9, 120, 0, 220);   // 掉落：兽皮
    npc_create(NPC_WOLF, "腐爪灰狼", "爪牙带腐毒的低阶妖兽，成群出没。",
               NPCType::MONSTER, RealmLevel::QI_REFINE,
               280, 54, 13, 180, 0, 221);  // 掉落：狼爪（血量修正280，对齐文档血280）
    npc_create(NPC_SNAKE, "雾影毒蟒", "潜行雾中的中阶妖兽，剧毒无比，咬伤附带持续毒素。",
               NPCType::MONSTER, RealmLevel::FOUNDATION,
               400, 86, 21, 320, 0, 222);  // 掉落：毒牙（血量修正400，对齐文档）
    npc_create(NPC_BEAR, "岩甲巨熊", "皮糙肉厚的中阶妖兽，单次重击惊人。",
               NPCType::MONSTER, RealmLevel::FOUNDATION,
               520, 104, 26, 400, 0, 223); // 掉落：熊皮（血量修正520）
    npc_create(NPC_APE, "烈焰魔猿", "喷吐烈焰的高阶妖兽，山林霸主，灼烧持续掉血。",
               NPCType::MONSTER, RealmLevel::GOLDEN_CORE,
               800, 172, 43, 750, 0, 225); // 掉落：高阶兽核（血量修正800）
    npc_create(NPC_DRAGON, "幻海魔蛟", "圣兽级妖兽，可施幻术令对手眩晕，极难对付。",
               NPCType::MONSTER, RealmLevel::NASCENT_SOUL,
               1400, 340, 85, 2200, 0, 226);// 掉落：圣兽兽核（血量修正1400，对齐文档）

    // 设定妖兽特性（毒素/灼烧/眩晕）
    NPC* n;
    if ((n = npc_get(NPC_SNAKE)))  { n->dot_damage = 15; n->dot_duration = 3; }
    if ((n = npc_get(NPC_APE)))    { n->dot_damage = 20; n->dot_duration = 3; }
    if ((n = npc_get(NPC_DRAGON))) { n->stun_chance = 30; }

    // ---- 常驻功能 NPC ----
    npc_create(NPC_QIAN, "钱掌柜", "藏宝阁负责人，精明市侩，认灵石不认人。",
               NPCType::MERCHANT, RealmLevel::FOUNDATION,
               800, 50, 30, 0, 0, -1);
    npc_create(NPC_MO, "墨长老", "传功讲堂主讲，古板严谨，极度看重悟性。",
               NPCType::ELDER, RealmLevel::GOLDEN_CORE,
               1500, 90, 50, 0, 0, -1);
    npc_create(NPC_TIE, "铁武师", "淬体演武场武技教习，豪爽粗犷，崇尚肉身强度。",
               NPCType::ELDER, RealmLevel::FOUNDATION,
               1000, 70, 40, 0, 0, -1);
    npc_create(NPC_SU, "苏玄", "百艺阁主事，精通四艺，温和儒雅。",
               NPCType::ELDER, RealmLevel::GOLDEN_CORE,
               1200, 80, 45, 0, 0, -1);
    npc_create(NPC_HUNTER, "老猎户", "灵兽囿管事，高级御兽师，与灵兽心意相通。",
               NPCType::QUEST_GIVER, RealmLevel::GOLDEN_CORE,
               1500, 95, 50, 0, 0, -1);
    npc_create(NPC_LI, "李执事", "宗门大殿庶务执事，公正刻板，按章办事。",
               NPCType::QUEST_GIVER, RealmLevel::GOLDEN_CORE,
               1200, 80, 45, 0, 0, -1);

    // ---- 榜单常驻 NPC ----
    npc_create(NPC_XIAO, "萧辰", "宗主亲传大弟子，元婴期巅峰，常年闭关。",
               NPCType::ELDER, RealmLevel::NASCENT_SOUL,
               3000, 220, 110, 0, 0, -1);
    npc_create(NPC_CHU, "楚狂", "执法长老亲传，化神初期，战斗狂人。",
               NPCType::ELDER, RealmLevel::SPIRIT_TRANS,
               5000, 300, 140, 0, 0, -1);
    npc_create(NPC_LIN, "林婉儿", "丹道长老孙女，高级丹师，常免费炼制低阶丹药。",
               NPCType::MERCHANT, RealmLevel::FOUNDATION,
               900, 60, 35, 0, 0, -1);
    npc_create(NPC_MENG, "孟野", "御兽长老亲传，中级御兽师，契约风啸云狼。",
               NPCType::QUEST_GIVER, RealmLevel::FOUNDATION,
               1100, 85, 45, 0, 0, -1);

    // ---- 主线剧情 NPC ----
    npc_create(501, "凌沧渊", "青云宗掌门，慈和仁厚。被墨阳子背刺身陨，主线核心人物。",
               NPCType::ELDER, RealmLevel::MAHAYANA, 20000, 500, 200, 0, 0, -1);
    npc_create(502, "墨阳子", "传功堂长老，道貌岸然，实为修炼《纯阳噬灵功》的伪君子。", 
               NPCType::ELDER, RealmLevel::MAHAYANA, 8000, 220, 90, 0, 0, -1);
    npc_create(503, "魔月", "魔族圣子，亦正亦邪。坚信魔族从未毁约，与玩家联手促成和平。",
               NPCType::QUEST_GIVER, RealmLevel::VOID_REFINE, 9000, 380, 150, 0, 0, -1);
    npc_create(504, "阿石", "落风谷山村的隐藏杂役，幸存的知情人。",
               NPCType::QUEST_GIVER, RealmLevel::QI_REFINE, 300, 20, 10, 0, 0, -1);
    npc_create(505, "清玄子", "玄阳宗卧底的叛徒，为揭发墨阳子而遭追杀。",
               NPCType::QUEST_GIVER, RealmLevel::SPIRIT_TRANS, 6000, 280, 120, 0, 0, -1);
    npc_create(506, "黑衣死士", "玄阳宗派出的截杀刺客，争夺青云令。",
               NPCType::MONSTER, RealmLevel::SPIRIT_TRANS, 2400, 200, 60, 1200, 300, -1);
    npc_create(507, "巡逻妖将", "边境巡逻的妖怪将领，魔族下属。",
               NPCType::MONSTER, RealmLevel::NASCENT_SOUL, 1400, 160, 50, 900, 200, -1);
}

// ===== 初始化房间 =====
static void init_rooms() {
    // 个人主页 / 住所区
    room_create(ROOM_HOME, "个人主页",
        "你的住所。青瓦小院，灵气环绕，是你在这青云宗的安身之所。可在此修炼、休息、查看自身状态与宗门情况。");
    room_create(ROOM_DISCIPLE, "弟子居所",
        "宗门弟子的居住区，院落错落有致。同门在此起居修行，萧辰等榜上人物亦在此闭关。");

    // 宗门功能建筑
    room_create(ROOM_LECTURE, "传功讲堂",
        "墨长老主讲之地，书声琅琅。可在此听课提升悟性、学习功法，是提升修为的宝地。");
    room_create(ROOM_TREASURY, "藏宝阁",
        "钱掌柜坐镇的藏宝阁，楼阁金碧辉煌。法器、丹药、书籍一应俱全，无品级限制，全身份自由选购。");
    room_create(ROOM_ARENA, "淬体演武场",
        "宽阔的演武场，石锁林立。铁武师在此主持淬体池、弟子切磋与晋升考核，可锤炼肉身、提升实战。");
    room_create(ROOM_ART, "百艺阁",
        "苏玄主事之地，分设丹房、炼器室、符堂三堂。可在此提升炼丹、炼器、画符等技艺熟练度。");
    room_create(ROOM_BEAST, "灵兽囿",
        "老猎户看管的灵兽园，奇珍异兽栖息于此。可契约低阶灵兽，园内藏有高阶灵兽秘境的入口。");
    room_create(ROOM_HALL, "宗门大殿",
        "青云宗的中枢大殿，气势恢宏。李执事在此发放月例、受理晋升申请与宗门任务，是宗门事务的核心所在。");

    // 妖兽山脉（四层）
    room_create(ROOM_MOUNT_OUT, "妖兽山脉·外围",
        "山脉最外层，草木茂密，低阶妖兽横行。是初入修行者历练的好去处。（低阶妖兽区）");
    room_create(ROOM_MOUNT_IN, "妖兽山脉·内围",
        "深入山脉，林深雾重，中阶妖兽出没。修为不足者切勿贸然深入。（中阶妖兽区）");
    room_create(ROOM_MOUNT_CORE, "妖兽山脉·核心",
        "山脉腹地，灵气暴烈，高阶妖兽盘踞。唯有金丹期以上的强者方敢踏足。（高阶妖兽区）");
    room_create(ROOM_MOUNT_FORB, "妖兽山脉·禁地",
        "山脉最深处，禁地封印之地，圣兽级妖兽栖息。传说此处藏着上古传承，危险与机缘并存。（圣兽级区域）");

    // ---- 主线剧情场景 ----
    room_create(ROOM_STUDY, "宗主书房",
        "宗门重地，凌沧渊宗主深夜传召之地。案头灯影摇曳，机要卷宗堆积如山。");
    room_create(ROOM_WENJUAN, "大殿文卷室",
        "宗门大殿之后的文卷重地，战报、账册尽数归档于此。李执事在此当值。");
    room_create(ROOM_ZUSHI, "祖师堂",
        "供奉青云宗历代祖师的庄严殿堂。灵位森森，香火缭绕，掌门遗体曾在此停灵。");
    room_create(ROOM_DANDAO, "丹道长老居所",
        "丹道长老的清修之地，药香弥漫，炉火不熄。可在此请验伤口灵力痕迹。");
    room_create(ROOM_MOUNT_SECRET, "妖兽山脉·秘洞",
        "妖兽山脉深处的隐秘洞穴，被追杀的玄阳宗叛徒清玄子藏身于此。");
    room_create(ROOM_FALONG, "落风谷",
        "宗门之外的山谷要道，常年朔风呼啸，是前往边境战地的必经之路。");
    room_create(ROOM_FALONG_CUN, "落风谷山村",
        "边境附近与世隔绝的小山村，劫后余生，瘢痕斑驳，隐藏着关键人证阿石。");
    room_create(ROOM_MILIN, "返宗密林",
        "回程必经的密林，林深叶茂，潜伏着截杀的黑衣死士。");
    room_create(ROOM_SHANMEN, "宗门山门",
        "云雾缭绕的青云宗山门，御兽长老率弟子镇守于此，戒备森严。");
    room_create(ROOM_PLAZA, "大殿广场",
        "宗门大殿前的开阔广场，各派代表齐聚，正是揭穿伪君子墨阳子的最终战场。");
    room_create(ROOM_BIANJING, "边境大营",
        "正魔两族对峙的边境大营，魔军统帅魔月驻守于此，气氛肃杀。");
    room_create(ROOM_DAVIDIAN, "宗门大典",
        "新君登位的宗门大典现场，万仙来贺，三山五岳各大宗门观礼。");

    // ---- 双向出口（全场景互通）----
    // 宗门大殿为中央枢纽
    link_rooms(ROOM_HALL, Direction::NORTH, ROOM_LECTURE);   // 大殿 ↔ 传功讲堂
    link_rooms(ROOM_HALL, Direction::EAST,  ROOM_TREASURY);  // 大殿 ↔ 藏宝阁
    link_rooms(ROOM_HALL, Direction::SOUTH, ROOM_HOME);      // 大殿 ↔ 个人主页
    link_rooms(ROOM_HALL, Direction::WEST,  ROOM_ARENA);     // 大殿 ↔ 淬体演武场

    // 住所区
    link_rooms(ROOM_HOME, Direction::SOUTH, ROOM_DISCIPLE);  // 个人主页 ↔ 弟子居所

    // 修炼区
    link_rooms(ROOM_ARENA, Direction::SOUTH, ROOM_ART);      // 淬体演武场 ↔ 百艺阁
    link_rooms(ROOM_ART,   Direction::EAST,  ROOM_BEAST);    // 百艺阁 ↔ 灵兽囿

    // 妖兽山脉（经灵兽囿进入，层层深入）
    link_rooms(ROOM_BEAST,      Direction::NORTH, ROOM_MOUNT_OUT);
    link_rooms(ROOM_MOUNT_OUT,  Direction::NORTH, ROOM_MOUNT_IN);
    link_rooms(ROOM_MOUNT_IN,   Direction::NORTH, ROOM_MOUNT_CORE);
    link_rooms(ROOM_MOUNT_CORE, Direction::NORTH, ROOM_MOUNT_FORB);

    // ---- 主线场景通路 ----
    // 宗门大殿 ↔ 宗主书房 / 大殿文卷室
    link_rooms(ROOM_HALL, Direction::UP,   ROOM_STUDY);       // 大殿 → 上楼到书房
    link_rooms(ROOM_HALL, Direction::DOWN, ROOM_WENJUAN);     // 大殿 → 下行到文卷室
    // 丹道长老居所 ↔ 祖师堂（靠宗门大殿东侧相连）
    link_rooms(ROOM_ART,   Direction::SOUTH, ROOM_DANDAO);
    link_rooms(ROOM_DANDAO, Direction::NORTH, ROOM_ZUSHI);
    // 灵兽囿/山脉 ↔ 秘洞
    link_rooms(ROOM_MOUNT_CORE, Direction::EAST, ROOM_MOUNT_SECRET);
    // 山门 ↔ 大殿（山门在宗门大殿之外）
    link_rooms(ROOM_SHANMEN, Direction::SOUTH, ROOM_HALL);
    link_rooms(ROOM_SHANMEN, Direction::NORTH, ROOM_MILIN);   // 山门 → 返宗密林
    link_rooms(ROOM_MILIN,   Direction::NORTH, ROOM_FALONG);  // 密林 → 落风谷
    link_rooms(ROOM_FALONG,  Direction::EAST,  ROOM_FALONG_CUN);
    link_rooms(ROOM_FALONG,  Direction::NORTH, ROOM_BIANJING);// 落风谷 → 边境大营
    link_rooms(ROOM_PLAZA,   Direction::NORTH, ROOM_HALL);    // 大殿广场 ↔ 大殿
    link_rooms(ROOM_DAVIDIAN,Direction::SOUTH, ROOM_PLAZA);   // 大典 ↔ 广场

    // ---- 境界锁（妖兽山脉四层 + 主线）----
    Room* r;
    r = room_get(ROOM_MOUNT_OUT);  if (r) r->min_realm = RealmLevel::QI_REFINE;   // 外围：炼气期
    r = room_get(ROOM_MOUNT_IN);   if (r) r->min_realm = RealmLevel::FOUNDATION;  // 内围：筑基期
    r = room_get(ROOM_MOUNT_CORE); if (r) r->min_realm = RealmLevel::GOLDEN_CORE; // 核心：金丹期
    r = room_get(ROOM_MOUNT_FORB); if (r) r->min_realm = RealmLevel::NASCENT_SOUL;// 禁地：元婴期

    // ---- 房间属性：妖兽山脉为非安全区（可战斗）----
    for (int id = ROOM_MOUNT_OUT; id <= ROOM_MOUNT_FORB; id++) {
        Room* rr = room_get(id);
        if (rr) rr->is_safe_zone = false;
    }
    Room* rfl = room_get(ROOM_MILIN); if (rfl) rfl->is_safe_zone = false;   // 返宗密林可战斗
    Room* rpl = room_get(ROOM_PLAZA); if (rpl) rpl->is_safe_zone = false;   // 广场BOSS战可战斗
    Room* rfg = room_get(ROOM_FALONG); if (rfg) rfg->is_safe_zone = false;  // 落风谷可战斗
}

// ===== 摆放 NPC =====
static void place_npcs() {
    // 功能 NPC
    room_add_npc(ROOM_TREASURY, NPC_QIAN);   // 藏宝阁：钱掌柜
    room_add_npc(ROOM_LECTURE,  NPC_MO);     // 传功讲堂：墨长老
    room_add_npc(ROOM_ARENA,    NPC_TIE);    // 淬体演武场：铁武师
    room_add_npc(ROOM_ART,      NPC_SU);     // 百艺阁：苏玄
    room_add_npc(ROOM_BEAST,    NPC_HUNTER); // 灵兽囿：老猎户
    room_add_npc(ROOM_HALL,     NPC_LI);     // 宗门大殿：李执事

    // 榜单 NPC
    room_add_npc(ROOM_DISCIPLE, NPC_XIAO);   // 萧辰（闭关）
    room_add_npc(ROOM_MOUNT_IN, NPC_CHU);    // 楚狂（常驻妖兽山脉）
    room_add_npc(ROOM_ART,      NPC_LIN);    // 林婉儿（百艺阁）
    room_add_npc(ROOM_BEAST,    NPC_MENG);   // 孟野（灵兽囿）

    // 妖兽（妖兽山脉四层）
    room_add_npc(ROOM_MOUNT_OUT,  NPC_HOG);   // 外围：尖刺豪猪
    room_add_npc(ROOM_MOUNT_OUT,  NPC_WOLF);  // 外围：腐爪灰狼
    room_add_npc(ROOM_MOUNT_IN,   NPC_SNAKE); // 内围：雾影毒蟒
    room_add_npc(ROOM_MOUNT_IN,   NPC_BEAR);  // 内围：岩甲巨熊
    room_add_npc(ROOM_MOUNT_CORE, NPC_APE);   // 核心：烈焰魔猿
    room_add_npc(ROOM_MOUNT_FORB, NPC_DRAGON);// 禁地：幻海魔蛟

    // ---- 主线剧情 NPC 摆放 ----
    room_add_npc(ROOM_STUDY,      501);   // 宗主书房：凌沧渊
    room_add_npc(ROOM_PLAZA,      502);   // 大殿广场：墨阳子（第八章 BOSS 战，对齐策划 8-2）
    room_add_npc(ROOM_MOUNT_CORE, 503);   // 妖兽山脉·核心:魔月（对齐策划 5-1 山林相遇）
    room_add_npc(ROOM_FALONG_CUN, 504);   // 落风谷山村：阿石
    room_add_npc(ROOM_MOUNT_SECRET,505);  // 秘洞：清玄子
    room_add_npc(ROOM_MILIN,      506);   // 返宗密林：黑衣死士
    room_add_npc(ROOM_FALONG,     507);   // 落风谷：巡逻妖将
}

// ===== 摆放地面道具 =====
// 注：道具表由成员D负责，这里仅引用已有模板做占位，后续随 D 的道具表更新。
static void place_items() {
    room_add_item(ROOM_HOME, 201);       // 个人主页：疗伤丹（新手福利）
    room_add_item(ROOM_HOME, 202);       // 个人主页：回灵丹（新手福利）
    room_add_item(ROOM_MOUNT_OUT, 207);  // 外围：灵草
    room_add_item(ROOM_MOUNT_IN,  208);  // 内围：妖丹
}

// ===== map 命令 =====
static void cmd_map(Player* player, const std::string& args) {
    (void)args;
    Room* cur = room_get(player->current_room_id);
    printf("\n════════════ 青云宗 · 世界地图 ════════════\n\n");
    printf("  宗门大殿 (8)  [中央枢纽]\n");
    printf("  ├─↕ 传功讲堂 (3)      ├─↥ 宗主书房 (13，主线)\n");
    printf("  ├─↔ 藏宝阁 (4)        └─↧ 大殿文卷室 (14，主线)\n");
    printf("  ├─↕ 个人主页 (1)\n");
    printf("  │   └─↕ 弟子居所 (2)\n");
    printf("  ├─↔ 淬体演武场 (5)\n");
    printf("  │   └─↕ 百艺阁 (6)\n");
    printf("  │       ├─↔ 灵兽囿 (7)\n");
    printf("  │       │   └─↕ 妖兽山脉·外围 (9)  [炼气期]\n");
    printf("  │       │       └─↕ 内围 (10) [筑基期]\n");
    printf("  │       │           └─↕ 核心 (11) [金丹期] ⇢ 秘洞 (17,主线)\n");
    printf("  │       │               └─↕ 禁地 (12) [元婴期]\n");
    printf("  │       └─↕ 丹道长老居所 (16,主线) ⇢ 祖师堂 (15,主线)\n");
    printf("  └─↕ 宗门山门 (21) ⇢ 返宗密林 (20) ⇢ 落风谷 (18)\n");
    printf("      ├─↔ 落风谷山村 (19,主线)      └─↕ 边境大营 (23,主线)\n");
    printf("  ├─↔ 大殿广场 (22,BOSS) ↔ 宗门大典 (24)\n");
    printf("\n  你当前所在: %s\n", cur ? cur->name.c_str() : "未知");
    printf("  注：↕/↔/↥↧ = 双向通道；(主线) 剧情场景由主线任务解锁\n");
    printf("\n════════════════════════════════════════\n\n");
}

// ---- 模块命令列表 ----
static std::vector<Command> world_commands = {
    {"map", {}, "查看世界地图", cmd_map},
};

// ---- 模块初始化/更新/清理 ----

static void world_mod_init() {
    printf("[模块C] 世界地图初始化\n");

    srand((unsigned)time(nullptr));

    init_npcs();
    init_rooms();
    place_npcs();
    place_items();

    // 注册机缘触发监听
    event_listen(EventType::PLAYER_ENTER_ROOM, on_room_enter);

    printf("[模块C] 房间数: %d, NPC模板数: %d\n", room_count(), npc_count());
}

static void world_mod_tick(Player* player) {
    (void)player;
}

static void world_mod_cleanup() {
    printf("[模块C] 世界地图清理\n");
    g_triggered.clear();
}

// ---- 模块导出 ----
Module world_module = {
    "世界地图",
    world_mod_init,
    world_mod_tick,
    world_mod_cleanup,
    world_commands
};
