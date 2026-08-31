#include "mud.hpp"

// ===== 玩家管理（对齐《修仙大世界MUD》V2.0 属性转化与战斗公式）=====

// 前向声明
Room* room_get(int id);
void room_remove_item(int room_id, int item_id);
void event_emit(EventType type, Player* player, void* data);

// ---- 属性重算 ----
// MaxHP = 100 + 体质×15 + 境界基础加成
// 基础攻击力 = 修为/50 + 灵力×1.2 + 法器攻击 + 灵兽攻击
// 防御 = 基于体质（DefRatio = 体质/(体质+250)，上限70%）
void player_recalc_stats(Player* player) {
    if (!player) return;

    int r = static_cast<int>(player->realm);

    // 契约灵兽被动加成（对齐 V2.0 3.1 被动技能加成）
    int beast_con = (player->beast_skill_id == 1) ? 5 : 0;   // 铁脊黑獠：体质+5
    if (player->beast_skill_id == 5) beast_con += 10;        // 九霄玄麟：全属性+10

    int eff_con = player->con + beast_con;
    int eff_spi = player->spi + ((player->beast_skill_id == 5) ? 10 : 0);

    // 境界基础加成（血量）
    int realm_hp_bonus = r * 150;

    // 血上限
    player->max_hp = 100 + eff_con * 15 + realm_hp_bonus;

    // 基础攻击力（修为/50 + 灵力×1.2）
    int base_atk = (int)(player->exp / 50.0 + eff_spi * 1.2);
    if (base_atk < 10) base_atk = 10;

    // 灵力上限
    player->max_mp = 50 + eff_spi * 5 + r * 40;

    // 法器攻击加成
    int artifact_atk = 0;
    for (auto& it : player->inventory) {
        if (it.type == ItemType::WEAPON && it.is_artifact) {
            artifact_atk += it.atk_bonus;
        }
    }

    // 灵兽攻击
    int beast_atk = player->beast_atk;
    player->atk = base_atk + artifact_atk + beast_atk;

    // 焚天焰狮：玩家攻击力+8%
    if (player->beast_skill_id == 4) player->atk = (int)(player->atk * 1.08f);

    // 防御：基于体质，DefRatio = 体质/(体质+250)（上限70%）
    float def_ratio = (float)eff_con / (float)(eff_con + 250);
    if (def_ratio > 0.70f) def_ratio = 0.70f;
    player->def = (int)(eff_con + def_ratio * 50);

    // 精力上限（随境界）
    player->max_stam = realm_stamina_cap(player->realm);
    if (player->max_stam < 100) player->max_stam = 100;
}

// ---- 创建/销毁 ----
Player* player_create(int id, const std::string& name, const std::string& password) {
    Player* p = new Player();
    p->id = id;
    p->name = name;
    p->password = password;
    p->spirit_root = SpiritRoot::GOLD;
    p->realm = RealmLevel::MORTAL;
    p->stage = RealmStage::EARLY;
    p->exp = 0;
    p->exp_to_next = realm_exp_required(RealmLevel::MORTAL, RealmStage::EARLY);
    p->con = 10;
    p->spi = 10;
    p->wu = 0;
    p->spd = 10;
    p->stam = 100;
    p->max_stam = 100;
    p->gold = 0;
    p->current_room_id = 1;
    p->sect_id = 1;
    p->monthly_got = -1;   // 月例记录"上次领取的月份"，-1表示未领过
    player_recalc_stats(p);
    p->hp = p->max_hp;
    p->mp = p->max_mp;
    return p;
}

void player_destroy(Player* player) {
    delete player;
}

// ---- 修为操作 ----
bool player_add_exp(Player* player, int amount) {
    if (amount <= 0) return false;
    player->exp += amount;
    printf("你获得了 %d 点修为！(当前: %d / %d)\n",
           amount, player->exp, player->exp_to_next);
    return true;
}

bool player_try_breakthrough(Player* player) {
    if (player->realm >= RealmLevel::TRANSCENDENT && player->stage >= RealmStage::PEAK) {
        printf("你已臻至渡劫飞升之境，叩问大道的尽头，无法继续突破。\n");
        return false;
    }

    if (player->exp < player->exp_to_next) {
        printf("修为不足，无法突破！还需要 %d 点修为。\n",
               player->exp_to_next - player->exp);
        return false;
    }

    // 消耗修为
    player->exp -= player->exp_to_next;

    // 进阶
    if (player->stage >= RealmStage::PEAK) {
        if (player->realm < RealmLevel::MAHAYANA) {
            int r = static_cast<int>(player->realm);
            player->realm = static_cast<RealmLevel>(r + 1);
            player->stage = RealmStage::EARLY;
            printf("\n====== 突破！======\n");
            printf("你成功突破至【%s】！\n", realm_name(player->realm));
            printf("宗门地位晋升为【%s】！\n", sect_rank_name(player->realm));
            printf("====================\n\n");

            if (player->realm == RealmLevel::NASCENT_SOUL) {
                printf("依门规，你晋升为内门执事，可向长老呈报边境战事。\n");
            }
            if (player->realm == RealmLevel::SPIRIT_TRANS) {
                printf("宗门封你为核心长老，执掌一方要务。\n");
            }
            if (player->realm == RealmLevel::VOID_REFINE) {
                printf("你荣升为一峰之主，坐镇宗门要冲。\n");
            }
            if (player->realm == RealmLevel::MERGE_ALIGN) {
                printf("你已成为宗主候选/准宗主，宗门威望与日俱增。\n");
            }
            if (player->realm == RealmLevel::MAHAYANA) {
                printf("宗门上下皆望你执掌大权。\n");
            }
            if (player->realm == RealmLevel::TRANSCENDENT) {
                printf("你勘破大道，踏入渡劫飞升之境！\n");
            }

            event_emit(EventType::PLAYER_LEVEL_UP, player, nullptr);
        } else {
            // 渡劫飞升之上不再突破
            printf("你已站在修真界之巅。\n");
        }
    } else {
        int s = static_cast<int>(player->stage);
        player->stage = static_cast<RealmStage>(s + 1);
        printf("你成功突破至【%s%s】！\n",
               realm_name(player->realm), stage_name(player->stage));
    }

    player->exp_to_next = realm_exp_required(player->realm, player->stage);
    player_recalc_stats(player);

    // 恢复满状态
    player->hp = player->max_hp;
    player->mp = player->max_mp;

    printf("下一阶段需要修为: %d\n", player->exp_to_next);
    printf("HP: %d  MP: %d  攻击: %d  防御: %d  精力: %d/%d\n",
           player->max_hp, player->max_mp, player->atk, player->def,
           player->stam, player->max_stam);

    return true;
}

// ---- 移动 ----
bool player_move_to(Player* player, int room_id) {
    Room* room = room_get(room_id);
    if (!room) {
        printf("那个方向没有路。\n");
        return false;
    }

    if (static_cast<int>(player->realm) < static_cast<int>(room->min_realm)) {
        printf("你的境界不足，无法进入该区域。（需要%s以上）\n",
               realm_name(room->min_realm));
        return false;
    }

    event_emit(EventType::PLAYER_LEAVE_ROOM, player, nullptr);
    player->current_room_id = room_id;
    event_emit(EventType::PLAYER_ENTER_ROOM, player, nullptr);
    printf("你来到了【%s】。\n", room->name.c_str());
    return true;
}

// ---- 背包操作 ----
bool player_add_item(Player* player, const Item& item) {
    // 可堆叠的先找同ID
    if (item.stackable) {
        for (auto& inv : player->inventory) {
            if (inv.id == item.id) {
                inv.quantity += item.quantity;
                printf("你获得了 %s x%d（共 %d）\n",
                       item.name.c_str(), item.quantity, inv.quantity);
                return true;
            }
        }
    }

    if (static_cast<int>(player->inventory.size()) >= MAX_INV_SLOTS) {
        printf("背包已满，无法拾取！\n");
        return false;
    }

    player->inventory.push_back(item);
    printf("你获得了 %s。\n", item.name.c_str());
    event_emit(EventType::PLAYER_GET_ITEM, player, nullptr);
    return true;
}

bool player_remove_item(Player* player, int slot) {
    if (slot < 0 || slot >= static_cast<int>(player->inventory.size())) return false;
    printf("你丢弃了 %s。\n", player->inventory[slot].name.c_str());
    player->inventory.erase(player->inventory.begin() + slot);
    return true;
}

// 获取品阶名
static const char* pill_grade_name(PillGrade g) {
    switch (g) {
        case PillGrade::LOW:  return "下品";
        case PillGrade::MID:  return "中品";
        case PillGrade::HIGH: return "上品";
        case PillGrade::TOP:  return "极品";
        default:              return "";
    }
}

// ---- 使用道具（对齐 V2.0 丹药效果与耐药性）----
bool player_use_item(Player* player, int slot) {
    if (slot < 0 || slot >= static_cast<int>(player->inventory.size())) return false;

    Item it = player->inventory[slot];

    switch (it.type) {
    case ItemType::PILL: {
        int gid = it.id;
        int group = (gid >= 301 && gid <= 304) ? 1 :   // 淬体丹
                    (gid >= 305 && gid <= 308) ? 2 :   // 聚气丹
                    (gid >= 309 && gid <= 312) ? 3 :   // 养神丹
                    (gid >= 313 && gid <= 316) ? 4 :   // 启悟丹
                    (gid >= 317 && gid <= 320) ? 5 :   // 培元丹
                    (gid >= 321 && gid <= 324) ? 6 : 0; // 精工丹

        if (group == 3) {
            // 养神丹：恢复精力（有耐药性，每日最多3颗，第4颗起拦截）
            if (player->pill_today >= 3) {
                printf("丹毒已满，今日已服 3 颗养神丹，无法再服用！\n");
                return false;
            }
            player->pill_today++;
            float mult = 1.0f;
            if (player->pill_today == 1) mult = 1.0f;
            else if (player->pill_today == 2) mult = 0.5f;
            else if (player->pill_today == 3) mult = 0.25f;

            int stam_gain = (int)(it.stam_bonus * mult);
            player->stam = std::min(player->stam + stam_gain, player->max_stam);
            printf("你服用了一颗%s养神丹，恢复 %d 点精力。(今日第%d颗，药效%.0f%%)\n",
                   pill_grade_name(it.grade), stam_gain, player->pill_today, mult*100);
        } else if (group == 5) {
            // 培元丹：修为
            printf("你服用了一颗%s培元丹，丹田暖流涌动，修为+%d。\n",
                   pill_grade_name(it.grade), it.exp_bonus);
            player_add_exp(player, it.exp_bonus);
        } else if (group == 1) {
            // 淬体丹：体质+HP
            printf("你服用了一颗%s淬体丹，体质+%d，血量上限+%d。\n",
                   pill_grade_name(it.grade), it.con_bonus, it.hp_bonus);
            player->con += it.con_bonus;
            player->max_hp += it.hp_bonus;
        } else if (group == 4) {
            // 启悟丹：悟性 + 速度%（策划：速度+5%/8%/12%/16%）
            int spd_gain = std::max(1, player->spd * it.spd_bonus / 100);
            printf("你服用了一颗%s启悟丹，悟性+%d，气息流转速度+%d（+%d%%）。\n",
                   pill_grade_name(it.grade), it.wu_bonus, spd_gain, it.spd_bonus);
            player->wu += it.wu_bonus;
            player->spd += spd_gain;
        } else if (group == 6) {
            // 精工丹：熟练度
            printf("你服用了一颗%s精工丹，四艺熟练度+%d。\n",
                   pill_grade_name(it.grade), it.prof_bonus);
            player->prof += it.prof_bonus;
        } else if (group == 2) {
            // 聚气丹：恢复灵力(MP)百分比
            int mp_gain = (int)(player->max_mp * ((float)it.mp_bonus / 100.0f));
            if (mp_gain < 1) mp_gain = 1;
            player->mp = std::min(player->mp + mp_gain, player->max_mp);
            printf("你服用了一颗%s聚气丹，灵力恢复 %d 点。\n",
                   pill_grade_name(it.grade), mp_gain);
        } else {
            // 常规恢复丹药（疗伤/回灵）
            printf("你服用了 %s。\n", it.name.c_str());

            // 战斗符箓：破障符(232)攻击加成 / 御灵符(233)减伤，持续当日
            if (it.id == 232) {
                player->atk_buff += it.atk_bonus;
                printf("破障符灵光一闪！当日攻击 +%d（当日总加成 %d）\n",
                       it.atk_bonus, player->atk_buff);
                break;
            }
            if (it.id == 233) {
                player->def_buff += it.def_bonus;
                printf("御灵符化作护罩！当日被击减伤 +%d（当日总减伤 %d）\n",
                       it.def_bonus, player->def_buff);
                break;
            }

            player->hp = std::min(player->hp + it.hp_bonus, player->max_hp);
            player->mp = std::min(player->mp + it.mp_bonus, player->max_mp);
        }
        break;
    }

    case ItemType::MANUAL:
        printf("你研读了秘籍《%s》。\n", it.name.c_str());
        if (it.exp_bonus > 0) player_add_exp(player, it.exp_bonus);
        break;

    default:
        printf("%s 无法直接使用。\n", it.name.c_str());
        return false;
    }

    player_recalc_stats(player);
    event_emit(EventType::PLAYER_USE_ITEM, player, nullptr);

    // 消耗道具
    if (it.stackable && it.quantity > 1) {
        it.quantity--;
        player->inventory[slot].quantity = it.quantity;
    } else {
        player->inventory.erase(player->inventory.begin() + slot);
    }

    return true;
}

Item* player_find_item(Player* player, int item_id) {
    for (auto& it : player->inventory) {
        if (it.id == item_id) return &it;
    }
    return nullptr;
}

// ---- 技能 ----
bool player_add_skill(Player* player, const Skill& skill) {
    if (static_cast<int>(player->skills.size()) >= MAX_SKILL_SLOTS) {
        printf("技能槽已满，无法学习新技能！\n");
        return false;
    }
    player->skills.push_back(skill);
    printf("你学会了新技能【%s】！\n", skill.name.c_str());
    return true;
}

// ---- 灵石 ----
void player_add_gold(Player* player, int amount) {
    player->gold += amount;
    printf("你获得了 %d 灵石。（余额: %d）\n", amount, player->gold);
}

bool player_spend_gold(Player* player, int amount) {
    if (player->gold < amount) {
        printf("灵石不足！需要 %d，当前只有 %d。\n", amount, player->gold);
        return false;
    }
    player->gold -= amount;
    printf("你花费了 %d 灵石。（余额: %d）\n", amount, player->gold);
    return true;
}