# Assets

Source images for everything drawn on the display.

- `sprites/` – the raccoon, apple, ball, lamp and stat icons, hand-drawn pixel art.
  Files are named `<set>_<frame>.png` (frames start at 1) and need an alpha channel.
- `rooms/` – the four room backgrounds (120×120, drawn at double size). These were generated
  with an image generator, not drawn by hand.

After changing a file, regenerate the C data from the project root:

```sh
pip install pillow
python3 tools/gen_sprites.py
```

This rewrites `components/ui/sprites_data.c`. Sizes and the order of the sets are defined in
`tools/gen_sprites.py` and must match `sprite_set_t` / `room_bg_t` in `components/ui/sprites.h`.
