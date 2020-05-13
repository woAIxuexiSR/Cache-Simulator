#ifndef CACHE_CACHE_H_
#define CACHE_CACHE_H_

#include <stdint.h>
#include <stdlib.h>
#include "storage.h"

typedef struct CacheConfig_ {
  int size;
  int associativity;
  int set_num; // Number of cache sets
  int write_through; // 0|1 for back|through
  int write_allocate; // 0|1 for no-alc|alc
  int replace_method; // 0|1 for random|LRU
  int prefetch;		// 0|1 for no-prefetch|prefetch
} CacheConfig;

typedef struct Row_
{
	uint64_t LRU_cnt;
	bool modify;
	bool valid;
	uint64_t tag;
	char* data;
} Row;

class Cache: public Storage {
 private:
  Row* row;

  uint64_t get_tag(uint64_t addr);
  uint64_t get_set(uint64_t addr);
  uint64_t get_block(uint64_t addr);
  void LRU_update(uint64_t set, uint64_t line);

 public:
  uint64_t S, E, B;
  int logs, logb;
  
  Cache(): S(0), E(0), B(0) {}
  ~Cache();

  // Sets & Gets
  void SetConfig(CacheConfig cc);
  void GetConfig(CacheConfig &cc) { cc = config_; }
  void SetLower(Storage *ll) { lower_ = ll; }
  // Main access process
  void HandleRequest(uint64_t addr, int bytes, int read,
                     char *content, int &hit, int &time);

 private:
  // Bypassing
  int BypassDecision();
  // Partitioning
  void PartitionAlgorithm();
  // Replacement
  uint64_t ReplaceDecision(uint64_t set, uint64_t tag);
  uint64_t ReplaceAlgorithm(uint64_t set);
  // Prefetching
  int PrefetchDecision();
  void PrefetchAlgorithm(uint64_t set, uint64_t tag, int& time);

  CacheConfig config_;
  Storage *lower_;
  DISALLOW_COPY_AND_ASSIGN(Cache);
  
};

#endif //CACHE_CACHE_H_ 
