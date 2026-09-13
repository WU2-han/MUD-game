/* ============================================
 * 模块E：策划 / 剧情、任务、全部文本素材
 * 负责人：成员E
 *
 * 主推：主线剧情任务链《沧渊遗恨·正邪辨》V3.0 重排
 *
 * 剧情顺序（V3.0 定版）：
 *   拜师（晋升亲传后，书房 story 触发）
 *   → 序章养成（日常渗透，自由活动）
 *   → 第一章·师父率军出征 → 第二章·师父战死
 *   → 第三章~第七章·查寻真相（引导详见 story 文档）
 *   → 第八章·真相大白 → 养成时间2（自由修炼，决战时机由玩家自行决定）
 *   → 最终决战（大殿广场输入 决战，玩家自行触发）
 *   → 胜利 = 主线完结（大字页 + 奶蛙/师父残魂对话，解锁飞升）
 *   → 失败 = 「拉完了」成就页（重来一世读档 / 嘉豪文案从头开始）
 *
 * 机制：
 *   1. 剧情一律由 story 命令在目标地点触发（不再进房自动触发）；
 *      剧情以「分段文本」呈现：一段一段输出、回车继续，结束标（）。
 *   2. 走剧情阶段（拜师、第一~七章）在触发条件满足时禁止自由活动，只能跟随引导推进；
 *      触发条件未满足（含主线未开启）时保持自由活动，玩家可退出剧情模式自行准备；
 *      养成时间（序章、真相大白后）完全自由。
 *   3. 主线剧情期间（含养成时间）飞升受限，境界止步大乘期；
 *      击败墨阳子、剧情完结后方可突破飞升。
 *   4. 最终决战由玩家自行触发，决战前自动存档；
 *      胜负皆不强制，战斗过程与寻常斗法无异。
 * ============================================ */

#include "../include/mud.hpp"
#include <set>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// 前向声明
bool player_add_exp(Player* player, int amount);
void player_add_gold(Player* player, int amount);
bool player_spend_gold(Player* player, int amount);
bool player_add_item(Player* player, const Item& item);
Item* item_get(int id);
NPC* npc_get(int id);
void event_listen(EventType type, EventCallback callback);
void event_emit(EventType type, Player* player, void* data);

// ---- 剧情步骤 ----
struct StoryStep {
    int room;            // 触发房间（0=任意）
    int rank_floor;      // 触发宗门地位下限（主线门槛：亲传=3）
    int day_offset;      // 触发游戏日下限偏移（相对主线起始日 story_day0，D+n 天）
    int cost;            // 需花费灵石（0无）
    int item_need;       // 需持有道具（0无）
    bool need_beat_boss; // 需已击败墨阳子(NPC 502)（V3.0 起不再使用，保留字段）
    int prestige_floor;  // 需宗门威望下限（V3.0 起不再使用，保留字段）
    const char* title;   // 章节标题
    const char* text;    // 剧情文本
    // 奖励
    int r_exp, r_gold, r_item, r_qty, r_con, r_wu, r_prof, r_prestige;
    const char* tag;     // 获得的标签/线索（""无；加前缀"~"表示称号）
    int r_spi = 0;       // 灵力奖励（置于末尾，旧步骤未写则默认 0）
};

// 最终决战所处 phase（养成时间2：真相大白后自由活动，决战时机由玩家自行决定）
static const int PHASE_FINAL_DUEL = 23;

static const StoryStep g_story[] = {
    // ============ 拜师：承道入门（亲传弟子，书房 story 触发）============
    { 13, 3, 0, 0, 0, false, 0, "拜师·承道入门",
      "你叩响宗主书房的门扉，片刻，门内传来一声沉稳的「进」。\n"
      "凌沧渊端坐于案后，手执一卷泛黄的古籍。抬眼见是你，他放下书卷，上下打量良久，忽然笑了：「修为尚可，心性也算沉稳——金丹境便敢闯亲传考核，倒有几分当年为师的气魄。」\n"
      "你依宗门古礼，郑重拜下：「弟子拜见师父！」\n"
      "凌沧渊起身虚扶，亲手为你整了整衣襟，声音低沉：「既入我门，你便是我凌沧渊唯一的关门弟子。从今往后，为师传你功法、护你周全——只望你守住本心，莫负青云宗百年道统。」\n"
      "他说到「青云宗」三字时，目光望向窗外远山，似有深意，许久才低声道：「近日天下恐有变数……你且安心修行，为师自有计较。」",
      200, 0, 0, 0, 0, 0, 0, 0, "", 0 },

    // ============ 序章：故交美名·日常渗透（养成时间1，自由活动）============
    { 3, 3, 0, 0, 0, false, 0, "序章·讲堂听道",
      "墨长老于传功讲堂宣讲先贤遗训，你端坐前排，听得入神。讲毕，你上前请教：「长老，当今正道除师父外，还有哪位前辈堪称表率？」\n"
      "墨长老抚须大笑，眼中满是敬意：「那必是玄阳宗墨阳子宗主！当年老朽修为卡滞金丹三十年，是他无偿赠我《纯阳悟道札记》，毫无门户之见。此人心怀苍生，实乃道门风骨。」\n"
      "你点头记下，隐约觉得——这名字，近来总在耳边响起。",
      150, 0, 0, 0, 0, 1, 0, 0, "", 0 },
    { 5, 3, 1, 0, 0, false, 0, "序章·演武切磋",
      "铁武师邀你切磋一局，拳脚往来间，你筋骨渐强。战后他拍着你的肩感慨：「你这点伤算啥！当年俺在妖兽山脉被三阶妖兽围杀，眼看没命，是墨阳宗主路过一剑斩了妖兽，还扔给俺一瓶上品养神丹。堂堂一宗之主，对俺个外门弟子都仗义，那才叫真豪杰！」",
      120, 0, 0, 0, 2, 0, 0, 0, "", 0 },
    { 6, 3, 2, 0, 0, false, 0, "序章·百艺论丹",
      "你在百艺阁与苏玄论丹，炼毕一炉下品淬体丹。苏玄含笑提起：「上月墨阳宗主还遣人送来玄阳宗独家《聚火丹方》，与我互补丹道。他与你师父是八拜之交，两宗亲如一家。你日后若去玄阳宗，尽可受他照拂。」",
      0, 0, 301, 2, 0, 0, 80, 0, "", 0 },
    { 4, 3, 3, 100, 0, false, 0, "序章·藏宝购物",
      "你在藏宝阁购得所需，结账时钱掌柜念叨：「要说正道里最讲情义的，还得是墨阳宗主。玄阳宗的商队从不压价，还特意给青云宗货物加价一成收。人家那格局，不愧是一宗之主。」\n"
      "你对这位素未谋面的师伯，留下了极佳印象。",
      0, 0, 0, 0, 0, 0, 0, 0, "墨阳子·仁厚印象", 0 },

    // ============ 第一章：烽烟骤起·临危受命（走剧情，锁定自由活动）============
    { 8, 3, 4, 0, 0, false, 0, "第一章·警钟骤响",
      "这一日，宗门警钟长鸣，声震九霄！李执事急传军报：边境魔族撕毁三百年和平条约，连灭三宗，屠戮修士上千。凌沧渊震怒，决意亲率大军出征。\n"
      "墨阳子一身白衣，温文尔雅，主动请缨担任联军副帅，当众表态：「凌兄，你我兄弟同心，定将魔族击退，护我正道安宁！」众皆动容，赞声如潮。\n"
      "你站在人群里，望着那道白衣身影，忽然想起师父书房窗前的那句话——近日天下恐有变数。",
      200, 0, 0, 0, 0, 0, 0, 0, "", 0 },
    { 13, 3, 4, 0, 0, false, 0, "第一章·深夜授命",
      "出征前夜，凌沧渊在书房独召你。他负手立于窗前，灯火将他的身影拉得颀长，许久方转过身来，神色凝重，从袖中取出一枚不起眼的玄黑令牌，郑重递到你手中：「此乃青云宗镇宗青云令，掌控护山大阵与宗门传承命脉。为师此去，心中总觉不安。你替我收好——切记不可示人，更不可交给任何人，哪怕是你墨阳师伯。」\n"
      "他顿了顿，目光深处似有一丝难以察觉的复杂，最终只化作一声长叹：「去吧，好生歇息。记住——令牌在，青云宗就在。」",
      300, 0, 601, 1, 0, 0, 0, 0, "", 10 },
    { 21, 3, 5, 0, 0, false, 0, "第一章·大军出征",
      "山门前，大军开拔，旌旗蔽日。墨阳子特意停下，拍了拍你的肩膀，温笑道：「师侄放心，我与你师父情同手足，定会护他周全。等我们大胜归来，我再指点你几招纯阳剑法。」\n"
      "凌沧渊深深看了你一眼，转身率军离去。尘土漫天，那道背影渐渐没入远方——你没想到，那是你最后一次见到他完好地站在你面前。",
      100, 0, 0, 0, 0, 0, 0, 0, "", 0 },

    // ============ 第二章：捷报悲音·恩师陨落（走剧情）============
    { 1, 3, 6, 0, 0, false, 0, "第二章·捷报虚堂",
      "前线捷报频传：联军势如破竹，凌沧渊与墨阳子并肩作战，斩杀魔族大将三名，魔族节节败退。宗门上下张灯结彩，都等着宗主凯旋。\n"
      "你却在连番捷报中，嗅到一丝说不清道不明的不安——师父出征前的那个眼神，总在你心头挥之不去。",
      150, 200, 0, 0, 0, 0, 0, 0, "", 0 },
    { 8, 3, 7, 0, 0, false, 0, "第二章·噩耗惊天",
      "三更时分，一阵急促的钟声撕破寂静，随之而来的是哭喊与奔走之声。噩耗突传——凌沧渊宗主追击残部时遭遇埋伏，力战殉国！执法、丹道二位长老重伤昏迷。\n"
      "你如遭雷击，跌跌撞撞奔至大殿。墨阳子一身素服，血染衣襟，亲自扶棺回宗。他跪在灵堂前伏棺痛哭，哭到呕血：「凌兄！你我约定共守正道百年，你怎可先走一步！」\n"
      "他红着眼当众表态：「凌兄不在，我定替他照拂宗门，辅佐师侄稳住局面。」在场众人无不动容，皆赞他重情重义。\n"
      "唯有你盯着那口棺木，浑身发冷——师父明明说过，他此行「心中总觉不安」。",
      0, 0, 0, 0, 0, 0, 0, 0, "", 0 },
    { 15, 3, 8, 0, 0, false, 0, "第二章·守灵观尸",
      "灵堂内烛火摇曳，纸钱纷飞，你为师父守灵。趁四下无人，你近前验看遗体：正面布满魔族爪痕，狰狞可怖；致命伤却在后心——一道细小的剑伤，灵力纯正中正，绝非魔族功法。\n"
      "你心中剧震，正要细看，重伤的丹道长老悠悠转醒，一把抓住你的手腕，面色煞白，气息奄奄：「宗主……宗主的伤……有些古怪……你……莫要声张……」话未说完，又昏厥过去。",
      200, 0, 0, 0, 0, 0, 0, 0, "致命纯阳剑伤", 0 },

    // ============ 第三章：疑窦初生·蛛丝马迹（走剧情）============
    { 14, 3, 8, 0, 0, false, 0, "第三章·查战报",
      "你以亲传弟子身份调取战报名册，逐行比对，发现两处破绽：最后追击的队伍里，青云宗仅二十人，玄阳宗却有百人之多，最终生还者全是玄阳宗弟子，青云宗无一生还；战报前文写「魔族伏兵上千」，后文却写「斩杀残部三百」，数字矛盾，且有明显涂改。\n"
      "李执事坦言：「战报是玄阳宗那边整理送来的，我当时也觉得奇怪，但墨阳宗主威名在外，无人敢质疑。」",
      200, 0, 0, 0, 0, 0, 0, 0, "战报涂改痕迹", 0 },
    { 16, 3, 8, 0, 0, false, 0, "第三章·验灵力",
      "你请丹道长老鉴定致命伤的灵力属性。他强撑伤体，神色凝重探查良久，面色煞白：「是玄阳宗的纯阳剑气，金丹以上高手全力一击，绝非魔族能伪装。」\n"
      "他再三叮嘱：「此事事关重大，没有铁证之前万万不可声张，否则会动摇正道根基。」",
      0, 0, 307, 2, 0, 0, 0, 0, "纯阳剑气鉴定", 0 },
    { 4, 3, 8, 200, 0, false, 0, "第三章·查账目",
      "你付了200灵石查账酬劳，钱掌柜翻查账本，越翻眉头越紧：「近半年玄阳宗疯狂收购高阶兽核、培元丹、淬体丹，数量远超正常宗门消耗。还有啊——墨阳宗主上个月私下问我，青云宗有没有镇宗的令牌类宝物。我当时只当闲聊，现在想来，着实反常。」",
      0, 0, 0, 0, 0, 0, 0, 0, "异常交易记录", 0 },
    { 7, 3, 8, 0, 0, false, 0, "第三章·探山情",
      "老猎户压低声音，目光闪烁：「妖兽山脉深处最近有零散魔族出没，却不主动攻击人，反而躲躲藏藏。我还撞见玄阳宗弟子偷偷潜入禁地，不像是打怪，倒像在找什么东西。你若要往边境去，千万小心。」",
      0, 0, 0, 0, 0, 0, 0, 50, "玄阳宗禁地异动", 0 },

    // ============ 第四章：战地寻踪（走剧情）============
    { 18, 3, 9, 0, 0, false, 0, "第四章·潜行出宗",
      "你乔装散修，避开宗门视线潜行出宗，直奔边境落风谷。沿途遭遇两波腐爪灰狼，你且战且走，终抵当年战场遗迹——断剑残旗，血迹已干，荒草萋萋。\n"
      "风穿过谷口，呜呜作响，像是亡者的呜咽。你蹲下身，指尖抚过地面一道深深的沟壑——那是剑气劈出的痕迹，笔直、凌厉，与妖兽的爪痕截然不同。",
      400, 0, 0, 0, 0, 0, 0, 0, "", 0 },
    { 19, 3, 10, 0, 0, false, 0, "第四章·山村访证",
      "你几经辗转，寻到隐姓埋名的杂役弟子阿石。他起初矢口否认，直到你出示亲传弟子令牌，他才红了眼眶，哭着道出真相：「那天宗主追魔族跑在最前，墨阳宗主跟在他身后，忽然拔剑刺进宗主后心！我听见他说：凌兄，别怪我，为了正道大业，牺牲你一个，能换天下太平。然后他喊有埋伏，玄阳宗的人一起动手，把咱们青云宗兄弟都杀了！」\n"
      "他递来一块混战中捡到的玄阳宗长老令牌碎片作证，又抹了把泪，压低声音：「墨阳子心狠手辣，你千万小心。」\n"
      "你如遭雷击——那个人人称颂的仁厚宗主，真的是杀害师父的凶手？",
      400, 0, 603, 1, 0, 0, 0, 0, "阿石证词", 0 },

    // ============ 第五章：魔窟遇故（走剧情）============
    { 11, 3, 11, 0, 0, false, 0, "第五章·魔月之遇",
      "你于妖兽山脉核心遭遇魔族小队，为首将领魔月瞥见你腰间青云令，当即喝止手下收兵，大步上前打量你良久：「你是凌沧渊的弟子？他倒是个言而有信的人，可惜死得不明不白。」\n"
      "你心头一紧，追问师父死因。魔月冷笑：「你们正道口口声声魔族毁约？可笑！三百年和平，我们从未越界。是玄阳宗的人潜入我族边境，屠了三个部落，连老人孩子都不放过，抽走灵元修炼邪功，再把罪名扣在我们头上。你师父大战第二天就发现了，私下找我说要查明真相——结果第三天，他就死了。」\n"
      "说罢，魔月将一枚记忆晶石掷入你手中：「这是玄阳宗屠戮的证据。你师父是好人，但你们正道，未必都是好人。」",
      350, 0, 602, 1, 0, 0, 0, 0, "", 0 },
    { 0, 3, 11, 0, 602, false, 0, "第五章·晶石显影",
      "你寻了个僻静处，注入灵力，记忆晶石中缓缓浮现画面：玄阳宗弟子屠戮魔族边境村落，连老人孩子都不放过，活生生吸取灵元修炼，为首者正是墨阳子的亲传弟子。\n"
      "画面一转，是你师父凌沧渊的身影——他站在边境，望着那堆焦黑的废墟，神色沉重。\n"
      "你终于明白——所谓的魔族入侵，根本是墨阳子自导自演：屠戮魔族吸取灵元，再嫁祸魔族挑起战争，只为借大战之机除掉师父这个障碍。",
      0, 0, 0, 0, 0, 2, 0, 0, "屠村实证", 0 },

    // ============ 第六章：途中遇刺（走剧情）============
    { 20, 3, 12, 0, 0, false, 0, "第六章·密林伏杀",
      "返宗途中行至密林，三名黑衣刺客猝然杀出，招招致命，口中大喝：「交出青云令，饶你不死！」其功法路数尽是玄阳宗招式。\n"
      "你拼死击败刺客，从为首者身上搜出一枚密令，落款墨阳子私印，赫然写着：「夺回青云令，格杀勿论。」\n"
      "墨阳子，终于撕下了伪装，对你动了杀心。",
      500, 300, 0, 0, 0, 0, 0, 0, "刺杀密令", 0 },
    { 21, 3, 13, 0, 0, false, 0, "第六章·星夜返宗",
      "你星夜兼程赶回宗门，山门处，御兽长老悄悄将你拉到暗处，神情焦灼：「墨阳子已以『辅佐少主』为名进驻大殿，多次索要青云令，称『替师侄保管，免得被奸人所夺』。是老朽以『宗主遗命，令牌由亲传弟子保管』为由苦苦压了下来——但，也撑不了多久了。」",
      0, 0, 0, 0, 0, 0, 0, 0, "", 0 },

    // ============ 第七章：叛道证人（走剧情）============
    { 4, 3, 14, 500, 0, false, 0, "第七章·钱掌柜牵线",
      "你花500灵石托钱掌柜牵线。他听完你收集的所有证据，沉吟良久，终于相信墨阳子是伪君子：「清玄道长是墨阳子的师弟，因反对以魔养道被玄阳宗追杀，如今躲在妖兽山脉深处。我可以帮你联络，但你千万小心——他若知道你查到了这一步，必不会善罢甘休。」",
      0, 0, 0, 0, 0, 0, 0, 0, "", 0 },
    { 17, 3, 15, 0, 0, false, 0, "第七章·秘洞之证",
      "妖兽山脉秘洞中，清玄道长听完来意，长叹一声，痛斥墨阳子：「他修炼禁术《纯阳噬灵功》，靠吞噬生灵灵元快速突破，口口声声为了正道，其实就是想当正道盟主！」\n"
      "他呈上两样铁证：墨阳子亲笔修炼手札，详录屠戮魔族、吸取灵元的过程，以及「除掉凌沧渊，夺取青云令，掌控青云宗」的计划；还有写给部下的密信，白纸黑字写着「战时背后出手，伪造魔族埋伏」。\n"
      "铁证如山！你攥紧那卷手札，指节发白——师父的血债，终于有了偿还之日。",
      600, 0, 0, 0, 0, 3, 0, 0, "墨阳子亲笔修炼手札", 0 },

    // ============ 第八章：最终决战（养成时间2，玩家自行触发）============
    { 22, 3, 0, 0, 0, false, 0, "第八章·最终决战",
      "大殿广场，黑云压顶，山雨欲来。\n"
      "墨阳子一身玄袍，负手立于广场中央，周身【大乘期】灵力如渊似海，再不见半分往日的仁厚温谦：「小辈，你查得倒是仔细。可那又如何？今日这青云令，我要定了！」\n"
      "你缓缓拔出长剑，剑尖遥指：「师父的仇，青云宗的债——今日，一并清算！」",
      0, 0, 0, 0, 0, 0, 0, 0, "", 0 },
};

static const int g_story_count = sizeof(g_story) / sizeof(g_story[0]);
static_assert(PHASE_FINAL_DUEL < g_story_count, "PHASE_FINAL_DUEL 越界");

// ---- 引导：每步任务目标（与 g_story 按 phase 一一对应）----
static const char* g_step_goals[] = {
    "前往宗主书房，面见凌沧渊宗主，正式拜师",
    "在传功讲堂听墨长老讲道，打听当今正道的名宿前辈",
    "与铁武师切磋一局，听他讲起墨阳宗主的仗义往事",
    "在百艺阁炼一炉淬体丹，与苏玄交流丹道",
    "到藏宝阁购买一件商品，听钱掌柜念叨墨阳宗主的商誉",
    "前往宗门大殿——魔族入侵了！",
    "深夜前往宗主书房，接受师父的托付",
    "到宗门山门为出征大军送行",
    "回个人主页接收前线捷报",
    "前往宗门大殿，接收前线的噩耗",
    "到祖师堂为师父守灵，仔细查验遗体",
    "到大殿文卷室调取战报与名册，查证其中破绽",
    "到丹道长老居所，请长老鉴定致命伤的灵力属性",
    "到藏宝阁付 200 灵石，请钱掌柜查玄阳宗的交易记录",
    "到灵兽囿向老猎户打听边境妖兽山脉的异动",
    "乔装潜行，前往边境落风谷战场遗迹",
    "在落风谷山村寻访当年的幸存者阿石",
    "在妖兽山脉核心遭遇魔族守将魔月，取得记忆晶石",
    "注入灵力，查看记忆晶石中的画面",
    "在返宗密林击败墨阳子派来的黑衣刺客",
    "星夜赶回宗门山门，向御兽长老了解宗门局势",
    "到藏宝阁付 500 灵石，请钱掌柜牵线清玄道长",
    "到妖兽山脉秘洞面见清玄道长，获取墨阳子亲笔罪证",
    "真相大白！可前往大殿广场输入 决战 与墨阳子了断！（此战凶险，建议先养精蓄锐，但何时决战由你自己决定）",
};
static_assert(sizeof(g_step_goals) / sizeof(g_step_goals[0]) == g_story_count,
              "g_step_goals 与 g_story 条数不一致");

// ---- 引导：章节时间线（days 为相对主线开启日的剧情时间）----
struct GuideChapter {
    const char* name;   // 章节名
    int first;          // 起始 phase
    int last;           // 结束 phase
    const char* days;   // 剧情时间（第1日 = 拜师日）
};
static const GuideChapter g_chapters[] = {
    { "拜师·承道入门",   0,  0,  "第 1 日"        },
    { "序章·故交美名",   1,  4,  "第 1-4 日"      },
    { "第一章·烽烟骤起", 5,  7,  "第 5-6 日"      },
    { "第二章·捷报悲音", 8,  10, "第 7-9 日"      },
    { "第三章·疑窦初生", 11, 14, "第 9 日"       },
    { "第四章·战地寻踪", 15, 16, "第 10-11 日"   },
    { "第五章·魔窟遇故", 17, 18, "第 12 日"      },
    { "第六章·途中遇刺", 19, 20, "第 13-14 日"   },
    { "第七章·叛道证人", 21, 22, "第 15-16 日"   },
    { "第八章·最终决战", 23, 23, "第 17 日后·自由择期" },
};

// ---- 寻路：BFS 求 from→to 的最短房间序列（含首尾；不可达返回空）----
static std::vector<int> find_path(int from, int to) {
    if (from == to) return { from };
    std::map<int, int> prev;
    std::set<int> seen;
    std::vector<int> q{ from };
    seen.insert(from);
    while (!q.empty()) {
        int cur = q.front();
        q.erase(q.begin());
        Room* r = room_get(cur);
        if (!r) continue;
        for (int d = 0; d < 6; d++) {
            int nxt = r->exits[d].room_id;
            if (nxt <= 0 || seen.count(nxt)) continue;
            prev[nxt] = cur;
            if (nxt == to) {
                std::vector<int> path;
                for (int x = to; x != from; x = prev.at(x)) path.push_back(x);
                path.push_back(from);
                std::reverse(path.begin(), path.end());
                return path;
            }
            seen.insert(nxt);
            q.push_back(nxt);
        }
    }
    return {};
}

// 路线文本：如「个人主页 → s 弟子居所 → s 宗门山门」
static std::string path_str(const std::vector<int>& path) {
    std::string s;
    for (size_t i = 0; i + 1 < path.size(); i++) {
        Room* a = room_get(path[i]);
        Room* b = room_get(path[i + 1]);
        if (!a || !b) continue;
        int dir = -1;
        for (int d = 0; d < 6; d++)
            if (a->exits[d].room_id == path[i + 1]) { dir = d; break; }
        if (dir < 0) continue;
        if (!s.empty()) s += " → ";
        s += std::string(dir_key_name(static_cast<Direction>(dir))) + " " + b->name;
    }
    return s;
}

static bool has_item(const Player* p, int item_id) {
    for (const auto& it : p->inventory) if (it.id == item_id) return true;
    return false;
}

// 记录标签/称号（前缀"~"为称号）
static void grant_tag(Player* p, const char* tg) {
    if (!tg || !*tg) return;
    if (tg[0] == '~') {                   // 称号
        p->title = tg + 1;
        printf("（获得称号：【%s】）\n", p->title.c_str());
        return;
    }
    if (p->tags.find(tg) != std::string::npos) return; // 去重
    if (!p->tags.empty()) p->tags += "、";
    p->tags += tg;
    printf("（获得标签/线索：【%s】）\n", tg);
}

// 主线起始日锚点（旧存档无 story_day0 时按第 0 天起算，仅作兜底）
static int anchor_day(const Player* p) {
    return p->story_day0 > 0 ? p->story_day0 : 0;
}

static bool step_ready(const Player* p, const StoryStep& s) {
    if (p->sect_rank < s.rank_floor) return false;
    if (s.day_offset > 0 && p->day < anchor_day(p) + s.day_offset) return false;
    if (s.cost > 0 && p->gold < s.cost) return false;
    if (s.item_need > 0 && !has_item(p, s.item_need)) return false;
    if (s.prestige_floor > 0 && p->prestige < s.prestige_floor) return false;
    if (s.need_beat_boss) {
        NPC* b = npc_get(502);
        if (b && b->is_alive) return false;
    }
    return true;
}

static void apply_story_rewards(Player* p, const StoryStep& s) {
    if (s.r_exp > 0) player_add_exp(p, s.r_exp);
    if (s.r_gold > 0) player_add_gold(p, s.r_gold);
    if (s.r_item > 0) {
        Item* tmpl = item_get(s.r_item);
        if (tmpl) {
            Item it = *tmpl;
            it.quantity = s.r_qty > 0 ? s.r_qty : 1;
            if (it.quantity > 1)
                printf("（获得剧情奖励：%s x%d）\n", it.name.c_str(), it.quantity);
            else
                printf("（获得剧情奖励：%s）\n", it.name.c_str());
            player_add_item(p, it);
        }
    }
    if (s.cost > 0) player_spend_gold(p, s.cost);
    if (s.r_con > 0) { p->con += s.r_con; printf("（体质 +%d）\n", s.r_con); }
    if (s.r_spi > 0) { p->spi += s.r_spi; printf("（灵力 +%d）\n", s.r_spi); }
    if (s.r_wu > 0)  { p->wu  += s.r_wu;  printf("（悟性 +%d）\n", s.r_wu);  }
    if (s.r_prof > 0){  // 四艺通用奖励
        p->prof_alchemy  = std::min(10000, p->prof_alchemy  + s.r_prof);
        p->prof_forge    = std::min(10000, p->prof_forge    + s.r_prof);
        p->prof_talisman = std::min(10000, p->prof_talisman + s.r_prof);
        p->prof_beast    = std::min(10000, p->prof_beast    + s.r_prof);
    }
    if (s.r_prestige > 0) { p->prestige += s.r_prestige;
                            printf("（宗门威望 +%d）\n", s.r_prestige); }
    if (s.tag) grant_tag(p, s.tag);
    player_recalc_stats(p);
}

static void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

// ---- 清屏（全页特效用）----
static void console_clear() {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
#endif
}

// 全屏居中大字（模拟「页面正中间一行大字」）
static void show_big_text(const std::string& text) {
    console_clear();
    printf("\n\n\n");
    std::string spaced;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        int len = 1;
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        spaced += text.substr(i, len);
        spaced += "   ";
        i += len;
    }
    int total = display_width(spaced);
    int pad = std::max(2, (78 - total) / 2);
    printf("%s%s\n", std::string(pad, ' ').c_str(), spaced.c_str());
    printf("\n\n\n\n");
}

// 逐句显示并等待回车（fflush 确保提示先落屏）
static void say_wait(const std::string& line) {
    printf("%s\n", line.c_str());
    fflush(stdout);
    std::string _;
    std::getline(std::cin, _);
}

// 分段剧情阅读：一段一段输出，回车继续，结束标（）
static void play_segmented_story(Player* player, const StoryStep& s) {
    printf("\n════════ 主线剧情 · %s ════════\n", s.title);
    std::string text = s.text;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string para = (nl == std::string::npos)
            ? text.substr(pos) : text.substr(pos, nl - pos);
        pos = (nl == std::string::npos) ? text.size() : nl + 1;
        if (!para.empty()) {
            printf("%s\n", para.c_str());
            if (pos < text.size()) {
                printf("\n（按回车键继续阅读）\n");
                fflush(stdout);
                std::string _;
                std::getline(std::cin, _);
            }
        }
    }
    printf("\n（该部分剧情已经结束——%s）\n", s.title);
    printf("════════════════════════════════\n\n");
    (void)player;
}

// 剧情推进后：输出「下一步目标」引导（紧凑版）
static void guide_next_hint(const Player* p) {
    if (p->story_phase >= g_story_count) {
        printf("\n（主线剧情已全部完成！墨阳子伏法，恩师大仇得报——自此可放心突破飞升！输入 guide 查看完结回顾。）\n");
        return;
    }
    if (p->story_phase == PHASE_FINAL_DUEL) {
        printf("\n【任务引导】真相大白，最终决战在即！\n");
        printf("  ▶ 此战凶险（墨阳子为【大乘期】），建议先闭关苦修提升境界（当前【%s】）；何时决战由你决定\n", realm_name(p->realm));
        printf("  ▶ 前往大殿广场，输入 决战 触发最终决战（决战前系统会自动存档）\n");
        printf("  ⚠ 主线剧情期间飞升受限——击败墨阳子、剧情完结之后，方可突破飞升。\n");
        return;
    }
    const StoryStep& s = g_story[p->story_phase];
    int need_day = anchor_day(p) + s.day_offset;
    printf("\n【任务引导】下一目标：%s\n", s.title);
    if (s.room > 0) {
        Room* r = room_get(s.room);
        printf("  ▶ 前往%s", r ? r->name.c_str() : "目标地点");
        if (p->current_room_id != s.room) {
            std::string path = path_str(find_path(p->current_room_id, s.room));
            if (!path.empty()) printf("（路线：%s）", path.c_str());
        } else {
            printf("（你已在此，输入 story 触发剧情）");
        }
        printf("\n");
    } else {
        printf("  ▶ 任意地点：输入 story 即可触发\n");
    }
    if (s.day_offset > 0 && p->day < need_day)
        printf("  ⏳ 需等至第 %d 天（当前第 %d 天），可在住所输入 sleep 度过一日。\n",
               need_day, p->day);
}

// 尝试推进剧情（story 命令在目标地点触发）
static void story_advance(Player* player) {
    if (player->story_phase >= g_story_count) return;
    const StoryStep& s = g_story[player->story_phase];
    if (s.room > 0 && player->current_room_id != s.room) return;
    if (!step_ready(player, s)) return;

    // 主线起始日锚点：拜师剧情实际触发的当天定为 D
    if (player->story_phase == 0 && player->story_day0 == 0)
        player->story_day0 = player->day;

    // 跨周目/跨玩家安全：确保最终 BOSS 墨阳子在场（大殿广场）
    if (player->story_phase == 0) {
        NPC* boss = npc_get(502);
        if (boss && !boss->is_alive) {
            boss->is_alive = true;
            boss->hp = boss->max_hp;
            boss->boss_rage = false;
        }
        Room* plaza = room_get(22);
        if (boss && plaza && std::find(plaza->npc_ids.begin(), plaza->npc_ids.end(), 502)
                == plaza->npc_ids.end()) {
            room_add_npc(22, 502);
        }
    }

    play_segmented_story(player, s);
    apply_story_rewards(player, s);
    player->story_phase++;
    guide_next_hint(player);
}

// ============================================================
//  最终决战：胜利 / 失败 / 结局页
// ============================================================

// 「拉完了」成就页：整齐的“拉完了”铺满整页，逐行滚动刷出
static void show_lalan_page() {
    console_clear();
    printf("\n（达成成就：【拉完了】）\n\n");
    for (int i = 0; i < 26; i++) {
        printf("%18s拉完了    拉完了    拉完了\n", "");
        fflush(stdout);
        sleep_ms(60);
    }
    printf("\n（按回车键返回）\n");
    fflush(stdout);
    std::string _;
    std::getline(std::cin, _);
    console_clear();
}
// ============================================================
//  战败彩蛋：三页奶蛙表情符号画（代替原嘉豪文案）
//  由奶蛙表情图片经字符映射生成，每页一屏、回车切换
// ============================================================

static const char* frog1[] = {
    "                          @%%%%%%%%@",
    "                    %#####%%%@@@@@%%%%##%@",
    "                 %##%                    %##%",
    "              @##@                          %##",
    "            @##      @%%%@@%%%                %*%",
    "          %#*%      %%   %**#@%@                ##",
    "         %**%      @#   +-----%%                 %*",
    "         *-+       @#   *-----#@                  %*",
    "         +-%         %%@ @#*#%                     %*",
    "         %*            @@@@@                        ##",
    "         #@                                          ##",
    "        ##%####%%%%%                                  #%",
    "         *#%@@@@@@@                                    *%",
    "          ##                                            *%",
    "           %*@                                           *%",
    "             *%                                           *#",
    "              +@                                           ##",
    "              %*                                            %*",
    "               *@                                            %*",
    "               ##                                             @*",
    "               ##",
    "               *%",
    "              @*",
    "              *%",
    "             *%",
    "            *%",
    "          @+%",
    "         %+@",
    "        #*",
    "      @*%",
    "     %#",
    "    *%",
    "  %*@",
    " %#",
};

static const char* frog2[] = {
    " :::::::::::::::========::::::::::::::::::::      ::::::::::",
    ":::::::::::=================:::::::::::::::: ::  :::::::::::",
    "::::::::=======================:::::::::::::::::   :::::::::",
    ":::::::===========================::::::::::::::::::::::::::",
    ":::: :===============================:::::::::::::::::::::::",
    "  : :======++++++==++**++++++=============::::::::::::::::::",
    "  : ===++++++++++*@@@@**++++++++===============:::::::::::::",
    "   :+*+++++++++++@******++++++++++================::::::::::",
    "    :*@@*+++++++++++++++++++++++++++++===============:::::::",
    "::  ::+**+++++++++++++@@*++++++++++++++++==============:::::",
    ":::::::=+++++++**+++*@@@@++++++++++++++++++==============:::",
    "::::::: :+@@@+**@@@@@@@@@*++++++++++++++++++===============:",
    ":::::::::=+*@@@@@@@@@@@@@*+++++++++++++++++++===============",
    "::::::::====+@@@@@@@@@@@@+++****++++++++++++++==============",
    ":::::::==+++++*@@*@@@@*@@++******++++++++++++++++===========",
    ":::: :=+++++++++@@@@@@@*+*******+++++++++++++++++++=========",
    "  : :=++++++++++++**************++++++++++++++++++++++======",
    " : :=++++++++++++++*************++++++++++++++++++++++++====",
    "   =+++++***+++++++++++++*++++++++++++++++++***+++++++++++==",
    ": :=+++++**@*+++++++++++++++++++++++++++++++*@@++++++++++++=",
    "  =++++++**@@+++++++++++++++++++++++++++++++**@*++++++++++++",
    " :++++++****@+++++++===++++++++++++++++++++++**@@+*+++++++++",
    " =+++*****@@@+=========================+++++++**@@@**+++++++",
    " =++****@@@@@+==============================+++*@@@*****++++",
    " =======++*@@==============================++++++========+++",
    " ========++*@*==========================++++++=========+++++",
    " :++======++*@+====================+++++++++=======+++++++++",
    ": :+++=====++@*++==============+++++++++++=====+++++++++++++",
    ":: :++++===++*@*+++++=======+++++++++++++=++++++++++++++****",
    ": : :+++++++++@@**@@@@=====*@@@*******+++++++++++++++******+",
    "   :: :+++++++*@@@@@@+++++++*@@@@***++++++++++++*********+++",
    "     :: :+*+**@@@@@@@*+++++**@@@@@@**++++++************+++++",
    "   ::::::::+*@@@@@@@@@@@@@@@@@@@@@@@@**************+++++++++",
    "   ::::::::::+@@@@@@@@@@@@@@@@@@@@@@@@@*******++++++++++++++",
    ":: :::::::::: =@@@@@@@@@@@@@@@@@@@@@@@@**+++++++++++++++++++",
    "::         :::::+*@@@@@@@@@@@@@@@@@@***+++++++++++++++++++++",
    ":::::   ::::::: :+++@@@***+++*@***++++++++++++++++++++***+++",
    "::::::: ::::::::: :++++++++++++++++++++++++++++++********+++",
    ":::::::::::::::::: =++++++++++++++++++++++++++********++++++",
    "::::::::::::::::::=++++*++++++++++++++++++**********++++++++",
    ":::::::::::::::::=+++++*****+++++++++++++*********++++++++++",
};


// 三页奶蛙表情符号画：每页一屏、回车切换；末页之后开新界面显示「轻松绷住」
static void show_frog_ascii_ending() {
    struct FrogPage {
        const char* const* art;
        int rows;
        const char* line;
        bool center;   // true=整幅居中；false=按原样左对齐（精确复刻截图）
    };
    static const FrogPage pages[] = {
        { frog1, static_cast<int>(sizeof(frog1) / sizeof(frog1[0])),
          "（奶蛙侧过脸，大眼睛里写满了不屑。）", true },
        { frog2, static_cast<int>(sizeof(frog2) / sizeof(frog2[0])),
          "（奶蛙笑得直不起腰——就这？）", false },
            };
    for (const auto& pg : pages) {
        console_clear();
        printf("\n");
        for (int i = 0; i < pg.rows; i++) {
            std::string line = pg.art[i];
            if (!line.empty()) {
                if (pg.center) {
                    int pad = std::max(1, (78 - display_width(line)) / 2);
                    printf("%s%s\n", std::string(pad, ' ').c_str(), line.c_str());
                } else {
                    printf("%s\n", line.c_str());
                }
            } else {
                printf("\n");
            }
        }
        printf("\n%s\n", pg.line);
        printf("（按回车键继续）\n");
        fflush(stdout);
        std::string _;
        std::getline(std::cin, _);
    }
    console_clear();
    show_big_text("轻松绷住");
    printf("\n（按回车键结束）\n");
    fflush(stdout);
    std::string _;
    std::getline(std::cin, _);
    console_clear();
}

// 决斗胜利后的奶蛙结局对话（师父残魂夺舍奶蛙）
static void final_frog_dialogue() {
    console_clear();
    say_wait("（奶蛙一蹦一跳来到你面前，缓缓仰起头。它开口时，声音忽然变得苍老而熟悉——那是你日夜思念的声音。）");
    say_wait("奶蛙（眼眶微湿，声音沙哑）：「孩子……你打赢了。」");
    say_wait("你（瞳孔骤缩，声音发颤）：「你……你是谁？！」");
    say_wait("奶蛙（轻声）：「是我。你师父，凌沧渊。」");
    say_wait("奶蛙：「那一战，墨阳子从背后暗算，为师肉身崩毁，魂飞魄散之际，一缕残魂无处可去，便钻进了这只奶蛙的躯壳。」");
    say_wait("奶蛙（苦笑）：「说来可笑——堂堂一宗之主，从此只能蹲在墙角，瞪着两只蛙眼，看着你长大。」");
    say_wait("奶蛙：「初入宗门时教你认路的是我；你挨了揍偷偷心疼的是我；你每走一步，为师都看在眼里。」");
    say_wait("奶蛙（声音发颤）：「为师没能亲手手刃仇人，却看着你一步步查清真相、修至大乘、替他讨回公道……孩子，为师以你为傲。」");
    say_wait("奶蛙（伸出小小的蛙蹼，轻轻覆在你的头顶，像许多年前那样，摸了摸你的头）：「好孩子，我终于可以瞑目了。」");
    say_wait("（奶蛙不再说话。绿幽幽的蛙眼里似有泪光，又似有释然的笑意。你伸出手，轻轻抱住了它。）");
    printf("\n（奶蛙与你的故事，至此圆满。你可以返回主世界继续修行了——按回车返回主世界）\n");
    fflush(stdout);
    std::string _;
    std::getline(std::cin, _);
    console_clear();
}

// 决斗胜利结算：主线完结 + 大字页 + 奶蛙对话
static void final_duel_victory(Player* player) {
    player->story_phase = g_story_count;   // 主线剧情全部完结
    player_add_exp(player, 3000);
    player->title = "青云宗宗主";
    printf("（获得称号：【青云宗宗主】）\n");
    printf("（主线《沧渊遗恨·正邪辨》全部完成！飞升之路自此再无羁绊。）\n");

    // 胜利大字页
    show_big_text("你也就还行吧");
    printf("\n（按回车键继续）\n");
    fflush(stdout);
    std::string _;
    std::getline(std::cin, _);
    show_big_text("哈哈……好吧，还是恭喜你");
    printf("\n（按回车键继续）\n");
    fflush(stdout);
    std::getline(std::cin, _);

    // 奶蛙（师父残魂）结局对话
    final_frog_dialogue();
}

// 决斗失败结算：「拉完了」成就页 + 两个选项
static void final_duel_defeat(Player* player) {
    show_lalan_page();
    printf("\n你身死道消——然而命运，尚未写下终局。\n\n");
    printf("  [1] 重来一世，我将夺回属于我的一切\n");
    printf("  [2] 别伤心，给你一个奖励\n");
    printf("请选择（1-2）> ");
    fflush(stdout);
    std::string line;
    if (!std::getline(std::cin, line)) line = "1";
    int choice = 1;
    try { choice = std::stoi(line); } catch (...) { choice = 1; }

    if (choice == 2) {
        printf("\n你闭上眼，任由那道温和的声音将你拉走……\n");
        show_frog_ascii_ending();
        engine_restart_to_menu();
        return;   // 返回主菜单，从头开始
    }

    // 选项1：重来一世 → 读取玩家最新存档
    printf("\n「重来一世，我将夺回属于我的一切。」你低语着，缓缓闭上了双眼……\n");
    Player* np = engine_reload_latest_save(player);
    if (!np) {
        printf("（未找到存档……天意难违，一切从头开始。）\n");
        engine_restart_to_menu();
        return;
    }
    printf("时光倒流，你回到了最终决战之前——命运，重来一次。\n");
    // 注意：旧 player 已被引擎释放，本函数必须立即返回，不可再触碰 player
}

// 战斗结束事件：仅处理最终决战结算；其余战斗不再推进剧情（剧情由 story 主动触发）
static void on_combat_end(EventType type, Player* player, void* data) {
    (void)type; (void)data;
    if (!player) return;
    if (!quest_duel_pending(player)) return;

    NPC* boss = npc_get(502);
    if (boss && !boss->is_alive) { final_duel_victory(player); return; }
    if (player->hp <= 0) { final_duel_defeat(player); return; }
    printf("（墨阳子负手而立，冷冷地看着你：决战未分胜负。待你重整旗鼓，可再输入 决战 一战！）\n");
}

// ============================================================
//  对外接口：锁定/完结/决战 判定 + 提示（供主循环与命令层调用）
// ============================================================

// 走剧情阶段（拜师、第一~七章）：禁止自由活动
// 仅当当前剧情步骤的触发条件已满足时才锁定；
// 条件未满足（含主线尚未开启：story_phase==0 且未达亲传门槛）时放行自由活动，
// 允许玩家退出剧情模式自行准备，避免被困在剧情模式中无法退出。
bool quest_story_locked(const Player* p) {
    if (!p) return false;
    if (p->story_phase >= g_story_count) return false;        // 完结
    if (p->story_phase >= 1 && p->story_phase <= 4) return false;  // 序章养成（自由）
    // 拜师引导期（phase 0）与第一~七章走剧情（phase 5~22）：条件齐备才锁定
    if (p->story_phase == 0 || (p->story_phase >= 5 && p->story_phase <= 22))
        return step_ready(p, g_story[p->story_phase]);
    return false;                                             // 养成时间2（自由）
}

bool quest_story_completed(const Player* p) {
    return p && p->story_phase >= g_story_count;
}

bool quest_duel_pending(const Player* p) {
    return p && p->story_phase == PHASE_FINAL_DUEL;
}

const char* quest_story_lock_target(const Player* p) {
    if (!p || !quest_story_locked(p)) return nullptr;
    const StoryStep& s = g_story[p->story_phase];
    Room* r = room_get(s.room);
    return r ? r->name.c_str() : nullptr;
}

const char* quest_story_room_hint(const Player* p) {
    if (!p || p->story_phase >= g_story_count) return nullptr;
    if (p->story_phase == PHASE_FINAL_DUEL) {
        return (p->current_room_id == 22) ? "此处输入 决战 触发最终决战" : nullptr;
    }
    const StoryStep& s = g_story[p->story_phase];
    if (s.room > 0) {
        return (p->current_room_id == s.room) ? "此处输入 story 触发剧情" : nullptr;
    }
    return "输入 story 触发主线剧情（任意地点）";
}

// 发起最终决战（决战命令 / fight 墨阳子 共用入口）
void quest_start_final_duel(Player* player) {
    if (!player) return;
    if (quest_story_completed(player)) {
        printf("墨阳子早已伏法，主线已完结——无需再战。\n");
        return;
    }
    if (!quest_duel_pending(player)) {
        printf("时机未到。你需要先查清师父之死的真相——输入 guide 查看当前主线进度。\n");
        return;
    }
    if (player->in_combat) {
        printf("你已经在战斗中！\n");
        return;
    }
    if (player->current_room_id != 22) {
        printf("墨阳子此刻在大殿广场。请先前往大殿广场，再输入 决战。\n");
        return;
    }
    NPC* boss = npc_get(502);
    if (!boss || !boss->is_alive) {
        printf("墨阳子已不在此处……（他已被击败，无需再战）\n");
        return;
    }
    // 境界提示：不强制，决战时机由玩家自行决定（仅提醒实力悬殊）
    if (static_cast<int>(player->realm) < static_cast<int>(RealmLevel::MAHAYANA)) {
        printf("你立于【%s】，直面墨阳子周身翻涌的【大乘期】威压，只觉呼吸一窒——\n",
               realm_name(player->realm));
        printf("此战实力悬殊，凶多吉少。但决战与否，全凭你自己决定。\n");
        printf("（若想再养精蓄锐，可先闭关修炼(train)提升修为，稍后再输入 决战。）\n");
    }

    // 存档提示 + 自动存档（⑥：开始最终决斗之前给玩家存档提示）
    printf("\n（最终决战在即——此战凶险万分，胜负难料！若你战败，一切将付诸东流。）\n");
    printf("（系统已为你自动存档一份；你也可以输入 save 手动存档，双保险。）\n");
    save_player(player);

    // 决战场景（对峙描写）
    printf("\n════════ 主线剧情 · 第八章·最终决战 ════════\n");
    printf("大殿广场，黑云压顶，山雨欲来。\n");
    printf("墨阳子一身玄袍，负手立于广场中央，周身【大乘期】灵力如渊似海，再不见半分往日的仁厚温谦：「小辈，你查得倒是仔细。可那又如何？今日这青云令，我要定了！」\n");
    printf("你缓缓拔出长剑，剑尖遥指：「师父的仇，青云宗的债——今日，一并清算！」\n");
    printf("════════════════════════════════\n\n");
    printf("（该部分剧情已经结束——第八章·最终决战）\n");

    printf("要开始最终决战吗？（y/n）> ");
    fflush(stdout);
    std::string line;
    if (!std::getline(std::cin, line)) line = "n";
    if (line != "y" && line != "Y" && line != "yes" && line != "是") {
        printf("你暂退了半步，压下决战的念头，收剑入鞘。\n");
        printf("（墨阳子冷笑一声，不再理会。你随时可再输入 决战 一战！）\n");
        return;
    }
    combat_begin_with(player, boss);
}

// ============================================================
//  story / guide 命令
// ============================================================

// 宗门地位晋升途径提示（用于引导玩家补齐地位门槛）
static const char* rank_hint(int rank) {
    switch (rank) {
        case 1: return "突破至炼气期可自动晋升【外门弟子】";
        case 2: return "突破至筑基期后，前往淬体演武场输入 kaohe 挑战赵青峰";
        case 3: return "突破至金丹期且任一四艺熟练度≥1000后，前往淬体演武场输入 kaohe 挑战金丹虚影";
        case 4: return "突破至元婴期可自动晋升【内门执事】";
        case 5: return "突破至化神期可自动晋升【核心长老】";
        case 6: return "突破至炼虚期可自动晋升【峰主】";
        case 7: return "突破至合体期可自动晋升【宗主候选】";
        case 8: return "突破至大乘期可自动晋升【宗主】";
        default: return "";
    }
}

// 逐条列出当前步骤的缺失条件（story 命令前置不足时展示）
static void print_missing_conditions(const Player* p, const StoryStep& s) {
    if (s.rank_floor > 0 && p->sect_rank < s.rank_floor)
        printf("  ✗ 需宗门地位 ≥ 【%s】（当前【%s】）\n",
               sect_rank_name_idx(s.rank_floor), sect_rank_name_idx(p->sect_rank));
    int need_day = anchor_day(p) + s.day_offset;
    if (s.day_offset > 0 && p->day < need_day)
        printf("  ✗ 需游戏第 %d 天（当前第 %d 天）——回住所输入 sleep 度过一日\n",
               need_day, p->day);
    if (s.cost > 0 && p->gold < s.cost)
        printf("  ✗ 需灵石 %d（当前 %d）——可藏宝阁 sell 变卖材料，或大殿 monthly 领月例\n",
               s.cost, p->gold);
    if (s.item_need > 0 && !has_item(p, s.item_need)) {
        Item* it = item_get(s.item_need);
        printf("  ✗ 需持有「%s」（通过前置剧情获得）\n",
               it ? it->name.c_str() : "关键道具");
    }
    if (s.prestige_floor > 0 && p->prestige < s.prestige_floor)
        printf("  ✗ 需宗门威望 ≥ %d（当前 %d）\n", s.prestige_floor, p->prestige);
}

static void cmd_story(Player* player, const std::string& args) {
    (void)args;
    if (player->story_phase >= g_story_count) {
        printf("主线《沧渊遗恨·正邪辨》已全部完成！你已成为【%s】。\n",
               player->title.empty() ? "青云宗宗主" : player->title.c_str());
        printf("（自此飞升之路再无羁绊——可安心修炼，突破渡劫飞升！）\n");
        return;
    }
    if (player->story_phase == PHASE_FINAL_DUEL) {
        printf("你已集齐墨阳子的全部罪证，真相大白，决战在即。\n");
        printf("（前往大殿广场，输入 决战 触发最终决战——此战凶险，胜负难料；何时决战由你自己决定！）\n");
        return;
    }
    const StoryStep& s = g_story[player->story_phase];

    if (s.room > 0 && player->current_room_id != s.room) {
        Room* r = room_get(s.room);
        printf("\n【当前主线】%s\n", s.title);
        printf("  请前往：%s\n", r ? r->name.c_str() : "某处");
        printf("  满足条件后，在目标地点输入 story 即可触发剧情。\n");
        return;
    }
    if (!step_ready(player, s)) {
        printf("\n【当前主线】%s\n", s.title);
        printf("  前置条件尚未满足：\n");
        print_missing_conditions(player, s);
        printf("  输入 guide 可查看完整引导。\n");
        return;
    }
    story_advance(player);
}

// 当前剧情步的条件核查：逐条列出满足/欠缺及补足途径
static void guide_conditions(const Player* p, const StoryStep& s) {
    bool all_ok = true;
    auto chk = [&](bool good, const std::string& text) {
        printf("  %s %s\n", good ? "✓" : "✗", text.c_str());
        if (!good) all_ok = false;
    };

    if (p->story_phase == PHASE_FINAL_DUEL) {
        bool inroom = p->current_room_id == 22;
        chk(inroom, "地点：需前往【大殿广场】（当前"
                   + std::string(inroom ? "已在此" : "未在此") + "）");
        if (static_cast<int>(p->realm) < static_cast<int>(RealmLevel::MAHAYANA))
            printf("  ⚠ 境界低于【大乘期】（当前【%s】）——此战凶险，但决战与否由你自己决定；可先修炼(train)提升修为\n", realm_name(p->realm));
        else
            printf("  ✓ 境界已达【大乘期】及以上\n");
        if (inroom)
            printf("  ✓ 已在大殿广场 —— 输入 决战 即可触发最终决战！（决战前系统会自动存档）\n");
        else
            printf("  ▶ 前往大殿广场后，输入 决战 即可触发最终决战\n");
        return;
    }

    if (s.rank_floor > 0) {
        bool good = p->sect_rank >= s.rank_floor;
        std::string t = "宗门地位：需【" + std::string(sect_rank_name_idx(s.rank_floor))
                      + "】（当前【" + sect_rank_name_idx(p->sect_rank) + "】）";
        if (!good) t += " —— " + std::string(rank_hint(s.rank_floor));
        chk(good, t);
    }
    int need_day = anchor_day(p) + s.day_offset;
    if (s.day_offset > 0) {
        bool good = p->day >= need_day;
        std::string t = "游戏时间：需第 " + std::to_string(need_day) + " 天（当前第 "
                      + std::to_string(p->day) + " 天）";
        if (!good) t += " —— 回住所（个人主页/弟子居所）输入 sleep 度过一日";
        chk(good, t);
    }
    if (s.cost > 0) {
        bool good = p->gold >= s.cost;
        std::string t = "灵石：需 " + std::to_string(s.cost) + "（当前 " + std::to_string(p->gold) + "）";
        if (!good) t += " —— 藏宝阁 sell 变卖材料，或宗门大殿 monthly 领取月例";
        chk(good, t);
    }
    if (s.item_need > 0) {
        Item* it = item_get(s.item_need);
        bool good = has_item(p, s.item_need);
        std::string t = "关键道具：需持有「" + std::string(it ? it->name.c_str() : "关键道具") + "」"
                      + (good ? "（已持有）" : "（未持有）");
        if (!good) t += " —— 通过前置剧情获得";
        chk(good, t);
    }
    if (s.prestige_floor > 0) {
        bool good = p->prestige >= s.prestige_floor;
        std::string t = "宗门威望：需 ≥ " + std::to_string(s.prestige_floor)
                      + "（当前 " + std::to_string(p->prestige) + "）";
        if (!good) t += " —— 通过晋升考核 / 突破境界获取威望";
        chk(good, t);
    }
    if (s.need_beat_boss) {
        NPC* b = npc_get(502);
        bool good = !(b && b->is_alive);
        std::string t = "强敌：需先在大殿广场击败墨阳子（当前"
                      + std::string(good ? "已击败" : "存活") + "）";
        if (!good) t += " —— 前往大殿广场输入 fight 墨阳子";
        chk(good, t);
    }
    if (all_ok)
        printf("  ✓ 条件已全部满足 —— 在目标地点输入 story 即可推进剧情\n");
}

// 章节时间线总览（第 1 日 = 拜师日）
static void guide_timeline(const Player* p) {
    printf("【主线时间线】（第 1 日 = 拜师日");
    if (p->story_day0 > 0) printf("，即游戏第 %d 天", p->story_day0);
    printf("）\n");
    for (const auto& c : g_chapters) {
        const char* status = "未开始";
        if (p->story_phase > c.last) status = "已完成";
        else if (p->story_phase >= c.first) status = "进行中";
        const char* mark = "○";
        if (strcmp(status, "已完成") == 0) mark = "✓";
        else if (strcmp(status, "进行中") == 0) mark = "●";
        printf("  %s %s %s %s\n",
               mark,
               pad_to_width(c.name, 16).c_str(),
               pad_to_width(c.days, 12).c_str(),
               status);
    }
}

// 下一章前瞻：汇总该章各步的最高门槛
static void guide_preview(const Player* p) {
    const GuideChapter* next = nullptr;
    for (const auto& c : g_chapters)
        if (p->story_phase < c.first) { next = &c; break; }
    if (!next) return;

    printf("【下一章前瞻】%s（%s）\n", next->name, next->days);
    if (next->first == PHASE_FINAL_DUEL) {
        printf("  · 建议境界 ≥ 【大乘期】（不强制——决战时机由你自己决定，但墨阳子是大乘期）\n");
        printf("  · 含最终 BOSS 战：墨阳子（大乘期）——决战前系统自动存档，胜负不强制\n");
        return;
    }
    int max_day = 0;
    std::string item;
    for (int i = next->first; i <= next->last && i < g_story_count; i++) {
        const StoryStep& st = g_story[i];
        max_day = std::max(max_day, st.day_offset);
        if (st.item_need > 0 && item.empty()) {
            Item* it = item_get(st.item_need);
            item = it ? it->name : "关键道具";
        }
    }
    bool any = false;
    if (max_day > 0) {
        if (p->story_day0 > 0)
            printf("  · 需推进至第 %d 天", anchor_day(p) + max_day);
        else
            printf("  · 拜师剧情开启后按剧情时间推进（第 %d 日前后）", max_day);
        any = true;
    }
    if (!item.empty()) { printf("  · 需持有「%s」", item.c_str()); any = true; }
    if (!any) printf("  · 无额外硬性门槛，跟随引导推进即可");
    printf("\n");
}

static void cmd_guide(Player* player, const std::string& args) {
    (void)args;
    const std::string sep = box_rep("═", 42);
    printf("\n╔%s╗\n", sep.c_str());
    printf("║ %s ║\n", pad_to_width("任务引导 · 主线《沧渊遗恨·正邪辨》", 42).c_str());
    printf("╠%s╣\n", sep.c_str());

    if (player->story_phase >= g_story_count) {
        printf("║ %s ║\n", pad_to_width("主线已全部完成！你已继任青云宗宗主。", 42).c_str());
        printf("║ %s ║\n", pad_to_width("墨阳子伏法，恩师之死真相大白。", 42).c_str());
        printf("║ %s ║\n", pad_to_width("飞升之路自此再无羁绊！", 42).c_str());
        printf("╚%s╝\n\n", sep.c_str());
        return;
    }

    // 养成时间2：最终决战待命
    if (player->story_phase == PHASE_FINAL_DUEL) {
        printf("║ %s ║\n", pad_to_width("【主线进度】第八章·最终决战（真相已明，决战待命）", 42).c_str());
        printf("║ %s ║\n", pad_to_width("  进度 " + std::to_string(player->story_phase + 1) + " / " + std::to_string(g_story_count)
                     + " 步 · 游戏第 " + std::to_string(player->day) + " 天", 42).c_str());
        printf("╚%s╝\n", sep.c_str());
        printf("\n【当前目标】养精蓄锐，决战墨阳子！\n");
        printf("  你已集齐墨阳子的全部罪证，真相大白，大仇只待清算。\n");
        printf("  ▶ 建议先提升修为（墨阳子为【大乘期】，当前【%s】）——但何时决战，由你自己决定\n", realm_name(player->realm));
        printf("  ▶ 前往大殿广场，输入 决战 触发最终决战（决战前系统自动存档）\n");
        printf("  ▶ 决战与寻常斗法无异，胜负全凭实力；战败亦可重来（重来一世）。\n");
        printf("\n【条件核查】\n");
        guide_conditions(player, g_story[player->story_phase]);
        printf("\n【提示】主线剧情期间飞升受限，境界止步大乘期；击败墨阳子、剧情完结后方可突破飞升。\n");
        printf("\n");
        guide_timeline(player);
        printf("\n╔%s╗\n╚%s╝\n\n", sep.c_str(), sep.c_str());
        return;
    }

    const StoryStep& s = g_story[player->story_phase];
    printf("║ %s ║\n", pad_to_width("【主线进度】" + std::string(s.title), 42).c_str());
    printf("║ %s ║\n", pad_to_width("  进度 " + std::to_string(player->story_phase + 1) + " / " + std::to_string(g_story_count)
                     + " 步 · 游戏第 " + std::to_string(player->day) + " 天", 42).c_str());
    printf("╚%s╝\n", sep.c_str());

    // ---- 主线未开启：先给开启指引 ----
    if (player->story_phase == 0 && player->sect_rank < 3) {
        printf("\n【主线尚未开启】要成为亲传弟子，方有资格踏入这段宿命。\n");
        printf("  开启条件：突破至【金丹期】 + 任一四艺熟练度≥1000\n");
        printf("  前往淬体演武场输入 kaohe 挑战【金丹虚影】，通过亲传考核。\n");
    }

    // ---- 当前目标 ----
    printf("\n【当前目标】%s\n", s.title);
    if (player->story_phase == 0) {
        printf("  你已晋升亲传弟子——先正式拜师！\n");
        printf("  ▶ 前往宗主书房，输入 story 触发拜师剧情。\n");
    } else {
        printf("  %s\n", g_step_goals[player->story_phase]);
    }
    if (s.room > 0) {
        Room* r = room_get(s.room);
        printf("  ▶ 目标地点：%s%s\n", r ? r->name.c_str() : "某处",
               player->current_room_id == s.room ? "（你已在此，输入 story 触发）" : "");
        if (player->current_room_id != s.room) {
            std::string path = path_str(find_path(player->current_room_id, s.room));
            if (!path.empty()) printf("  路线：%s\n", path.c_str());
        }
    } else {
        printf("  ▶ 任意地点：输入 story 即可触发\n");
    }

    // ---- 条件核查 ----
    printf("\n【条件核查】\n");
    guide_conditions(player, s);

    // ---- 主线时间线 ----
    printf("\n");
    guide_timeline(player);

    // ---- 下一章前瞻 ----
    printf("\n");
    guide_preview(player);

    // ---- 阶段提示 ----
    if (quest_story_locked(player))
        printf("\n【提示】剧情期间无法自由活动（修炼/战斗/购物等暂不可用），请跟随引导推进剧情。\n");
    else if (player->story_phase >= 5 && player->story_phase <= 22)
        printf("\n【提示】当前剧情步骤的前置条件尚未满足，你已退出剧情锁定，可自由行动准备；满足条件后回到目标地点输入 story 继续剧情。\n");
    else if (player->story_phase >= 1 && player->story_phase <= 4)
        printf("\n【提示】当前为养成时间，可自由修炼、提升四艺与战力，为后续剧情积蓄力量。\n");
    printf("\n【提示】满足条件后在目标地点输入 story 即可触发剧情；输入 guide 随时查看引导。\n");
    printf("╔%s╗\n╚%s╝\n\n", sep.c_str(), sep.c_str());
}

static void cmd_final_duel(Player* player, const std::string& args) {
    (void)args;
    quest_start_final_duel(player);
}

static std::vector<Command> quest_commands = {
    {"story", {}, "主线剧情：在目标地点输入 story 触发/查看剧情", cmd_story},
    {"guide", {"引导"}, "任务引导：主线目标/路线/条件/时间线", cmd_guide},
    {"决战", {"决斗"}, "触发与墨阳子的最终决战（需大乘期，大殿广场）", cmd_final_duel},
};

static void quest_init() {
    printf("[模块E] 剧情任务系统初始化（主线《沧渊遗恨·正邪辨》V3.0）\n");
    event_listen(EventType::COMBAT_END, on_combat_end);
}

static void quest_tick(Player* player) {
    (void)player;
}

static void quest_cleanup() {
    printf("[模块E] 剧情任务系统清理\n");
}

Module quest_module = {
    "剧情任务",
    quest_init,
    quest_tick,
    quest_cleanup,
    quest_commands
};
