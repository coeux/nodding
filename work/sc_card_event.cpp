#include "sc_spec_event.h"
#include "date_helper.h"
#include "sc_push_def.h"
#include "sc_cache.h"
#include "random.h"
#include "sc_message.h"
#include "sc_mailinfo.h"
#include "sc_statics.h"
#include <ctime>

#define LOG "SC_SPEC_EVENT"
#define MAX_SPEC_EVENT_NUM 1000
#define EVENT_POINT 16101
/* 关卡ID */
#define SPEC_EVENT_ROUND_BEGIN 6001
#define SPEC_EVENT_ROUND_END 6100
#define SPEC_EVENT_ROUND_LEVEL_MAX 5
#define SPEC_EVENT_ROUND_LEVEL_EVER 3
/* 活动 [关闭/开启/关卡通过] 状态 */
#define SPEC_EVENT_ROUND_UNOPEN 0x00
#define SPEC_EVENT_ROUND_OPEN 0x01
#define SPEC_EVENT_ROUND_PASS 0x02 /* 关卡通过数字为3 */
/* 重置敌人时刻 */
#define SPEC_EVENT_ROUND_RESET_3 10800 /* 凌晨3点更新 */
/* 各类configure对应ID */
#define SPEC_EVENT_RESET_AND_OPEN_CONSUME_ID 12
#define SPEC_EVENT_DROP_PROBABILITY_ID 13
#define SPEC_EVENT_REVIVE_ID 14
#define SPEC_EVENT_COIN_TIMES 15
/* 积分item id  */
#define SPEC_EVENT_COIN_ITEM_ID 16001
/* 积分奖励ID */
#define SPEC_EVENT_POINT_BEGIN 1001
#define SPEC_EVENT_POINT_END 1018
/* 千名之外 */
#define SPEC_EVENT_RANK_OUT_RANK 501
/* 排行榜更新频率（时间） 每1小时更新一次排行榜 */
#define SPEC_EVENT_RANK_UPDATE_FREQUENCY 3600

/* sc_spec_event_t begin */
sc_spec_event_t::sc_spec_event_t():
m_start_stamp(0),
m_end_stamp(0),
m_reset_stamp(0),
m_eid(0),
m_open(false),
m_close(true),
m_newhost_lock(true)
{
}

void
sc_spec_event_t::load_db(vector<int32_t>& hostnums_)
{
    for (int32_t host : hostnums_)
    {
        m_hosts.push_back(host);
    }

    /* event.json中最大id为最新活动，填表时不要把未开启过的的活动加至表中 
     * 默认ID最大的为最新活动
     */
    /* 从表中获取活动 */
    for (size_t i = 1; i <= MAX_SPEC_EVENT_NUM; ++i) {
        repo_def::spec_event_t* rp = repo_mgr.spec_event.get(i);
        if (NULL == rp) {
            logwarn((LOG, "load_db, spec_event not exists"));
            break;
        }
        m_eid = i;
        m_begin_month = rp->start_time.substr(5,2);
        m_begin_day = rp->start_time.substr(8,2);
        m_begin_hour = rp->start_time.substr(11,2);
        m_end_month = rp->end_time.substr(5,2);
        m_end_day = rp->end_time.substr(8,2);
        m_end_hour = rp->end_time.substr(11,2);
        m_start_stamp = str2stamp(rp->start_time);
        m_end_stamp = str2stamp(rp->end_time);
        m_reset_stamp = str2stamp(rp->reset_time);
    }
    /*
    uint32_t cur_sec = date_helper.cur_sec();
    if (m_start_stamp <= cur_sec && cur_sec <= m_end_stamp) {
        spec_event_begin();
    } else {
        logwarn((LOG, 
        "load_db failed, spec_event cur_sec NOT IN (start, end)"));
        return ;
    }
    */

    /* 从数据库中获取关卡敌人信息 */
    m_spec_event_round_fp.clear();
    sql_result_t res;
    if (0 == db_SpecEventRound_t::sync_select_all(m_eid, res)) {
        for  (size_t i = 0; i < res.affect_row_num(); ++i) {
            sp_spec_event_round_t sp_spec_event_round(new spec_event_round_t);
            m_spec_event_round_fp.insert(make_pair(sp_spec_event_round->round, 0));
            sp_spec_event_round->init(*res.get_row_at(i));
            if (sp_spec_event_round->view_data.empty()) {
                continue;
            }
            m_spec_event_round.insert(make_pair(sp_spec_event_round->round,
                                      sp_spec_event_round));
        }
    }
}

void
sc_spec_event_t::getBeginEndDate(string& beginMonth, string& beginDay, string& beginHour, string& endMonth, string& endDay, string& endHour)
{
    beginMonth = m_begin_month;
    beginDay = m_begin_day;
    beginHour = m_begin_hour;
    endMonth = m_end_month;
    endDay = m_end_day;
    endHour = m_end_hour;
}

void
sc_spec_event_t::update()
{
    uint32_t cur_sec = date_helper.cur_sec();
    uint32_t today_sec_ = cur_sec - date_helper.cur_0_stmp();
    if (cur_sec == m_reset_stamp) {
        reset_user_info(cur_season()+1);
    }
    if (0 == m_start_stamp || 0 == m_end_stamp) {
        return;
    }
    /* spec event begin */
    if (SPEC_EVENT_ROUND_RESET_3 == today_sec_) {
        reset_spec_event_round();
    }
    m_close = cur_sec >= m_end_stamp;

    if (m_newhost_lock)
    {
        m_open = false;
        return;
    }
    if (m_start_stamp <= cur_sec && cur_sec <= m_end_stamp) {
        if (!m_open) {
            spec_event_begin();
            m_open = true;
        }
        return;
    }
    /* spec event end */
    if (m_open) {
        logwarn((LOG, "Begin spec_event_end %d", m_eid));
        spec_event_end();
    }
    m_open = false;
    // auto going to next event
    /*
    if (cur_sec > m_end_stamp) {
        repo_def::spec_event_t* rp = repo_mgr.spec_event.get(m_eid+1);
        if (NULL == rp) {
            logwarn((LOG, "update spec_event, not exists event %d", m_eid + 1));
            m_start_stamp = 0;
            m_end_stamp = 0;
            return ;
        }
        m_eid++;
        m_start_stamp = str2stamp(rp->start_time);
        m_end_stamp = str2stamp(rp->end_time);
        m_reset_stamp = str2stamp(rp->reset_time);
    }
    */
}

int32_t
sc_spec_event_t::cur_season()
{
    return m_eid;
}

int32_t
sc_spec_event_t::latest_season()
{
    if (m_open)
    {
        return m_eid;
    }
    else
    {
        int32_t event_id;
        for (size_t i = 1; i <= MAX_SPEC_EVENT_NUM; ++i) 
        {
            repo_def::spec_event_t* rp = repo_mgr.spec_event.get(i);
            if (NULL == rp) {
                logwarn((LOG, "load_db, spec_event not exists"));
                break;
            }
            event_id = i;
        }
        return event_id;
    }
}

/*
 * date string to uint32_t time stamp
 * 2015-01-01 00:00:00 => 1420041600
 */
uint32_t
sc_spec_event_t::str2stamp(string str)
{
    tm tm_;
    strptime(str.c_str(), "%Y-%m-%d %H:%M:%S", &tm_);
    time_t time_ = mktime(&tm_);
    return (uint32_t)time_;
}

/* 计算属性加成 */
void cal_pro(float& atk_, float& mgc_, float& def_, float& res_, float& hp_, int32_t round_id_)
{
    auto rp = repo_mgr.spec_event_round.get(round_id_);
    if (rp == NULL)
        return;
    atk_ = atk_ * (1.0 + rp->atk);
    mgc_ = mgc_ * (1.0 + rp->mgc);
    def_ = def_ * (1.0 + rp->def);
    res_ = res_ * (1.0 + rp->res);
    hp_ = hp_ * (1.0 + rp->hp);
}

void
sc_spec_event_t::reset_spec_event_round()
{
    /* 计算技能效果战斗力 */
    auto cal_skl_fp = [&](int32_t resid_, int32_t level_) {
        auto skill_ = repo_mgr.skill.get(resid_);
        if (skill_ != NULL) {
            if (skill_->next_skill == 0)
                return 6 * level_;
            return level_ << 2;
        }
        auto pskill_ = repo_mgr.tskill.get(resid_);
        if (pskill_ != NULL) {
            if (pskill_->next_skill == 0)
                return 6 * level_;
            return level_ << 2;
        }
        return level_;
    };
    /* 计算基础属性战斗力 */
    auto cal_pro_fp = [&](int32_t atk_, int32_t mgc_, int32_t def_,
                         int32_t res_, int32_t hp_, int32_t round_id_, int32_t lv_)
    {
        double fp = (atk_ * 1.10f) + (mgc_ * 1.00f) +
                   (def_ * 1.20f) + (res_ * 1.00f) +
                   (hp_ * 0.3f) + lv_ * 20;
        return (int32_t)fp;
    };

    /* 排行榜前50名 按以下规则取
     * [1,1]   1 -> 1
     * [2,5]   2 -> 0
     * [6,10]  3 -> 1
     * [11,20] 8 -> 0
     * [21,50] 16-> 1
     */
    int range_list[101];
    for(int i=0;i<=100;++i)
        range_list[i] = 0;
    auto get_random_num = [](int range_begin, int range_end, int count, int* array)
    {
        if (count <= 0)
            return;
        int randomList[range_end+1];
        for(int i=range_begin;i<=range_end;++i)
            randomList[i]=i;
        for(int i=range_begin;i<range_begin+count;++i)
        {
            int num = random_t::rand_integer(i,range_end);
            swap(randomList[i],randomList[num]);
            array[randomList[i]] = 1;
        }
    };
    get_random_num(1,1,1,range_list);
    get_random_num(2,5,0,range_list);
    get_random_num(6,10,1,range_list);
    get_random_num(11,20,0,range_list);
    get_random_num(21,50,1,range_list);

    int32_t index=0;
    /* old */
    /* 生成的30个玩家数据， 分为10组 10个难度， 共计300条数据*/
    /* 第一个人1-30， 第二个人1-30 依次*/
    /* new */
    /* 生成的3个玩家数据， 分5组 5个难度， 共计15条数据 */
    /* 第一个人1-5， 第二个人1-5 依次*/
    m_spec_event_round_fp.clear();
    for (int from=1;from<=100;++from)
    {
        if(range_list[from] != 1)
            continue;
        index++;
        int32_t pid_[5];
        int32_t uid_;
        bool succ;
        sp_fight_view_user_t sp_view = arena_info_cache.get_arena_view(from, uid_, succ);
        int32_t pids[5] = {-1, -1, -1, -1, -1};
        if (!succ) {
            continue;
        } else {
            for(int level=1;level<=SPEC_EVENT_ROUND_LEVEL_MAX;++level)
            {
                sp_fight_view_user_t sp_view_copy(new sc_msg_def::jpk_fight_view_user_data_t(*sp_view));
                string view_data;

                int32_t round_id = SPEC_EVENT_ROUND_BEGIN - index + SPEC_EVENT_ROUND_LEVEL_EVER + (level-1)*100;
                /* 由加成重新计算战斗力 */
                int32_t i = 0;
                int32_t total_fp = 0;
                for (auto it = sp_view_copy->roles.begin(); it != sp_view_copy->roles.end(); ++it) {
                    cal_pro(it->second.pro[0], it->second.pro[1], it->second.pro[2], it->second.pro[3], it->second.pro[4], round_id);
                    int32_t pro_fp = cal_pro_fp(it->second.pro[0], it->second.pro[1],
                                                it->second.pro[2], it->second.pro[3],
                                                it->second.pro[4], round_id, it->second.lvl[0]);
                    if (pro_fp != 0) 
                    {
                        total_fp += pro_fp;
                        size_t size = it->second.skls.size();
                        for (size_t j = 0; j < size; ++j) {
                            total_fp += cal_skl_fp(it->second.skls[j][1],
                                               it->second.skls[j][2]);
                        }
                    } 
                    else 
                    {
                        total_fp += it->second.pro[8];
                    }
                    pids[i++] = it->second.pid;
                }
                sp_view_copy->fp = total_fp;
                (*sp_view_copy) >> view_data;

                sp_spec_event_round_t sp_spec_event_round(new spec_event_round_t);
                sp_spec_event_round->eid = m_eid;
                sp_spec_event_round->round = round_id;
                sp_spec_event_round->pid1 = pids[0];
                sp_spec_event_round->pid2 = pids[1];
                sp_spec_event_round->pid3 = pids[2];
                sp_spec_event_round->pid4 = pids[3];
                sp_spec_event_round->pid5 = pids[4];
                sp_spec_event_round->view_data = view_data;
                m_spec_event_round.insert(make_pair(sp_spec_event_round->round,
                                          sp_spec_event_round));
                m_spec_event_round_fp.insert(make_pair(sp_spec_event_round->round, 0));
/*
                char sql[4096];
                sprintf(sql, "REPLACE INTO SpecEventRound(eid, round, pid1, pid2, pid3,"
                             "pid4, pid5, view_data) VALUES(%d, %d, %d, %d, %d, %d, %d,"
                             " '%s')",
                             m_eid, round_id,
                             pids[0], pids[1], pids[2], pids[3], pids[4],
                             view_data.c_str());
                db_service.async_do([](string &sql_) {
                    db_service.async_execute(sql_);
                }, string(sql));
                */
            }
        }
    }
}

void sc_spec_event_t::unlock_new_host()
{
    if (!m_newhost_lock)
    {
        return;
    }
    m_newhost_lock = false;
}

void
sc_spec_event_t::spec_event_begin()
{
    /* 全服广播活动开始 */
    sc_msg_def::nt_spec_event_change_state_t nt_;
    nt_.state = 1;
    string msg_;
    nt_ >> msg_;
    logic.broadcast_message(sc_msg_def::nt_spec_event_change_state_t::cmd(), msg_);

    reset_spec_event_round();
    /* 发送公告 推送通知 */
    //sc_msg_def::nt_spec_event_t nt;
    //nt.info = pushinfo.get_push(push_worldboss_escaped, 99999);
    //nt.status = 1; /* 活动开始 */
    //string msg; nt >> msg;
    //logic.usermgr().broadcast(sc_msg_def::nt_spec_event_t::cmd(), msg);
}

void 
sc_spec_event_t::spec_event_end()
{
    /* 发送公告 推送通知 */
    //sc_msg_def::nt_spec_event_t nt;
    //nt.info = pushinfo.get_push(push_worldboss_escaped, 99999);
    //nt.status = 0; /* 活动关闭 */
    //string msg; nt >> msg;
    //logic.usermgr().broadcast(sc_msg_def::nt_spec_event_t::cmd(), msg);

    /* 全服广播活动结束 */
    logwarn((LOG, "spec_event_end board cast"));
    sc_msg_def::nt_spec_event_change_state_t nt_;
    nt_.state = 0;
    string msg_;
    nt_ >> msg_;
    logic.broadcast_message(sc_msg_def::nt_spec_event_change_state_t::cmd(), msg_);
    /* 添加活动log */
    logwarn((LOG, "spec_event_end add user log"));
    add_user_log();
    /* 活动结束再次更新排行榜 */
    logwarn((LOG, "spec_event_end inter_update"));
    spec_event_rank.inter_update();
    /* 获取排行榜 */
    logwarn((LOG, "spec_event_end get rank"));
    int32_t ranking_list[SPEC_EVENT_RANK_OUT_RANK];
    int32_t max_rank = 0;
    spec_event_rank.get_ranking_list(max_rank, ranking_list);
    if (max_rank == 0) {
    logwarn((LOG, "spec_event_end max rank 0"));
        return;
    }
    logwarn((LOG, "spec_event_end begin_offmail, max rank: %d", max_rank));
    sc_gmail_mgr_t::begin_offmail();
    for (int32_t rank = 1; rank <= max_rank; ++rank) {
    logwarn((LOG, "spec_event_end send mail, rank: %d", rank));
        /* 最大100阶段 */
        repo_def::spec_event_ranking_t *rp = NULL;
        for (size_t order = 1; order < 100; ++order) {
            rp = repo_mgr.spec_event_ranking.get(m_eid * 1000 + order);
            if (rp != NULL && rank >= rp->ranking[1] && rank <= rp->ranking[2]) {
                break;
            }
            rp = NULL;
        }
        if (rp == NULL) {
            break;
        }
        sc_msg_def::nt_bag_change_t nt;

        size_t size = rp->reward.size();
        for (size_t i = 1; i < size; ++i) {
            sc_msg_def::jpk_item_t item;
            item.itid = 0;
            item.resid = rp->reward[i][0];
            item.num = rp->reward[i][1];
            nt.items.push_back(item);
        }
        auto rp_gmail = mailinfo.get_repo(mail_spec_event_ranking);
        if (rp_gmail != NULL) {
            string msg = mailinfo.new_mail(mail_spec_event_ranking, rank);
            sc_gmail_mgr_t::add_offmail(ranking_list[rank], rp_gmail->title,
                rp_gmail->sender, msg, rp_gmail->validtime, nt.items);
        }
    }
    sc_gmail_mgr_t::do_offmail();
    logwarn((LOG, "spec_event_end send mail end"));
}

void 
sc_spec_event_t::reset_user_info(int32_t season_id_)
{
    string str_hosts;
    for (int32_t host : m_hosts)
    {
        str_hosts.append(std::to_string(host)+",");
    }
    str_hosts = str_hosts.substr(0, str_hosts.length()-1);

    // find a user
    char sql[256];
    sprintf(sql, "DELETE FROM `SpecEventUser` WHERE hostnum in (%s);", str_hosts.c_str());
    db_service.sync_execute(sql);
    spec_event_rank.inter_update();
/*    db_service.async_do([](string& sql_) {
        db_service.async_execute(sql_);
    }, sql);
*/
/*
    string m_del_user_sql = "DELETE FROM `SpecEventUser` WHERE uid in (";
    sql_result_t res;
    char sql[256];
    sprintf(sql, "SELECT uid FROM `SpecEventUser` WHERE hostnum in (%s);", str_hosts.c_str());
    db_service.sync_select(sql, res);

    for (size_t i=0; i<res.affect_row_num(); ++i)
    {
        if (res.get_row_at(i) == NULL)
        {
            logerror((LOG, "get row is NULL!, at:%lu", i));
            continue;
        }
        
        sql_result_row_t& row_ = *res.get_row_at(i);
        int32_t uid = (int32_t)std::atoi(row_[0].c_str());
        sp_user_t sp_user;
        if (logic.usermgr().get(uid, sp_user))
        {
            sp_user->spec_event.reset_user(season_id_);
        }
        else
        {
            m_del_user_sql += (std::to_string(uid)+",");
        }
    }

    m_del_user_sql = m_del_user_sql.substr(0, m_del_user_sql.length()-1);
    m_del_user_sql += ");";
    db_service.sync_execute(m_del_user_sql);
    spec_event_rank.inter_update();
    */
}

void
sc_spec_event_t::add_user_log()
{
    string str_hosts;
    for (int32_t host : m_hosts)
    {
        str_hosts.append(std::to_string(host)+",");
    }
    str_hosts = str_hosts.substr(0, str_hosts.length()-1);

    string m_add_log_sql = "INSERT IGNORE INTO `SpecEventUserLog` ( `eid`, `uid`, `coin`, `score`, `goal_level`, `round`, `round_status`, `round_max`, `reset_time`, `pid1`, `pid2`, `pid3`, `pid4`, `pid5`, `anger` ) VALUES ";

    sql_result_t res;
    char sql[256];
    sprintf(sql, "SELECT * FROM `SpecEventUser` WHERE hostnum in (%s);", str_hosts.c_str());
    db_service.sync_select(sql, res);

    for (size_t i=0; i<res.affect_row_num(); ++i)
    {
        if (res.get_row_at(i) == NULL)
        {
            logerror((LOG, "get row is NULL!, at:%lu", i));
            continue;
        }
        
        sql_result_row_t& row_ = *res.get_row_at(i);
        int32_t uid = (int32_t)std::atoi(row_[0].c_str());
        int32_t coin = (int32_t)std::atoi(row_[2].c_str());
        int32_t score = (int32_t)std::atoi(row_[3].c_str());
        int32_t goal_level = (int32_t)std::atoi(row_[4].c_str());
        int32_t round = (int32_t)std::atoi(row_[5].c_str());
        int32_t round_status = (int32_t)std::atoi(row_[6].c_str());
        int32_t round_max = (int32_t)std::atoi(row_[7].c_str());
        int32_t difficult = (int32_t)std::atoi(row_[8].c_str());
        int32_t reset_time = (int32_t)std::atoi(row_[9].c_str());
        int32_t pid1 = (int32_t)std::atoi(row_[10].c_str());
        int32_t pid2 = (int32_t)std::atoi(row_[11].c_str());
        int32_t pid3 = (int32_t)std::atoi(row_[12].c_str());
        int32_t pid4 = (int32_t)std::atoi(row_[13].c_str());
        int32_t pid5 = (int32_t)std::atoi(row_[14].c_str());
        int32_t anger = (int32_t)std::atoi(row_[15].c_str());
        int32_t eid = (int32_t)std::atoi(row_[22].c_str());
        char tmp_sql[256];
        sprintf(tmp_sql, "(%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d),", eid, uid, coin, score, goal_level, round, round_status, round_max, reset_time, pid1, pid2, pid3, pid4, pid5, anger);
        m_add_log_sql += string(tmp_sql);
    }
    m_add_log_sql = m_add_log_sql.substr(0, m_add_log_sql.length()-1);
    db_service.sync_execute(m_add_log_sql);
}
    
void
sc_spec_event_t::get_round_data(int32_t resid_, string &view_data_,
                                int32_t *pids, sc_user_t& user_)
{
    logwarn((LOG, "get_round_data: %d", resid_));
    auto it = m_spec_event_round.find(resid_);
    if (it == m_spec_event_round.end()) {
        return ;
    }
    pids[0] = it->second->pid1;
    pids[1] = it->second->pid2;
    pids[2] = it->second->pid3;
    pids[3] = it->second->pid4;
    pids[4] = it->second->pid5;
    
    view_data_ = it->second->view_data;

    if( true)
    //if (resid_%1000 < 500)
    {
        double coefficient;
        double enemy_fp;
        double self_fp;
        
        auto it_diff = repo_mgr.spec_event_difficult.get(int((resid_%1000)/100)+1);
        if (it_diff == NULL)
            return;
        coefficient = it_diff->attribute;

        int max_fp_default = 40000;
        auto it_cur_fp = m_spec_event_round_fp.find(resid_);
        if (it_cur_fp != m_spec_event_round_fp.end())
        {
            if (it_cur_fp->second == 0)
            {
                int max_fp = 0;
                int begin_resid = int(resid_/100)*100+1;
                int end_resid = int(resid_/100)*100+SPEC_EVENT_ROUND_LEVEL_EVER;
                for(int round = begin_resid; round <= end_resid; ++round)
                {
                    auto it_fp = m_spec_event_round.find(round);
                    if (it_fp == m_spec_event_round.end())
                        continue;
                    sc_msg_def::jpk_fight_view_user_data_t data_;
                    data_ << it_fp->second->view_data;
                    logwarn((LOG, "resid: %d, fp: %lf", round, data_.fp));
                    if (data_.fp > max_fp)
                        max_fp = data_.fp;
                }
                for(int round = begin_resid; round <= end_resid; ++round)
                {
                    auto it_fp = m_spec_event_round_fp.find(round);
                    if (it_fp == m_spec_event_round_fp.end())
                    {
                        m_spec_event_round_fp.insert(make_pair(round, max_fp));
                    }
                    else
                    {
                        it_fp->second = max_fp;
                    }
                }                
            }
            max_fp_default = it_cur_fp->second;
        }

        enemy_fp = max_fp_default;
        
        self_fp = user_.get_max_5_fp();
        logwarn((LOG, "coefficient %lf, enemy_fp %lf, self_fp %lf", coefficient, enemy_fp, self_fp));

        if (enemy_fp > self_fp*coefficient)
        {
            sc_msg_def::jpk_fight_view_user_data_t user_data_;
            user_data_ << view_data_;
            if (enemy_fp < 1)
                enemy_fp = 1;
            double coefficient_user = self_fp*coefficient/enemy_fp;
            for (auto it=user_data_.roles.begin(); it!=user_data_.roles.end(); ++it)
            {
                it->second.pro[0] *= coefficient_user;
                it->second.pro[1] *= coefficient_user;
                it->second.pro[2] *= coefficient_user;
                it->second.pro[3] *= coefficient_user;
                it->second.pro[4] *= coefficient_user;
            }
            user_data_.fp = int(user_data_.fp*coefficient_user);

            view_data_ = "";
            user_data_ >> view_data_;
        }
    }
}
void sc_spec_event_t::getBeginResetStamp(uint32_t& start_stamp, uint32_t& reset_stamp)
{
    start_stamp = m_start_stamp;
    reset_stamp = m_reset_stamp;
}
void sc_spec_event_t::getBeginEndStamp(uint32_t& start_stamp, uint32_t& end_stamp)
{
    start_stamp = m_start_stamp;
    end_stamp = m_end_stamp;
}

/* sc_spec_event_t end */

/* sc_spec_event_rank_t begin */
sc_spec_event_rank_t::sc_spec_event_rank_t():
m_timestamp(0)
{
}

bool
spec_event_compare(sp_spec_event_rank_t a, sp_spec_event_rank_t b)
{
    if (a->score > b->score) {
        return true;
    }
    if (a->score == b->score) {
        if (a->uid > b->uid) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

void
sc_spec_event_rank_t::update()
{
    /* 活动未开启且非第一次捞数据 */
    if (!spec_event_s.isopen() && m_timestamp != 0) {
        return;
    }
    inter_update();
}

void
sc_spec_event_rank_t::inter_update()
{
    uint32_t cur_sec = date_helper.cur_sec();
    /* 整点更新 客户端最多每一小时请求一次服务器数据 */
    if (cur_sec % SPEC_EVENT_RANK_UPDATE_FREQUENCY == 0 || m_timestamp == 0) { 
        m_timestamp = cur_sec;
        sql_result_t res;
        
        string str_hosts;
        for (int32_t host : m_hosts)
        {
            str_hosts.append(std::to_string(host)+",");
        }
        str_hosts = str_hosts.substr(0, str_hosts.length()-1);

        char buf[1024];
        /* 最多捞1,000条数据入内存 */
        sprintf(buf, "SELECT ui.uid, nickname, grade, fp, ceu.score,viplevel from UserInfo ui "
                     "LEFT JOIN SpecEventUser ceu ON ui.uid = ceu.uid "
                     "WHERE ceu.score > 0 AND ui.hostnum in (%s) AND ceu.season = %d "
                     "ORDER BY ceu.score DESC LIMIT 1000 ", 
                str_hosts.c_str(), spec_event_s.latest_season());
        db_service.sync_select(buf, res);
        m_spec_event_rank_list.clear();
        for (size_t i = 0; i < res.affect_row_num(); ++i) {
            if (res.get_row_at(i) == NULL) {
                logerror((LOG, "load spec event rank get_row_at is NULL!!, at:%lu", i));
                break;
            }
            sp_spec_event_rank_t sp_spec_event_rank(
                new sc_msg_def::jpk_spec_event_rank_t);
            sql_result_row_t &row = *res.get_row_at(i);
            sp_spec_event_rank->uid = (int)std::atoi(row[0].c_str());
            sp_spec_event_rank->nickname = row[1];
            sp_spec_event_rank->level = (int)std::atoi(row[2].c_str());
            sp_spec_event_rank->fp = (int)std::atoi(row[3].c_str());
            sp_spec_event_rank->score = (int)std::atoi(row[4].c_str());
            sp_spec_event_rank->vip = (int)std::atoi(row[5].c_str());
            m_spec_event_rank_list.push_back(sp_spec_event_rank);
        }
        if (!m_spec_event_rank_list.empty()) {
            m_spec_event_rank_list.sort(spec_event_compare);
        }
    }
}

void
sc_spec_event_rank_t::unicast_spec_event_rank(int32_t uid_)
{
    sc_msg_def::ret_spec_event_rank_t ret;
    auto it_list = m_spec_event_rank_list.begin();
    for (size_t n = 0; it_list != m_spec_event_rank_list.end() && n < 100; ++n) {
        ret.users.push_back(*(*it_list));
        ++it_list;
    }
    ret.rank = get_rank(uid_);
    m_spec_event_rank_list_str.clear();
    ret >> m_spec_event_rank_list_str;
    logic.unicast(uid_, sc_msg_def::ret_spec_event_rank_t::cmd(),
                  m_spec_event_rank_list_str);
}

int32_t
sc_spec_event_rank_t::get_rank(int32_t uid_)
{
    int32_t rank_ = 1;
    for (auto it = m_spec_event_rank_list.begin(); 
         it != m_spec_event_rank_list.end(); ++it, ++rank_)
    {
        if ((*it)->uid == uid_) {
            return rank_;
        }
    }
    return SPEC_EVENT_RANK_OUT_RANK;
}

void
sc_spec_event_rank_t::get_ranking_list(int32_t &max_rank_, int32_t *ranking_list_)
{
    int32_t rank_ = 0;
    for (auto it = m_spec_event_rank_list.begin();
         it != m_spec_event_rank_list.end(); ++it, ++rank_)
    {
        ranking_list_[rank_ + 1] = (*it)->uid;
    }
    max_rank_ = rank_;
}
/* sc_spec_event_rank_t end */

/* sc_spec_event_user_t begin */
sc_spec_event_user_t::sc_spec_event_user_t(sc_user_t& user_):
m_user(user_)
{
}

void
sc_spec_event_user_t::load_db()
{
    sql_result_t res;
    if (0 == db_SpecEventUser_t::sync_select_uid(m_user.db.uid, res)) {
        db.init(*res.get_row_at(0));
        int season_id = spec_event_s.cur_season();
        if (season_id > 0 && db.season != season_id)
        {
            if (db.score == db.coin)
            {
                db.season = season_id;
                db_service.async_do((uint64_t)m_user.db.uid, [](db_SpecEventUser_t& db_) {
                    db_.update();
                }, db.data());
            }
            else
            {
                reset_user(season_id);
            }
        }
    } else {
        init_new_user();
    }
}

void
sc_spec_event_user_t::save_db()
{
    string sql = db.get_up_sql();
    if (sql.empty()) return;
    db_service.async_do((uint64_t)db.uid, [](string& sql_) {
        db_service.async_execute(sql_);
    }, sql);
}

void
sc_spec_event_user_t::init_new_user()
{
    db.uid = m_user.db.uid;
    db.hostnum = m_user.db.hostnum;
    db.score = 0;
    db.coin = 0;
    db.goal_level = 0;
    db.round = SPEC_EVENT_ROUND_BEGIN;
    db.round_status = SPEC_EVENT_ROUND_UNOPEN; /* 默认未开启关卡且未通关*/
    db.round_max = SPEC_EVENT_ROUND_BEGIN;
    db.reset_time = 0;
    db.pid1 = -1;
    db.pid2 = -1;
    db.pid3 = -1;
    db.pid4 = -1;
    db.pid5 = -1;
    db.anger = 0;
    db.enemy_view_data = "";
    db.hp1 = 1;
    db.hp2 = 1;
    db.hp3 = 1;
    db.hp4 = 1;
    db.hp5 = 1;
    db.difficult = 0;
    db.season = spec_event_s.cur_season();
    db.open_times = 0;
    db.open_level = 0;
    db.first_enter_time = 0;
    db.next_count = 0;
    db_service.async_do((uint64_t)m_user.db.uid, [](db_SpecEventUser_t& db_) {
        db_.insert();
    }, db.data());
}

void
sc_spec_event_user_t::reset_user(int season_id)
{
    // 更新当前
    db.uid = m_user.db.uid;
    db.hostnum = m_user.db.hostnum;
    db.score = 0;
    db.coin = 0;
    db.goal_level = 0;
    db.round = SPEC_EVENT_ROUND_BEGIN;
    db.round_status = SPEC_EVENT_ROUND_UNOPEN; /* 默认未开启关卡且未通关*/
    db.round_max = SPEC_EVENT_ROUND_BEGIN;
    db.reset_time = 0;
    db.pid1 = -1;
    db.pid2 = -1;
    db.pid3 = -1;
    db.pid4 = -1;
    db.pid5 = -1;
    db.anger = 0;
    db.enemy_view_data = "";
    db.hp1 = 1;
    db.hp2 = 1;
    db.hp3 = 1;
    db.hp4 = 1;
    db.hp5 = 1;
    db.difficult = 0;
    db.season = season_id;
    db.open_times = 0;
    db.open_level = 0;
    db.next_count = 0;
    db_service.async_do((uint64_t)m_user.db.uid, [](db_SpecEventUser_t& db_) {
        db_.update();
    }, db.data());
}

void
sc_spec_event_user_t::get_user_info(sc_msg_def::jpk_spec_event_t& 
                                               jpk_)
{
    jpk_.is_open = spec_event_s.isopen();
    jpk_.is_close = spec_event_s.isclose();
    jpk_.event_id = spec_event_s.get_event_id();
    jpk_.score = db.score;
    jpk_.coin = db.coin;
    jpk_.round = db.round;
    jpk_.goal_level = db.goal_level;
    jpk_.round_status = db.round_status;
    jpk_.round_max = db.round_max;
    jpk_.rank = spec_event_rank.get_rank(m_user.db.uid);
    jpk_.difficult = db.difficult;
    jpk_.open_times = db.open_times;
    jpk_.open_level = db.open_level;
    jpk_.next_count = db.next_count;
    jpk_.is_first_enter = date_helper.offday(db.first_enter_time);
    spec_event_s.getBeginEndStamp(jpk_.begin_stamp,jpk_.end_stamp);
    spec_event_s.getBeginEndDate(jpk_.begin_month, jpk_.begin_day, jpk_.begin_hour, jpk_.end_month, jpk_.end_day, jpk_.end_hour);
}

int32_t
sc_spec_event_user_t::consume_coin(int32_t coin_)
{
    if (coin_ < 0) {
        return ERROR_SC_CONSUME;
    }
    if (db.coin < coin_) {
        return ERROR_SC_SPEC_EVENT_COIN_NOT_ENOUGH;
    }
    /* 做功能时道具和活动币未明确区分 */
    db.set_coin(db.coin - coin_);
    save_db();
    sc_msg_def::nt_spec_event_coin_t nt;
    nt.now = db.coin;
    logic.unicast(m_user.db.uid, nt);
    return SUCCESS;
}

void
sc_spec_event_user_t::add_coin()
{
    if (!spec_event_s.isopen()) {
        return;
    }
    uint32_t prob = repo_mgr.configure.find(SPEC_EVENT_DROP_PROBABILITY_ID)
                                       ->second.value;
    uint32_t rand_ = random_t::rand_integer(1, 10000);
    if (rand_ > prob) {
        return;
    }
    /* 做功能时道具和活动币未明确区分 后续改动后增加时只增加道具即可 */
    /* 是道具 需要单独加个道具通知 */
    sc_msg_def::nt_bag_change_t nt_bag;
    m_user.item_mgr.addnew(SPEC_EVENT_COIN_ITEM_ID, 1, nt_bag);
    m_user.item_mgr.on_bag_change(nt_bag);
    save_db();
    /* 需要单独加个道具掉落通知，有些冗余。。。 */
    sc_msg_def::nt_spec_event_drop_coin_t nt_drop_coin;
    nt_drop_coin.resid = SPEC_EVENT_COIN_ITEM_ID;
    nt_drop_coin.num = 1;
    logic.unicast(m_user.db.uid, nt_drop_coin);
}

int
sc_spec_event_user_t::on_coin_change(int32_t coin_)
{
    if (coin_ == 0) {
        return db.coin;
    }
    if (coin_ > 0) {
        int n = repo_mgr.configure.find(SPEC_EVENT_COIN_TIMES)->second.value;
        add_score(coin_ * n);
    }
    db.set_coin(db.coin + coin_);
    if (db.coin < 0) {
        db.set_coin(0);
    }
    sc_msg_def::nt_spec_event_coin_t nt;
    nt.now = db.coin;
    save_db();
    logic.unicast(m_user.db.uid, nt);
    return db.coin;
}

void
sc_spec_event_user_t::add_score(int32_t score_)
{
    if (score_ <= 0) {
        return;
    }
    db.set_score(db.score + score_);
    save_db();
    int32_t goal_level = point_reward();
    sc_msg_def::nt_spec_event_score_t nt;
    nt.now = db.score;
    nt.goal_level = goal_level;
    logic.unicast(m_user.db.uid, nt);
    statics_ins.unicast_eventlog(m_user, 5472, db.score, score_);

    sc_msg_def::nt_bag_change_t nt_p;
    m_user.item_mgr.addnew(EVENT_POINT, score_, nt_p);
    m_user.item_mgr.on_bag_change(nt_p);
}

void
sc_spec_event_user_t::get_enemy_team(sc_msg_def::jpk_spec_event_team_t &jpk_)
{
    jpk_.anger = db.anger;
    jpk_.actors[0].pid = db.pid1;
    jpk_.actors[0].hp  = db.hp1;
    jpk_.actors[1].pid = db.pid2;
    jpk_.actors[1].hp  = db.hp2;
    jpk_.actors[2].pid = db.pid3;
    jpk_.actors[2].hp  = db.hp3;
    jpk_.actors[3].pid = db.pid4;
    jpk_.actors[3].hp  = db.hp4;
    jpk_.actors[4].pid = db.pid5;
    jpk_.actors[4].hp  = db.hp5;
}

int32_t
sc_spec_event_user_t::open(sc_msg_def::req_spec_event_open_round_t& jpk_,
                           sc_msg_def::ret_spec_event_open_round_t& ret_)
{
    if (!spec_event_s.isopen()) {
        return ERROR_SC_SPEC_EVENT_CLOSED;
    }
    int32_t begin_round = int(jpk_.resid/100)*100+1;
    int32_t end_round = int(jpk_.resid/100)*100+SPEC_EVENT_ROUND_LEVEL_EVER;
    if (!(begin_round <= jpk_.resid && jpk_.resid <= end_round))
        return ERROR_SC_SPEC_EVENT_ROUND;
    /*
    if (jpk_.resid != db.round) {
        return ERROR_SC_SPEC_EVENT_ROUND;
    }*/
    int32_t ret = SUCCESS;
    /* 每一阶层开启才消耗活动币 */
    if (jpk_.resid % 100 == 1 && db.round_status == SPEC_EVENT_ROUND_UNOPEN) {
        int difficult = (jpk_.resid % 1000) / 100 + 1;
        repo_def::spec_event_difficult_t *rp_difficult = repo_mgr.spec_event_difficult.get(difficult);
        int consume = repo_mgr.spec_event_difficult.find(difficult)->second.open_cost; 
        ret = consume_coin(consume);
        m_user.team_mgr.spec_event_reset_partner();
        if (SUCCESS == ret)
        {
            db.open_times++;
            db.set_open_times(db.open_times);
            open_reward();
            db.set_next_count(0);
            sc_msg_def::nt_spec_event_open_t nt;
            nt.open_times = db.open_times;
            nt.open_level = db.open_level;
            nt.next_count = db.next_count;
            logic.unicast(m_user.db.uid, nt);
        }
    }
    if (SUCCESS == ret) {
        string view_data;
        int32_t pids[5] = {-1, -1, -1, -1, -1};
        spec_event_s.get_round_data(jpk_.resid, view_data, pids, m_user);

        db.set_round_status(SPEC_EVENT_ROUND_OPEN); /* 打开关卡并设置未为通关 */
        db.set_enemy_view_data(view_data);
        db.set_pid1(pids[0]);
        db.set_hp1(1.0);
        db.set_pid2(pids[1]);
        db.set_hp2(1.0);
        db.set_pid3(pids[2]);
        db.set_hp3(1.0);
        db.set_pid4(pids[3]);
        db.set_hp4(1.0);
        db.set_pid5(pids[4]);
        db.set_hp5(1.0);
        db.set_round(jpk_.resid);
        save_db();
    }
    return ret;
}
int32_t sc_spec_event_user_t::next_round_end(sc_msg_def::ret_next_spec_event_t &ret_)
{
    repo_def::spec_event_round_t* rp = 
        repo_mgr.spec_event_round.get(db.round);
    if (NULL == rp) {
        logwarn((LOG, "load_db, spec_event_point not exists"));
        return FAILED;
    }
    add_score(rp->point);

    int32_t yb = 0;
    if (db.next_count == 0){
        yb = repo_mgr.configure.find(38)->second.value;
    }else if(db.next_count == 1){
        yb = repo_mgr.configure.find(39)->second.value;
    }else{
        yb = repo_mgr.configure.find(40)->second.value;
    }
    if (m_user.rmb() >= yb)
    {
        m_user.consume_yb(yb);
        m_user.save_db();
    }else{
       return ERROR_SC_SPEC_EVENT_NEXT_NO_MONEY;
    }
    db.set_next_count(db.next_count + 1);


    db.set_round_status(db.round_status | SPEC_EVENT_ROUND_PASS);
    if (db.round > db.round_max) {
        db.set_round_max(db.round);
    }
    difficult_reward();

    save_db();
    ret_.max_round = db.round_max;
    ret_.goal_level = db.goal_level;
    ret_.next_count = db.next_count;
    return SUCCESS;
}

int32_t sc_spec_event_user_t::set_first_enter()
{
    if(date_helper.offday(db.first_enter_time) >= 1){
        db.set_first_enter_time(date_helper.cur_sec());
        save_db();
    }
    return SUCCESS;
}


void
sc_spec_event_user_t::get_starnum(int32_t uid, int32_t pid, int32_t &rank)
{
    sql_result_t res;
    char sql[256];
    sprintf(sql, "SELECT quality FROM Partner WHERE uid=%d and pid=%d;",
            uid, pid);
    db_service.sync_select(sql, res);
    if (0 == res.affect_row_num()) {
        rank = 0;
        return;
    }
    sql_result_row_t &row = *res.get_row_at(0);
    rank = (int)std::atoi(row[0].c_str());
}

int32_t
sc_spec_event_user_t::get_enemy_info(sc_msg_def::ret_spec_event_round_enemy_t &ret_)
{
    if (!spec_event_s.isopen()) {
        return ERROR_SC_SPEC_EVENT_CLOSED;
    }
    if (db.round_status != SPEC_EVENT_ROUND_OPEN) {
        return ERROR_SC_SPEC_EVENT_ROUND_UNOPEN;
    }
    if (db.enemy_view_data.empty()) {
        return ERROR_SC_SPEC_EVENT_ROUND_NO_ENEMY;
    }

    sc_msg_def::jpk_fight_view_user_data_t data_;
    data_ << db.enemy_view_data;
    ret_.role.nickname = data_.name;
    ret_.role.viplv = data_.viplevel;
    ret_.role.rank = data_.rank;
    ret_.role.unionname = "無";
    ret_.role.level = data_.lv;
    ret_.role.fp = data_.fp;

    ret_.combo_pro.combo_d_down = data_.combo_pro.combo_d_down;
    ret_.combo_pro.combo_r_down = data_.combo_pro.combo_r_down;
    ret_.combo_pro.combo_d_up =  data_.combo_pro.combo_d_up;
    ret_.combo_pro.combo_r_up =  data_.combo_pro.combo_r_up;
    for (auto it = data_.combo_pro.combo_anger.begin(); it != data_.combo_pro.combo_anger.end(); ++it){
       ret_.combo_pro.combo_anger.insert(make_pair(it->first,it->second)); 
    }

    auto fill_jpk = [&](int32_t pid) {
        if (pid < 0) return;
        sc_msg_def::jpk_spec_event_enemy_t jpk_;
        for (auto it = data_.roles.begin(); it != data_.roles.end(); ++it) {
            if (it->second.pid == pid) {
                jpk_.resid = it->second.resid;
                jpk_.level = it->second.lvl[0];
                jpk_.lovelevel = it->second.lvl[1];
                if (pid == 0) {
                    sql_result_t buf;
                    char sql[256];
                    sprintf(sql, "SELECT quality FROM UserInfo WHERE uid=%d;",
                            data_.uid);
                    db_service.sync_select(sql, buf);
                    if (0 == buf.affect_row_num()) {
                        jpk_.starnum = 1;
                    } else {
                        sql_result_row_t &row = *buf.get_row_at(0);
                        jpk_.starnum = (int)std::atoi(row[0].c_str());
                    }
                } else {
                    get_starnum(it->second.uid, pid, jpk_.starnum);
                }
                jpk_.equiplv = sc_user_t::get_equip_level(it->second.uid, pid);
                break;
            }
        }
        if (pid == db.pid1) {
            jpk_.hp = db.hp1;
        } else if (pid == db.pid2) {
            jpk_.hp = db.hp2;
        } else if (pid == db.pid3) {
            jpk_.hp = db.hp3;
        } else if (pid == db.pid4) {
            jpk_.hp = db.hp4;
        } else if (pid == db.pid5) {
            jpk_.hp = db.hp5;
        } else {
            jpk_.hp = 1;
        }
        ret_.enemys.push_back(std::move(jpk_));
    };
    fill_jpk(db.pid1);
    fill_jpk(db.pid2);
    fill_jpk(db.pid3);
    fill_jpk(db.pid4);
    fill_jpk(db.pid5);
    return SUCCESS;
}

int32_t
sc_spec_event_user_t::fight(sc_msg_def::req_spec_event_fight_t& jpk_,
                            sc_msg_def::ret_spec_event_fight_t& ret_)
{
    if (!spec_event_s.isopen()) {
        return ERROR_SC_SPEC_EVENT_CLOSED;
    }
    /*
    if (jpk_.resid != db.round) {
        return ERROR_SC_SPEC_EVENT_ROUND;
    }
    */
    if (db.round_status != SPEC_EVENT_ROUND_OPEN) {
        return ERROR_SC_SPEC_EVENT_ROUND_UNOPEN;
    }
    ret_.view_data = db.enemy_view_data;
    ret_.resid = jpk_.resid;
    ret_.team[0].pid = db.pid1;
    ret_.team[0].hp = db.hp1;
    ret_.team[1].pid = db.pid2;
    ret_.team[1].hp = db.hp2;
    ret_.team[2].pid = db.pid3;
    ret_.team[2].hp = db.hp3;
    ret_.team[3].pid = db.pid4;
    ret_.team[3].hp = db.hp4;
    ret_.team[4].pid = db.pid5;
    ret_.team[4].hp = db.hp5;
    return SUCCESS;
}

int32_t
sc_spec_event_user_t::fight_end(sc_msg_def::req_spec_event_round_exit_t& jpk_,
                                sc_msg_def::ret_spec_event_round_exit_t& ret_)
{
    ret_.win = false;
    if (jpk_.is_win) {
        ret_.win = true;
        /* 发积分 */
        repo_def::spec_event_round_t* rp = 
            repo_mgr.spec_event_round.get(db.round);
        if (NULL == rp) {
            logwarn((LOG, "load_db, spec_event_point not exists"));
            return FAILED;
        }
        add_score(rp->point);

        /* 关卡通过 */
        db.set_round_status(db.round_status | SPEC_EVENT_ROUND_PASS);
        if (db.round > db.round_max) {
            db.set_round_max(db.round);
        }
        /* 发奖 */
        difficult_reward();
    
    }
    for (auto actor : jpk_.enemy) {
        if (actor.pid == db.pid1) {
            db.set_hp1(actor.hp);
        } else if (actor.pid == db.pid2) {
            db.set_hp2(actor.hp);
        } else if (actor.pid == db.pid3) {
            db.set_hp3(actor.hp);
        } else if (actor.pid == db.pid4) {
            db.set_hp4(actor.hp);
        } else if (actor.pid == db.pid5) {
            db.set_hp5(actor.hp);
        }
    }
    db.set_anger(jpk_.anger);
    save_db();
    ret_.goal_level = db.goal_level;
    ret_.max_round = db.round_max;
    m_user.team_mgr.spec_event_update_team(jpk_.ally);
    return SUCCESS;
}
void sc_spec_event_user_t::spec_event_sweep()
{
    sc_msg_def::ret_spec_event_sweep_t ret;    
    ret.score = 0;
    int32_t sweep_end_round = db.round_max-5;  
    int32_t sweep_start_round = db.round;
    int32_t cur_round_type = db.round/100;
    int32_t cur_max_round_type = db.round_max/100;
    if (cur_round_type < cur_max_round_type){
        sweep_end_round = cur_round_type*100+25;      
    }  
    else if(cur_round_type > cur_max_round_type){
        ret.code = ERROR_SPEC_EVENT_CANT_SWEEP;
        logic.unicast(m_user.db.uid,ret);
        return; 
    }

    if ( db.round == SPEC_EVENT_ROUND_BEGIN && db.round_max == SPEC_EVENT_ROUND_BEGIN){
        ret.code = ERROR_SPEC_EVENT_CANT_SWEEP;
        logic.unicast(m_user.db.uid,ret);
        return; 
    }

    if (sweep_start_round>sweep_end_round){
        ret.code = ERROR_SPEC_EVENT_CANT_SWEEP;
        logic.unicast(m_user.db.uid,ret);
       return; 
    }
    for(int32_t i = sweep_start_round;i <= sweep_end_round;++i){
        repo_def::spec_event_round_t* rp = repo_mgr.spec_event_round.get(i); 
        if (NULL == rp){
           logwarn((LOG,"spec_event_sweep,spec_event_point not exists")); 
           continue;
        }
        ret.score += rp->point;
        add_score(rp->point);
    }

    string view_data;
    int32_t pids[5] = {-1, -1, -1, -1, -1};
    spec_event_s.get_round_data(sweep_end_round+1, view_data, pids, m_user);

    db.set_round_status(SPEC_EVENT_ROUND_OPEN); /* 打开关卡并设置未为通关 */
    db.set_enemy_view_data(view_data);
    db.set_pid1(pids[0]);
    db.set_hp1(1.0);
    db.set_pid2(pids[1]);
    db.set_hp2(1.0);
    db.set_pid3(pids[2]);
    db.set_hp3(1.0);
    db.set_pid4(pids[3]);
    db.set_hp4(1.0);
    db.set_pid5(pids[4]);
    db.set_round(sweep_end_round+1);
    save_db();
    ret.start_round = sweep_start_round;
    ret.end_round = sweep_end_round;
    ret.code = SUCCESS;
    logic.unicast(m_user.db.uid,ret);
    return; 
}
int32_t
sc_spec_event_user_t::point_reward()
{
    repo_def::spec_event_point_t *rp_point = NULL;
    int32_t id = db.goal_level == 0 ? spec_event_s.get_event_id() * 1000 + 1: 
                 db.goal_level + 1;
    for (int index = id; index < id + 999; ++index)
    {
        rp_point = repo_mgr.spec_event_point.get(index);
        if (rp_point != NULL) {
            if (db.score >= rp_point->goal_point) {
                /* 满足下一阶段积分条件 */
                db.set_goal_level(index);
                sc_msg_def::nt_bag_change_t nt;
                auto item_size = rp_point->reward.size();
                for (auto i = 1; i < item_size; ++i) {
                    sc_msg_def::jpk_item_t item_;
                    item_.itid = 0;
                    item_.resid = rp_point->reward[i][0];
                    item_.num = rp_point->reward[i][1];
                    nt.items.push_back(item_);
                }
                string msg = mailinfo.new_mail(mail_spec_event_stage, rp_point->goal_point);
                auto rp_gmail = mailinfo.get_repo(mail_spec_event_stage);
                if (rp_gmail != NULL) {
                    m_user.gmail.send(rp_gmail->title, rp_gmail->sender, msg,
                                     rp_gmail->validtime, nt.items);
                }
                sc_msg_def::nt_spec_event_get_reward_t nt1;
                nt1.level = index;
                logic.unicast(m_user.db.uid, nt1);            
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }
    return db.goal_level;
}

void sc_spec_event_user_t::difficult_reward()
{
   
    repo_def::spec_event_reward_t *rp_reward = NULL;
    int difficult = int((db.round % 1000) / 100) + 1;
    int level = db.round % 100;
    if (level != SPEC_EVENT_ROUND_LEVEL_EVER)
        return;

    //开服活动,活动通关    
    m_user.reward.update_opentask(open_task_specevent_pass);
 
    if (get_reward_difficult(difficult))
        set_reward_difficult(difficult);
    else
        return;
    rp_reward = repo_mgr.spec_event_reward.get(difficult+spec_event_s.get_event_id() * 1000);
    if (rp_reward != NULL) {
        sc_msg_def::nt_bag_change_t nt;
        auto item_size = rp_reward->reward.size();
        for (auto i = 1; i < item_size; ++i)
        {
            sc_msg_def::jpk_item_t item_;
            item_.itid = 0;
            item_.resid = rp_reward->reward[i][0];
            item_.num = rp_reward->reward[i][1];
            nt.items.push_back(item_);
        }
        repo_def::spec_event_difficult_t *rp_difficult = repo_mgr.spec_event_difficult.get(difficult);
        string msg = mailinfo.new_mail(mail_spec_event_difficult, rp_difficult->name+rp_difficult->name_1);
        auto rp_gmail = mailinfo.get_repo(mail_spec_event_difficult);
        if (rp_gmail != NULL) {
            m_user.gmail.send(rp_gmail->title, rp_gmail->sender, msg,
            rp_gmail->validtime, nt.items);
        }
        sc_msg_def::nt_spec_event_get_level_t nt1;
        nt1.level = difficult+spec_event_s.get_event_id() * 1000;
        logic.unicast(m_user.db.uid, nt1);            
    }
}

void sc_spec_event_user_t::open_reward()
{
    auto rp_reward = repo_mgr.spec_event_open_reward.get(db.open_level+1);
    if (rp_reward != NULL) {
        if (db.open_times >= rp_reward->times) {
            /* 满足下一阶段积分条件 */
            db.open_level++;
            db.set_open_level(db.open_level);
            sc_msg_def::nt_bag_change_t nt;
            auto item_size = rp_reward->reward.size();
            for (auto i = 1; i < item_size; ++i) {
                sc_msg_def::jpk_item_t item_;
                item_.itid = 0;
                item_.resid = rp_reward->reward[i][0];
                item_.num = rp_reward->reward[i][1];
                nt.items.push_back(item_);
            }
            string msg = mailinfo.new_mail(mail_spec_event_open, db.open_times);
            auto rp_gmail = mailinfo.get_repo(mail_spec_event_open);
            if (rp_gmail != NULL) {
                m_user.gmail.send(rp_gmail->title, rp_gmail->sender, msg,
                                 1, nt.items);
            }


        }
    }
    save_db();
}

bool sc_spec_event_user_t::get_reward_difficult(int difficult)
{
    int digit = pow(2,difficult-1);
    int is_achieve = db.difficult & digit;
    return (is_achieve == 0);
}

void sc_spec_event_user_t::set_reward_difficult(int difficult)
{
    int digit = pow(2,difficult-1);
    int difficult_value = db.difficult | digit;
    db.difficult = difficult_value;
    db.set_difficult(difficult_value);
    save_db();
}

int32_t
sc_spec_event_user_t::reset()
{
    if (!spec_event_s.isopen()) {
        return ERROR_SC_SPEC_EVENT_CLOSED;
    }
    db.set_reset_time(db.reset_time + 1);
    db.set_round(SPEC_EVENT_ROUND_BEGIN);
    db.set_round_status(SPEC_EVENT_ROUND_UNOPEN);
    db.set_hp1(1.0);
    db.set_hp2(1.0);
    db.set_hp3(1.0);
    db.set_hp4(1.0);
    db.set_hp5(1.0);
    save_db();
    
    return SUCCESS;
}

int32_t
sc_spec_event_user_t::revive(sc_msg_def::req_spec_event_revive_t &jpk_,
                             sc_msg_def::ret_spec_event_revive_t &ret_)
{
    if (!spec_event_s.isopen()) {
        return ERROR_SC_SPEC_EVENT_CLOSED;
    }
    ret_.pid = jpk_.pid;
    int consume = repo_mgr.configure.find(SPEC_EVENT_REVIVE_ID)->second.value;
    if (m_user.rmb() >= consume)
    {
        m_user.consume_yb(consume);
        m_user.save_db();
        m_user.team_mgr.spec_event_revive(jpk_.pid);
    }
    else
    {
        return ERROR_PUB_NO_YB;
    }
    return SUCCESS;
}
/* sc_spec_event_user_t end */
