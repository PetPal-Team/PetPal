# romfs/

Everything in this directory is mounted read-only at `romfs:/` on the device.

* `romfs:/gfx/sprites.t3x` — generated automatically from `gfx/*.t3s` by the
  Makefile (do not hand-edit; it is git-ignored).
* Place audio (`*.wav` / `*.bcwav`) and any other runtime data files here.

`UIManager::init()` calls `romfsInit()`, so loaders can open `romfs:/...` paths
directly. Loading is fault-tolerant: a missing sprite atlas falls back to the
procedural pet renderer.
