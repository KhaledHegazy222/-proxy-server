#pragma once
#include "csapp.h"
#include <stdbool.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 1049000
#define CACHE_ENTRIES_SIZE 1000
struct uri_identifier
{
  char method[10];
  char path[100];
  char http_version[20];
};

void init_identifier(struct uri_identifier *ptr, char *_method, char *_path, char *_http_version);

bool uri_match(struct uri_identifier *first, struct uri_identifier *second);

struct cache_entry
{
  struct uri_identifier id;
  int lru_time;
  int valid;
  void *buffer;
  int buffer_size;
};

void init_cache();

struct cache_entry *find_match(struct uri_identifier *identifier);

void evict();
bool canAccommodate(int added_size);
bool canFit(int added_size);
void add(struct uri_identifier *id, void *buffer, int buffer_size);
void use_cache(struct cache_entry *cache);