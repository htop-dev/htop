/*
 * Copyright (c) 2007 Cisco Systems, Inc.  All rights reserved.
 *
 * Portions of this file originally contributed by Advanced Micro
 * Devices, Inc.  See notice below.
 */
/* ============================================================
 License Agreement

 Copyright (c) 2006, 2007 Advanced Micro Devices, Inc.
 All rights reserved.

 Redistribution and use in any form of this material and any product 
 thereof including software in source or binary forms, along with any 
 related documentation, with or without modification ("this material"), 
 is permitted provided that the following conditions are met:

 + Redistributions of source code of any software must retain the above
 copyright notice and all terms of this license as part of the code.

 + Redistributions in binary form of any software must reproduce the
 above copyright notice and all terms of this license in any related 
 documentation and/or other materials.

 + Neither the names nor trademarks of Advanced Micro Devices, Inc. or
 any copyright holders or contributors may be used to endorse or 
 promote products derived from this material without specific prior 
 written permission.

 + Notice about U.S. Government restricted rights: This material is
 provided with "RESTRICTED RIGHTS." Use, duplication or disclosure by 
 the U.S. Government is subject to the full extent of restrictions set 
 forth in FAR52.227 and DFARS252.227 et seq., or any successor or 
 applicable regulations. Use of this material by the U.S. Government 
 constitutes acknowledgment of the proprietary rights of Advanced Micro 
 Devices, Inc.
 and any copyright holders and contributors.

 + In no event shall anyone redistributing or accessing or using this
 material commence or participate in any arbitration or legal action 
 relating to this material against Advanced Micro Devices, Inc. or any 
 copyright holders or contributors. The foregoing shall survive any 
 expiration or termination of this license or any agreement or access 
 or use related to this material.

 + ANY BREACH OF ANY TERM OF THIS LICENSE SHALL RESULT IN THE IMMEDIATE
 REVOCATION OF ALL RIGHTS TO REDISTRIBUTE, ACCESS OR USE THIS MATERIAL.

 THIS MATERIAL IS PROVIDED BY ADVANCED MICRO DEVICES, INC. AND ANY 
 COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" IN ITS CURRENT CONDITION 
 AND WITHOUT ANY REPRESENTATIONS, GUARANTEE, OR WARRANTY OF ANY KIND OR 
 IN ANY WAY RELATED TO SUPPORT, INDEMNITY, ERROR FREE OR UNINTERRUPTED 
 OPERATION, OR THAT IT IS FREE FROM DEFECTS OR VIRUSES.  ALL 
 OBLIGATIONS ARE HEREBY DISCLAIMED - WHETHER EXPRESS, IMPLIED, OR 
 STATUTORY - INCLUDING, BUT NOT LIMITED TO, ANY IMPLIED WARRANTIES OF 
 TITLE, MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, ACCURACY, 
 COMPLETENESS, OPERABILITY, QUALITY OF SERVICE, OR NON-INFRINGEMENT. IN 
 NO EVENT SHALL ADVANCED MICRO DEVICES, INC. OR ANY COPYRIGHT HOLDERS 
 OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 SPECIAL, PUNITIVE, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
 NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF 
 USE, REVENUE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
 CAUSED OR BASED ON ANY THEORY OF LIABILITY ARISING IN ANY WAY RELATED 
 TO THIS MATERIAL, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 THE ENTIRE AND AGGREGATE LIABILITY OF ADVANCED MICRO DEVICES, INC. AND 
 ANY COPYRIGHT HOLDERS AND CONTRIBUTORS SHALL NOT EXCEED TEN DOLLARS 
 (US $10.00). ANYONE REDISTRIBUTING OR ACCESSING OR USING THIS MATERIAL 
 ACCEPTS THIS ALLOCATION OF RISK AND AGREES TO RELEASE ADVANCED MICRO 
 DEVICES, INC. AND ANY COPYRIGHT HOLDERS AND CONTRIBUTORS FROM ANY AND 
 ALL LIABILITIES, OBLIGATIONS, CLAIMS, OR DEMANDS IN EXCESS OF TEN 
 DOLLARS (US $10.00). THE FOREGOING ARE ESSENTIAL TERMS OF THIS LICENSE 
 AND, IF ANY OF THESE TERMS ARE CONSTRUED AS UNENFORCEABLE, FAIL IN 
 ESSENTIAL PURPOSE, OR BECOME VOID OR DETRIMENTAL TO ADVANCED MICRO 
 DEVICES, INC. OR ANY COPYRIGHT HOLDERS OR CONTRIBUTORS FOR ANY REASON, 
 THEN ALL RIGHTS TO REDISTRIBUTE, ACCESS OR USE THIS MATERIAL SHALL 
 TERMINATE IMMEDIATELY. MOREOVER, THE FOREGOING SHALL SURVIVE ANY 
 EXPIRATION OR TERMINATION OF THIS LICENSE OR ANY AGREEMENT OR ACCESS 
 OR USE RELATED TO THIS MATERIAL.

 NOTICE IS HEREBY PROVIDED, AND BY REDISTRIBUTING OR ACCESSING OR USING 
 THIS MATERIAL SUCH NOTICE IS ACKNOWLEDGED, THAT THIS MATERIAL MAY BE 
 SUBJECT TO RESTRICTIONS UNDER THE LAWS AND REGULATIONS OF THE UNITED 
 STATES OR OTHER COUNTRIES, WHICH INCLUDE BUT ARE NOT LIMITED TO, U.S.
 EXPORT CONTROL LAWS SUCH AS THE EXPORT ADMINISTRATION REGULATIONS AND 
 NATIONAL SECURITY CONTROLS AS DEFINED THEREUNDER, AS WELL AS STATE 
 DEPARTMENT CONTROLS UNDER THE U.S. MUNITIONS LIST. THIS MATERIAL MAY 
 NOT BE USED, RELEASED, TRANSFERRED, IMPORTED, EXPORTED AND/OR RE- 
 EXPORTED IN ANY MANNER PROHIBITED UNDER ANY APPLICABLE LAWS, INCLUDING 
 U.S. EXPORT CONTROL LAWS REGARDING SPECIFICALLY DESIGNATED PERSONS, 
 COUNTRIES AND NATIONALS OF COUNTRIES SUBJECT TO NATIONAL SECURITY 
 CONTROLS.
 MOREOVER,
 THE FOREGOING SHALL SURVIVE ANY EXPIRATION OR TERMINATION OF ANY 
 LICENSE OR AGREEMENT OR ACCESS OR USE RELATED TO THIS MATERIAL.

 This license forms the entire agreement regarding the subject matter 
 hereof and supersedes all proposals and prior discussions and writings 
 between the parties with respect thereto. This license does not affect 
 any ownership, rights, title, or interest in, or relating to, this 
 material. No terms of this license can be modified or waived, and no 
 breach of this license can be excused, unless done so in a writing 
 signed by all affected parties. Each term of this license is 
 separately enforceable. If any term of this license is determined to 
 be or becomes unenforceable or illegal, such term shall be reformed to 
 the minimum extent necessary in order for this license to remain in 
 effect in accordance with its terms as modified by such reformation.
 This license shall be governed by and construed in accordance with the 
 laws of the State of Texas without regard to rules on conflicts of law 
 of any state or jurisdiction or the United Nations Convention on the 
 International Sale of Goods. All disputes arising out of this license 
 shall be subject to the jurisdiction of the federal and state courts 
 in Austin, Texas, and all defenses are hereby waived concerning 
 personal jurisdiction and venue of these courts.
 ============================================================ */

#include "plpa_config.h"
#include "plpa.h"
#include "plpa_internal.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>

typedef struct tuple_t_ {
    int processor_id, socket, core;
} tuple_t;

static int supported = 0;
static int num_processors = -1;
static int max_processor_num = -1;
static int num_sockets = -1;
static int max_socket_id = -1;
static int *max_core_id = NULL;
static int *num_cores = NULL;
static int max_core_id_overall = -1;
static tuple_t *map_processor_id_to_tuple = NULL;
static tuple_t ***map_tuple_to_processor_id = NULL;

static void clear_cache(void)
{
    if (NULL != max_core_id) {
        free(max_core_id);
        max_core_id = NULL;
    }
    if (NULL != num_cores) {
        free(num_cores);
        num_cores = NULL;
    }
    if (NULL != map_processor_id_to_tuple) {
        free(map_processor_id_to_tuple);
        map_processor_id_to_tuple = NULL;
    }
    if (NULL != map_tuple_to_processor_id) {
        if (NULL != map_tuple_to_processor_id[0]) {
            free(map_tuple_to_processor_id[0]);
            map_tuple_to_processor_id = NULL;
        }
        free(map_tuple_to_processor_id);
        map_tuple_to_processor_id = NULL;
    }

    num_processors = max_processor_num = -1;
    num_sockets = max_socket_id = -1;
    max_core_id_overall = -1;
}

static void load_cache(const char *sysfs_mount)
{
    int i, j, k, invalid_entry, fd;
    char path[PATH_MAX], buf[8];
    PLPA_NAME(cpu_set_t) *cores_on_sockets;
    int found;

    /* Check for the parent directory */
    sprintf(path, "%s/devices/system/cpu", sysfs_mount);
    if (access(path, R_OK|X_OK)) {
        return;
    }

    /* Go through and find the max processor ID */
    for (num_processors = max_processor_num = i = 0; 
         i < PLPA_BITMASK_CPU_MAX; ++i) {
        sprintf(path, "%s/devices/system/cpu/cpu%d", sysfs_mount, i);
        if (0 != access(path, (R_OK | X_OK))) {
            max_processor_num = i - 1;
            break;
        }
        ++num_processors;
    }

    /* If we found no processors, then we have no topology info */
    if (0 == num_processors) {
        clear_cache();
        return;
    }

    /* Malloc space for the first map (processor ID -> tuple).
       Include enough space for one invalid entry. */
    map_processor_id_to_tuple = malloc(sizeof(tuple_t) * 
                                       (max_processor_num + 2));
    if (NULL == map_processor_id_to_tuple) {
        return;
    }
    for (i = 0; i <= max_processor_num; ++i) {
        map_processor_id_to_tuple[i].processor_id = i;
        map_processor_id_to_tuple[i].socket = -1;
        map_processor_id_to_tuple[i].core = -1;
    }
    /* Set the invalid entry */
    invalid_entry = i;
    map_processor_id_to_tuple[invalid_entry].processor_id = -1;
    map_processor_id_to_tuple[invalid_entry].socket = -1;
    map_processor_id_to_tuple[invalid_entry].core = -1;

    /* Build a cached map of (socket,core) tuples */
    for (found = 0, i = 0; i <= max_processor_num; ++i) {
        sprintf(path, "%s/devices/system/cpu/cpu%d/topology/core_id", 
                sysfs_mount, i);
        fd = open(path, O_RDONLY);
        if ( fd < 0 ) {
            continue;
        }
        if ( read(fd, buf, 7) <= 0 ) {
            continue;
        }
        sscanf(buf, "%d", &(map_processor_id_to_tuple[i].core));
        close(fd);
        
        sprintf(path,
                "%s/devices/system/cpu/cpu%d/topology/physical_package_id",
                sysfs_mount, i);
        fd = open(path, O_RDONLY);
        if ( fd < 0 ) {
            continue;
        }
        if ( read(fd, buf, 7) <= 0 ) {
            continue;
        }
        sscanf(buf, "%d", &(map_processor_id_to_tuple[i].socket));
        close(fd);
        found = 1;
        
        /* Keep a running tab on the max socket number */
        if (map_processor_id_to_tuple[i].socket > max_socket_id) {
            max_socket_id = map_processor_id_to_tuple[i].socket;
        }
    }

    /* Now that we know the max number of sockets, allocate some
       arrays */
    max_core_id = malloc(sizeof(int) * (max_socket_id + 1));
    if (NULL == max_core_id) {
        clear_cache();
        return;
    }
    num_cores = malloc(sizeof(int) * (max_socket_id + 1));
    if (NULL == num_cores) {
        clear_cache();
        return;
    }
    for (i = 0; i <= max_socket_id; ++i) {
        num_cores[i] = -1;
        max_core_id[i] = -1;
    }

    /* Find the max core number on each socket */
    for (i = 0; i <= max_processor_num; ++i) {
        if (map_processor_id_to_tuple[i].core > 
            max_core_id[map_processor_id_to_tuple[i].socket]) {
            max_core_id[map_processor_id_to_tuple[i].socket] = 
                map_processor_id_to_tuple[i].core;
        }
        if (max_core_id[map_processor_id_to_tuple[i].socket] > 
            max_core_id_overall) {
            max_core_id_overall = 
                max_core_id[map_processor_id_to_tuple[i].socket];
        }
    }

    /* If we didn't find any core_id/physical_package_id's, then we
       don't have the topology info */
    if (!found) {
        clear_cache();
        return;
    }

    /* Go through and count the number of unique sockets found.  It
       may not be the same as max_socket_id because there may be
       "holes" -- e.g., sockets 0 and 3 are used, but sockets 1 and 2
       are empty. */
    for (j = i = 0; i <= max_socket_id; ++i) {
        if (max_core_id[i] >= 0) {
            ++j;
        }
    }
    if (j > 0) {
        num_sockets = j;
    }

    /* Count how many cores are available on each socket.  This may
       not be the same as max_core_id[socket_num] if there are
       "holes".  I don't know if holes can happen (i.e., if specific
       cores can be taken offline), but what the heck... */
    cores_on_sockets = malloc(sizeof(PLPA_NAME(cpu_set_t)) * 
                              (max_socket_id + 1));
    if (NULL == cores_on_sockets) {
        clear_cache();
        return;
    }
    for (i = 0; i <= max_socket_id; ++i) {
        PLPA_CPU_ZERO(&(cores_on_sockets[i]));
    }
    for (i = 0; i <= max_processor_num; ++i) {
        if (map_processor_id_to_tuple[i].socket >= 0) {
            PLPA_CPU_SET(map_processor_id_to_tuple[i].core,
                         &(cores_on_sockets[map_processor_id_to_tuple[i].socket]));
        }
    }
    for (i = 0; i <= max_socket_id; ++i) {
        int count = 0;
        for (j = 0; j < PLPA_BITMASK_CPU_MAX; ++j) {
            if (PLPA_CPU_ISSET(j, &(cores_on_sockets[i]))) {
                ++count;
            }
        }
        if (count > 0) {
            num_cores[i] = count;
        }
    }

    /* Now go through and build the map in the other direction:
       (socket,core) => processor_id.  This map simply points to
       entries in the other map (i.e., it's by reference instead of by
       value). */
    map_tuple_to_processor_id = malloc(sizeof(tuple_t **) *
                                       (max_socket_id + 1));
    if (NULL == map_tuple_to_processor_id) {
        clear_cache();
        return;
    }
    map_tuple_to_processor_id[0] = malloc(sizeof(tuple_t *) * 
                                          ((max_socket_id + 1) * 
                                           (max_core_id_overall + 1)));
    if (NULL == map_tuple_to_processor_id[0]) {
        clear_cache();
        return;
    }
    /* Set pointers for 2nd dimension */
    for (i = 1; i <= max_socket_id; ++i) {
        map_tuple_to_processor_id[i] = 
            map_tuple_to_processor_id[i - 1] + max_core_id_overall + 1;
    }
    /* Compute map */
    for (i = 0; i <= max_socket_id; ++i) {
        for (j = 0; j <= max_core_id_overall; ++j) {
            /* Default to the invalid entry in the other map, meaning
               that this (socket,core) combination doesn't exist
               (e.g., the core number does not exist in this socket,
               although it does exist in other sockets). */
            map_tuple_to_processor_id[i][j] = 
                &map_processor_id_to_tuple[invalid_entry];

            /* See if this (socket,core) tuple exists in the other
               map.  If so, set this entry to point to it (overriding
               the invalid entry default). */
            for (k = 0; k <= max_processor_num; ++k) {
                if (map_processor_id_to_tuple[k].socket == i &&
                    map_processor_id_to_tuple[k].core == j) {
                    map_tuple_to_processor_id[i][j] = 
                        &map_processor_id_to_tuple[k];
#if defined(PLPA_DEBUG) && PLPA_DEBUG
                    printf("Creating map: (socket %d, core %d) -> ID %d\n",
                           i, j, k);
#endif
                    break;
                }
            }
        }
    }

    supported = 1;
}

/* Internal function to setup the mapping data.  Guaranteed to be
   calling during PLPA_NAME(init), so we don't have to worry about
   thread safety here. */
int PLPA_NAME(map_init)(void)
{
    const char *sysfs_mount = "/sys";
    char *temp;

    temp = getenv("PLPA_SYSFS_MOUNT");
    if (temp) {
        sysfs_mount = temp;
    }

    load_cache(sysfs_mount);
    return 0;
}

/* Internal function to cleanup allocated memory.  Only called by one
   thread (during PLPA_NAME(finalize), so don't need to worry about
   thread safety here. */
void PLPA_NAME(map_finalize)(void)
{
    clear_cache();
}

/* Return whether this kernel supports topology information or not */
int PLPA_NAME(have_topology_information)(int *supported_arg)
{
    int ret;

    /* Initialize if not already done so */
    if (!PLPA_NAME(initialized)) {
        if (0 != (ret = PLPA_NAME(init)())) {
            return ret;
        }
    }

    /* Check for bozo arguments */
    if (NULL == supported_arg) {
        return EINVAL;
    }

    *supported_arg = supported;
    return 0;
}

int PLPA_NAME(map_to_processor_id)(int socket, int core, int *processor_id)
{
    int ret;

    /* Initialize if not already done so */
    if (!PLPA_NAME(initialized)) {
        if (0 != (ret = PLPA_NAME(init)())) {
            return ret;
        }
    }

    /* Check for bozo arguments */
    if (NULL == processor_id) {
        return EINVAL;
    }

    /* If this system doesn't support mapping, sorry Charlie */
    if (!supported) {
        return ENOSYS;
    }

    /* Check for some invalid entries */
    if (socket < 0 || socket > max_socket_id ||
        core < 0 || core > max_core_id_overall) {
        return ENOENT;
    }
    /* If the mapping returns -1, then this is a non-existent
       socket/core combo (even though they fall within the max socket
       / max core overall values) */
    ret = map_tuple_to_processor_id[socket][core]->processor_id;
    if (-1 == ret) {
        return ENOENT;
    }

    /* Ok, all should be good -- return the mapping */
    *processor_id = ret;
    return 0;
}

int PLPA_NAME(map_to_socket_core)(int processor_id, int *socket, int *core)
{
    int ret;

    /* Initialize if not already done so */
    if (!PLPA_NAME(initialized)) {
        if (0 != (ret = PLPA_NAME(init)())) {
            return ret;
        }
    }

    /* Check for bozo arguments */
    if (NULL == socket || NULL == core) {
        return EINVAL;
    }

    /* If this system doesn't support mapping, sorry Charlie */
    if (!supported) {
        return ENOSYS;
    }

    /* Check for some invalid entries */
    if (processor_id < 0 || processor_id > max_processor_num) {
        return ENOENT;
    }
    ret = map_processor_id_to_tuple[processor_id].socket;
    if (-1 == ret) {
        return ENOENT;
    }

    /* Ok, all should be good -- return the mapping */
    *socket = ret;
    *core = map_processor_id_to_tuple[processor_id].core;
    return 0;
}

int PLPA_NAME(get_processor_info)(int *num_processors_arg,
                                  int *max_processor_num_arg)
{
    int ret;

    /* Initialize if not already done so */
    if (!PLPA_NAME(initialized)) {
        if (0 != (ret = PLPA_NAME(init)())) {
            return ret;
        }
    }

    /* Check for bozo arguments */
    if (NULL == max_processor_num_arg || NULL == num_processors_arg) {
        return EINVAL;
    }

    /* If this system doesn't support mapping, sorry Charlie */
    if (!supported) {
        return ENOSYS;
    }

    /* All done */
    *num_processors_arg = num_processors;
    *max_processor_num_arg = max_processor_num;
    return 0;
}

/* Return the max socket number */
int PLPA_NAME(get_socket_info)(int *num_sockets_arg, int *max_socket_id_arg)
{
    int ret;

    /* Initialize if not already done so */
    if (!PLPA_NAME(initialized)) {
        if (0 != (ret = PLPA_NAME(init)())) {
            return ret;
        }
    }

    /* Check for bozo arguments */
    if (NULL == max_socket_id_arg || NULL == num_sockets_arg) {
        return EINVAL;
    }

    /* If this system doesn't support mapping, sorry Charlie */
    if (!supported) {
        return ENOSYS;
    }

    /* All done */
    *num_sockets_arg = num_sockets;
    *max_socket_id_arg = max_socket_id;
    return 0;
}

/* Return the number of cores in a socket and the max core ID number */
int PLPA_NAME(get_core_info)(int socket, int *num_cores_arg, 
                             int *max_core_id_arg)
{
    int ret;

    /* Initialize if not already done so */
    if (!PLPA_NAME(initialized)) {
        if (0 != (ret = PLPA_NAME(init)())) {
            return ret;
        }
    }

    /* Check for bozo arguments */
    if (NULL == max_core_id_arg || NULL == num_cores_arg) {
        return EINVAL;
    }

    /* If this system doesn't support mapping, sorry Charlie */
    if (!supported) {
        return ENOSYS;
    }

    /* Check for some invalid entries */
    if (socket < 0 || socket > max_socket_id || -1 == max_core_id[socket]) {
        return ENOENT;
    }
    ret = num_cores[socket];
    if (-1 == ret) {
        return ENOENT;
    }

    /* All done */
    *num_cores_arg = ret;
    *max_core_id_arg = max_core_id[socket];
    return 0;
}
