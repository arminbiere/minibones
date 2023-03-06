# New Simplified Build

The almost a decade old build system from 2014 and before failed for me and
thus I added a `configure` script which generates `makefile` from
`makefile.in`.  Lower-case `makefile` has higher priority than upper-case
`Makefile` and thus for pain-less compilation simply use `./configure &&
make`.  You might want to consider checking out `./configure -h` and
particularly the '--expert' option to allow run-time options for.

Our new build process requires 'sed', 'find' and 'xargs' and the result of
compilation is `minbones` and `make clean` will clean up generated files.

Armin Biere, March 2023
