# tasks

i have raging adhd. i need a lot of stimulants and also people to remind me of shit i need to do.
this discord bot is intended to solve one of those needs

# building

depends on the following
- json-c
- libcurl
- concord (`concord-git` on aur)
- libnyoravim (`libnyoravim-git` on aur)

```bash
# configure
cmake . -B build

# build
cmake --build build -j $(nproc)
```
