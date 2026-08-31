#include "mud.hpp"
#include <sys/stat.h>
#include <direct.h>   // _mkdir on Windows

// 前向声明
NPC* npc_get(int id);
Item* item_get(int id);

#define SAVE_DIR "data/saves"

// ---- 内部辅助 ----
static void ensure_save_dir() {
    struct stat st = {0};
    if (stat("data", &st) == -1) {
        _mkdir("data");
    }
    if (stat(SAVE_DIR, &st) == -1) {
        _mkdir(SAVE_DIR);
    }
}

static std::string make_save_path(const std::string& name) {
    return std::string(SAVE_DIR) + "/" + name + ".sav";
}

// ---- 保存玩家 ----
bool save_player(Player* player) {
    if (!player) return false;
    ensure_save_dir();

    std::string path = make_save_path(player->name);
    std::ofstream fp(path);
    if (!fp.is_open()) {
        printf("保存失败: 无法写入文件 %s\n", path.c_str());
        return false;
    }

    fp << "{\n";
    fp << "  \"id\": " << player->id << ",\n";
    fp << "  \"name\": \"" << player->name << "\",\n";
    fp << "  \"password\": \"" << player->password << "\",\n";
    fp << "  \"spirit_root\": " << static_cast<int>(player->spirit_root) << ",\n";
    fp << "  \"realm\": " << static_cast<int>(player->realm) << ",\n";
    fp << "  \"stage\": " << static_cast<int>(player->stage) << ",\n";
    fp << "  \"exp\": " << player->exp << ",\n";
    fp << "  \"exp_to_next\": " << player->exp_to_next << ",\n";
    fp << "  \"hp\": " << player->hp << ",\n";
    fp << "  \"max_hp\": " << player->max_hp << ",\n";
    fp << "  \"mp\": " << player->mp << ",\n";
    fp << "  \"max_mp\": " << player->max_mp << ",\n";
    fp << "  \"atk\": " << player->atk << ",\n";
    fp << "  \"def\": " << player->def << ",\n";
    fp << "  \"gold\": " << player->gold << ",\n";
    fp << "  \"current_room_id\": " << player->current_room_id << ",\n";
    fp << "  \"sect_id\": " << player->sect_id << ",\n";
    fp << "  \"disciple_count\": " << player->disciple_count << ",\n";

    // ---- V2.0 核心/每日/灵兽/剧情 ----
    fp << "  \"con\": " << player->con << ",\n";
    fp << "  \"spi\": " << player->spi << ",\n";
    fp << "  \"wu\": " << player->wu << ",\n";
    fp << "  \"spd\": " << player->spd << ",\n";
    fp << "  \"stam\": " << player->stam << ",\n";
    fp << "  \"max_stam\": " << player->max_stam << ",\n";
    fp << "  \"prof\": " << player->prof << ",\n";
    fp << "  \"day\": " << player->day << ",\n";
    fp << "  \"pill_today\": " << player->pill_today << ",\n";
    fp << "  \"rested_today\": " << (player->rested_today ? 1 : 0) << ",\n";
    fp << "  \"monthly_got\": " << player->monthly_got << ",\n";
    fp << "  \"beast_grade\": " << static_cast<int>(player->beast_grade) << ",\n";
    fp << "  \"beast_id\": " << player->beast_id << ",\n";
    fp << "  \"beast_atk\": " << player->beast_atk << ",\n";
    fp << "  \"beast_hp\": " << player->beast_hp << ",\n";
    fp << "  \"beast_skill_id\": " << player->beast_skill_id << ",\n";
    fp << "  \"prestige\": " << player->prestige << ",\n";
    fp << "  \"atk_buff\": " << player->atk_buff << ",\n";
    fp << "  \"def_buff\": " << player->def_buff << ",\n";
    fp << "  \"story_phase\": " << player->story_phase << ",\n";
    fp << "  \"title\": \"" << player->title << "\",\n";
    fp << "  \"tags\": \"" << player->tags << "\",\n";

    // 背包
    fp << "  \"inventory\": [\n";
    for (size_t i = 0; i < player->inventory.size(); i++) {
        auto& it = player->inventory[i];
        fp << "    {\"id\": " << it.id << ", \"qty\": " << it.quantity << "}";
        if (i < player->inventory.size() - 1) fp << ",";
        fp << "\n";
    }
    fp << "  ],\n";

    // 技能
    fp << "  \"skills\": [\n";
    for (size_t i = 0; i < player->skills.size(); i++) {
        auto& sk = player->skills[i];
        fp << "    {\"id\": " << sk.id << ", \"level\": " << sk.level << "}";
        if (i < player->skills.size() - 1) fp << ",";
        fp << "\n";
    }
    fp << "  ]\n";
    fp << "}\n";
    fp.close();

    printf("存档成功！(%s)\n", path.c_str());
    return true;
}

// ---- 加载玩家 ----
Player* load_player(const std::string& name) {
    std::string path = make_save_path(name);
    std::ifstream fp(path);
    if (!fp.is_open()) return nullptr;

    Player* p = new Player();
    std::string line;
    bool in_inv = false, in_skills = false;

    while (std::getline(fp, line)) {
        // 跳过空行和括号
        auto trimmed = [](const std::string& s) -> std::string {
            size_t start = s.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) return "";
            return s.substr(start);
        }(line);

        if (trimmed.empty() || trimmed[0] == '{' || trimmed[0] == '}' ||
            trimmed[0] == '[' || trimmed[0] == ']' || trimmed[0] == ',')
            continue;

        if (trimmed.find("\"inventory\"") != std::string::npos) { in_inv = true; continue; }
        if (trimmed.find("\"skills\"") != std::string::npos) { in_skills = true; continue; }

        if (in_inv) {
            int id, qty;
            if (sscanf(trimmed.c_str(), "{\"id\": %d, \"qty\": %d}", &id, &qty) == 2) {
                Item* tmpl = item_get(id);
                if (tmpl && static_cast<int>(p->inventory.size()) < MAX_INV_SLOTS) {
                    Item it = *tmpl;
                    it.quantity = qty;
                    p->inventory.push_back(it);
                }
            }
            if (trimmed.find(']') != std::string::npos) in_inv = false;
            continue;
        }

        if (in_skills) {
            int id, level;
            if (sscanf(trimmed.c_str(), "{\"id\": %d, \"level\": %d}", &id, &level) == 2) {
                if (static_cast<int>(p->skills.size()) < MAX_SKILL_SLOTS) {
                    Skill sk;
                    sk.id = id;
                    sk.level = level;
                    p->skills.push_back(sk);
                }
            }
            if (trimmed.find(']') != std::string::npos) in_skills = false;
            continue;
        }

        // 解析键值对
        char key[64] = {0}, value[256] = {0};
        if (sscanf(trimmed.c_str(), " \"%[^\"]\": \"%[^\"]\"", key, value) == 2) {
            if (strcmp(key, "name") == 0) p->name = value;
            else if (strcmp(key, "password") == 0) p->password = value;
            else if (strcmp(key, "title") == 0) p->title = value;
            else if (strcmp(key, "tags") == 0) p->tags = value;
        } else {
            // 解析数字值
            auto colon = trimmed.find(':');
            if (colon != std::string::npos) {
                auto kstart = trimmed.find('"');
                if (kstart != std::string::npos) {
                    auto kend = trimmed.find('"', kstart + 1);
                    if (kend != std::string::npos) {
                        std::string k = trimmed.substr(kstart + 1, kend - kstart - 1);
                        int num = 0;
                        // 在冒号后查找数字
                        auto num_start = trimmed.find_first_of("-0123456789", colon + 1);
                        if (num_start != std::string::npos) {
                            num = std::stoi(trimmed.substr(num_start));
                        }
                        if (k == "id") p->id = num;
                        else if (k == "spirit_root") p->spirit_root = static_cast<SpiritRoot>(num);
                        else if (k == "realm") p->realm = static_cast<RealmLevel>(num);
                        else if (k == "stage") p->stage = static_cast<RealmStage>(num);
                        else if (k == "exp") p->exp = num;
                        else if (k == "exp_to_next") p->exp_to_next = num;
                        else if (k == "hp") p->hp = num;
                        else if (k == "max_hp") p->max_hp = num;
                        else if (k == "mp") p->mp = num;
                        else if (k == "max_mp") p->max_mp = num;
                        else if (k == "atk") p->atk = num;
                        else if (k == "def") p->def = num;
                        else if (k == "gold") p->gold = num;
                        else if (k == "current_room_id") p->current_room_id = num;
                        else if (k == "sect_id") p->sect_id = num;
                        else if (k == "disciple_count") p->disciple_count = num;
                        // V2.0 字段
                        else if (k == "con") p->con = num;
                        else if (k == "spi") p->spi = num;
                        else if (k == "wu") p->wu = num;
                        else if (k == "spd") p->spd = num;
                        else if (k == "stam") p->stam = num;
                        else if (k == "max_stam") p->max_stam = num;
                        else if (k == "prof") p->prof = num;
                        else if (k == "day") p->day = num;
                        else if (k == "pill_today") p->pill_today = num;
                        else if (k == "rested_today") p->rested_today = (num != 0);
                        else if (k == "monthly_got") p->monthly_got = num;
                        else if (k == "beast_grade") p->beast_grade = static_cast<BeastGrade>(num);
                        else if (k == "beast_id") p->beast_id = num;
                        else if (k == "beast_atk") p->beast_atk = num;
                        else if (k == "beast_hp") p->beast_hp = num;
                        else if (k == "beast_skill_id") p->beast_skill_id = num;
                        else if (k == "prestige") p->prestige = num;
                        else if (k == "atk_buff") p->atk_buff = num;
                        else if (k == "def_buff") p->def_buff = num;
                        else if (k == "story_phase") p->story_phase = num;
                    }
                }
            }
        }
    }

    fp.close();
    printf("读档成功！欢迎回来，%s。\n", p->name.c_str());
    return p;
}

// ---- 列出存档 ----
void save_list_players() {
    ensure_save_dir();
    printf("\n========== 存档列表 ==========\n");

    std::string cmd = "dir /b \"" + std::string(SAVE_DIR) + "\\*.sav\" 2>nul";
    FILE* fp = _popen(cmd.c_str(), "r");
    if (!fp) {
        printf("  暂无存档\n");
        return;
    }

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        char* dot = strrchr(line, '.');
        if (dot) *dot = '\0';
        printf("  [%d] %s\n", ++count, line);
    }
    _pclose(fp);

    if (count == 0) printf("  暂无存档\n");
    printf("==============================\n\n");
}

// ---- 删除存档 ----
bool save_delete_player(const std::string& name) {
    std::string path = make_save_path(name);
    if (remove(path.c_str()) == 0) {
        printf("存档已删除: %s\n", name.c_str());
        return true;
    }
    printf("删除失败: 存档不存在\n");
    return false;
}

bool save_player_exists(const std::string& name) {
    std::string path = make_save_path(name);
    std::ifstream fp(path);
    return fp.good();
}

const char* save_get_dir() { return SAVE_DIR; }

// ---- 世界状态 ----
bool save_world_state() {
    return true;
}

bool load_world_state() {
    return true;
}