# New Simplified Build

The almost a decade old build system from 2014 and before failed for me and
thus I added a `configure` script which generates `makefile` from
`makefile.in`.  Lower-case `makefile` has higher priority than upper-case
`Makefile` and thus for pain-less compilation simply use `./configure &&
make`.  You might want to consider checking out `./configure -h` and
particularly the `--expert` option to allow run-time options for.

Beside `notangle` and `libz` which was already needed for the old build
process our new build process also requires `sed`, `find` and `xargs`.

On Ubuntu you can for instance install the following packages to meet
this build depedencies:

```
sudo apt install noweb zlib1g sed findutils
```

The result of compilation is `minibones` and `make clean` will clean up
generated files.

Armin Biere, March 2023
