Understanding htop versions
===========================

Release versions
------------------

Release versions are marked in `configure.ac` with
m4_define([htop_release_version], [9.9.9]) for a version `9.9.9`.

A release version will identify in `htop --version` and the crash report as:
- htop 9.9.9
- Your htop version: '9.9.9'

Development versions
----------------------

Development versions are marked in `configure.ac` with
m4_define([htop_release_version], [10.9.8-dev]) for a version `10.9.8` to be released sometime in the future.

This version id will be amended by

```
git describe --abbrev=7 --dirty --always --tags
```

which you can find as `htop_git_version` in `configure.ac`.

A development version will identify in `htop --version` and the crash report as:
- htop 10.9.8-dev-10.9.0-16-gabbccdd
- Your htop version: '10.9.8-dev-10.9.0-16-gabbccdd'

The breakdown of this version string is as follows:

    10.9.8                - dev                  - 10.9.0       - 16                     - gabbccdd
    Next release version  - still in development - last git tag - commits since last tag - g(it) + commit id for the build
