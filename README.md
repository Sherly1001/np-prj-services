# np-prj-services

## Install dependent
This project using [libwebsockets](https://libwebsockets.org/) library, so you must install it first. Check [here](https://github.com/warmcat/libwebsockets/blob/main/READMEs/README.build.md).

If you are using linux, just install [libwebsockets-dev](https://archlinux.org/packages/extra/x86_64/libwebsockets/).

## Compile
```hs
cmake -B build -S . [-A x64] [-DCMAKE_TOOLCHAIN_FILE=<path to vcpkg\scripts\buildsystems\vcpkg.cmake>]
cd build
cmake --build .
```
*`[arguments] are optional`*
