#include "cache.h"

Cache::~Cache()
{
	for(uint64_t i = 0; i < S; ++i)
	{
		for(uint64_t j = 0; j < E; ++j)
		{
			uint64_t rownum = i * E + j;
			if(row[rownum].data) delete[] row[rownum].data;
		}
	}
	if(row) delete[] row;
}

void Cache::SetConfig(CacheConfig cc)
{
	config_ = cc;
	
	logs = cc.set_num;
	S = (uint64_t)1 << logs;
	E = (uint64_t)1 << cc.associativity;
	logb = cc.size - logs - cc.associativity;
	B = (uint64_t)1 << logb;

	row = new Row[S * E];
	for(uint64_t i = 0; i < S; ++i)
	{
		for(uint64_t j = 0; j < E; ++j)
		{
			uint64_t rownum = i * E + j;
			row[rownum].LRU_cnt = 0;
			row[rownum].modify = 0;
			row[rownum].valid = 0;
			row[rownum].tag = 0;
			row[rownum].data = new char[B];
		}
	}
}

uint64_t Cache::get_tag(uint64_t addr)
{
	int tag_bits = 64 - logs - logb;
	uint64_t tag_mask = ((uint64_t)1 << tag_bits) - 1;

	return (addr >> (logs + logb)) & tag_mask;
}

uint64_t Cache::get_set(uint64_t addr)
{
	uint64_t set_mask = ((uint64_t)1 << logs) - 1;
	return (addr >> logb) & set_mask;
}

uint64_t Cache::get_block(uint64_t addr)
{
	uint64_t block_mask = ((uint64_t)1 << logb) - 1;
	return addr & block_mask;
}

void Cache::LRU_update(uint64_t set, uint64_t line)
{
	for(uint64_t i = 0; i < E; ++i)
		if(i == line) row[set * E + i].LRU_cnt = 0;
		else row[set * E + i].LRU_cnt++;
}

void Cache::HandleRequest(uint64_t addr, int bytes, int read,
                          char *content, int &hit, int &time)
{
	stats_.access_counter++;


	hit = 0;
	time = 0;

	uint64_t tag = get_tag(addr);
	uint64_t set = get_set(addr);
	uint64_t block = get_block(addr);

	// Bypass?
	if (!BypassDecision())
	{
		//PartitionAlgorithm();
		uint64_t idx = ReplaceDecision(set, tag);
		if(!config_.write_allocate && !read && idx >= E)
		{
			stats_.miss_num++;
			lower_->HandleRequest(addr, bytes, 0, content, hit, time);
			hit = 0;
			time += latency_.bus_latency + latency_.hit_latency;
			stats_.access_time += time;
			return;
		}

		if(idx == E)
		{
			stats_.miss_num++;
			stats_.fetch_num++;
			stats_.replace_num++;
			
			uint64_t rep = ReplaceAlgorithm(set);
			int tmp_hit = 0, tmp_time = 0;
			if(!config_.write_through && row[set * E + rep].modify)
			{
				uint64_t pre_addr = (row[set * E + rep].tag << (logs + logb)) + (set << logb);
				lower_->HandleRequest(pre_addr, B, 0, row[set * E + rep].data, tmp_hit, tmp_time);
			}
			lower_->HandleRequest(addr - block, B, 1, row[set * E + rep].data, hit, time);
			hit = 0;
			time = time > tmp_time ? time : tmp_time;
			idx = rep;
		}
		else if(idx == E + 1)
		{
			stats_.miss_num++;
			stats_.fetch_num++;

			for(uint64_t i = 0; i < E; ++i)
			{
				if(row[set * E + i].valid == false)
				{
					lower_->HandleRequest(addr - block, B, 1, row[set * E + i].data, hit, time);
					hit = 0;
					idx = i;
					break;
				}
			}
		}
		else
		{
			hit = 1;
		}

		if(!hit && PrefetchDecision() && set + 1 < S)
		{
			int tmp_time;
			PrefetchAlgorithm(set + 1, tag, tmp_time);
			if(tmp_time) stats_.prefetch_num++;
			time = time > tmp_time ? time : tmp_time;
		}

		row[set * E + idx].valid = 1;
		row[set * E + idx].tag = tag;
		if(!read) row[set * E +idx].modify = 1;
		// return hit & time
		for(int i = 0; i < bytes; ++i)
		{
			if(read)
				content[i] = row[set * E + idx].data[block + i];
			else
				row[set * E + idx].data[block + i] = content[i];
		}
		
		if(config_.replace_method)
			LRU_update(set, idx);
		int tmp_hit = 0, tmp_time = 0;
		if(!read && config_.write_through)
			lower_->HandleRequest(addr, bytes, 0, content, tmp_hit, tmp_time);

		time = time > tmp_time ? time : tmp_time;
		time += latency_.bus_latency + latency_.hit_latency;
		
		stats_.access_time += time;
		return;
    }
}

int Cache::BypassDecision() {
  return 0;
}

void Cache::PartitionAlgorithm() {
}

uint64_t Cache::ReplaceDecision(uint64_t set, uint64_t tag)
{
	bool empty = false; 
	for(uint64_t i = 0; i < E; ++i)
	{
		if(!row[set * E + i].valid)
			empty = true;
		if(row[set * E + i].valid && row[set * E + i].tag == tag)
			return i;
	}
	if(!empty) return E;
	return E + 1;
}

uint64_t Cache::ReplaceAlgorithm(uint64_t set)
{
	if(!config_.replace_method)
	{
		uint64_t r1 = rand(), r2 = rand();
		uint64_t r = (r1 + (r2 << 32)) % E;
		return r;
	}
	else
	{
		uint64_t r = 0, maxx = row[set * E].LRU_cnt;
		for(uint64_t i = 1; i < E; ++i)
		{
			if(row[set * E + i].LRU_cnt > maxx)
			{
				maxx = row[set * E + i].LRU_cnt;
				r = i;
			}
		}
		return r;
	}
}

int Cache::PrefetchDecision()
{
	return config_.prefetch;
}

void Cache::PrefetchAlgorithm(uint64_t set, uint64_t tag, int& time)
{
	time = 0;

	uint64_t idx = ReplaceDecision(set, tag);
	int hit = 0;
	if(idx == E)
	{
		uint64_t rep = ReplaceAlgorithm(set);
		int tmp_hit = 0, tmp_time = 0;
		if(!config_.write_through && row[set * E + rep].modify)
		{
			uint64_t pre_addr = (row[set * E + rep].tag << (logs + logb)) + (set << logb);
			lower_->HandleRequest(pre_addr, B, 0, row[set * E + rep].data, tmp_hit, tmp_time);
		}
		lower_->HandleRequest((tag << (logb + logs)) + (set << logb), B, 1, row[set * E + rep].data, hit, time);
		time = time > tmp_time ? time : tmp_time;
		idx = rep;
	}
	else if(idx == E + 1)
	{
		for(uint64_t i = 0; i < E; ++i)
		{
			if(row[set * E + i].valid == false)
			{
				lower_->HandleRequest((tag << (logb + logs)) + (set << logb), B, 1, row[set * E + i].data, hit, time);
				idx = i;
				break;
			}
		}
	}
	else return;

	row[set * E + idx].valid = 1;
	row[set * E + idx].tag = tag;
	row[set * E + idx].modify = 0;
}

