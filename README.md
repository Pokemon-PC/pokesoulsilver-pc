# Pokémon Argent SoulSilver — version PC native / native PC port

[![PC port](https://github.com/Pokemon-PC/pokesoulsilver-pc/actions/workflows/pc-port.yml/badge.svg)](https://github.com/Pokemon-PC/pokesoulsilver-pc/actions/workflows/pc-port.yml)

**Projet de fans, non affilié à Nintendo, Game Freak, The Pokémon Company ni
à [pret](https://pret.github.io/).** C'est un fork du dépôt pokeheartgold de
pret, dont l'outillage d'origine leur appartient. pret n'accepte pas le code
généré par IA et ne veut pas y être associé : n'envoyez rien d'ici à pret et ne
les sollicitez pas à son sujet. Décompilation complète (tout est en C/C++, les
deux ROM se reconstruisent à l'octet près) et port PC réalisés avec des agents IA.

**HeartGold** : [Pokemon-PC/pokeheartgold-pc](https://github.com/Pokemon-PC/pokeheartgold-pc).
Les deux jeux partagent le même code ; ce dépôt construit SoulSilver par défaut
(`-DPHG_GAME_VERSION=SOULSILVER`) et suit les évolutions de celui de HeartGold.

(English below)

## Français

Pokémon Argent SoulSilver qui tourne **nativement sur PC** (Windows, Linux,
macOS Intel), **sans émulateur** : le code du jeu est compilé pour le PC et le
matériel de la DS est reproduit (affichage 2D et 3D, son, sauvegarde, écran
tactile à la souris, horloge).

- **Votre ROM est nécessaire** : posez la ROM de *votre* cartouche SoulSilver
  (`.nds`, code IPG…) à côté de `pokesoulsilver.exe` ; ses fichiers sont
  extraits au premier lancement dans `pc_data_ss/`. Le dépôt et le paquet ne
  contiennent aucune donnée de la cartouche.
- **Langue** : celle de votre ROM, ou l'anglais (textes construits depuis le
  code source du jeu) : `config.ini` → `[options] language = auto | fr | en`.
- **À part de HeartGold** : sauvegarde `save_ss.bin`, mods dans `mods_ss/`,
  données dans `pc_data_ss/` ; les deux jeux peuvent partager un dossier.
- **Moddable** : un fichier posé dans `mods_ss/` au même chemin que dans la ROM
  remplace l'original ; tout le code du jeu est en C et se recompile.

### Jouer

| DS | Clavier | Manette |
|---|---|---|
| Croix | Flèches | Croix / stick gauche |
| A / B / X / Y | X / Z / S / A | positions Nintendo |
| L / R | Q / W | gâchettes hautes |
| Start / Select | Entrée / Maj droite | Start / Back |
| Écran tactile | souris | — |

Raccourcis : Tab avance rapide, F9 disposition des écrans, F11 plein écran,
F12 capture, M son. Tout se change dans `config.ini`. Guide complet :
[docs/pc_port/README_PC.md](docs/pc_port/README_PC.md).

### Construire

Prérequis : Clang (Windows : `winget install MartinStorsjo.LLVM-MinGW.UCRT`),
CMake, Ninja, Python 3. SDL2 est téléchargé par CMake.

```
python tools/pc_port/nitrofs.py extract <votre-rom-soulsilver.nds> pc_data_ss/fr
cmake -S . -B build-ss -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build-ss
build-ss/pokesoulsilver.exe
```

Paquet : `python tools/pc_port/package.py --build build-ss` →
`PokemonSoulSilver-PC-….zip` (exécutable, notice, dossier `mods_ss/` ; jamais
de ROM, de données ni de sauvegarde).

### État

SoulSilver partage tout le moteur de HeartGold, dont l'histoire a été jouée de
bout en bout (voir [PORTING_ROADMAP.md](PORTING_ROADMAP.md)). Vérifié avec une
ROM SoulSilver française : intro (Lugia), écran titre, début de nouvelle partie,
en français et en anglais ; autotest et CI sur Windows, Linux et macOS.

### Documentation

- [docs/pc_port/README_PC.md](docs/pc_port/README_PC.md) : installation, commandes, options
- [docs/pc_port/MODDING.md](docs/pc_port/MODDING.md) : modifier le jeu
- [docs/pc_port/LAUNCHER.md](docs/pc_port/LAUNCHER.md) : réglages pilotables par un lanceur
- [PORTING_ROADMAP.md](PORTING_ROADMAP.md) : journal du portage
- [INSTALL.md](INSTALL.md) : construire les ROM DS (`pokesoulsilver.us.nds`
  `sha1: f8dc38ea20c17541a43b58c5e6d18c1732c7e582`)

### Mentions légales

Pokémon est une marque de Nintendo, Creatures Inc. et Game Freak. Ce dépôt ne
contient pas de ROM ; le jeu lit la ROM que *vous* possédez. Ne distribuez pas
de ROM ni les fichiers qui en sont extraits.

## English

Pokémon SoulSilver running **natively on PC** (Windows, Linux, macOS Intel),
**without an emulator**: the game code is compiled for the PC and the DS
hardware is reproduced.

- **Your ROM is required**: put the ROM of *your* SoulSilver cartridge next to
  `pokesoulsilver.exe`; its files are extracted on first run to `pc_data_ss/`.
- **Language**: your ROM's, or English (text built from the game's source):
  `config.ini` → `[options] language = auto | fr | en`.
- Kept apart from HeartGold (`save_ss.bin`, `mods_ss/`, `pc_data_ss/`).

Build: see the commands above. HeartGold:
[Pokemon-PC/pokeheartgold-pc](https://github.com/Pokemon-PC/pokeheartgold-pc).

**Fan project, not affiliated with Nintendo, Game Freak, The Pokémon Company or
pret.** This is a fork of pret's pokeheartgold; the original tooling is theirs.
pret doesn't accept AI-generated code and doesn't want to be associated with
this: don't send anything from here upstream or bother them about it. Do not
distribute ROMs or files extracted from them.
