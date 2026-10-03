#ifndef HEADER_DockerMgr
#define HEADER_DockerMgr
/*
htop - DockerMgr.h
Retrieves running Docker container names via the Docker API over the unix socket.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

char* DockerMgr_getContainerName(const char* id);

#endif /* HEADER_DockerMgr */
