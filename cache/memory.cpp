#include "memory.h"

Memory::Memory()
{
	//data = new char[400000000];
}

Memory::~Memory()
{
	//delete[] data;
}

void Memory::SetMemory(char *content, int bytes)
{
	//memcpy(data, content, bytes);
}

void Memory::HandleRequest(uint64_t addr, int bytes, int read,
                          char *content, int &hit, int &time)
{
	if(read)
	{
		for(int i = 0; i < bytes; ++i)
			content[i] = 0; //data[addr + i];
	}
	//else
	//{
	//	for(int i = 0; i < bytes; ++i)
	//		data[addr + i] = content[i];
	//}

	hit = 1;
	time = latency_.hit_latency + latency_.bus_latency;
	stats_.access_time += time;
	stats_.access_counter++;
}


