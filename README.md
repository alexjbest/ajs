ajs (asmjit superoptimiser)
===========================

This is a prototype of an assembly superoptimiser based on Petr Kobalicek's asmjit C++ library.
In order to build this using make you will need a copy of the asmjit library source (not the tests) in a subfolder called asmjit, for example by running
```
ln -s /path/to/asmjit/src/asmjit/ /path/to/ajs
```

You can then (probably) run make to build the superoptimiser, and then try ``./superopt YOUR-FILE.asm``.
