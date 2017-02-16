#ifndef _sc_rank_list_h_
#define _sc_rank_list_h_

using namespace std;
struct data_pair
{
	int32_t id;
	int32_t value;
}

class sc_rank_list_t
{
public:
    sc_rank_list_t(int32_t max_size_);
    bool insert(int32_t id_, int32_t value_);
    int32_t find(int32_t id_);
    const vector<int32_t>& getList();
    const vector<int32_t>& getList(int32_t count_);
    int32_t max_size;
    int32_t cur_size;
    int32_t max_value;
    int32_t min_value;
    vector<data_pair> list;
};

#endif
