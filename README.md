ajs (asmjit superoptimiser)
===========================

AJS is an assembly superoptimiser written for aiding the development of the [mpir library](mpir.org), developed as part of the OpenDreamKit project funded by the Horizon 2020 European Research Infrastructures Work Programme.
AJS is built using a patched version Petr Kobalicek's asmjit C++ library which can be found at https://github.com/alexjbest/asmjit, so in order to build this using `make` you will need a copy of the asmjit library source (but not the tests and build scripts) in a subfolder of this directory called asmjit, for example by running:
```bash
git clone git@github.com:alexjbest/asmjit.git ~/
ln -s ~/asmjit/src/asmjit/ ~/ajs/
```
replacing `~/` with another path if required.

You can then (hopefully) run `make ajs` to build the superoptimiser.
If this was successful `./ajs --help` will show usage information.
The helper script `superopt` is handy for optimising files which require pre-processing (via `m4` or `yasm`) as it will do this for you and pipe the output to ajs. Its basic usage is `./superopt YOUR-FILE`, for more information run `./superopt --help`. For this to work you will need a copy of `m4` installed and a built version of the MPIR source located in the directory above `ajs`, if this is not the case the script can be modified to work by changing the `MPIR_ROOT` variable inside.

There is also a script called `mpir` that will automatically call `superopt` for many MPIR functions, this uses the `.txt` files whose names are given in `sigs.txt` to determine the function signatures.

AJS is no longer being actively developed full time, but if you try to use it and have issues please report them and I'll take a look when I can.

Contributors
------------
Alex J. Best - initial version
Alexander Kruppa - vast improvements and bugfixes, first useful version?

Features
--------
 - Intel (YASM) and AT&T (GAS) syntax input supported.
 - Automatic and manual dependency detection
 - Transforming swaps
 - `nop` insertion

![Flag of EU](http://opendreamkit.org/public/logos/Flag_of_Europe.svg)
![ODK logo](http://opendreamkit.org/public/logos/odk-elected-logo.svg)
