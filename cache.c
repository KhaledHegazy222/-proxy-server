#include "cache.h"
#include <string.h>

static struct cache_entry allocated_cache[CACHE_ENTRIES_SIZE];
static volatile int current_cache_size;
static volatile int lru_last_use;
sem_t table_mutex;
sem_t current_cache_size_mutex;
sem_t lru_last_use_mutex;

void init_identifier(struct uri_identifier *ptr, char *_method, char *_path, char *_http_version)
{
  strcpy(ptr->method, _method);
  strcpy(ptr->path, _path);
  strcpy(ptr->http_version, _http_version);
}

bool uri_match(struct uri_identifier *first, struct uri_identifier *second)
{
  return (!strcmp(first->method, second->method)) && (!strcmp(first->path, second->path)) && (!strcmp(first->http_version, second->http_version));
}

void init_cache()
{
  current_cache_size = 0;
  lru_last_use = 0;
  Sem_init(&table_mutex, 0, 1);
  Sem_init(&current_cache_size_mutex, 0, 1);
  Sem_init(&lru_last_use_mutex, 0, 1);
  for (int i = 0; i < CACHE_ENTRIES_SIZE; i++)
  {
    allocated_cache[i].valid = 0;
  }
}

struct cache_entry *find_match(struct uri_identifier *identifier)
{
  for (int i = 0; i < CACHE_ENTRIES_SIZE; i++)
  {
    if (allocated_cache[i].valid && uri_match(&allocated_cache[i].id, identifier))
      return allocated_cache + i;
  }
  return NULL;
}

void evict()
{
  printf("Evicting\n");
  int min_lru_time = -1;
  int min_index = 0;
  for (int i = 0; i < CACHE_ENTRIES_SIZE; i++)
  {
    if (!allocated_cache[i].valid)
      continue;
    if (min_lru_time == -1)
    {
      min_index = i;
      min_lru_time = allocated_cache[i].lru_time;
    }
    else if (min_lru_time > allocated_cache[i].lru_time)
    {
      min_index = i;
      min_lru_time = allocated_cache[i].lru_time;
    }
  }
  P(&table_mutex);
  allocated_cache[min_index].valid = 0;
  Free(allocated_cache[min_index].buffer);
  V(&table_mutex);
  P(&current_cache_size_mutex);
  current_cache_size -= allocated_cache[min_index].buffer_size;
  V(&current_cache_size_mutex);
}

bool canAccommodate(int added_size)
{
  return current_cache_size <= MAX_OBJECT_SIZE;
}

bool canFit(int added_size)
{
  return current_cache_size + added_size <= MAX_CACHE_SIZE;
}

void add(struct uri_identifier *id, void *buffer, int buffer_size)
{
  for (int i = 0; i < CACHE_ENTRIES_SIZE; i++)
  {
    if (!allocated_cache[i].valid)
    {

      P(&lru_last_use_mutex); // Lock lru
      ++lru_last_use;
      V(&lru_last_use_mutex); // Unlock lru
      P(&table_mutex);        // Lock cache data
      allocated_cache[i]
          .valid = 1;
      init_identifier(&allocated_cache[i].id, id->method, id->path, id->http_version);
      allocated_cache[i].buffer = Malloc(buffer_size);
      allocated_cache[i].buffer_size = buffer_size;
      memcpy(allocated_cache[i].buffer, buffer, buffer_size);
      allocated_cache[i].lru_time = lru_last_use;
      V(&table_mutex);              // Unlock cache data
      P(&current_cache_size_mutex); // Lock current cache size
      current_cache_size += buffer_size;
      V(&current_cache_size_mutex); // Unlock current cache size
      return;
    }
  }
}

void use_cache(struct cache_entry *cache)
{
  P(&lru_last_use_mutex);
  ++lru_last_use;
  V(&lru_last_use_mutex);
  P(&table_mutex);
  cache->lru_time = lru_last_use;
  V(&table_mutex);
}