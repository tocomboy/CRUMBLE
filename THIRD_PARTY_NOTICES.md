# Third-party notices

The root [MIT license](LICENSE) applies to CRUMBLE's own code, maps, sprites and the project effects identified below.
The following third-party components retain their respective licenses. Original
copyright and attribution comments in `third_party/json.hpp` are preserved.

## nlohmann/json 3.11.3

- File: `third_party/json.hpp` (unmodified single header).
- Copyright 2013–2023 Niels Lohmann; additional contributor notices remain in the header.
- Upstream: https://github.com/nlohmann/json/tree/v3.11.3
- License: [MIT](licenses/nlohmann-json-MIT.txt), copied from upstream v3.11.3.
- The header identifies code from Google Abseil (Copyright 2018 The Abseil Authors).
  The corresponding [Apache License 2.0](licenses/Apache-2.0.txt) is included.
  Original reference: https://github.com/abseil/abseil-cpp/blob/10cb35e459f5ecca5b2ff107635da0bfa41011b4/absl/utility/utility.h

## Ubuntu fonts 0.83

- Files: `assets/fonts/Ubuntu-Regular.ttf`, `assets/fonts/Ubuntu-Bold.ttf`.
- Copyright 2011 Canonical Ltd.
- License: [Ubuntu Font Licence 1.0](licenses/Ubuntu-Font-Licence-1.0.txt).
- Official terms: https://canonical.com/legal/font-licence
- The font files retain their original names and are distributed under the Ubuntu
  Font Licence, not the project's MIT license.

## SDL dependencies

SDL2, SDL2_image, SDL2_mixer and SDL2_ttf are installed separately by the user.
Their sources and compiled libraries are not bundled in this source repository.
Each library and its dependencies retain their upstream licenses.

- https://github.com/libsdl-org/SDL/tree/SDL2
- https://github.com/libsdl-org/SDL_image/tree/SDL2
- https://github.com/libsdl-org/SDL_mixer/tree/SDL2
- https://github.com/libsdl-org/SDL_ttf/tree/SDL2

## Audio

Confirmed files below use CC0 1.0. Credits identify the original creators even where attribution is not required.

| File | Creator | Original source | Modification |
|---|---|---|---|
| `assets/bgm/bgm_title.ogg` | isaiah658 | [Heavenly Loop_0.ogg](https://opengameart.org/content/heavenly-loop) | Renamed only; byte-identical. |
| `assets/bgm/bgm_battle.ogg` | pmiller | [battle_music_01-loop.ogg](https://opengameart.org/content/chiptune-battle-music) | Renamed only; byte-identical. |
| `assets/sfx/sfx_hit.wav` | phoenix1291 | [Sound effects Pack 2/Hit/WAV/Hit 1 - Sound effects Pack 2.wav](https://opengameart.org/content/sound-effects-pack-2) | Renamed only; byte-identical. |
| `assets/sfx/sfx_mortar_blast.wav` | phoenix1291 | [Sound effects Pack 2/Explosions/WAV/Explosion 6 - Sound effects Pack 2.wav](https://opengameart.org/content/sound-effects-pack-2) | WAV metadata removed; signed 16-bit PCM multiplied by 6 with clipping to [-32768, 32767]. All samples verified against the official original. |
| `assets/sfx/sfx_mortar_fire.wav` | rubberduck | [bang_01.ogg](https://opengameart.org/content/25-cc0-bang-firework-sfx) | The repository extension is .wav, but the byte-identical source is Ogg. |
| `assets/sfx/sfx_rifle.wav` | phoenix1291 | [Sound effects Pack 2/Laser-weapon/WAV/Laser-weapon 1 - Sound effects Pack 2.wav](https://opengameart.org/content/sound-effects-pack-2) | Renamed only; byte-identical. |
| `assets/sfx/sfx_round.wav` | Kenney Vleugels | [Audio/8-Bit jingles/jingles_NES00.ogg](https://kenney.nl/assets/music-jingles) | The repository extension is .wav, but the byte-identical source is Ogg. |
| `assets/sfx/sfx_select.wav` | Kenney | [Audio/select_001.ogg](https://kenney.nl/assets/interface-sounds) | The repository extension is .wav, but the byte-identical source is Ogg. |
| `assets/sfx/sfx_shotgun.wav` | phoenix1291 | [Sound effects Pack 2/Laser-weapon/WAV/Laser-weapon 9 - Sound effects Pack 2.wav](https://opengameart.org/content/sound-effects-pack-2) | Renamed only; byte-identical. |
| `assets/sfx/sfx_swap.wav` | Kenney Vleugels | [Audio/switch1.ogg](https://kenney.nl/assets/ui-audio) | The repository extension is .wav, but the byte-identical source is Ogg. |

CC0 terms: https://creativecommons.org/publicdomain/zero/1.0/legalcode.en

### Project-synthesized effects (owner-reported provenance)

- Files: `assets/sfx/sfx_jump.wav`, `assets/sfx/sfx_land.wav`.
- The project owner recalls synthesizing these effects with a coding AI. The
  generation script and tool record were not found during the publication review;
  this provenance is owner-reported and was not independently verified.
- Distributed by the project owner under the root [MIT license](LICENSE).
  These two files are not represented as verified CC0 downloads.
