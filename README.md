# Tempest 2000 (T2K mode) for Game Boy Color

A fan port of the Tempest 2000 game mode to the Game Boy Color, built from the
platform-independent C core of
[haroldo-ok/tempest-2k-sms](https://github.com/haroldo-ok/tempest-2k-sms)
(itself from [haroldo-ok/tempest-2k-32x](https://github.com/haroldo-ok/tempest-2k-32x)).

The GBC cannot draw vector graphics, scale sprites or rotate anything in real
time. Webs, sprites, the title rotation, warp-in and zoom-out are all
pre-rendered offline and streamed from ROM.

| | |
|---|---|
| ROM | `rom/t2k.gbc` — 2 MiB, GBC only, MBC5 (cart may be up to 4 MiB) |
| Screen | 160×144, 8 background palettes, 8 sprite palettes, double-speed CPU |
| Webs | All 48 level entries (34 distinct shapes) |
| Sprites | 845 pre-rendered images (5 sizes × up to 16 orientations) |
| FMV | Title loop, per-web warp-in and zoom-out (32 frames each) |
| Audio | Original long-form GBC chiptunes; Jaguar voice PCM on CH3; 14 SFX |

## Credits and legal

* **Tempest** (1981) was created by Dave Theurer for Atari. **Tempest 2000**
  (1994) is by Jeff Minter / Llamasoft for the Atari Jaguar. This is an
  unofficial, non-commercial fan port; no ownership of the game or its
  trademarks is claimed.
* Game rules are ported from the SMS/32X reimplementation (`src/game.c`).
* Web geometry comes from `third_party/webdata.h`, originally extracted from
  [mwenge's reconstructed Tempest 2000 source](https://github.com/mwenge/tempest2k).
* Spoken lines are the original Jaguar samples (Excellent, Superzapper Recharge,
  yes, oneup, sexy yes 1/2) from that same tree (`src/sounds/samples` 22–25 and
  27–28), downsampled to 4-bit CH3 PCM. The Jaguar logo, bitmap art and
  soundtrack MODs are *not* used.
* **Original to this port:** sprite designs, HUD font, title art, chiptune
  score, effects, and the GBC FMV encoder/player. In-game music is four long
  looping tracks (pulse lead, wave bass, sparse drums) that keep playing across
  webs until the original 16-level `webtunes` boundary.
* Built with [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020) / SDCC.

## Playing

Open `rom/t2k.gbc` in a GBC emulator (SameBoy, Emulicious, BGB, mGBA, RetroArch)
or a flash cart that supports MBC5 GBC ROMs. There is also an in-browser player
under `web/` (serve that folder over HTTP).

### Controls

| Input | Title / menus | In game |
|---|---|---|
| D-pad Left / Right | Choose start level | Move the claw around the web |
| D-pad Up | – | Jump (after collecting the Jump powerup) |
| A | Start / confirm | Fire (hold for autofire) |
| B | Back to title | Superzapper |
| Start | Start | Pause / resume |

In the browser player: arrows or WASD = D-pad, **X** / **K** = A, **Z** / **J** = B, Enter = Start.

### Flow

Title (rotating web) → level select (odd levels up to the best you have reached
this session) → warp-in FMV → play → clear the web → zoom-out FMV (steer and
shoot remaining spikes; hitting one costs a life) → next level.

### What's implemented

* All 48 web entries (34 distinct shapes), cycling for levels 49–99.
* Enemies: flippers, mutant flippers, tankers (flipper / pulsar / fuseball
  payloads), spikers and their spikes, fuseballs, pulsars, demonheads, mirrors.
* Superzapper (full clear first, single kill second), jumping.
* Powerups: particle laser, jump, AI droid, Zappo 2000, Outta Here, warp tokens.
  The AI droid is a second claw: it moves on its own, seeks the nearest threat
  and fires down that lane (including enemies on the rim).
* Autofire (hold A) and the shot pool survive a death correctly.
* Highlighted player lane (palette swap only — flip and VRAM-bank bits from the
  FMV layout are left alone), extra lives every 20,000 then every 100,000,
  pause, game over.
* Warp-in, zoom-out and the title rotation are 32-frame FMV streams. Tiles live
  in a shared 2bpp dictionary (~42k unique 8×8s) and play through both GBC VRAM
  banks. Warp-in converges to the same nametable gameplay uses.

### Audio

**Music** is original GBC chiptune, not the Jaguar MODs. Title ~2:30; four
in-game loops ~3:12 each. The current web's tune is started on warp-in and is
*not* restarted between levels until the 16-web `webtunes` boundary.

Channels never share a job:

| CH | Job |
|---|---|
| CH1 pulse | melody |
| CH2 pulse | SFX only |
| CH3 wave | bass, stolen for voice PCM |
| CH4 noise | sparse kick/snare (explode / die / zap borrow it) |

**Voices** are the Jaguar signed-8 samples, resampled to 4-bit @ 8192 Hz and
streamed into CH3 from a 256 Hz timer (GBDK `add_TIM`). Bass ducks while a clip
plays. The Jaguar does *not* speak a unique name for every powerup — pickups
are a whoosh plus on-screen text — so the port uses the original spoken lines
as a commentator over those banners:

| When | Clip (Jaguar sample) |
|---|---|
| Menu confirm (title / level select) | sexy yes 2 |
| After warp-in (`TEMPEST 2000`) | Superzapper Recharge |
| Odd web clear (`EXCELLENT!`) | Excellent |
| Even web clear (`YES YES YES!`) | yes |
| Extra life | oneup |
| Particle laser | sexy yes 1 |
| Jump enabled | sexy yes 2 |
| AI droid | yes |
| Zappo 2000 | Excellent |
| Outta Here | sexy yes 2 |
| Warp token | yes |

## Building

### Requirements

* [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020) 4.4+ (`GBDK` in the Makefile)
* Python 3 with NumPy and Pillow

### Steps

```sh
export GBDK=/path/to/gbdk
make assets          # tools/build.py → gen/blob.bin + gen/assets.h
                     # tools/compose.py → gen/tunes.bin
                     # tools/vox.py → gen/voices.bin
make                 # compile, link, splice data banks → rom/t2k.gbc
```

`tools/build.py` pre-renders webs, sprites, title frames and FMV streams into
`gen/blob.bin`. `tools/compose.py` writes the chiptune banks. `tools/vox.py`
converts `third_party/t2k-vox/*.bin` (needs `gen/blob.bin` and `gen/tunes.bin`
already sized so bank numbers in `gen/voices.h` match the splice). `make`
compiles the C code with GBDK (bank 0 platform/FMV/sound, bank 1 game) and
`tools/mkrom.py` overlays the blob, tunes and voices from bank 3, padding to a
power-of-two size (2 MiB here; 4 MiB / 256 banks is the ceiling).

Keep GBDK runtime, `main.c`, `fmv.c` and `sound.c` in bank 0. The FMV player is
`NONBANKED` so it can `SWITCH_ROM` while streaming. The music sequencer is
installed with `add_VBL()` and the voice PCM player with `add_TIM()`; both
must stay in bank 0.

## Project layout

```
src/            GBDK C code
  main.c        state machine, palettes, sprite streaming, lit lane, HUD
  fmv.c/.h      FMV player (NONBANKED)
  game.c/.h     game rules, AI droid, shots (8.8 fixed point) — bank 1
  sound.c/.h    VBlank music/SFX + TIM CH3 PCM player (bank 0)
tools/          offline data pipeline (Python)
  build.py      webs, sprites, title, FMV encode, blob pack
  compose.py    original chiptunes → gen/tunes.bin
  vox.py        Jaguar voice samples → gen/voices.bin (4-bit CH3 PCM)
  fmvenc.py     tile dictionary, VRAM slot allocator, frame encoder
  mkrom.py      splice code ROM + blob + tunes + voices, pad, fix header (MBC5)
third_party/
  webdata.h     original web geometry
  t2k-vox/      Jaguar samples: excellent, zapper, yes, oneup, sexy1, sexy2
gen/            generated blob, tunes, voices, assets.h / assets.c
rom/t2k.gbc     the ROM
web/            in-browser player (GameBoy-Online)
```

ROM map: bank 0 platform + FMV player + sound ISR, bank 1 game, banks 3+ asset
blob (font, sprites, static webs, FMV streams, tile dictionary, chiptunes,
voices). Music and SFX tick from VBlank so tempo does not stall on heavy
frames. Voices stream 4-bit PCM on CH3 at 8192 Hz from a 256 Hz timer.

## Known limitations

* Not implemented: UFO, bonus round (three warp tokens still award 10,000),
  camera panning, particle explosions, SRAM high-score save.
* The web is wireframe (no filled lanes).
* With many enemies on one scanline, sprites drop (GBC 10-per-line limit).
* CH3 voice PCM is 4-bit / 8192 Hz (crunchy; that is the hardware).
