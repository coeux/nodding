#ifndef _sc_spec_event_h_
#define _sc_spec_event_h_

#include "singleton.h"
#include "repo_def.h"
#include "db_ext.h"
#include "code_def.h"
#include "msg_def.h"

#include <boost/shared_ptr.hpp>
using namespace std;

struct spec_event_wave
{
	int32_t round;
	int32_t wave;
	string view_data;
}

struct spec_event_round
{
	unordered_map<int32_t, spec_event_wave> wave_map;
	int32_t round;
}

class sc_user_t;
class sc_spec_event_t
{
    typedef unordered_map<int32_t,spec_event_round> spec_event_round_hm_t;
public:
    /* event */
    sc_spec_event_t();
    vector<int32_t>  m_hosts;
    void load_db(vector<int32_t>& hostnums_);
    void getBeginEndDate(string& beginMonth, string& beginDay, string& beginHour, string& endMonth, string& endDay, string& endHour);
    bool isopen() { return m_open; }
    bool isclose() { return m_close; }
    int32_t cur_season();
    int32_t latest_season();
    int32_t get_event_id() { return m_eid; }
    uint32_t spec_event_id() { return m_eid; }
    void update();
    void spec_event_begin();
    void spec_event_end();
    void get_round_data(int32_t resid_, string &view_data_, int32_t *pids, sc_user_t& user_);
    void unlock_new_host();
    void reset_user_info();
    void add_user_log();
    void getBeginResetStamp(uint32_t& start_stamp, uint32_t& reset_stamp);
    void getBeginEndStamp(uint32_t& start_stamp, uint32_t& end_stamp);
private:
    uint32_t str2stamp(string str);
    void gen_all_spec_event_round();
	void gen_spec_event_wave(int32_t round_, int32_t wave_, spec_event_wave& wave_data_);
private:
    uint32_t              m_start_stamp;
    uint32_t              m_end_stamp;
    uint32_t              m_reset_stamp;
    int32_t               m_eid;
	int32_t				  m_state;
    bool                  m_has_open;
    bool                  m_has_close;
    spec_event_round_hm_t m_spec_event_round;
    unordered_map<int32_t, int32_t> m_spec_event_round_fp;
    string                m_begin_month;
    string                m_begin_day;
    string                m_begin_hour;
    string                m_end_month;
    string                m_end_day;
    string                m_end_hour;
};
#define spec_event_s (singleton_t<sc_spec_event_t>::instance())

typedef boost::shared_ptr<sc_msg_def::jpk_spec_event_rank_t> sp_spec_event_rank_t;
class sc_spec_event_rank_t : public sc_rank_list_t
{
public:
    /* rank */
    sc_spec_event_rank_t();
    /* reward */
    void update();
    void inter_update();
    void unicast_spec_event_rank(int32_t uid_);
    int32_t get_rank(int32_t uid_);
    void get_ranking_list(int32_t &max_rank, int32_t *ranking_list_);
    void set_hosts(const vector<int32_t>& hostnums_) { m_hosts = hostnums_; }
private:
    uint32_t m_timestamp;
    string m_spec_event_rank_list_str;
    list<sp_spec_event_rank_t> m_spec_event_rank_list;
    vector<int32_t>  m_hosts;
};
#define spec_event_rank (singleton_t<sc_spec_event_rank_t>::instance())

typedef db_SpecEventUser_ext_t spec_event_user_t;
typedef db_SpecEventUserPartner_ext_t spec_event_user_partner_t;
typedef boost::shared_ptr<spec_event_user_t> sp_spec_event_user_t;
typedef boost::shared_ptr<spec_event_user_partner_t> 
        sp_spec_event_user_partner_t;
class sc_spec_event_user_t
{
public:
    sc_spec_event_user_t(sc_user_t& user_);
    void load_db();
    void save_db();
    void init_new_user();
    void reset_user(int season_id);
    void get_user_info(sc_msg_def::jpk_spec_event_t &jpk_);
    int32_t open(sc_msg_def::req_spec_event_open_round_t &jpk_,
                 sc_msg_def::ret_spec_event_open_round_t &ret_);
    int32_t fight(sc_msg_def::req_spec_event_fight_t &jpk_,
                  sc_msg_def::ret_spec_event_fight_t &ret_);
    int32_t fight_end(sc_msg_def::req_spec_event_round_exit_t &jpk_,
                     sc_msg_def::ret_spec_event_round_exit_t &ret_);
    int32_t reset();
    int32_t revive(sc_msg_def::req_spec_event_revive_t &jpk_,
                   sc_msg_def::ret_spec_event_revive_t &ret_);
    void add_coin();
    int on_coin_change(int32_t coin_);
    void    get_enemy_team(sc_msg_def::jpk_spec_event_team_t &jpk_);
    int32_t get_enemy_info(sc_msg_def::ret_spec_event_round_enemy_t &ret_);
    void spec_event_sweep();
    int32_t next_round_end(sc_msg_def::ret_next_spec_event_t &ret_);
    int32_t set_first_enter();
private:
    int32_t consume_coin(int32_t coin_);
    void add_score(int32_t score_);
    void get_starnum(int32_t uid, int32_t pid, int32_t &rank);
    //void get_equip_level(int32_t uid, int32_t pid, int32_t &equiplv);
    int32_t point_reward();
    void difficult_reward();
    void open_reward();
    bool get_reward_difficult(int difficult);
    void set_reward_difficult(int difficult);
private:
    db_SpecEventUser_ext_t db;
    sc_user_t&             m_user;
    //int32_t m_round; /* 当前关卡，默认1 */
    //int32_t m_round_status; /* 当前关卡状态 */
    //int32_t m_round_max; /* 历史通过的最高关卡 */
    //int32_t m_reset_time; /* 重置次数 */
    //int32_t m_anger;
    //vector<int32_t> m_team;
    //string m_enemy_view_data;
};

#endif
