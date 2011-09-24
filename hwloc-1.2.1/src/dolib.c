/*
 * Copyright © 2009 CNRS
 * Copyright © 2009 INRIA.  All rights reserved.
 * Copyright © 2009 Université Bordeaux 1
 * See COPYING in top-level directory.
 */

/* Wrapper to avoid msys' tendency to turn / into \ and : into ;  */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  char *prog, *arch, *def, *name, *lib;
  char s[1024];

  if (argc != 6) {
    fprintf(stderr,"bad number of arguments");
    exit(EXIT_FAILURE);
  }

  prog = argv[1];
  arch = argv[2];
  def = argv[3];
  name = argv[4];
  lib = argv[5];

  snprintf(s, sizeof(s), "\"%s\" /machine:%s /def:%s /name:%s /out:%s",
      prog, arch, def, name, lib);
  if (system(s)) {
    fprintf(stderr, "%s failed\n", s);
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
