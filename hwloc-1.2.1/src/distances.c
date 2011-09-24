/*
 * Copyright © 2010-2011 INRIA.  All rights reserved.
 * Copyright © 2011 Université Bordeaux 1
 * Copyright © 2011 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

#include <private/autogen/config.h>
#include <hwloc.h>
#include <private/private.h>
#include <private/debug.h>

#include <float.h>

/* called during topology init */
void hwloc_topology_distances_init(struct hwloc_topology *topology)
{
  unsigned i;
  for (i=0; i < HWLOC_OBJ_TYPE_MAX; i++) {
    /* no distances yet */
    topology->os_distances[i].nbobjs = 0;
    topology->os_distances[i].objs = NULL;
    topology->os_distances[i].indexes = NULL;
    topology->os_distances[i].distances = NULL;
  }
}

/* called when reloading a topology.
 * keep initial parameters (from set_distances and environment),
 * but drop what was generated during previous load().
 */
void hwloc_topology_distances_clear(struct hwloc_topology *topology)
{
  unsigned i;
  for (i=0; i < HWLOC_OBJ_TYPE_MAX; i++) {
    /* remove final distance matrices, but keep physically-ordered ones */
    free(topology->os_distances[i].objs);
    topology->os_distances[i].objs = NULL;
  }
}

/* called during topology destroy */
void hwloc_topology_distances_destroy(struct hwloc_topology *topology)
{
  unsigned i;
  for (i=0; i < HWLOC_OBJ_TYPE_MAX; i++) {
    /* remove final distance matrics AND physically-ordered ones */
    free(topology->os_distances[i].indexes);
    topology->os_distances[i].indexes = NULL;
    free(topology->os_distances[i].objs);
    topology->os_distances[i].objs = NULL;
    free(topology->os_distances[i].distances);
    topology->os_distances[i].distances = NULL;
  }
}

/* insert a distance matrix in the topology.
 * the caller gives us those pointers, we take care of freeing them later and so on.
 */
void hwloc_topology__set_distance_matrix(hwloc_topology_t __hwloc_restrict topology, hwloc_obj_type_t type,
					 unsigned nbobjs, unsigned *indexes, hwloc_obj_t *objs, float *distances)
{
  free(topology->os_distances[type].indexes);
  free(topology->os_distances[type].objs);
  free(topology->os_distances[type].distances);
  topology->os_distances[type].nbobjs = nbobjs;
  topology->os_distances[type].indexes = indexes;
  topology->os_distances[type].objs = objs;
  topology->os_distances[type].distances = distances;
}

/* make sure a user-given distance matrix is sane */
static int hwloc_topology__check_distance_matrix(hwloc_topology_t __hwloc_restrict topology __hwloc_attribute_unused, hwloc_obj_type_t type __hwloc_attribute_unused,
						 unsigned nbobjs, unsigned *indexes, hwloc_obj_t *objs __hwloc_attribute_unused, float *distances __hwloc_attribute_unused)
{
  unsigned i,j;
  /* make sure we don't have the same index twice */
  for(i=0; i<nbobjs; i++)
    for(j=i+1; j<nbobjs; j++)
      if (indexes[i] == indexes[j]) {
	errno = EINVAL;
	return -1;
      }
  return 0;
}

static hwloc_obj_t hwloc_find_obj_by_type_and_os_index(hwloc_obj_t root, hwloc_obj_type_t type, unsigned os_index)
{
  hwloc_obj_t child;
  if (root->type == type && root->os_index == os_index)
    return root;
  child = root->first_child;
  while (child) {
    hwloc_obj_t found = hwloc_find_obj_by_type_and_os_index(child, type, os_index);
    if (found)
      return found;
    child = child->next_sibling;
  }
  return NULL;
}

static void hwloc_get_type_distances_from_string(struct hwloc_topology *topology,
						 hwloc_obj_type_t type, char *string)
{
  /* the string format is: "index[0],...,index[N-1]:distance[0],...,distance[N*N-1]"
   * or "index[0],...,index[N-1]:X*Y" or "index[0],...,index[N-1]:X*Y*Z"
   */
  char *tmp = string, *next;
  unsigned *indexes;
  float *distances;
  unsigned nbobjs = 0, i, j, x, y, z;

  /* count indexes */
  while (1) {
    size_t size = strspn(tmp, "0123456789");
    if (tmp[size] != ',') {
      /* last element */
      tmp += size;
      nbobjs++;
      break;
    }
    /* another index */
    tmp += size+1;
    nbobjs++;
  }

  if (*tmp != ':') {
    fprintf(stderr, "Ignoring %s distances from environment variable, missing colon\n",
	    hwloc_obj_type_string(type));
    return;
  }

  indexes = calloc(nbobjs, sizeof(unsigned));
  distances = calloc(nbobjs*nbobjs, sizeof(float));
  tmp = string;

  /* parse indexes */
  for(i=0; i<nbobjs; i++) {
    indexes[i] = strtoul(tmp, &next, 0);
    tmp = next+1;
  }

  /* parse distances */
  z=1; /* default if sscanf finds only 2 values below */
  if (sscanf(tmp, "%u*%u*%u", &x, &y, &z) >= 2) {
    /* generate the matrix to create x groups of y elements */
    if (x*y*z != nbobjs) {
      fprintf(stderr, "Ignoring %s distances from environment variable, invalid grouping (%u*%u*%u=%u instead of %u)\n",
	      hwloc_obj_type_string(type), x, y, z, x*y*z, nbobjs);
      free(indexes);
      free(distances);
      return;
    }
    for(i=0; i<nbobjs; i++)
      for(j=0; j<nbobjs; j++)
	if (i==j)
	  distances[i*nbobjs+j] = 1;
	else if (i/z == j/z)
	  distances[i*nbobjs+j] = 2;
	else if (i/z/y == j/z/y)
	  distances[i*nbobjs+j] = 4;
	else
	  distances[i*nbobjs+j] = 8;

  } else {
    /* parse a comma separated list of distances */
    for(i=0; i<nbobjs*nbobjs; i++) {
      distances[i] = atof(tmp);
      next = strchr(tmp, ',');
      if (next) {
        tmp = next+1;
      } else if (i!=nbobjs*nbobjs-1) {
	fprintf(stderr, "Ignoring %s distances from environment variable, not enough values (%u out of %u)\n",
		hwloc_obj_type_string(type), i+1, nbobjs*nbobjs);
	free(indexes);
	free(distances);
	return;
      }
    }
  }

  if (hwloc_topology__check_distance_matrix(topology, type, nbobjs, indexes, NULL, distances) < 0) {
    fprintf(stderr, "Ignoring invalid %s distances from environment variable\n", hwloc_obj_type_string(type));
    free(indexes);
    free(distances);
    return;
  }

  hwloc_topology__set_distance_matrix(topology, type, nbobjs, indexes, NULL, distances);
}

/* take distances in the environment, store them as is in the topology.
 * we'll convert them into object later once the tree is filled
 */
void hwloc_store_distances_from_env(struct hwloc_topology *topology)
{
  hwloc_obj_type_t type;
  for(type = HWLOC_OBJ_SYSTEM; type < HWLOC_OBJ_TYPE_MAX; type++) {
    char *env, envname[64];
    snprintf(envname, sizeof(envname), "HWLOC_%s_DISTANCES", hwloc_obj_type_string(type));
    env = getenv(envname);
    if (env)
      hwloc_get_type_distances_from_string(topology, type, env);
  }
}

/* take the given distance, store them as is in the topology.
 * we'll convert them into object later once the tree is filled.
 */
int hwloc_topology_set_distance_matrix(hwloc_topology_t __hwloc_restrict topology, hwloc_obj_type_t type,
				       unsigned nbobjs, unsigned *indexes, float *distances)
{
  unsigned *_indexes;
  float *_distances;

  if (hwloc_topology__check_distance_matrix(topology, type, nbobjs, indexes, NULL, distances) < 0)
    return -1;

  /* copy the input arrays and give them to the topology */
  _indexes = malloc(nbobjs*sizeof(unsigned));
  memcpy(_indexes, indexes, nbobjs*sizeof(unsigned));
  _distances = malloc(nbobjs*nbobjs*sizeof(float));
  memcpy(_distances, distances, nbobjs*nbobjs*sizeof(float));
  hwloc_topology__set_distance_matrix(topology, type, nbobjs, _indexes, NULL, _distances);

  return 0;
}

/* cleanup everything we created from distances so that we may rebuild them
 * at the end of restrict()
 */
void hwloc_restrict_distances(struct hwloc_topology *topology, unsigned long flags)
{
  hwloc_obj_type_t type;
  for(type = HWLOC_OBJ_SYSTEM; type < HWLOC_OBJ_TYPE_MAX; type++) {
    /* remove the objs array, we'll rebuild it from the indexes
     * depending on remaining objects */
    free(topology->os_distances[type].objs);
    topology->os_distances[type].objs = NULL;
    /* if not adapting distances, drop everything */
    if (!(flags & HWLOC_RESTRICT_FLAG_ADAPT_DISTANCES)) {
      free(topology->os_distances[type].indexes);
      topology->os_distances[type].indexes = NULL;
      free(topology->os_distances[type].distances);
      topology->os_distances[type].distances = NULL;
      topology->os_distances[type].nbobjs = 0;
    }
  }
}

/* convert distance indexes that were previously stored in the topology
 * into actual objects if not done already.
 * it's already done when distances come from backends.
 * it's not done when distances come from the user.
 */
void hwloc_convert_distances_indexes_into_objects(struct hwloc_topology *topology)
{
  hwloc_obj_type_t type;
  for(type = HWLOC_OBJ_SYSTEM; type < HWLOC_OBJ_TYPE_MAX; type++) {
    unsigned nbobjs = topology->os_distances[type].nbobjs;
    unsigned *indexes = topology->os_distances[type].indexes;
    float *distances = topology->os_distances[type].distances;
    unsigned i, j;
    if (!topology->os_distances[type].objs) {
      hwloc_obj_t *objs = calloc(nbobjs, sizeof(hwloc_obj_t));
      /* traverse the topology and look for the relevant objects */
      for(i=0; i<nbobjs; i++) {
	hwloc_obj_t obj = hwloc_find_obj_by_type_and_os_index(topology->levels[0][0], type, indexes[i]);
	if (!obj) {

	  /* shift the matrix */
#define OLDPOS(i,j) (distances+(i)*nbobjs+(j))
#define NEWPOS(i,j) (distances+(i)*(nbobjs-1)+(j))
	  if (i>0) {
	    /** no need to move beginning of 0th line */
	    for(j=0; j<i-1; j++)
	      /** move end of jth line + beginning of (j+1)th line */
	      memmove(NEWPOS(j,i), OLDPOS(j,i+1), (nbobjs-1)*sizeof(*distances));
	    /** move end of (i-1)th line */
	    memmove(NEWPOS(i-1,i), OLDPOS(i-1,i+1), (nbobjs-i-1)*sizeof(*distances));
	  }
	  if (i<nbobjs-1) {
	    /** move beginning of (i+1)th line */
	    memmove(NEWPOS(i,0), OLDPOS(i+1,0), i*sizeof(*distances));
	    /** move end of jth line + beginning of (j+1)th line */
	    for(j=i; j<nbobjs-1; j++)
	      memmove(NEWPOS(j,i), OLDPOS(j+1,i+1), (nbobjs-1)*sizeof(*distances));
	    /** move end of (nbobjs-2)th line */
	    memmove(NEWPOS(nbobjs-2,i), OLDPOS(nbobjs-1,i+1), (nbobjs-i-1)*sizeof(*distances));
	  }

	  /* shift the indexes array */
	  memmove(indexes+i, indexes+i+1, (nbobjs-i-1)*sizeof(*indexes));

	  /* update counters */
	  nbobjs--;
	  i--;
	  continue;
	}
	objs[i] = obj;
      }

      topology->os_distances[type].nbobjs = nbobjs;
      if (!nbobjs) {
	/* the whole matrix was invalid */
	free(objs);
	free(topology->os_distances[type].indexes);
	topology->os_distances[type].indexes = NULL;
	free(topology->os_distances[type].distances);
	topology->os_distances[type].distances = NULL;
      } else {
	/* setup the objs array */
	topology->os_distances[type].objs = objs;
      }
    }
  }
}

static void
hwloc_setup_distances_from_os_matrix(struct hwloc_topology *topology,
				     unsigned nbobjs,
				     hwloc_obj_t *objs, float *osmatrix)
{
  unsigned i, j, li, lj, minl;
  float min = FLT_MAX, max = FLT_MIN;
  hwloc_obj_t root;
  float *matrix;
  hwloc_cpuset_t set;
  unsigned relative_depth;
  int idx;

  /* find the root */
  set = hwloc_bitmap_alloc();
  for(i=0; i<nbobjs; i++)
    hwloc_bitmap_or(set, set, objs[i]->cpuset);
  root = hwloc_get_obj_covering_cpuset(topology, set);
  assert(root);
  if (!hwloc_bitmap_isequal(set, root->cpuset)) {
    /* partial distance matrix not including all the children of a single object */
    /* TODO insert an intermediate object (group?) covering only these children ? */
    hwloc_bitmap_free(set);
    return;
  }
  hwloc_bitmap_free(set);
  relative_depth = objs[0]->depth - root->depth; /* this assume that we have distances between objects of the same level */

  /* get the logical index offset, it's the min of all logical indexes */
  minl = UINT_MAX;
  for(i=0; i<nbobjs; i++)
    if (minl > objs[i]->logical_index)
      minl = objs[i]->logical_index;

  /* compute/check min/max values */
  for(i=0; i<nbobjs; i++)
    for(j=0; j<nbobjs; j++) {
      float val = osmatrix[i*nbobjs+j];
      if (val < min)
	min = val;
      if (val > max)
	max = val;
    }
  if (!min) {
    /* Linux up to 2.6.36 reports ACPI SLIT distances, which should be memory latencies.
     * Except of SGI IP27 (SGI Origin 200/2000 with MIPS processors) where the distances
     * are the number of hops between routers.
     */
    hwloc_debug("%s", "minimal distance is 0, matrix does not seem to contain latencies, ignoring\n");
    return;
  }

  /* store the normalized latency matrix in the root object */
  idx = root->distances_count++;
  root->distances = realloc(root->distances, root->distances_count * sizeof(struct hwloc_distances_s *));
  root->distances[idx] = malloc(sizeof(struct hwloc_distances_s));
  root->distances[idx]->relative_depth = relative_depth;
  root->distances[idx]->nbobjs = nbobjs;
  root->distances[idx]->latency = matrix = malloc(nbobjs*nbobjs*sizeof(float));
  root->distances[idx]->latency_base = (float) min;
#define NORMALIZE_LATENCY(d) ((d)/(min))
  root->distances[idx]->latency_max = NORMALIZE_LATENCY(max);
  for(i=0; i<nbobjs; i++) {
    li = objs[i]->logical_index - minl;
    matrix[li*nbobjs+li] = NORMALIZE_LATENCY(osmatrix[i*nbobjs+i]);
    for(j=i+1; j<nbobjs; j++) {
      lj = objs[j]->logical_index - minl;
      matrix[li*nbobjs+lj] = NORMALIZE_LATENCY(osmatrix[i*nbobjs+j]);
      matrix[lj*nbobjs+li] = NORMALIZE_LATENCY(osmatrix[j*nbobjs+i]);
    }
  }
}

/* convert internal distances into logically-ordered distances
 * that can be exposed in the API
 */
void
hwloc_finalize_logical_distances(struct hwloc_topology *topology)
{
  unsigned nbobjs;
  hwloc_obj_type_t type;
  int depth;

  for (type = HWLOC_OBJ_SYSTEM; type < HWLOC_OBJ_TYPE_MAX; type++) {
    nbobjs = topology->os_distances[type].nbobjs;
    if (!nbobjs)
      continue;

    depth = hwloc_get_type_depth(topology, type);
    if (depth == HWLOC_TYPE_DEPTH_UNKNOWN || depth == HWLOC_TYPE_DEPTH_MULTIPLE)
      continue;

    if (topology->os_distances[type].objs) {
      assert(topology->os_distances[type].distances);

      hwloc_setup_distances_from_os_matrix(topology, nbobjs,
					   topology->os_distances[type].objs,
					   topology->os_distances[type].distances);
    }
  }
}

/* destroy a object distances structure */
void
hwloc_free_logical_distances(struct hwloc_distances_s * dist)
{
  free(dist->latency);
  free(dist);
}

static void hwloc_report_user_distance_error(const char *msg, int line)
{
    static int reported = 0;

    if (!reported) {
        fprintf(stderr, "****************************************************************************\n");
        fprintf(stderr, "* Hwloc has encountered what looks like an error from user-given distances.\n");
        fprintf(stderr, "*\n");
        fprintf(stderr, "* %s\n", msg);
        fprintf(stderr, "* Error occurred in topology.c line %d\n", line);
        fprintf(stderr, "*\n");
        fprintf(stderr, "* Please make sure that distances given through the interface or environment\n");
        fprintf(stderr, "* variables do not contradict any other topology information.\n");
        fprintf(stderr, "****************************************************************************\n");
        reported = 1;
    }
}

/*
 * Place objects in groups if they are in a transitive graph of minimal distances.
 * Return how many groups were created, or 0 if some incomplete distance graphs were found.
 */
static unsigned
hwloc_setup_group_from_min_distance(unsigned nbobjs,
				    float *_distances,
				    unsigned *groupids)
{
  float min_distance = FLT_MAX;
  unsigned groupid = 1;
  unsigned i,j,k;
  unsigned skipped = 0;

#define DISTANCE(i, j) _distances[(i) * nbobjs + (j)]

  memset(groupids, 0, nbobjs*sizeof(*groupids));

  /* find the minimal distance */
  for(i=0; i<nbobjs; i++)
    for(j=i+1; j<nbobjs; j++)
      if (DISTANCE(i, j) < min_distance)
        min_distance = DISTANCE(i, j);
  hwloc_debug("found minimal distance %f between objects\n", min_distance);

  if (min_distance == FLT_MAX)
    return 0;

  /* build groups of objects connected with this distance */
  for(i=0; i<nbobjs; i++) {
    unsigned size;
    int firstfound;

    /* if already grouped, skip */
    if (groupids[i])
      continue;

    /* start a new group */
    groupids[i] = groupid;
    size = 1;
    firstfound = i;

    while (firstfound != -1) {
      /* we added new objects to the group, the first one was firstfound.
       * rescan all connections from these new objects (starting at first found) to any other objects,
       * so as to find new objects minimally-connected by transivity.
       */
      int newfirstfound = -1;
      for(j=firstfound; j<nbobjs; j++)
	if (groupids[j] == groupid)
	  for(k=0; k<nbobjs; k++)
              if (!groupids[k] && DISTANCE(j, k) == min_distance) {
	      groupids[k] = groupid;
	      size++;
	      if (newfirstfound == -1)
		newfirstfound = k;
	      if (i == j)
		hwloc_debug("object %u is minimally connected to %u\n", k, i);
	      else
	        hwloc_debug("object %u is minimally connected to %u through %u\n", k, i, j);
	    }
      firstfound = newfirstfound;
    }

    if (size == 1) {
      /* cancel this useless group, ignore this object and try from the next one */
      groupids[i] = 0;
      skipped++;
      continue;
    }

    /* valid this group */
    groupid++;
    hwloc_debug("found transitive graph with %u objects with minimal distance %f\n",
	       size, min_distance);
  }

  if (groupid == 2 && !skipped)
    /* we created a single group containing all objects, ignore it */
    return 0;

  /* return the last id, since it's also the number of used group ids */
  return groupid-1;
}

/*
 * Look at object physical distances to group them,
 * after having done some basic sanity checks.
 */
static void
hwloc__setup_groups_from_distances(struct hwloc_topology *topology,
				   unsigned nbobjs,
				   struct hwloc_obj **objs,
				   float *_distances,
				   int fromuser)
{
  unsigned *groupids = NULL;
  unsigned nbgroups;
  unsigned i,j;

  hwloc_debug("trying to group %s objects into Group objects according to physical distances\n",
	     hwloc_obj_type_string(objs[0]->type));

  if (nbobjs <= 2) {
      return;
  }

  groupids = malloc(sizeof(unsigned) * nbobjs);
  if (NULL == groupids) {
      return;
  }

  nbgroups = hwloc_setup_group_from_min_distance(nbobjs, _distances, groupids);
  if (!nbgroups) {
      goto outter_free;
  }

  /* For convenience, put these declarations inside a block.  It's a
     crying shame we can't use C99 syntax here, and have to do a bunch
     of mallocs. :-( */
  {
      hwloc_obj_t *groupobjs = NULL;
      unsigned *groupsizes = NULL;
      float *groupdistances = NULL;

      groupobjs = malloc(sizeof(hwloc_obj_t) * nbgroups);
      groupsizes = malloc(sizeof(unsigned) * nbgroups);
      groupdistances = malloc(sizeof(float) * nbgroups * nbgroups);
      if (NULL == groupobjs || NULL == groupsizes || NULL == groupdistances) {
          goto inner_free;
      }
      /* create new Group objects and record their size */
      memset(&(groupsizes[0]), 0, sizeof(groupsizes[0]) * nbgroups);
      for(i=0; i<nbgroups; i++) {
          /* create the Group object */
          hwloc_obj_t group_obj;
          group_obj = hwloc_alloc_setup_object(HWLOC_OBJ_GROUP, -1);
          group_obj->cpuset = hwloc_bitmap_alloc();
          group_obj->attr->group.depth = topology->next_group_depth;
          for (j=0; j<nbobjs; j++)
              if (groupids[j] == i+1) {
                  hwloc_bitmap_or(group_obj->cpuset, group_obj->cpuset, objs[j]->cpuset);
                  groupsizes[i]++;
              }
          hwloc_debug_1arg_bitmap("adding Group object with %u objects and cpuset %s\n",
                                  groupsizes[i], group_obj->cpuset);
          hwloc__insert_object_by_cpuset(topology, group_obj,
					 fromuser ? hwloc_report_user_distance_error : hwloc_report_os_error);
          groupobjs[i] = group_obj;
      }

      /* factorize distances */
      memset(&(groupdistances[0]), 0, sizeof(groupdistances[0]) * nbgroups * nbgroups);
#undef DISTANCE
#define DISTANCE(i, j) _distances[(i) * nbobjs + (j)]
#define GROUP_DISTANCE(i, j) groupdistances[(i) * nbgroups + (j)]
      for(i=0; i<nbobjs; i++)
	if (groupids[i])
	  for(j=0; j<nbobjs; j++)
	    if (groupids[j])
                GROUP_DISTANCE(groupids[i]-1, groupids[j]-1) += DISTANCE(i, j);
      for(i=0; i<nbgroups; i++)
          for(j=0; j<nbgroups; j++)
              GROUP_DISTANCE(i, j) /= groupsizes[i]*groupsizes[j];
#ifdef HWLOC_DEBUG
      hwloc_debug("%s", "generated new distance matrix between groups:\n");
      hwloc_debug("%s", "  index");
      for(j=0; j<nbgroups; j++)
	hwloc_debug(" % 5d", (int) j); /* print index because os_index is -1 fro Groups */
      hwloc_debug("%s", "\n");
      for(i=0; i<nbgroups; i++) {
	hwloc_debug("  % 5d", (int) i);
	for(j=0; j<nbgroups; j++)
          hwloc_debug(" %2.3f", GROUP_DISTANCE(i, j));
	hwloc_debug("%s", "\n");
      }
#endif

      topology->next_group_depth++;
      hwloc__setup_groups_from_distances(topology, nbgroups, groupobjs, (float*) groupdistances, fromuser);

  inner_free:
      /* Safely free everything */
      if (NULL != groupobjs) {
          free(groupobjs);
      }
      if (NULL != groupsizes) {
          free(groupsizes);
      }
      if (NULL != groupdistances) {
          free(groupdistances);
      }
  }

 outter_free:
  if (NULL != groupids) {
      free(groupids);
  }
}

/*
 * Look at object physical distances to group them.
 */
static void
hwloc_setup_groups_from_distances(struct hwloc_topology *topology,
				  unsigned nbobjs,
				  struct hwloc_obj **objs,
				  float *_distances,
				  int fromuser)
{
  unsigned i,j;

  if (getenv("HWLOC_IGNORE_DISTANCES"))
    return;

#ifdef HWLOC_DEBUG
  hwloc_debug("%s", "trying to group objects using distance matrix:\n");
  hwloc_debug("%s", "  index");
  for(j=0; j<nbobjs; j++)
    hwloc_debug(" % 5d", (int) objs[j]->os_index);
  hwloc_debug("%s", "\n");
  for(i=0; i<nbobjs; i++) {
    hwloc_debug("  % 5d", (int) objs[i]->os_index);
    for(j=0; j<nbobjs; j++)
      hwloc_debug(" %2.3f", DISTANCE(i, j));
    hwloc_debug("%s", "\n");
  }
#endif

  /* check that the matrix is ok */
  for(i=0; i<nbobjs; i++) {
    for(j=i+1; j<nbobjs; j++) {
      /* should be symmetric */
      if (DISTANCE(i, j) != DISTANCE(j, i)) {
	hwloc_debug("distance matrix asymmetric ([%u,%u]=%f != [%u,%u]=%f), aborting\n",
                    i, j, DISTANCE(i, j), j, i, DISTANCE(j, i));
	return;
      }
      /* diagonal is smaller than everything else */
      if (DISTANCE(i, j) <= DISTANCE(i, i)) {
	hwloc_debug("distance to self not strictly minimal ([%u,%u]=%f <= [%u,%u]=%f), aborting\n",
                    i, j, DISTANCE(i, j), i, i, DISTANCE(i, i));
	return;
      }
    }
  }

  hwloc__setup_groups_from_distances(topology, nbobjs, objs, _distances, fromuser);
}

void
hwloc_group_by_distances(struct hwloc_topology *topology)
{
  unsigned nbobjs;
  hwloc_obj_type_t type;

  for (type = HWLOC_OBJ_SYSTEM; type < HWLOC_OBJ_TYPE_MAX; type++) {
    nbobjs = topology->os_distances[type].nbobjs;
    if (!nbobjs)
      continue;

    if (topology->os_distances[type].objs) {
      /* if we have objs, we must have distances as well,
       * thanks to hwloc_convert_distances_indexes_into_objects()
       */
      assert(topology->os_distances[type].distances);
      hwloc_setup_groups_from_distances(topology, nbobjs,
					topology->os_distances[type].objs,
					topology->os_distances[type].distances,
					topology->os_distances[type].indexes != NULL);
    }
  }
}
