#include "stdio.h"
#include "cache.h"
#include "memory.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>


int main(int argc, char** argv)
{
	srand(time(NULL));

	int cache_size = 15, block_size = 6, associativity = 3;
	int write_through = 0, write_allocate = 1;
	int replace_method = 1;
	int use_l2 = 0;
	int prefetch = 0;
	char* filename;
	for(int i = 0; i < argc; ++i)
	{
		if(!strcmp(argv[i], "-c"))
			cache_size = atoi(argv[i + 1]);
		else if(!strcmp(argv[i], "-b"))
			block_size = atoi(argv[i + 1]);
		else if(!strcmp(argv[i], "-a"))
			associativity = atoi(argv[i + 1]);
		else if(!strcmp(argv[i], "-t"))
			filename = argv[i + 1];
		else if(!strcmp(argv[i], "-through"))
			write_through = 1;
		else if(!strcmp(argv[i], "-no-allocate"))
			write_allocate = 0;
		else if(!strcmp(argv[i], "-random"))
			replace_method = 0;
		else if(!strcmp(argv[i], "-l1"))
			use_l2 = 0;
		else if(!strcmp(argv[i], "-l2"))
			use_l2 = 1;
		else if(!strcmp(argv[i], "-prefetch"))
			prefetch = 1;
		else if(!strcmp(argv[i], "-h"))
		{
			printf(" -c [num]         log cache size\n");
			printf(" -b [num]         log block size\n");
			printf(" -a [num]         log associativity\n");
			printf(" -t [filepath]	  trace file path\n");
			printf(" -through         use write through(default write back)\n");
			printf(" -no-allocate     not use write allocate(default write allocate)\n");
			printf(" -random          use random replace algorithom(default LRU)\n");
			printf(" -prefetch        use prefetch(default not)\n");
			printf(" -l1              only use l1 cache\n");
			printf(" -l2              use l1 l2 cache\n");
			printf(" -h               help\n");
			return 0;
		}
	}

	if(!filename)
	{
		printf(" you should input one trace file\n");
		exit(1);
	}

	std::ifstream trace_fp;
	trace_fp.open(filename);
	if(!trace_fp)
	{
		printf(" can't open file %s\n", filename);
		exit(1);
	}

	Memory m;
	Cache l1;
	Cache l2;
	l1.SetLower(&m);

	StorageStats s;
	m.SetStats(s);
	l1.SetStats(s);

	StorageLatency ml;
	ml.bus_latency = 0;
	ml.hit_latency = 100;
	m.SetLatency(ml);

	StorageLatency ll;
	ll.bus_latency = 0;
	ll.hit_latency = 1;
	l1.SetLatency(ll);

	CacheConfig_ l1_cc;
	l1_cc.size = cache_size;
	l1_cc.associativity = associativity;
	l1_cc.set_num = cache_size - associativity - block_size;
	l1_cc.write_through = write_through;
	l1_cc.write_allocate = write_allocate;
	l1_cc.replace_method = replace_method;
	l1_cc.prefetch = prefetch;

	l1.SetConfig(l1_cc);

	if(use_l2)
	{
		l1.SetLower(&l2);
		l2.SetLower(&m);
		l2.SetStats(s);

		StorageLatency l2l;
		l2l.bus_latency = 0;
		l2l.hit_latency = 2;
		l2.SetLatency(l2l);

		CacheConfig_ l2_cc;
		l2_cc.size = 18;
		l2_cc.associativity = 3;
		l2_cc.set_num = 18 - 3 - 6;
		l2_cc.write_through = 0;
		l2_cc.write_allocate = 1;
		l2_cc.replace_method = replace_method;
		l2_cc.prefetch = prefetch;

		l2.SetConfig(l2_cc);
	}

	int hit, time;
	char content[64];
	for(int i = 0; i < 64; ++i) content[i] = 'c';

	std::string op;
	uint64_t addr;
	int cnt = 0;
	//std::cout << l1.S << " " << l1.E << " " << l1.B << std::endl; 
	while(!trace_fp.eof())
	{
		if(use_l2)
			trace_fp >> op >> std::hex >> addr;
		else trace_fp >> op >> addr;

		//std::cout << cnt << " " << op << " " << addr << std::endl;
		if(op == "w")
			l1.HandleRequest(addr, 1, 0, content, hit, time);
		else
			l1.HandleRequest(addr, 1, 1, content, hit, time);
		cnt++;
	}

	trace_fp.close();

	StorageStats_ ls, ms;
	l1.GetStats(ls);
	m.GetStats(ms);
	printf(" l1 cache total access count: %d, total access time: %d\n", ls.access_counter, ls.access_time);
	printf("		  hit count: %d, miss count: %d\n", ls.access_counter - ls.miss_num, ls.miss_num);
	printf("          replace count: %d, fetch lower layer count: %d\n", ls.replace_num, ls.fetch_num);
	printf("          prefetch count: %d\n", ls.prefetch_num);
	printf(" l1 cache miss rate : %lf\n", (double)ls.miss_num / (double)ls.access_counter);
	printf(" l1 cache average access time : %lf\n", (double)ls.access_time / (double)ls.access_counter);

	if(use_l2)
	{
		l2.GetStats(ls);
		printf("\n");
		printf(" l2 cache total access count: %d, total access time: %d\n", ls.access_counter, ls.access_time);
		printf("		  hit count: %d, miss count: %d\n", ls.access_counter - ls.miss_num, ls.miss_num);
		printf("          replace count: %d, fetch lower layer count: %d\n", ls.replace_num, ls.fetch_num);
		printf("          prefetch count: %d\n", ls.prefetch_num);
		printf(" l2 cache miss rate : %lf\n", (double)ls.miss_num / (double)ls.access_counter);
		printf(" l2 cache average access time : %lf\n", (double)ls.access_time / (double)ls.access_counter);
		printf("\n");
	}

	printf(" memory total access count: %d\n", ms.access_counter);
	return 0;
}

