#include "sc_rank_list.h"

sc_rank_list::sc_rank_list(int32_t max_size_):
max_size(max_size_),
cur_size(0)
{
	list.clear();
}

bool
sc_rank_list::insert(int32_t id_, int32_t value_)
{
	if (cur_size == 0)
	{
		data_pair pair;
		pair.id = id_;
		pair.value = value_;
		list.push_back(std::move(pair));
		cur_size++;
		max_value = value_;
		min_value = value_;
		return true;
	}

	if (value_ <= min_value)
	{
		if (cur_size >= max_size)
		{
			return false;
		}
		else
		{
			data_pair pair;
			pair.id = id_;
			pair.value = value_;
			list.push_back(std::move(pair));
			cur_size++;
			min_value = value_;
			return true;			l
		}
	}

	if (value_ >= max_value)
	{
		data_pair pair;
		pair.id = -1;
		pair.value = -1;
		list.push_back(std::move(pair));
		for(auto j=cur_size;j>0; --j)
		{
			list[j].value = list[j-1].value;
			list[j].id = list[j-1].id;
		}
		list[0].id = id_;
		list[0].value = value_;
		max_value = value_;
		cur_size++;
		if (cur_size > max_size)
		{
			list.pop_back();
			cur_size--;
		}
		return true;			l
	}

	for(auto i=0; i<cur_size; ++i)
	{
		data_pair pair;
		pair.id = -1;
		pair.value = -1;
		list.push_back(std::move(pair));
		if(value_ > list[i].value)
		{
			for(auto j=cur_size; j>i; --j)
			{
				list[j].value = list[j-1].value;
				list[j].id = list[j-1].id;
			}
			list[i].id = id_;
			list[i].value = value_;
			break;
		}
	}
	if(cur_size > max_size)
	{
		list.pop_back();
		cur_size--;
	}
	return true;
}

int32_t
sc_rank_list::find(int32_t id_)
{
	bool rank = -1;
	for(auto i=0; i<cur_size; ++i)
	{
		if (list[i].id == id_)
		{
			rank = i+1;
			break;
		}
	}
	return rank;
}

const vector<int32_t>&
sc_rank_list::getList()
{
	vector<int32_t> print_list;
	for(auto i=0; i<cur_size; ++i)
	{
		print_list.push_back(list[i].id);
	}
	return print_list;
}

const vector<int32_t>&
sc_rank_list::getList(int count_)
{
	int32_t size = count_< cur_size ? count_ : cur_size;
	vector<int32_t> print_list;
	for(auto i=0; i<size; ++i)
	{
		print_list.push_back(list[i].id);
	}
	return print_list;
}
