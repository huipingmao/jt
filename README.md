Program for tracking immersion

Every entry in database <ins>has</ins> to follow this pattern: `YYYY-MM-DD HH:MM {some tag with 32 char. limit}`, and each should be in it's own line.

Eg. entries:
```
2026-10-10 03:20 anime:bocchi the rock
2026-10-09 00:45 youtube:podcast
2026-09-25 02:30 reading:light novel
#  ^date   ^time      ^tag
```

Dates are read from top to bottom, meaning the most recent entry should be at the top.

Lines beginning with `#` (comments) and new lines will be ignored.

Path to database and text editor of choice is read from JT_PATH and JT_EDITOR env. variables respectively, and have to be set manually by the user.

`$ jt H` for help message.

to-do:
- make tags work better
- tui
- input without text editor (idk maybe)

![pic unrelated](./image.jpg)
pic unrelated (254KB wasted)

made cuz i was bored
still wip
