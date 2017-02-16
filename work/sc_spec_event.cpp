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
#define EVENT_POINT 16102
/* 关卡ID */
#define SPEC_EVENT_ROUND_BEGIN 9001
#define SPEC_EVENT_ROUND_END 9999
#define SPEC_EVENT_ROUND_LEVEL_MAX 5
#define SPEC_EVENT_ROUND_LEVEL_EVER 1
/* 活动 [关闭/开启/关卡通过] 状态 */
#define SPEC_EVENT_ROUND_UNOPEN 0x00
#define SPEC_EVENT_ROUND_OPEN 0x01
#define SPEC_EVENT_ROUND_PASS 0x02 /* 关卡通过数字为3 */
/* 各类configure对应ID */
#define SPEC_EVENT_DROP_PROBABILITY_ID 13
#define SPEC_EVENT_REVIVE_ID 14
#define SPEC_EVENT_COIN_TIMES 15
/* 千名之外 */
#define SPEC_EVENT_RANK_OUT_RANK 1000

/* sc_spec_event_t begin */
sc_spec_event_t::sc_spec_event_t():
m_start_stamp(0),
m_end_stamp(0),
m_reset_stamp(0),
m_eid(0),
m_state(1),
m_has_open(false),
m_has_close(false),
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

	// TODO
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
        reset_user_info();
		return;
    }
    if (0 == m_start_stamp || 0 == m_end_stamp) {
        return;
    }
    /* spec event begin */
	if (cur_sec < m_start_stamp) {
		m_state = 1;
	}
	else if (m_start_stamp <= cur_sec && cur_sec < m_end_stamp) {
		/* spec event begin */
		m_state = 2;
        if (!m_has_open) {
            spec_event_begin();
			m_has_open = true;
        }
    }
	else
	{
		/* spec event end */
		m_state = 3;
		if (!m_has_close) {
			spec_event_end();
			m_has_close = true;
		}
	}
}

int32_t
sc_spec_event_t::cur_season()
{
    return m_eid;
}

int32_t
sc_spec_event_t::latest_season()
{
    if (m_state == 2 || m_state == 3)
    {
        return m_eid;
    }
    else
    {
		return 0;
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
sc_spec_event_t::gen_all_spec_event_round()
{
	m_spec_event_round.clear();
	for(int32_t level=0; level < SPEC_EVENT_ROUND_LEVEL_MAX; ++level)
	{
		for(int32_t index=0; index < SPEC_EVENT_ROUND_LEVEL_EVER; ++index)
		{
			int32_t round_id_ = SPEC_EVENT_ROUND_BEGIN - index + (level-1)*100;
			spec_event_round round_;
			round_.round = round_id_;
			for(int32_t i=0; i<3; ++i)
			{
				spec_event_wave wave_;
				gen_spec_event_wave(round_id_, i+1, wave_);
				round_.insert(make_pair(i+1, wave_));
			}
			m_spec_event_round.insert(make_pair(round_id_, round_));
		}
	}
}

void
sc_spec_event_t::gen_spec_event_wave(int32_t round_, int32_t wave_, spec_event_wave& wave_data_)
{
	wave_data_.round = round_;
	wave_data_.wave_ = wave_;

    sc_msg_def::jpk_fight_view_user_data_t view_data;
	view_data.uid = -1;
	view_data.name = "";
	view_data.rank = -1;
	view_data.viplevel = 1;
	view_data.combo_pro.combo_d_down = 0;
	view_data.combo_pro.combo_r_down = 0;
    view_data.combo_pro.combo_d_up = 0;
    view_data.combo_pro.combo_r_up = 0;
	view_data.combo_r_up.combo_anger.insert(make_pair(50,0));
    auto repo_round = repo_mgr.spec_event_round.get(round_);
	bool gen = false;
	if( repo_round != NULL)
	{
		if (wave_ <= repo_round->monster.size())
		{
			int sum = 0;
			for(auto it_role=repo_round->monster[wave_].begin(); it_role!=repo_round->monster[wave_].end(); ++it_role)
			{
				int32_t role_id = *it_role;
				auto repo_role = repo_def.spec_event_monster_t.get(role_id);
				if( repo_role == NULL )
					continue;
				auto repo_role_resid = repo_def.role.get(repo_role->resid);
				if( repo_role_resid == NULL)
					continue;

				jpk_fight_view_role_data_t role_;
				role_.uid = -1;
				role_.pid = sum;
				role_.resid = repo_role->resid;
				int32_t count = 1;
				if( repo_role_resid->skill_id > 0 )
				{
					vector<int32_t> skills;
					skills.push_back(count++);
					skills.push_back(repo_role_resid->skill_id);
					skills.push_back(repo_role->lv);
					role_.skls.push_back(std::move(skills));
				}
				if( repo_role_resid->skill_id2 > 0 )
				{
					vector<int32_t> skills;
					skills.push_back(count++);
					skills.push_back(repo_role_resid->skill_id2);
					skills.push_back(repo_role->lv);
					role_.skls.push_back(std::move(skills));
				}
				if( repo_role_resid->skill_id3 > 0 )
				{
					vector<int32_t> skills;
					skills.push_back(count++);
					skills.push_back(repo_role_resid->skill_id3);
					skills.push_back(repo_role->lv);
					role_.skls.push_back(std::move(skills));
				}
				if( repo_role_resid->skill_passive > 0 )
				{
					vector<int32_t> skills;
					skills.push_back(count++);
					skills.push_back(repo_role_resid->skill_passive);
					skills.push_back(repo_role->lv);
					role_.skls.push_back(std::move(skills));
				}
				if( repo_role_resid->skill_passive2 > 0 )
				{
					vector<int32_t> skills;
					skills.push_back(count++);
					skills.push_back(repo_role_resid->skill_passive2);
					skills.push_back(repo_role->lv);
					role_.skls.push_back(std::move(skills));
				}
				if( repo_role_resid->skill_auto > 0 )
				{
					vector<int32_t> skills;
					skills.push_back(count++);
					skills.push_back(repo_role_resid->skill_auto);
					skills.push_back(repo_role->lv);
					role_.skls.push_back(std::move(skills));
				}
				if( repo_role_resid->skill_auto2 > 0 )
				{
					vector<int32_t> skills;
					skills.push_back(count++);
					skills.push_back(repo_role_resid->skill_auto2);
					skills.push_back(repo_role->lv);
					role_.skls.push_back(std::move(skills));
				}
				if( repo_role_resid->skill_auto3 > 0 )
				{
					vector<int32_t> skills;
					skills.push_back(count++);
					skills.push_back(repo_role_resid->skill_auto3);
					skills.push_back(repo_role->lv);
					role_.skls.push_back(std::move(skills));
				}
				role_.pro[0] = repo_role->atk;
				role_.pro[1] = repo_role->mgc;
				role_.pro[2] = repo_role->def;
				role_.pro[3] = repo_role->res;
				role_.pro[4] = repo_role->hp;
				role_.pro[5] = repo_role_resid->base_crit;
				role_.pro[6] = repo_role_resid->base_acc;
				role_.pro[7] = repo_role_resid->base_dodge;
				role_.pro[8] = 0;
				role_.pro[9] = 0;
				role_.pro[10] = repo_role_resid->base_ten;
				role_.pro[11] = repo_role_resid->base_imm_dam_pro;
				role_.pro[12] = 0;
				role_.pro[13] = repo_role_resid->factor[0];
				role_.pro[14] = repo_role_resid->factor[1];
				role_.pro[15] = repo_role_resid->factor[2];
				role_.pro[16] = repo_role_resid->factor[3];
				role_.pro[17] = repo_role_resid->factor[4];
				role_.pro[18] = repo_role_resid->atk_power;
				role_.pro[19] = repo_role_resid->hit_power;
				role_.pro[20] = repo_role_resid->kill_power;
				role_.wid = -1;

				view_data.roles.insert(make_pair(sum,role_));
				sum++;
			}
			if( sum > 0 )
				gen = true;
		}
	}
	if( !gen )
	{
		view_data.lv = 1;		
		view_data.fp = 500;
		
		jpk_fight_view_role_data_t role_;
		role_.uid = -1;
		role_.pid = 0;
		role_.resid = 1;
		vector<int32_t> skills;
		skills.push_back(1);
		skills.push_back(290);
		skills.push_back(1);
		role_.skls.push_back(std::move(skills));
		role_.pro[0] = 10;
		role_.pro[1] = 10;
		role_.pro[2] = 10;
		role_.pro[3] = 10;
		role_.pro[4] = 100;
		role_.pro[5] = 0;
		role_.pro[6] = 0;
		role_.pro[7] = 0;
		role_.pro[8] = 0;
		role_.pro[9] = 0;
		role_.pro[10] = 0;
		role_.pro[11] = 0;
		role_.pro[12] = 0;
		role_.pro[13] = 0.85;
		role_.pro[14] = 0.85;
		role_.pro[15] = 0.85;
		role_.pro[16] = 0.85;
		role_.pro[17] = 0.85;
		role_.pro[18] = 1;
		role_.pro[19] = 0.6;
		role_.pro[20] = 0;
		role_.wid = -1;
		view_data.roles.insert(make_pair(0,role_));
	}
	view_data >> wave_data_.view_data;
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

    gen_all_spec_event_round();
}

void 
sc_spec_event_t::spec_event_end()
{
    sc_msg_def::nt_spec_event_change_state_t nt_;
    nt_.state = 0;
    string msg_;
    nt_ >> msg_;
    logic.broadcast_message(sc_msg_def::nt_spec_event_change_state_t::cmd(), msg_);
	/* 活动结束发奖励 */
	send_event_end_reward();
	//TODO
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
sc_spec_event_t::reset_user_info()
{
    string str_hosts;
    for (int32_t host : m_hosts)
    {
        str_hosts.append(std::to_string(host)+",");
    }
    str_hosts = str_hosts.substr(0, str_hosts.length()-1);

    char sql[256];
    sprintf(sql, "DELETE FROM `SpecEventUser` WHERE hostnum in (%s);", str_hosts.c_str());
    db_service.sync_execute(sql);
}

/* sc_spec_event_rank_t begin */
sc_spec_event_rank_t::sc_spec_event_rank_t():
sc_rank_list_t(SPEC_EVENT_RANK_OUT_RANK)
{
}

void
sc_spec_event_rank_t::unicast_spec_event_rank(int32_t uid_)
{
    sc_msg_def::ret_spec_event_rank_t ret;
    ret.rank = this->find(uid_);
    vector<int32_t>& rank_list = this->getList(100);
    for (auto i=0; i<100; ++i)
    {
	    if (i+1 > rank_list.size())
		    continue;
	    auto it_list = m_spec_event_rank_list.find(rank_list[i]);
	    if (it_list != m_spec_event_rank_list.end())
	    {
		    ret.users.push_back(*(*it_list));
	    }
    }
    m_spec_event_rank_list_str.clear();
    ret >> m_spec_event_rank_list_str;
    logic.unicast(uid_, sc_msg_def::ret_spec_event_rank_t::cmd(),
                  m_spec_event_rank_list_str);
}

void
sc_spec_event_rank_t::get_ranking_list(int32_t &max_rank_, int32_t *ranking_list_)
{
    vector<int32_t>& rank_list = this->getList();
    max_rank_ = this->cur_size;
    int32_t rank = 0
    for (auto it=rank_list.begin(); it!=rank_list.end(); ++it, ++rank)
    {
        ranking_list_[rank + 1] = *it;
    }
}

void
sc_spec_event_rank_t::add_info(db_SpecEventUser_ext_t& db_, db_UserInfo_ext_t& user_db_);
{
	sp_card_event_rank_t sp_card_event_rank(new sc_msg_def::jpk_card_event_rank_t);
	auto it=m_card_event_rank_list.find(db_.uid);
	bool is_insert = true;
	if (it!=m_card_event_rank_list.end())
	{
		is_insert = false;
		sp_card_event_rank = it->second;
	}
	sp_card_event_rank->uid = db_.uid;
	sp_card_event_rank->nickname = user_db_.nickname();
	sp_card_event_rank->level = user_db_.grade;
	sp_card_event_rank->fp = user_db_.fp;
	sp_card_event_rank->score = db_score;
	sp_card_event_rank->vip = user_db_.viplevel;
	if (is_insert)
		m_card_event_rank_list.insert(make_pair(db_.uid, sp_card_event_rank));
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
	if (spec_event_rank.insert(db.uid, db.score))
	{
		spec_event_rank.add_info(db, m_user.db);
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
    db.goal_level = 0;
    db.round = SPEC_EVENT_ROUND_BEGIN;
    db.round_status = SPEC_EVENT_ROUND_UNOPEN; /* 默认未开启关卡且未通关*/
    db.round_max = SPEC_EVENT_ROUND_BEGIN;
    db.reset_time = 0;
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
    jpk_.round = db.round;
    jpk_.goal_level = db.goal_level;
    jpk_.round_status = db.round_status;
    jpk_.round_max = db.round_max;
    jpk_.rank = spec_event_rank.find(m_user.db.uid);
    jpk_.difficult = db.difficult;
    jpk_.open_times = db.open_times;
    jpk_.open_level = db.open_level;
    jpk_.next_count = db.next_count;
    jpk_.is_first_enter = date_helper.offday(db.first_enter_time);
    spec_event_s.getBeginEndStamp(jpk_.begin_stamp,jpk_.end_stamp);
    spec_event_s.getBeginEndDate(jpk_.begin_month, jpk_.begin_day, jpk_.begin_hour, jpk_.end_month, jpk_.end_day, jpk_.end_hour);
}

void
sc_spec_event_user_t::add_score(int32_t score_)
{
    if (score_ <= 0) {
        return;
    }
    db.score += score_;
    db.set_score(db.score);
    if (spec_event_rank.find(db.uid) == -1)
    {
	    if (spec_event_rank.insert(db.uid, db.score))
	    {
		    spec_event_rank.add_info(db, m_user.db);
	    }
    }
    else
    {
	    spec_event_rank.change(db.uid, db.score);
	    spec_event_rank.add_info(db, m_user.db);
    }
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

    int32_t code = SUCCESS;
    /* 每一阶层开启才消耗活动币 */
    if (jpk_.resid % 100 == 1 && db.round_status == SPEC_EVENT_ROUND_UNOPEN) {
        int difficult = (jpk_.resid % 1000) / 100 + 1;
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
    else
    {
	return code;
    }
    string view_data;

    db.set_round_status(SPEC_EVENT_ROUND_OPEN); /* 打开关卡并设置未为通关 */
    db.set_round(jpk_.resid);
    save_db();
    return SUCCESS;
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
