
MUSL Math library

Musl combines libc and libm into libc. We don't want libc so we
have our own makefile to pick specific math functions we want: [apps/libm/Makefile](../../apps/libm/Makefile).

[libm.a](./libm.a) is built with the following:

```bash
git clone <musl-website>
cd musl
./configure
make -j

cd elos
make -f apps/libm/Makefile
cp int/libm/libm.a extern/musl/libm.a
```

