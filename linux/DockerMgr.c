/*
htop - DockerMgr.c
Retrieves running Docker container names via the Docker API over the unix socket.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/DockerMgr.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "Hashtable.h"
#include "XUtils.h"

#define DOCKER_SOCKET "/var/run/docker.sock"
#define RESPONSE_SIZE 262144
#define MAX_CONTAINER_NAME_LEN 25

/* ponytail: simplest possible cache; refresh by dropping cache on long IDs is out of scope */
static Hashtable* nameCache = NULL;

static unsigned int hashStr(const char* s) {
   unsigned int h = 5381;
   while (*s)
      h = (h * 33) ^ (unsigned char)*s++;
   return h;
}

static Hashtable* getCache(void) {
   if (!nameCache)
      nameCache = Hashtable_new(16, true);
   return nameCache;
}

static char* queryName(const char* id) {
   int fd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (fd < 0)
      return NULL;

   struct sockaddr_un addr;
   memset(&addr, 0, sizeof(addr));
   addr.sun_family = AF_UNIX;
   snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", DOCKER_SOCKET);

   if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      close(fd);
      return NULL;
   }

   char req[256];
   int reqLen = snprintf(req, sizeof(req),
      "GET /containers/%s/json HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Accept: application/json\r\n"
      "Connection: close\r\n\r\n", id);
   if (reqLen <= 0 || reqLen >= (int)sizeof(req) || write(fd, req, (size_t)reqLen) != reqLen) {
      close(fd);
      return NULL;
   }

   char buf[RESPONSE_SIZE];
   size_t used = 0;
   ssize_t rd;
   while (used < sizeof(buf) - 1 && (rd = read(fd, buf + used, sizeof(buf) - 1 - used)) > 0)
      used += (size_t)rd;
   close(fd);

   if (used == 0)
      return NULL;
   buf[used] = '\0';

   char* key = strstr(buf, "\"Name\":\"");
   if (!key)
      return NULL;
   char* val = key + strlen("\"Name\":\"");
   if (*val == '/')
      val++;
   char* end = strchr(val, '"');
   if (!end)
      return NULL;
   return xStrndup(val, (size_t)(end - val));
}

char* DockerMgr_getContainerName(const char* id) {
   Hashtable* cache = getCache();
   ht_key_t key = hashStr(id);

   char* cached = Hashtable_get(cache, key);
   if (cached)
      return xStrdup(cached);

   char* name = queryName(id);
   if (name) {
      /* Limit the container name shown in the CONTAINER column to 25 characters */
      char* shortName = xStrndup(name, MAX_CONTAINER_NAME_LEN);
      free(name);
      Hashtable_put(cache, key, shortName);
      return xStrdup(shortName);
   }
   return NULL;
}
