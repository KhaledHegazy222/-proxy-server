#include <stdio.h>
#include <pthread.h>
#include "client.h"
#include "cache.h"

#define MAX_BUFFER_SIZE 1024
int pass_response(rio_t *rp, int client_fd, char *cached_bf);
int pass_requesthdrs(rio_t *rp, int server_fd);

void handle_client(int client_fd)
{

  char buffer[MAX_BUFFER_SIZE];
  int n;
  rio_t rio_client, rio_server;
  rio_readinitb(&rio_client, client_fd);

  if ((n = Rio_readlineb(&rio_client, buffer, MAX_BUFFER_SIZE)) > 0)
  {
    char method[100], uri[100], protocol[100];
    sscanf(buffer, "%s %s %s", method, uri, protocol);
    strcpy(protocol, "HTTP/1.0");
    char host[100], path[100];
    int portNumber;
    if (strstr(uri + 7, ":"))
    {
      int i;
      char *ptr;
      for (ptr = uri + 7, i = 0; *ptr != ':'; ptr++, i++)
        host[i] = *ptr;
      host[i] = '\0';
      sscanf(ptr + 1, "%d", &portNumber);
    }
    else
    {

      int i;
      char *ptr;
      for (ptr = uri + 7, i = 0; (*ptr) && (*ptr != '/'); ptr++, i++)
        host[i] = *ptr;
      host[i] = '\0';
      portNumber = 80;
    }
    char portStr[10];
    sprintf(portStr, "%d", portNumber);
    char *firstOccurrence = strstr(uri + 7, "/");
    if (firstOccurrence)
    {
      strcpy(path, strstr(uri + 7, "/"));
    }
    else
    {
      strcpy(path, "/");
    }
    struct uri_identifier uri_id;
    init_identifier(&uri_id, method, uri, "HTTP/1.0");
    printf("Requesting : %s\n", uri_id.path);
    struct cache_entry *cache = find_match(&uri_id);
    if (cache)
    {
      printf("Hit\n");
      Rio_writen(client_fd, cache->buffer, cache->buffer_size);
      use_cache(cache);
    }
    else
    {
      printf("Miss\n");
      int server_fd = open_clientfd(host, portStr);
      rio_readinitb(&rio_server, server_fd);

      char modified_http_request[200];
      sprintf(modified_http_request, "GET %s HTTP/1.0\r\n", path);

      rio_writen(server_fd, modified_http_request, strlen(modified_http_request));

      if (pass_requesthdrs(&rio_client, server_fd) == 0)
      {
        return;
      }
      char cached_bf[MAX_OBJECT_SIZE];

      int total_size = pass_response(&rio_server, client_fd, cached_bf);
      if (total_size)
      {
        while (!canFit(total_size))
          evict();
        add(&uri_id, cached_bf, total_size);
        rio_writen(client_fd, cached_bf, total_size);
      }
    }
  }
  close(client_fd);
}

int pass_requesthdrs(rio_t *rp, int server_fd)
{
  char buf[MAXLINE];

  int buffer_len = Rio_readlineb(rp, buf, MAXLINE);
  if (!buffer_len)
    return 0;

  Rio_writen(server_fd, buf, buffer_len);
  while (strcmp(buf, "\r\n"))
  {
    buffer_len = Rio_readlineb(rp, buf, MAXLINE);
    Rio_writen(server_fd, buf, buffer_len);
  }
  return 1;
}

int pass_response(rio_t *rp, int client_fd, char *cached_bf)
{
  int total_size = 0;
  char buffer[MAXLINE];
  int buffer_len = Rio_readlineb(rp, buffer, MAXLINE);

  if (!buffer_len)
    return 0;
  if (canAccommodate(total_size + buffer_len))
  {
    memcpy(cached_bf + total_size, buffer, buffer_len);
  }
  total_size += buffer_len;

  int content_length = 0;
  while (strcmp(buffer, "\r\n"))
  {
    if ((buffer_len = Rio_readlineb(rp, buffer, MAXLINE)) > 0)
    {

      if (strstr(buffer, "Content-Length:") || strstr(buffer, "Content-length:") || strstr(buffer, "content-length:"))
      {
        sscanf(buffer + 15, "%d", &content_length);
      }

      if (canAccommodate(total_size + buffer_len))
      {
        memcpy(cached_bf + total_size, buffer, buffer_len);
      }
      total_size += buffer_len;
    }
    else
    {
      break;
    }
  }
  char responseBody[content_length];
  Rio_readnb(rp, responseBody, content_length);
  if (canAccommodate(total_size + content_length))
  {
    memcpy(cached_bf + total_size, responseBody, content_length);
  }
  total_size += content_length;
  if (canAccommodate(total_size))
    return total_size;
  return 0;
}