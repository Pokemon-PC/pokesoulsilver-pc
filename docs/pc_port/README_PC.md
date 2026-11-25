Pokémon Or HeartGold / Argent SoulSilver — portage PC natif / native PC port
============================================================================

(English below)

FRANÇAIS
--------

Ce programme fait tourner Pokémon Or HeartGold nativement sur PC, à partir du
code source décompilé du jeu. Il ne contient AUCUNE donnée du jeu : vous devez
fournir la ROM de VOTRE PROPRE cartouche.

### Installation

1. Décompressez ce dossier où vous voulez.
2. Copiez la ROM de votre cartouche Pokémon Or HeartGold (fichier `.nds`) dans
   ce dossier, à côté de `pokeheartgold.exe`.
3. Lancez `pokeheartgold.exe`. Au premier lancement, les fichiers du jeu sont
   extraits dans `pc_data/<langue>` (quelques secondes). La langue suit votre
   ROM (française, anglaise…). Vous pouvez ensuite retirer la ROM.

Votre partie est enregistrée dans `save.bin` (même format que la mémoire de
sauvegarde de la cartouche, 512 Kio).

**Argent SoulSilver** : c'est un jeu à part, avec son propre exécutable
(`pokesoulsilver.exe`, paquet `PokemonSoulSilver-PC-…`) et la ROM de votre
cartouche SoulSilver. Ses fichiers sont séparés : `pc_data_ss/`, `mods_ss/`,
`save_ss.bin`. Les deux jeux peuvent partager un dossier, ROM comprises :
chacun prend la sienne (code de jeu IPK… pour HeartGold, IPG… pour SoulSilver).

**Dossier de ROM commun** : si vous rangez toutes vos ROM dans un même dossier,
indiquez-le dans `config.ini`, section `[options]` :
`rom_dir = D:\Jeux\roms` (ou lancez avec `--rom-dir DOSSIER`). Quand aucune
ROM n'est à côté du jeu et que les données ne sont pas encore extraites, le
jeu y cherche sa ROM d'après son en-tête (code de jeu), quel que soit le nom du
fichier ; les autres ROM du dossier sont ignorées.

### Langue

Le jeu prend la langue de votre ROM. Pour jouer dans l'autre langue
(anglais avec une ROM française), réglez `language = en` dans `config.ini`
(section `[options]`), ou lancez avec `--lang en` : les textes anglais viennent
du code source du jeu (`pc_data/en`), le reste de votre ROM. Le logo du titre
et quelques images restent dans la langue de la ROM.

### Commandes par défaut

| DS | Clavier | Manette |
|---|---|---|
| Croix | Flèches | Croix / stick gauche |
| A / B / X / Y | X / Z / S / A | positions Nintendo (droite = A, bas = B) |
| L / R | Q / W | gâchettes hautes |
| Start / Select | Entrée / Maj droite ou Retour arrière | Start / Back |
| Écran tactile | souris (clic gauche) | — |

Raccourcis : **Tab** avance rapide (maintenue), **F8** avance rapide on/off,
**M** couper le son, **F12** capture d'écran (dossier `screenshots`), **F9**
disposition des écrans, **F10** fermer/ouvrir le couvercle (veille), **F11**
plein écran, **Échap** quitter. Réinitialisation : L+R+Start+Select, comme
sur la console.

Tout se change dans `config.ini` (touches, manette, raccourcis, vitesse de
l'avance rapide, plein écran, disposition `large_top` / `stacked` /
`side_by_side`, étirement, taille de fenêtre, filtre d'image).

### Options de lancement

`pokeheartgold.exe [--lang fr|en] [--save FICHIER] [--scale N] [--rom-dir DOSSIER] [--data DOSSIER]`

### Limites

Les fonctions sans fil et en ligne (échanges et combats sans fil, Wi-Fi,
GTS, Pokéwalker infrarouge) ne sont pas disponibles.

### Mods

Voir `docs/pc_port/MODDING.md` dans le dépôt source. En bref : un fichier
placé dans `mods/` (ou `mods/<langue>/`) au même chemin que dans la ROM
remplace l'original.


ENGLISH
-------

This program runs Pokémon HeartGold natively on PC, built from the game's
decompiled source code. It contains NO game data: you must provide the ROM
dumped from YOUR OWN cartridge.

### Setup

1. Unzip this folder anywhere.
2. Copy your HeartGold cartridge ROM (`.nds` file) into this folder, next to
   `pokeheartgold.exe`.
3. Run `pokeheartgold.exe`. On first run the game files are extracted to
   `pc_data/<language>` (a few seconds); the language follows your ROM. You
   can remove the ROM afterwards.

Your game is saved in `save.bin` (same format as the cartridge's 512 KiB
backup memory).

**SoulSilver** is a separate game with its own executable (`pokesoulsilver.exe`,
package `PokemonSoulSilver-PC-…`) and your SoulSilver cartridge ROM. Its files
are kept apart: `pc_data_ss/`, `mods_ss/`, `save_ss.bin`. Both games can share
one folder, ROMs included: each picks its own (game code IPK… for HeartGold,
IPG… for SoulSilver).

**Shared ROM folder**: if you keep all your ROMs in one folder, name it in
`config.ini`, section `[options]`: `rom_dir = D:\Games\roms` (or start with
`--rom-dir DIR`). When no ROM sits next to the game and the data is not
extracted yet, the game looks there for its ROM by its header (game code),
whatever the file is called; the other ROMs in the folder are ignored.

### Language

The game uses your ROM's language. To play in the other one (English with a
French ROM), set `language = en` in `config.ini` (`[options]`) or start with
`--lang en`: English text comes from the game's source code (`pc_data/en`),
everything else from your ROM. The title logo and a few pictures keep the ROM's
language.

### Default controls

Arrows = D-pad, X/Z/S/A = A/B/X/Y, Q/W = L/R, Enter = Start, Right Shift or
Backspace = Select, mouse = touch screen. Gamepads use Nintendo positions
(east = A, south = B). Hotkeys: Tab fast-forward (hold), F8 fast-forward
toggle, M mute, F12 screenshot, F9 screen layout, F10 lid (sleep), F11
fullscreen, Esc quit. Soft reset: L+R+Start+Select.

Everything is configurable in `config.ini`.

### Limitations

Wireless and online features (trades, wireless battles, Wi-Fi, GTS,
Pokéwalker) are not available.
