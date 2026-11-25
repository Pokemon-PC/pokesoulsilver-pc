# Portage PC natif — feuille de route

Objectif : exécuter le code décompilé de Pokémon HeartGold/SoulSilver nativement
(Windows `.exe`, Linux, macOS — x86_64) via **CMake + SDL2 + OpenGL**, sans
émulateur, tout en conservant intacte la compilation de la ROM DS (`make`).

| Jalon | Contenu | État |
|---|---|---|
| **M1** | Audit, CMake, squelette HAL `src/platform_pc/`, fenêtre SDL2/GL, NitroFS sur `files/` | ✅ terminé |
| **M2** | En-têtes Nitro compilables sur PC, carte mémoire DS native, adaptateurs `OS_/FS_/PAD_/TP_/CARD_/RTC_` | ✅ terminé |
| **M3** | Tout le jeu compilé pour l'hôte (Clang), données asm → C, stubs hors ligne : **le jeu s'exécute** (intro → écran titre, écran noir) | ✅ terminé (limites listées) |
| M4 | Moteur 2D logiciel (BG0-3, OAM, fenêtres, blending, mosaïque, luminosité) → écran titre | ⏳ suivant |
| M5 | Backend 3D OpenGL (FIFO de géométrie G3) → overworld, combats | — |
| M6 | Audio : séquenceur SSEQ/SBNK/SWAR/STRM mixé par SDL | — |
| M7 | Sauvegarde, RTC, horloge, finitions, packaging | — |

---

## 1. Audit du dépôt

### 1.1 Arborescence (état au jalon M1)

| Dossier | Contenu | Conséquence pour le portage |
|---|---|---|
| `src/` | **1068 fichiers `.c`, 0 `.s`** — le jeu est 100 % décompilé en C | Compilable pour l'hôte après abstraction matérielle |
| `include/` | En-têtes du jeu, dont `filesystem_files_def.h` (table `sNarcFileList` des chemins NitroFS) | Inchangé |
| `asm/` | 80 fichiers `.s` **de données uniquement** (`.rodata`/`.data` : tables BG, couleurs…, aucun code) | À convertir en C (script) au jalon M3 — contiennent des pointeurs `.word sym` |
| `lib/NitroSDK` | 190 `.c`, 3 `.s` (crt0, contexte CP) — SDK Nintendo décompilé | Recompilé tel quel pour fx/mi/math/std ; remplacé pour os/gx-regs/card/pad/tp/snd/spi |
| `lib/NitroSystem` | 94 `.c` — NNS G2d/G3d/Gfd/Fnd/Snd | **Recompilé tel quel** (logique pure qui écrit dans VRAM/registres/FIFO) |
| `lib/MSL_C` | Runtime Metrowerks (93 `.c`, 28 `.s`) | Remplacé par la libc de l'hôte |
| `lib/NitroDWC`, `NitroWiFi`, `libCPS`, `libVCT`, `dsprot` | Wi-Fi Connection, sockets, chat vocal, anti-piratage | **Stubs hors-ligne** (le service WFC n'existe plus) |
| `files/` | Sources du système de fichiers ROM (png, json, gmm, NCGR/NCLR/NSCR, narc pré-construits…) | `make filesystem` produit les fichiers ROM **en place** : chemin ROM `X` ⇔ `files/X` |
| `tools/` | Outils hôtes (nitrogfx, narccomp, msgenc, …) déjà en CMake | Réutilisables pour reconstruire les données |
| `main.lsf` | 129 overlays | Sur PC : tout est lié statiquement, `FS_LoadOverlay` → réinit `.data/.bss` |

### 1.2 Dépendances matérielles / NitroSDK

Inventaire exact et reproductible : **[docs/PORTING_API_AUDIT.md](docs/PORTING_API_AUDIT.md)**
(généré par `python tools/pc_port/audit_nitro_api.py`, symbole par symbole, avec
nombre d'appels et de fichiers). Synthèse (`src/` + `include/`, 1932 fichiers) :

| Groupe | Symboles | Appels | Fichiers | Remplacement PC |
|---|---:|---:|---:|---|
| `OS_*` noyau, threads, IRQ, tick, tas, reset | 69 | 538 | 145 | `os_pc.c` |
| `GX_*`/`GXS_*` contrôle affichage, banques VRAM | 112 | 1077 | 212 | registres virtuels + `gfx_pc.c` |
| `G2_*`/`G2S_*` BG/OBJ/fenêtres/blend | 88 | 890 | 150 | registres virtuels (inline Nitro conservés) |
| `G3_*`/`G3X_*` géométrie 3D | 34 | 342 | 49 | décodeur FIFO → OpenGL |
| `NNS_G2d*` cellules, OAM, polices | 82 | 369 | 123 | NitroSystem recompilé |
| `NNS_G3d*` modèles NSBMD, anims NSBCA/NSBTA/NSBTP | 80 | 615 | 77 | NitroSystem recompilé + backend GL |
| `NNS_Gfd*` gestion VRAM | 29 | 133 | 39 | NitroSystem recompilé |
| `NNS_Fnd*` tas / listes | 12 | 49 | 10 | NitroSystem recompilé |
| `NNS_Snd*` lecteur SDAT | 67 | 103 | 10 | NitroSystem recompilé + `SND_*` → `audio_pc.c` |
| `FS_*` NitroFS + overlays | 31 | 640 | 90 | `fs_pc.c` |
| `PAD_*` boutons | 2 | 26 | 10 | `input_pc.c` → `reg_PAD_KEYINPUT` virtuel |
| `TP_*` écran tactile | 11 | 35 | 4 | `input_pc.c` (souris) |
| `CARD_*` ROM / sauvegarde flash | 21 | 40 | 7 | `save_pc.c` (`save.bin`, 512 Kio) |
| `CTRDG_*` slot GBA (Pal Park) | 15 | 53 | 7 | stub « pas de cartouche » |
| `RTC_*` horloge temps réel | 9 | 50 | 25 | `os_pc.c` (horloge système) |
| `PM_*` alimentation, rétroéclairage | 9 | 24 | 5 | `os_pc.c` (no-op / état) |
| `MI_*` copie/fill/DMA/décompression | 21 | 1464 | 389 | C portable (DMA = memcpy synchrone) |
| `DC_*`/`IC_*` caches | 4 | 186 | 98 | no-op |
| `CP_*` diviseur/racine matériels | 8 | 18 | 1 | C portable |
| `FX_/MTX_/VEC_/MATH_` virgule fixe | 69 | 1021 | 133 | NitroSDK `fx` recompilé |
| `WM_/WVR_/MB_` sans-fil local | 55 | 153 | 12 | stub (aucun partenaire) |
| `DWC_/SOC_/WCM_/CPS_` Wi-Fi Connection | 119 | 596 | 80 | stub hors-ligne |
| `reg_*` accès registres directs | 48 | 485 | 85 | **fichier de registres virtuel** |
| `HW_*` carte mémoire (VRAM, OAM, palettes) | 36 | 202 | 32 | **mémoire virtuelle** |
| Adresses brutes `*(vu32 *)0x04000xxx` | 3 | 3 | 2 | réécrire en `reg_*` |

Registres les plus utilisés : `reg_G2_BLDALPHA` (44), `reg_G2_BLDCNT` (37),
`reg_G3X_DISP3DCNT` (36), `reg_GX_POWCNT` (35), `reg_GX_DISPCNT` (35),
fenêtres `WININ/WINOUT/WIN0H/WIN0V`, `reg_GXS_DB_DISPCNT`, `reg_GX_VCOUNT`,
`reg_GX_MASTER_BRIGHT`, matrices `reg_G3_MTX_*`.

Adresses brutes : `src/application/spin_trade/spin_trade_group_app.c:436`,
`src/pokemon_summary_scene.c:48` (`0x04000540` = `SWAP_BUFFERS`),
`src/wireless_status_icon.c:197`.

### 1.3 Autres risques de portabilité

| Risque | Ampleur | Stratégie |
|---|---|---|
| Pointeur stocké dans un `u32`/`s32` (cassé en 64 bits) | ~392 occurrences, 129 fichiers (heuristique) | `uintptr_t` au cas par cas ; `-Wpointer-to-int-cast` en erreur pour les repérer ; build `-m32` possible comme filet de sécurité |
| Structures de fichiers (NSBMD, NCER, SDAT…) contenant des **pointeurs « dépaquetés » en place** (`NNS_G2dGetUnpacked*`, offsets convertis en pointeurs 32 bits) | ~123 sites | Variante 64 bits des fonctions *Unpack* : table de pointeurs annexe ou résolution d'offset à l'usage |
| Pragmas Metrowerks (`force_active`, `opt_*`) | 228 / 80 fichiers | Ignorés par GCC/Clang/MSVC (`-Wno-unknown-pragmas`) |
| `asm void` (ARM) | 0 dans `src/`, 2 dans NitroSDK (`CP_SaveContext`, `_start`) | Remplacés par du C |
| Overlays (129) — même plage d'adresses sur DS | 178 appels de chargement | Lien statique ; au chargement d'un overlay, restaurer ses `.data` et remettre ses `.bss` à zéro (sections nommées par overlay) |
| Threads Nitro (CARD, CTRDG, Pokéwalker IR, socket GTS, setup Wi-Fi) + IRQ VBlank/HBlank | ~8 threads | Ordonnanceur coopératif (fibres/`ucontext`/SDL threads + verrou global) ; VBlank appelé par la boucle principale ; HBlank émulé par ligne dans le moteur 2D |
| Endianness | DS = little-endian | x86_64/ARM64 hôtes little-endian : aucun travail |
| `wchar_t`, `size_t`, `long` | MWCC : 32 bits | Les types Nitro (`u32`, `s32`…) restent à taille fixe ; vérifier les `long` |

---

## 2. Architecture retenue

**Approche hybride « matériel virtuel + HLE ciblé »** plutôt qu'une réécriture
de toutes les API :

1. **Garder le code Nitro qui est de la logique pure** : `NitroSystem` (G2d,
   G3d, Gfd, Fnd, Snd) et une grande partie de `NitroSDK` (fx, mi, math, std)
   sont recompilés tels quels. Ils écrivent dans la VRAM, l'OAM, les palettes,
   les registres et la FIFO de géométrie — comme sur DS.
2. **Reproduire la carte mémoire DS à ses vraies adresses** (`hw_pc.c`, M2) :
   le processus 64 bits réserve 0x01FF0000-0x03010000 (ITCM, RAM principale,
   DTCM, zone système, WRAM), 0x04000000 (registres I/O), 0x05000000
   (palettes), 0x06000000 (VRAM) et 0x07000000 (OAM). `reg_GX_DISPCNT`,
   `HW_BG_VRAM`, `*(vu32 *)0x04000000`… fonctionnent **sans aucune
   modification**, et tous ces pointeurs tiennent dans un `u32`.
   L'exécutable est en plus lié à une base basse fixe (0x10000000, sans ASLR) :
   code, variables globales, pile et tas de l'hôte sont aussi sous 4 Gio, ce
   qui neutralise la plupart des ~392 conversions pointeur → `u32` du jeu.
   Restent à traiter au cas par cas les **structures de fichiers** contenant
   des pointeurs (NNS « unpack »), dont la taille change en 64 bits.
3. **Rendu** : à chaque VBlank, le moteur 2D logiciel lit cet état virtuel et
   produit deux images 256×192 ; les commandes G3 interceptées sont rejouées
   en OpenGL dans un FBO composé comme BG0 du moteur A.
4. **HLE là où le matériel n'a pas de sens sur PC** : NitroFS (fichiers hôtes),
   CARD (fichier `save.bin`), PAD/TP (SDL), OS (threads/IRQ/tick), SND (mixeur).

Avantage : le code du jeu reste quasiment intact ⇒ les mods (et les futures
mises à jour de la décompilation) s'appliquent directement.

### Organisation des fichiers

```
CMakeLists.txt              PHG_TARGET=PC (défaut) | DS_IDE
cmake/PhgSDL2.cmake         SDL2 installé, sinon téléchargé et compilé (statique)
cmake/ds_ide.cmake          ancien projet CMake ARM9 (indexation IDE), inchangé
src/platform_pc/
  platform_pc.h             interface HAL hôte (aucun en-tête Nitro)
  os_pc.c                   tick, VBlank 59,8261 Hz, allocation alignée
  fs_pc.c                   NitroFS → <data>/chemin, lecteur NARC, vérif. manifeste
  input_pc.c                clavier / manette → masque PAD, souris → TP
  save_pc.c                 flash 512 Kio ↔ save.bin (écriture atomique)
  gfx_pc.c                  fenêtre SDL2, OpenGL 3.3 core, 2 écrans, layouts
  audio_pc.c                périphérique SDL 32 768 Hz stéréo, callback mixeur
  log_pc.c                  journalisation / erreurs fatales
  main_pc.c                 harnais M1 (remplacé par NitroMain() au M3)
  hw_pc.c / hw_pc.h          carte mémoire DS aux adresses natives (M2)
  nitro/                     API Nitro sur la HAL, compilée avec les vrais en-têtes du SDK (M2)
    nitro_pc.c               initialisation + mise à jour par image (VBlank)
    os_nitro_pc.c            arènes, interruptions, table IRQ, OS_WaitIrq, tick, infos console
    fs_nitro_pc.c            FS_* (sémantique NitroFS : un FSFile ne possède aucune ressource)
    input_nitro_pc.c         TP_* (échantillonnage automatique), PAD via registres
    card_nitro_pc.c          CARD_* sauvegarde → save_pc ; SPI Pokéwalker = absent
    rtc_nitro_pc.c           RTC_* depuis l'heure locale
    mi_nitro_pc.c            MI_Cpu* en C (remplace l'assembleur ARM de mi_memory.c)
    nnsys_data_pc.c          globales NitroSystem issues de lib/asm/nnsys_data.s
  nitro_compat/              en-tête préinclus pour le code jeu/SDK sur PC
  tests/m2_selftest.c        test d'acceptation M2 (CTest)
cmake/PhgGame.cmake          options de compilation du code jeu, fx_const.h généré, lien à base basse
tools/pc_port/audit_nitro_api.py   génère docs/PORTING_API_AUDIT.md
```

La cible DS n'est pas modifiée :
- `common.mk` exclut `src/platform_pc/` de la collecte récursive des sources
  (`rwildcard`) — **correction** : jusqu'au jalon M2 ce n'était pas le cas, et
  `make` aurait tenté de compiler les fichiers PC avec MWCC. Vérifié par
  évaluation de la règle : seuls les 18 fichiers de `src/platform_pc/` sont
  retirés. La compilation complète de la ROM n'a pas pu être relancée ici
  (pas de MWCC/Wine sur cette machine).
- Les changements d'en-têtes du SDK sont tous gardés par `#ifdef PLATFORM_PC`
  (`nitro/types.h`, `nitro/hw/ARM9/mmap.h`, `string.h`/`stddef.h`/`stdarg.h`
  de MSL), sauf `g2d_CellTransferManager.h` où un `int` implicite devient
  explicite (même code généré par MWCC).
- L'ancien projet CMake ARM9 (`DS_IDE`) exclut aussi `src/platform_pc/`.

---

## 3. Compiler et lancer

### Le jeu complet (depuis M3)

Prérequis supplémentaires : **Clang** (sous Windows : `winget install
MartinStorsjo.LLVM-MinGW.UCRT`), Python 3, et les données de **votre propre
cartouche** extraites dans `pc_data/fr` :

```bash
python tools/pc_port/nitrofs.py extract <votre-rom.nds> pc_data/fr
cmake -S . -B build-clang -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build-clang
build-clang/pokeheartgold.exe            # options : --lang fr|en, --save FICHIER, --scale N
# tests : PHG_EXIT_AFTER=N (quitter après N images), PHG_TEST_RESET_AT=N[,M]
# (combo L+R+Start+Select à l'image N), PHG_WATCH=0xADRESSE (point d'arrêt
# matériel en écriture, pour traquer une corruption mémoire)
```

La génération des en-têtes (messages, index d'archives, overlays) est faite par
CMake. Les données converties et les stubs sont versionnés dans
`src/platform_pc/generated/` (libclang n'est nécessaire que pour les
régénérer). En cas de plantage, le journal contient une pile d'appels :
`python tools/pc_port/symbolize_crash.py journal.txt`.

### Harnais du jalon M1

Prérequis : CMake ≥ 3.16, un compilateur C11 (GCC/MinGW-w64, Clang ou MSVC),
OpenGL 3.3. SDL2 est utilisé s'il est installé, sinon téléchargé
automatiquement (`-DPHG_FETCH_SDL2=OFF` pour l'interdire).

Sous Windows, le plus simple : `winget install Kitware.CMake BrechtSanders.WinLibs.POSIX.UCRT`
(GCC + Ninja). Mettre le CMake **de Kitware** en premier dans le `PATH` : le
`cmake.exe` fourni par WinLibs n'utilise pas le magasin de certificats Windows
et échoue à télécharger SDL2 (erreur SSL 60). La première configuration de
SDL2 est longue (~10-15 min sous Windows, des centaines de tests).

```bash
cmake -S . -B build-pc -G Ninja          # ou "MinGW Makefiles", "Visual Studio 17 2022"…
cmake --build build-pc
./build-pc/pokeheartgold_pc              # Windows : build-pc\pokeheartgold_pc.exe
```

Options du binaire :

| Option | Effet |
|---|---|
| `--data DIR` | racine NitroFS (défaut : `files/` du dépôt, ou `$PHG_DATA_DIR`) |
| `--fs-check [--verbose]` | vérifie la présence de chaque fichier de la ROM (manifeste extrait de `filesystem.mk`) |
| `--save FILE` | fichier de sauvegarde (défaut `save.bin`, compatible `.sav` brut 512 Kio) |
| `--scale N`, `--side-by-side` | taille / disposition initiales |
| `--frames N` | quitte après N images (tests automatisés) |
| `--dump FILE.bmp` | enregistre les deux écrans (256×384) en quittant |

Audio : si le pilote SDL par défaut échoue (vu ici avec WASAPI :
« Descripteur non valide »), les autres pilotes sont essayés automatiquement
(DirectSound, WinMM…). `SDL_AUDIODRIVER=<nom>` force un pilote.

Touches : flèches = croix, X = A, Z = B, S = X, A = Y, Q/W = L/R,
Entrée = Start, Maj droite/Retour arrière = Select, F9 = disposition,
F10 = fermer le couvercle, F11 = plein écran, Échap = quitter. Souris sur
l'écran du bas = stylet. Manettes : disposition positionnelle Nintendo.

Le harnais M1 décode de vraies ressources du jeu via `fs_pc` :
`data/ground0.NSCR/NCGR/NCLR` (écran du haut, rendu comme un BG texte DS) et
`data/cell0.NCGR` (planche de tuiles en bas), lit `data/arealight.narc` et
affiche l'état des boutons et du stylet.

**Données** : beaucoup de fichiers de la ROM sont générés par `make filesystem`
(NARC construits depuis png/json/gmm). Tant qu'ils n'existent pas,
`--fs-check` les liste comme manquants. Alternative : extraire le système de
fichiers d'une ROM que vous possédez (`ndstool -x rom.nds -d data/`) et lancer
avec `--data data`.

---

## 4. Prochaines étapes détaillées

### M2 — Couche de compatibilité Nitro ✅
- [x] Carte mémoire DS aux adresses natives (`hw_pc.c`) ; sous Windows,
      réservée dans un processus enfant suspendu avant le chargement des DLL
      (`user32`/`gdi32` occupent sinon la zone) — `PHG_NO_RELAUNCH=1` désactive.
- [x] En-têtes Nitro compilés par GCC : `u32`/`s32` forcés à 32 bits (LP64),
      DTCM à 0x027E0000, libc MSL redirigée vers celle de l'hôte, `fx_const.h`
      généré par l'outil du dépôt (`tools/gen_fx_consts`).
- [x] Adaptateurs `OS_*` (arènes, IRQ, VBlank, tick), `FS_*`, `PAD_*`, `TP_*`,
      `CARD_*`, `RTC_*`, `MI_Cpu*`.
- [x] Recompilés tels quels : NitroSystem `fnd_*` (tas étendus, frame, unit,
      allocateurs), NitroSDK `os_alloc.c`, `rtc_convert_*.c` ;
      jeu : `src/heap.c`, `src/filesystem.c`.
- [x] Test `phg_m2_selftest` (CTest) : **825 vérifications OK** — tas du jeu
      dans la RAM DS, 199/267 NARC lues par `filesystem.c` et identiques octet
      pour octet au lecteur indépendant, PAD/couvercle, sauvegarde
      écriture+vérification, RTC, IRQ VBlank à 59,83 Hz, tactile.
- [ ] Reportés à M3 (nécessaires quand tout le jeu sera lié) : threads,
      mutex, alarmes, files de messages, `OS_ResetSystem` (réinitialisation
      douce), chargement d'overlays avec réinitialisation `.data/.bss`.

Règle pour la suite : **un fichier compilé avec les chemins du jeu n'inclut
jamais SDL** (le `math.h` du SDK masque celui de l'hôte) ; il passe par
`platform_pc.h`.

### M3 — Tout le code en hôte

Décision (utilisateur, 2026-11-20) : rester en x86_64 et compiler avec
**Clang** (llvm-mingw 23 sous Windows) pour annoter `__ptr32 __uptr` les 25
champs pointeurs des formats 2D NitroSystem (`lib/include/nnsys/g2d/fmt/*.h`),
qui gardent ainsi leur taille DS sans toucher au code du jeu. Vérifié :
structure `{u16,u16,ptr32,u32}` = 12 octets, conversion vers pointeur 64 bits
correcte. Pièges constatés :
- les en-têtes MinGW (`_mingw.h`) font `#define __ptr32` (vide) : il faut
  `#undef __ptr32 __ptr64 __uptr __sptr` après les en-têtes système, sinon
  l'annotation disparaît silencieusement ;
- un pointeur `__ptr32` ne peut pas figurer dans un initialiseur statique
  (`addrspacecast` refusé) : affectation à l'exécution uniquement ;
- `__ptr32` n'existe que sur x86 : pas de Mac ARM avec cette option.

Bilan de compilation initial (`tools/pc_port/host_compile_check.py`,
[docs/PORTING_COMPILE_STATUS.md](docs/PORTING_COMPILE_STATUS.md)) :
1001/1543 fichiers compilent sans modification ; 332 fichiers du jeu attendent
les en-têtes générés par la chaîne de données (`msgdata/msg.naix`,
`msg_XXXX.h`, `*.naix`). Cette chaîne exige MSYS2/WSL et `mwasmarm` (Metrowerks,
non redistribuable) pour les scripts et tables.

**Données — décision (utilisateur) : jeu en français depuis sa propre
cartouche, anglais prévu ensuite.**
- La ROM HeartGold **FR** (`IPKF` ; ex `roms/gen4_nds/rom_gen4.nds`, désormais
  `D:\LUC\pokemonall\roms\Pokemon - Version Or HeartGold (France).nds`, dossier
  commun de toutes les ROM, lu par les ports via `[options] rom_dir` de
  `config.ini` ou `--rom-dir`, ROM choisie par code jeu). Son système de fichiers a les
  mêmes 384 chemins que le dépôt : 268 fichiers identiques, 44 localisés,
  et il fournit les 72 manquants. Extraction : `python tools/pc_port/nitrofs.py
  extract <rom> pc_data/fr` (dossier ignoré par git, à ne jamais partager).
- Couches de fichiers à l'exécution (`--lang fr|en`, défaut `fr`) :
  `mods/<lang>` → `mods` → `pc_data/<lang>` → `files`. Un mod ne contient
  que les fichiers qu'il remplace. En FR : 384/384 fichiers, 267/267 NARC lues
  par `filesystem.c` du jeu (autotest 1097/1097).
- La **langue du code** suit aussi `--lang` : sur PC, `GAME_LANGUAGE` /
  `gGameLanguage` est une valeur choisie au lancement (`include/config.h`,
  `pm_version.h`, `pokedex_language.h`, `overlay_25.c`, sous `PLATFORM_PC`),
  et l'en-tête de ROM simulé devient IPKF, IPKE... Un seul exécutable sert
  toutes les langues : langue des Pokémon capturés, Pokédex, Dico, etc.
  cohérents avec les données. (Le build DS reste `ENGLISH` uniquement.)
- En-têtes générés **sans Metrowerks**, depuis les sources du dépôt :
  - `tools/pc_port/gen_naix.py` : les 37 `.naix` inclus par le jeu, en
    reproduisant `nitroarc` (`.narcorder`, `.narcignore`, règles des `.mk`,
    substitutions Make, `--exclude`, liens symboliques), `o2narc -n/-N`
    (via `jsonproc`) et `csv2bin --naix`. **Tous validés** contre le nombre
    de membres des NARC de la ROM (et la taille membre à membre pour
    `zukan_data`, dont les listes de tri alphabétique diffèrent en FR) ;
  - `tools/pc_port/gen_msg.py` : les 829 textes via `msgenc` du dépôt
    → `msg_XXXX.h` + `.bin` anglais (base de la future version EN).
  - Outils hôtes compilés : `msgenc`, `jsonproc`, `csv2bin` (C++ du dépôt).
- Version anglaise (à faire) : `pc_data/en` = textes `.bin` anglais
  ré-empaquetés + les ~44 fichiers localisés du dépôt ; vérifier quels
  fichiers manquants dépendent de la langue (ex. `zukan_data`).

Bilan après génération (même outil) : **1308/1543** fichiers compilent, dont
**870/1012** pour le jeu ; plus aucun en-tête manquant. Restent : asserts de
taille 64 bits (`typedef char X[cond?1:-1]`, ~91 fichiers dont 62 via
l'itérateur DWC), rembourrages calculés (`wifi_setup_ui.h`, 25),
`FS_OVERLAY_ID` (19), quelques conflits de déclarations.

**Résultat M3 : le jeu complet se compile, se lie et s'exécute.** `build-clang/pokeheartgold.exe`
lance le vrai `NitroMain()` : initialisation système, sauvegarde, puis toute la
cinématique d'introduction (scènes 2D, carte 3D, particules), puis l'écran titre
(overlay `intro_title` rechargé vers 68 s). Testé 100 s / ~5700 images sans
plantage, sans blocage ni échec d'allocation. **Écran noir** : le rendu est M4.

Ce qui a été fait :
- [x] Sources du jeu prises dans `main.lsf` (les objets réellement liés dans la
      ROM : `src/text_0205B4EC.c`, doublon non utilisé, est ainsi exclu) —
      `cmake/PhgGameSources.cmake` ; bibliothèques : NitroSystem entier, NitroSDK
      fx/gx/math/mi/std/dgt/rtc_convert/os_alloc, lib/src (particules SPL).
- [x] Toutes les données assembleur converties en C par
      `tools/pc_port/asm_data_to_c.py` (libclang) : 3814 objets sans pointeur en
      octets identiques, 648 objets typés réécrits avec des types reconstruits
      (mêmes champs, disposition PC native), types propagés le long des
      pointeurs, **alias** (étiquettes au milieu d'un objet, 71 cas) recréés au
      bon décalage PC par `.set`. Sortie versionnée dans
      `src/platform_pc/generated/asm_data/` (voir son README).
- [x] 394 stubs hors ligne générés (`tools/pc_port/gen_stubs.py`) pour DWC/Wi-Fi,
      sans-fil local (WM/MB/WVR), Place Wi-Fi, CTRDG (slot GBA), micro, PM,
      anti-piratage (répond « cartouche authentique »), pilote son ARM7 (M6).
- [x] Routines du SDK en assembleur ARM réécrites en C d'après le code d'origine
      (`sdk_asm_pc.c`) : matrices fx, LZ77/LZ11, MATH_QSort (même ordre), SHA-1,
      FIFO GX ; DMA émulée par son mot de contrôle (`mi_dma_pc.c`) ; diviseur et
      racine carrée calculés à la lecture (`hw_pc.c`, macros de `io_reg.h`).
- [x] OS complété (`os_extra_pc.c`) : alarmes sur VBlank, files de messages,
      mutex, LockID, printf ; FS : `FS_ConvertPathToFileID`/`OpenFileFast`
      (archive sonore), archive « rom » et en-tête ROM laissés par le chargeur DS.
- [x] Corrections 64 bits dans le code du jeu/SDK (toutes sous `PLATFORM_PC`) :
      tailles de blocs VRAM 3D codées en dur (`gfd_*`), tas ×4 (`heap.c`, arène
      principale étendue à 0x027C0000), tampon de particules ×3, quelques
      constructions propres à MWCC (affectation de tableaux, `int` implicite…).
- [x] Données ROM FR vs code US : certains membres sont compressés dans la ROM
      française et pas dans la version US ; le chargeur détecte la compression
      d'après les octets (`pc_lz_autodetect`, journalisé).
- [x] Outils de diagnostic : gestionnaire de plantage avec pile d'appels,
      chien de garde (pile du jeu si aucune image pendant 5 s), pile sur
      `GF_ASSERT` et échec d'allocation ; adresses stables (base fixe) →
      `python tools/pc_port/symbolize_crash.py <log>`.
- [x] Le processus relancé (réservation mémoire) est lié au parent par un Job
      Windows : fermer la console ferme le jeu.

- [x] **Variables des overlays remises à neuf à chaque chargement**, comme sur
      DS (rechargement de `.data`, `.bss` à zéro). `tools/pc_port/gen_overlay_sections.py`
      lit `main.lsf` à la configuration CMake : chaque source d'un overlay est
      compilée avec `-fno-zero-initialized-in-bss` et
      `#pragma clang section data=".data$ovlNNN$1"` (en-tête forcé), entre deux
      marqueurs `.data$ovlNNN$0` / `$2` (l'éditeur de liens trie ces sections
      par nom). `FS_Init` photographie les 129 plages (~366 Kio) avant
      `NitroMain` ; `FS_LoadOverlay*` les restaurent. Clang uniquement.
- [x] `FS_LoadOverlayInfo` donne une fausse plage RAM distincte par overlay :
      avant, toutes valaient 0 et le test de chevauchement de `poke_overlay.c`
      refusait en silence tout 2ᵉ overlay chargé en même temps.

- [x] **Réinitialisation douce** (`OS_ResetSystem` : L+R+Start+Select, fin du
      jeu, Tour de Combat, effacement de la sauvegarde…) : même mécanisme de
      sections pour tout le reste du jeu (`.data$main$1`, ~850 Kio : programme
      principal, données asm, stubs, partie « jeu » de la couche Nitro).
      `pc_nitro_soft_reset` restaure ces variables et tous les overlays,
      remet la mémoire DS à zéro (sauf le paramètre de reset lu par `main.c`),
      refait l'amorçage Nitro et relance `NitroMain` via `longjmp`, dans la
      même fenêtre ; la sauvegarde est vidée sur disque avant.
- [x] Corrigé : `NNS_G3dAnmObjCalcSizeRequired` (NitroSystem) codait en dur
      la taille DS (0x1C) de l'en-tête d'objet d'animation 3D ; avec des
      pointeurs 64 bits, `mapData[]` débordait sur le bloc suivant du tas.
      Bug latent, révélé par le changement de disposition des variables
      (plantage dans la scène 3 de l'intro) et trouvé avec `PHG_WATCH`.

Reste à faire (limites connues) :
- [ ] D'autres tailles DS codées en dur peuvent exister (structures
      contenant des pointeurs) : à surveiller, `PHG_WATCH` aide à les trouver.
- [ ] Les données d'overlays fournies par la couche PC (`misc_data_pc.c`,
      `online_data_pc.c`, stubs) ne sont pas remises à zéro (modules en ligne / IR).
- [ ] Threads secondaires (Pokéwalker IR, GTS, Wi-Fi) non exécutés.
- [ ] 24 objets de données ignorés (sans type C et mélangeant entiers/pointeurs)
      et 3 alias non placés : tous dans des modules en ligne pour l'instant
      (voir `asm_data_report.txt`).
- [ ] Autres différences FR/US possibles (compression ailleurs que dans
      `gf_gfx_loader.c`, scripts) : à surveiller pendant les tests de M4.
- [ ] Le build GCC (`build-pc`) ne sert plus qu'aux jalons M1/M2 : le jeu exige
      Clang (`__ptr32`).

### M4 — Moteur 2D logiciel (`nitro/ppu_pc.c`) ✅
- [x] **Banques VRAM A-I** (`nitro/vram_pc.c`) : chaque fonction de
      `lib/NitroSDK/src/gx/gx_vramcnt.c` qui écrit VRAMCNT appelle
      `pc_vram_sync()` (sous `PLATFORM_PC`). Une banque vit à son adresse CPU
      quand elle est mappée (BG/OBJ A/B, LCDC), sinon dans un stockage interne ;
      au remappage, le contenu est déplacé. Palettes étendues, textures et
      palettes de textures (pour M5) sont lues depuis ce stockage.
- [x] DISPCNT modes 0-5, BG texte (4 tailles, 4/8 bpp, retournements, palettes
      étendues), affines, étendus (cartes 16 bits, bitmaps 256 couleurs et
      couleurs directes), bouclage, mosaïque BG.
- [x] OAM : 128 sprites normal/affine/double taille, 4/8 bpp, mapping 1D/2D et
      frontières, sprites bitmap, palettes étendues, semi-transparence, fenêtre
      OBJ, priorités.
- [x] Fenêtres 0/1/OBJ, BLDCNT/BLDALPHA/BLDY, MASTER_BRIGHT, modes
      d'affichage 0/1/2, blanc forcé, alimentation des moteurs, échange des
      écrans (`POWCNT1` bit 15).
- [x] Rendu ligne par ligne : DMA H-Blank et interruption H-Blank rejouées entre
      les lignes (effets de trame), points de référence affines rechargés.
- [x] Moment du rendu : après le travail V-Blank du jeu (`src/main.c` appelle
      `pc_nitro_frame_ready()`, sinon au V-Blank suivant), comme le balayage
      DS des lignes 0-191. 60 images/s tenues.
- [x] Débogage : `PHG_DUMP_FRAMES=N,M` (captures PNG des deux écrans),
      `PHG_DUMP_VIDEO=1` (registres, palettes, OAM et VRAM bruts à côté).
- [x] Corrigé : `Sprite.animationData` (`include/sprite.h`) avait la taille
      DS (29 mots, déjà signalée « UB » par le décomp) ; avec des pointeurs
      64 bits, les données d'animation écrasaient `imageProxy` → sprites aux
      mauvaises tuiles. Taille calculée sur PC.
- [x] Vérifié : intro (soleil, Bourg Geon, Lyra, Silver, bêtes légendaires,
      Héricendre…) et **écran titre en français**, aussi après un reset.
- [x] **Du titre jusqu'au monde du jeu sans plantage** (menu principal,
      tutoriel, discours du professeur, choix du personnage, nom, arrivée dans
      la chambre). Corrigé en route, tous des cas « 64 bits » (sous `PLATFORM_PC`) :
  - menu principal : `gMainMenuUtilWork` et ses voisins (`…Sprites`,
    `…Renderer`, `…GiftSpriteWork`, `…VBlankWork`) forment un seul bloc sur DS,
    atteint aussi via `&gMainMenuUtilCryptoHeapId + 1` → un seul objet sur PC
    (le menu restait noir) ; idem `&gMigrateFromAgbIcons + 1` ;
  - `render_window_frame_icon.c` redéclarait `BlitPixelRect4Bit` avec des
    `u8` au lieu de `u16` : en x86-64 les bits hauts sont indéfinis → blit
    géant, tas corrompu (le **plantage signalé** après l'intro) ;
  - `Field3dObjectList_New` / `MapPropAnimationPoolList_AddPool` : tailles
    DS en dur (`* 0x5C`, `* 4`, `<< 2`) → `sizeof` ;
  - `MapObjectManager` : 0x10C octets réservés pour `MapObjectRender`, qui en
    fait 0x130 en 64 bits → zone agrandie + `_Static_assert` ;
  - scan complet (`find_ds_sizes.py`) : 5 cas, 3 trop petits corrigés
    (`font_sprite.c` 12 × n, `wifi_club.c` 0x3C ×2), 2 déjà assez grands ;
  - files de tâches V-Blank/V-Wait 32 → 256 : sur PC un chargement tient dans
    une seule image, les tâches s'accumulent avant le V-Blank suivant.
- [x] Outils de test : `PHG_TURBO=1` (pas de cadence 60 Hz : ~4× plus
      rapide), `PHG_TEST_KEYS=N:A+B,...`, `PHG_TEST_TOUCH=N:X:Y,...`,
      `PHG_BACKTRACE_AT=N`, et `tools/pc_port/find_ds_sizes.py` (libclang) qui
      repère les allocations à taille DS en dur.
- [x] Écran du bas dans le monde (panneau vert vide + bouton « AGIR ») :
      **pas un bug de rendu**. L'image correspond à la VRAM, et au début du jeu
      aucune icône du menu tactile n'est encore débloquée (Pokédex, Pokémon,
      Sac, Pokégear… arrivent avec l'histoire). À revérifier en jouant.
- [x] **Sauvegarde + « Continuer » vérifiés** : sauvegarde dans la chambre comme
      le menu (`Field_SaveGameNormal`, test `PHG_TEST_SAVE_AT=N`), puis
      relance → le héros réapparaît à la même case. Corrigé : avec une
      sauvegarde, le menu principal lançait la recherche de cadeaux sans fil
      (`WVR_StartUpAsync` → `OS_Terminate`) ; sur PC il n'y a pas de sans-fil
      local, la recherche est sautée (`main_menu.c`, sous `PLATFORM_PC`).
      Reste à vérifier : compatibilité avec une sauvegarde de vraie console.
- [x] Capture d'écran (`DISPCAPCNT`, `ppu_pc.c`) : source A (image du
      moteur A avant luminosité maîtresse, ou 3D seule), source B (banque
      VRAM), mélange EVA/EVB, 4 tailles, décalages lecture/écriture, bit 31
      remis à 0 en fin d'image. Sert aux transitions des combats d'arène
      (`encounter_effect_particles.c`), au flou de mouvement (Celebi,
      `overlay116_scene.c`) et aux cinématiques des légendaires. Vérifié par
      le selftest (`test_display_capture`) ; pas encore vu en jeu (scènes
      tardives).
- [ ] Reste : mode 6 (grand bitmap), mosaïque OBJ.

### M5 — 3D logiciel (`nitro/gpu3d_pc.c`) ✅
Rendu logiciel 256×192 (comme la console), composé comme BG0 du moteur A ;
un rendu OpenGL haute résolution pourra venir plus tard par-dessus.
- [x] **Capture des commandes** par trois chemins :
  - écritures dans les registres de commande : sur PC, chaque `reg_G3_*` /
    `reg_G3X_GXFIFO` passe par `pc_g3_port()` (`io_reg.h`, sous `PLATFORM_PC`).
    Le C ne permet pas d'intercepter une écriture : le port note le registre et
    lit la valeur au prochain accès (ou au V-Blank, ou à la lecture d'un
    registre d'état/résultat), l'ordre est préservé. 3 sites qui écrivaient
    via un pointeur (`g2d_softsprite.c`, `g3d_sbc.c`, `g2d_renderer_core.c`)
    passent par les macros ;
  - copies/DMA vers la FIFO (`MIi_CpuSend32`, `GX_SendFifo*`, `MI_Copy*B`,
    DMA) ;
  - display lists (`MI_SendGXCommand`).
- [x] Moteur géométrique : piles de matrices (projection, position/direction,
      texture), toutes les commandes de matrices, sommets (16/10 bits, XY/XZ/YZ,
      différentiel), couleurs, normales et éclairage (4 lumières, spéculaire,
      table de brillance), transformations de coordonnées de texture (modes
      1-3), triangles/quads/bandes, découpage homogène, plan lointain, viewport,
      BOX/POS/VEC_TEST, registres de résultat (matrice de clip, niveaux de pile).
- [x] Rastériseur : 7 formats de texture (dont 4×4 compressé), répétition et
      miroir, modulate/decal/toon/highlight, faces avant/arrière, test de
      profondeur Z/W (et « égal »), alpha test, polygones translucides (mélange,
      règle d'ID, mise à jour de profondeur), brouillard, couleur/profondeur
      d'effacement. Couche 3D mélangée au 2D avec son alpha par pixel.
- [x] **Corrigé** : le diviseur matériel émulé ne calculait qu'à la lecture des
      macros de résultat, or `FX_GetDivResultFx64c`/`FX_InvFx64c` lisent
      l'adresse brute après la boucle d'attente → résultats périmés (matrice de
      projection fausse, mais aussi tout calcul `FX_Inv`). Les lectures de
      `DIVCNT`/`SQRTCNT` déclenchent maintenant le calcul.
- [x] Vérifié : villes 3D de l'intro, Ho-Oh de l'écran titre, chambre du
      héros, déplacements du joueur (la caméra suit).
- [x] Polygones d'ombre (mode 3) : masque (ID 0) → stencil là où le test de
      profondeur échoue ; ombre (ID ≠ 0) seulement sur le stencil et sur un
      autre ID ; stencil vidé à chaque nouveau groupe masque. Selftest
      `test_shadow_polygons`. Aucun dans l'intro ni la chambre du héros.
- [ ] Reste : edge marking, anti-aliasing, polygones fil de fer (alpha 0),
      tri des translucides par Y, image de fond (rear-plane bitmap).

### M6 — Audio (`nitro/snd_pc.c`) ✅
Le pilote son de l'ARM7 est réécrit en C et tourne dans le fil audio SDL
(32 768 Hz) : l'horloge audio cadence la musique, les appels `SND_*` du jeu
prennent un verrou (`pc_audio_lock`). NitroSystem (`NNS_Snd*`) est inchangé.
- [x] API `SND_*` complète (53 fonctions utilisées par NitroSystem) : commandes
      exécutées aussitôt, état des lecteurs/canaux, infos pilote, canaux
      directs (wave out), verrouillage, alarmes (rappels exécutés au V-Blank).
- [x] Séquenceur SSEQ : 16 lecteurs, 32 pistes, tempo à 192 Hz, notes (tie,
      note-wait), appels/boucles/sauts, variables et préfixes aléatoire /
      variable / conditionnel, volumes, pan, transposition, pitch bend, ADSR,
      LFO, sweep, portamento.
- [x] Instruments SBNK (PCM, PSG, bruit, batteries, key split), ondes SWAR
      PCM8/PCM16/IMA-ADPCM avec boucles, enveloppe DS, priorités de canaux.
- [x] Corrigés en route :
  - table de fichiers de l'archive son (`NNSSndArcFileInfo.mem`) : pointeur
    lu depuis le fichier → `PC_PTR32` ;
  - `_0210D750` (table volume → décibels) : alias que le convertisseur ne sait
    pas placer → nom de la vraie table sur PC ;
  - un lecteur « préparé » doit déjà compter comme actif, sinon NitroSystem le
    libère avant le démarrage.
- [x] Vérifié : musique de l'intro (spectrogramme : accords et mélodie nets),
      jeu jusqu'à la chambre avec le son ; validé à l'écoute par l'utilisateur.
      `PHG_AUDIO_DUMP=fichier.raw`
      enregistre la sortie (s16 stéréo 32 768 Hz).
- [x] Option Mono du jeu (`SND_SetMasterPan`) ; hauteur des sons PSG/bruit
      (période de référence 8006 cycles = do4 à la note racine).
- [x] Capture son : **non nécessaire**, HGSS ne démarre jamais la
      réverbération (il ne fait que tester/arrêter la capture).
- [x] `PHG_MUTE=1` : pilote actif mais haut-parleurs muets (tests).
- [ ] Reste : comparaison fine avec la console (volumes, enveloppes).

### Parcours de jeu testé (début de l'aventure)
- [x] Chambre → maman (Sac, Carte, Sauver, Options débloqués) → Bourg
      Geon → labo d'Orme → choix du starter (appli 3D, écran tactile) →
      surnom → assistant (objets) → Célesta → retour chez maman (Pokématos)
      → Orme (numéro) → **Route 29 → combat sauvage complet** (Hoothoot :
      attaques, dégâts, attaque adverse, victoire, retour sur la route).
      Bas d'écran juste à chaque étape (icônes débloquées au fil du jeu).
- [x] 6 bugs corrigés en route :
  - sortie de maison : tas corrompu par les animations de décor
    (`map_prop_render.c`, en-tête `NNSG3dAnmObj` 0x1C en dur, même bug que
    `g3d_kernel.c`) ;
  - « Continuer » dans le labo : sauts relatifs des scripts lus en `u32`
    (saut arrière = +4 Gio en 64 bits) → macro `SCRIPT_REL` (`global.h`)
    sur les scripts de terrain, de la Tour de Combat et des animations de
    combat (sans effet sur la ROM DS) ;
  - Poké Ball du choix du starter : texture en miroir fausse pour les
    coordonnées négatives (division tronquée au lieu d'arrondi vers le bas,
    `gpu3d_pc.c`) — touchait toute la 3D ;
  - entrée en combat : `NNS_G2dInitCellTransferStateManager` écrivait le
    gestionnaire par mots de 32 bits (champs décalés en 64 bits) ;
  - pendant le combat : `cryStopTask` / `queuedBankInfo` du pilote son
    déclarés `u32` mais contenant des pointeurs (`sound.c`).
- [x] Outils de test ajoutés : `PHG_TEST_KEYS=N:TOUCHE*D` (maintien D
      images), `PHG_TEST_POS=N` (carte + case du joueur), `PHG_HEAPCHECK=1`
      (vérifie tous les tas à chaque image et allocation, nomme le bloc qui
      déborde et son allocateur). Les scripts d'entrées ignorent le clavier
      et la manette réels (la fenêtre peut prendre le focus). Sauvegardes de
      test enchaînées (`--save`) pour reprendre à chaque étape.
- [x] Suite jouée : Route 29 → Ville Griotte (visite guidée du vieil homme,
      Chaussures de Course) → Route 30 (Boîte à Noigrumes) → maison de
      M. Pokémon (Œuf Mystère, **Pokédex** du Prof. Chen) → **appel d'Orme
      sur le Pokématos** → retour à Ville Griotte → **combat contre le rival**
      (transition spéciale, IA du dresseur, victoire, niveau 6 avec tableau
      des stats).
- [x] Corrigé : l'IA des dresseurs plantait — `ov10_02220AAC` est un alias
      de `gTrainerAITable` que le convertisseur de données a émis comme un
      objet vide séparé (seul autre cas : `_0210D750`, déjà traité).
- [x] Outils : `PHG_TEST_MAP=N` (carte ASCII autour du joueur : murs, herbes,
      portes, rebords, PNJ ; différée tant que la carte n'est pas chargée),
      `PHG_TEST_NO_ENCOUNTERS=1` (pas de rencontres en marchant, pour les
      trajets scriptés). Un petit BFS hors dépôt calcule les trajets.
- [x] Suite jouée : retour au labo (policier, **nom du rival** au clavier,
      Œuf rendu à Orme) → **tutoriel de capture** de Célesta (combat de
      démonstration, Poké Balls) → **capture d'un Hoothoot** soi-même (Sac en
      combat, poche Balls, lancer, échec puis réussite, fiche Pokédex, surnom)
      → **Centre Pokémon** de Ville Griotte (soins, oui/non tactile).
      Vu en passant : répertoire du Pokématos depuis le terrain.
- [x] Corrigé : textes du Pokédex (catégorie, description) en anglais dans
      le jeu français — le code US passait la langue `2` en dur (langue
      native de la version US) ; remplacé par `GAME_LANGUAGE` (identique en
      DS US), ainsi que les tests « langue native » du Pokédex.
- [x] Outils : `PHG_TEST_SAVE_AT` attend que le joueur soit libre (pas de
      script en cours, carte chargée) avant d'enregistrer.
- [x] Suite jouée : Route 30 (couloir ouest, appel de maman sur le
      Pokématos) → Route 31 → maison de garde (scène de Célesta) →
      **Mauville (Violette)**. Menus du terrain ouverts depuis l'écran
      tactile : **Pokédex** (Vus/Pris), **Carte de Dresseur** (+ signature au
      stylet), **Pokémon** (équipe), **Sac**, **Options**, **Pokématos**.
- [x] Corrigé : le Pokédex plantait à l'ouverture — tableaux de pointeurs
      de sprites parcourus avec un pas de 4 octets en dur (`pokedex_sprites.c`,
      aussi `battle_link_comm.c`) ; remplacé par `sizeof(pointeur)`.
- [x] Outil : `PHG_TEST_NO_TRAINERS=1` (les dresseurs ne repèrent plus le
      joueur, pour les trajets scriptés).
- [x] Arène d'Albert : entrée, labyrinthe de passerelles OK ; l'ascenseur
      est bloqué par un garde tant que la Tour Chétiflor n'est pas faite
      (`FLAG_HIDE_VIOLET_GYM_GYM_GUY_AFTER_SPROUT`) : verrou de scénario
      normal. Tour Chétiflor : entrée et 1er étage affichés.
- [x] Tour Chétiflor : labyrinthe sur 3 étages (escaliers aller-retour),
      scène du rival, **combat contre le Sage Li** (CT Flash). Arène :
      ascenseur 3D (passerelles transparentes), **combat contre Albert →
      Badge Zéphyr** + CT51, prime en argent sur la Carte de Dresseur.
      Vu en passant : messages d'efficacité (« Ça n'affecte pas… »),
      proposition de changer de Pokémon, K.O. et choix forcé d'un Pokémon.
- [x] Aides de test (jamais actives sans variable d'environnement) :
      `PHG_TEST_PARTY_LEVEL=N` (équipe au niveau N : expérience, stats,
      attaques, soins), `PHG_TEST_WARP=CARTE:X:Z[:IMAGE]` (téléportation via
      la tâche de warp des scripts).
- [x] Après le badge : appel d'Orme, **Œuf de l'assistant** (Boutique de
      Mauville ; en version FR le oui/non est intégré au texte), garde de la
      Route 32 qui renvoie sans l'Œuf (verrou normal), **évolution** testée :
      Germignon N.15 → combat gagné → N.16 → animation → « évolue en
      MACRONIUM ! ». Note : l'aide `PHG_TEST_PARTY_LEVEL` fixe le niveau sans
      « montée de niveau », donc sans évolution ; `N+` place à 1 point du
      niveau suivant pour tester montée et évolution. Débogage :
      `PHG_TRACE_SCRIPT=1` journalise chaque commande de script exécutée.
- [x] **Systèmes de jeu testés (vague autonome)** :
  - jour/soir/nuit (teinte du décor, lampadaires) — `PHG_TEST_HOUR=H` ;
  - Boutique (achat, quantité, argent), Résumé d'un Pokémon (4 pages),
    sauvegarde par le menu SAUVER (écran tactile, écrasement oui/non) ;
  - **durée de jeu** : restait à 0:00 → corrigé (le minuteur matériel 3
    n'était pas émulé ; `timer3.c` dérive maintenant ses ticks du compte
    de V-Blank émulés, même cadence que la console) ;
  - **PC de stockage** : plantait à l'ouverture → corrigé (42 fonctions
    d'état `void` de `pc_box_states.c` qui laissaient le prochain état dans
    r0 via un appel final ; macros `PCBOX_TAIL_STATE/RETURN`, identiques sur
    DS). Dépôt testé, refus de déposer le dernier Pokémon OK ;
  - CT depuis le Sac (APTE/PAS APTE, refus si incompatible), Boîte à
    Noigrumes, Bicyclette (interdite en intérieur, vitesse ×4 dehors) ;
  - **Surf** : plantait au bord de l'eau (reflet) → corrigé (zone de
    travail des effets de terrain : 128 octets sur DS, trop petite avec des
    pointeurs 64 bits ; 256 sur PC + vérification) ; surf sur l'eau OK ;
  - **Vol** (carte, confirmation, animation, arrivée), **pêche** (Super
    Canne, animation, reflet), **éclosion d'Œuf** (Togepi), **combat
    double** (jumelles, choix des cibles), évènement de Primo (Œuf).
  - Lieux visités sans plantage : Caverne Jumelle, Route 33 (pluie),
    Écorcia, Puits Ramoloss, Bois aux Chênes, arène d'Écorcia.
  - Aides de test ajoutées : `PHG_TEST_GIVE_ITEMS`, `PHG_TEST_BADGES`,
    `PHG_TEST_MOVES`, `PHG_TEST_EGG_READY`, `PHG_TEST_HOUR`.
- [x] 21 lieux clés visités par téléportation sans plantage ni défaut
      visible : Doublonville, Rosalia, Oliville, Irisia, Acajou, Lac Colère,
      Ébènelle, Parc Naturel, Tour Ferraille, Tour Cendrée, Tour Radio, Route
      de Glace, Antre du Dragon, Safari, Zone de Combat, Pokéathlon, Plateau
      Indigo, Bourg Palette, Carmin, Safrania, Céladopole.
- [x] **Pokéathlon** : la course se terminait instantanément (« Merci
      d'avoir participé » sans épreuve) → corrigé : `sPokeathlonStateInfoFuncTable`
      était déclaré comme un seul pointeur, le convertisseur de données n'en
      gardait que la 1re entrée (déclaré tableau, même code DS, données
      régénérées : 39 états). Inscription, choix du parcours, écran d'équipe
      (stats de performance, glisser-déposer) OK. Les épreuves elles-mêmes
      (mini-jeux tactiles) ne sont pas encore jouées.
- [x] Conversion complète des données asm relancée à part : les 79 autres
      fichiers sont identiques ; les notes restantes concernent des alias
      gérés, du code en ligne (Wi-Fi) ou des objets inutilisés.
- [x] Outil : `PHG_TEST_TOUCH=N:X:Y>X2:Y2` (glisser).
- [x] **Modes spéciaux** (sans nouveau bug du jeu) :
  - Usine de Combat : inscription, sauvegarde, location de 3 Pokémon
    (menu Résumé/Emprunter), combat aux règles de la Zone, écran
    d'échange après K.O. ;
  - Parc Safari : entrée (droit d'entrée, 30 Safari Balls, menu spécial),
    combat Safari (BALL/APPÂT/BOUE/FUITE), capture ;
  - Concours de Capture d'Insectes (mardi via `PHG_TEST_DAY_OFFSET=2`) :
    inscription, Pokémon confiés, Compét'Balls, combat, capture et
    Pokémon gardé. Le jugement final n'est pas encore vu. Note : le
    concours ne se sauvegarde pas (pas de SAUVER dans son menu) ; une
    sauvegarde de test prise pendant le concours n'est pas valable ;
  - Ruines d'Alpha : puzzle coulissant (indice, prise et dépôt des
    tuiles au stylet).
- [x] Outils : `PHG_TEST_DAY_OFFSET=N` (jour), plusieurs téléportations
      (`PHG_TEST_WARP=A,B,...`), carte ASCII avec toutes les cases à
      rencontres et les PC (`P`).
- [ ] Non testé : jugement du concours, épreuves du Pokéathlon, Tour /
      Salle / Château / Arcade de Combat, fonctions sans fil et en ligne
      (non disponibles hors ligne), Pokéwalker (infrarouge).
- [ ] Suite : Route 32, Grottes Jumelles / Ruines d'Alpha, Écorcia,
      arène d'Hector ; évolutions ; échanges et fonctions en ligne (hors
      ligne sur PC).

### M7 — Finitions
- [x] Couvercle (F10) : `PM_GoSleepMode` réel (`nitro_pc.c`) — écrans noirs,
      son en pause, la fenêtre reste réactive, réveil à l'ouverture
      (avant : boucle infinie → « ne répond pas »). `PM_[GS]etBackLight`
      éteignent vraiment l'écran. Test : `PHG_TEST_LID=600:900`.
- [x] `[display]` dans `config.ini` : `fullscreen`, `layout`
      (`large_top` par défaut : écran du haut ×3 + écran tactile à droite,
      16:9 exact ; `stacked` ; `side_by_side`), `stretch` (remplit la
      fenêtre en déformant), `window_scale` (`--scale` reste prioritaire).
      F9 fait défiler les 3 dispositions. Test : `PHG_DUMP_WINDOW=N` écrit
      `window.png` (fenêtre telle qu'affichée).
- [x] RTC / évènements horaires : l'horloge de l'ordinateur ; jour/nuit
      vérifié (aides de test `PHG_TEST_HOUR`, `PHG_TEST_DAY_OFFSET`).
      Sauvegarde : `--save FICHIER` (une sauvegarde par fichier).
- [x] **`config.ini`** (`src/platform_pc/config_pc.c`) : touches clavier et
      manette (plusieurs par bouton, gâchettes, stick analogique en croix),
      raccourcis, options. Cherché dans `$PHG_CONFIG`, `./config.ini`, puis à
      côté de l'exécutable ; s'il n'existe pas, un fichier commenté y est écrit.
      Erreurs de nom → avertissement, les autres réglages restent valides.
- [x] Raccourcis par défaut : **Tab** avance rapide (maintenue), **F8**
      avance rapide on/off, **M** couper le son, **F12** capture d'écran
      (`screenshots/`), F9 disposition des écrans, F10 couvercle, F11 plein
      écran, Échap quitter. Vitesse d'avance rapide `fast_forward_speed`
      (×4 par défaut, 0 = maximum) ; l'image reste à cadence normale.
- [x] **Filtres d'affichage** (`gfx_pc.c`) : `nearest`, `sharp` (défaut,
      bilinéaire net), `smooth`, `scanlines`, `lcd` ; `[display] filter` dans
      `config.ini`, **F7** fait défiler.
- [x] **3D** : marquage des contours (EDGE_COLOR, par identifiant de
      polygone), anticrénelage approché, polygones fil de fer (alpha 0).
- [x] **Sans fil hors ligne** (`nitro/wm_pc.c`) : une « radio vide » — chaque
      requête WM réussit et répond à la V-Blank suivante, personne n'est
      trouvé. Salle Union et Colisée utilisables seuls, recherche annulable ;
      `WVR_*` ne coupent plus le jeu.
- [x] **Installation** (`install_pc.c`) : à côté de l'exécutable, `pc_data/`
      ou bien le premier `*.nds` (HeartGold IPK*) qui est extrait au premier
      lancement ; langue tirée du code du jeu ; sauvegarde à côté de l'exe.
- [x] **Paquet** : `python tools/pc_port/package.py` → `dist/…zip` (exe
      allégé, README, dossier `mods/`, symboles à part). Refuse tout
      fichier `.nds/.sav/.narc/.bin`. Docs : `docs/pc_port/README_PC.md`,
      `docs/pc_port/MODDING.md`.
- [x] **Linux/macOS** : code prêt (pile d'appels `execinfo`, exe non-PIE à
      0x10000000, `mallopt`), CI GitHub `.github/workflows/pc-port.yml`
      (Windows MSYS2, Ubuntu, macOS Intel `macos-15-intel`). **La CI passe sur
      les trois systèmes** : compilation, autotest sans données (827
      vérifications sur `files/`), paquet. Corrigés pour y arriver : sections
      d'overlays par format (COFF `.data$X$N`, ELF `__start_/__stop_`, Mach-O
      `section$start`), options d'édition de liens propres à lld réservées à
      Windows, macOS sans « chained fixups » (sinon l'adresse fixe est
      ignorée), comptes de membres NARC connus sans ROM, fins de ligne CRLF,
      rythme d'images adaptatif (minuteurs imprécis des VM macOS). **Linux testé avec la ROM** (Ubuntu 26.04 sous
      WSL2, compilation native Clang 21, Mesa llvmpipe, PulseAudio) :
      extraction de la ROM, autotest complet 1103/0, Pokéathlon, Tour /
      Salle / Arcade / Castel de Combat, sauvegarde + rechargement, saves
      Windows compatibles. Corrigé : **pile du jeu sous 4 Gio**
      (`pc_run_on_low_stack`, `hw_pc.c`) — le code DS range des adresses dans
      des mots de 32 bits ; sous Windows la pile du fil principal est basse,
      sous Linux/macOS elle est vers 0x7FFF… (plantage de la sauvegarde de la
      Tour). Reste : un vrai Mac (seulement testé en CI).

#### Vérification du build DS (matching)
- mwccarm et le Nitro SDK (propriétaires) ne sont pas disponibles ici : la ROM
  DS n'a pas été recompilée. À la place, `tools/pc_port/check_ds_sources.py
  ORIGINAL_DIR` compare chaque source à la décompilation d'origine, code
  `PLATFORM_PC` retiré et macros d'identité développées (SCRIPT_REL(x) → x,
  PC_PTR32 → rien…) : **0 fichier différent**. Les changements « sans doute
  équivalents » (`sizeof(ptr)` au lieu de 4, includes ajoutés, auto-affectation)
  ont été mis sous `#ifdef PLATFORM_PC` pour une identité textuelle.

#### Recherche automatique des bugs 64 bits (`tools/pc_port/find_ds_offsets.py`)
- libclang sur tout le jeu (drapeaux du build PC) : lectures au-delà d'un objet
  global (`gX[N]`, `((T *)&gX)[N]`, `(&gX)[N]`, `&gX + N`) et décalages
  d'octets constants dans des structures à pointeurs (`((u8 *)p)[N]`,
  `(u8 *)p + N`, `u8 *unk = ptr; unk[N]`). `--as-ds` montre ce que les
  corrections PC remplacent (sert à valider l'outil).
- Corrigé grâce à lui : moteur 3D (`g3d_sbc.c`, génération de textures
  envmap/prjmap : mauvaise commande GPU + écritures hors objet), gestionnaire
  de VRAM des textures, PalPad (écrans de détail), Dress Up (marges et
  remises à zéro), Échange de Données (pas de 4 dans des tableaux de
  pointeurs), Terminal Global (palettes), test de cartouche GBA, et dans le
  code en ligne seulement : Spin Trade, mini-jeux de la Place Wi-Fi, chat.
- Restent signalés et laissés : Place Wi-Fi (instruments) et réglages Wi-Fi
  (point d'accès), inatteignables hors ligne ; un faux positif (Mime Jr).

#### Classes de bugs 64 bits trouvées pendant les finitions
- Prototypes locaux aux largeurs d'entiers différentes de la définition
  (`u8` déclaré, `int` défini, ou retour large) : en x64 les bits hauts du
  registre sont indéfinis. 32 cas corrigés (`#ifdef PLATFORM_PC` avec les
  types de la définition).
- Lecture au-delà d'un objet `.rodata` vers ses voisins (le DS les place
  côte à côte, le PC les aligne) : Pokéathlon (Capture Drapeau, Poussée
  Cercle, Course Relais), menu du Castel de Combat, pavé Wi-Fi.
- Décalages d'octets bruts dans des structures à pointeurs : unions
  `raw` du Pokéathlon (fichiers d'icônes), résultat de la sélection
  d'équipe de la Zone de Combat (`PartyMenuArgs`, Tour de Combat).

---

## 5. Journal

| Date | Jalon | Notes |
|---|---|---|
| 2026-11-20 | M1 | Audit (`docs/PORTING_API_AUDIT.md`), CMake PC/DS_IDE, HAL `src/platform_pc/`, fenêtre SDL2 + OpenGL 3.3, NitroFS sur `files/`, décodage NCLR/NCGR/NSCR/NARC. |
| 2026-11-20 | M1 ✅ | **Compilé et lancé** sous Windows 11 (GCC 16.2 WinLibs UCRT, CMake 4.4.4, Ninja, SDL 2.30.9 statique) : `pokeheartgold_pc.exe` autonome, 0 avertissement dans `src/platform_pc`. OpenGL 3.3 OK (RTX 3050 Ti). `--fs-check` : 384 fichiers NitroFS, 312 présents, 72 à générer par `make filesystem` (`tel/`, `data/mushi/`, une partie de `a/x/y/z`…). Décor de combat `data/ground0` rendu correctement à l'écran du haut. Corrigés en route : manifeste CMake (continuations `\`), dossiers pris pour des fichiers par `fopen` Windows, repli audio quand WASAPI échoue. Testé par l'utilisateur : les boutons → indicateurs du masque PAD s'allument correctement. Restent à confirmer : manette, stylet, F9/F10/F11, audio WASAPI hors shell de l'agent. Note : le 1er lancement après chaque édition de liens est lent (analyse antivirus du nouvel `.exe`). Cible `DS_IDE` : le projet ARM se configure, puis `tools/` exige libpng (dépendance préexistante). |
| 2026-11-20 | M2 ✅ | Carte mémoire DS native + base d'image basse ; couche Nitro (`src/platform_pc/nitro/`) ; `phg_m2_selftest` : 825/825 (3 exécutions CTest consécutives). Problèmes rencontrés et résolus : DLL Windows occupant 0x01FF0000+ avant `main` (→ relancement suspendu), `math.h` du SDK masquant celui de l'hôte pour SDL, fuite de descripteurs (le jeu ne ferme pas toujours ses `FSFile`, normal sur DS), rythme VBlank faussé par la granularité de 15,6 ms de `Sleep` (→ minuteur haute résolution). **Correction** : le `Makefile` DS aurait compilé `src/platform_pc/` (exclusion ajoutée à `common.mk`). Non vérifiés : build Linux/macOS (macOS : `__PAGEZERO` de 4 Gio, options de lien prévues mais non testées), compilation complète de la ROM DS. |
| 2026-11-21 | M3 ✅ | Jeu complet compilé avec Clang 23 (llvm-mingw) et lancé : `NitroMain()` → intro (2D, 3D, particules) → écran titre, 100 s sans plantage. 1097/1097 à l'autotest. Données asm converties en C (libclang), 394 stubs hors ligne, routines ARM réécrites en C, DMA/diviseur émulés, alias de données recréés, corrections 64 bits (tas, VRAM 3D, particules). Pièges notés : différences de compression ROM FR/US, étiquettes-alias dans les données, symboles d'overlays en double, `msgenc` qui « réussit » sans écrire. Reste : rendu (M4), réinitialisation `.data/.bss` des overlays, threads secondaires, réinitialisation douce. |
| 2026-11-21 | M3+ | Variables d'overlays réinitialisées à chaque chargement (sections par overlay + instantané au démarrage, 932 sources réparties sur 129 overlays). Corrigé en passant : les chargements d'overlays secondaires échouaient en silence (plages RAM nulles → chevauchement). |
| 2026-11-21 | M3+ | Réinitialisation douce dans la même fenêtre (testée : 2 resets puis 60 s d'intro, et 5000 images sans reset, 0 erreur). Langue du code choisie au lancement (`--lang`, FR par défaut, en-tête IPKF). Corrigé : débordement des objets d'animation 3D (taille DS codée en dur). Ajouts de test : `PHG_EXIT_AFTER`, `PHG_TEST_RESET_AT`, `PHG_WATCH`, registres dans les rapports de plantage. |
| 2026-11-21 | M4 ✅ | Moteur 2D logiciel : les deux écrans s'affichent (intro et écran titre en français), 60 images/s. Banques VRAM émulées (VRAMCNT), rendu par ligne avec DMA/IRQ H-Blank, fenêtres, mélanges, luminosité. Corrigé : tampon d'animation des sprites trop petit en 64 bits (tuiles décalées). Restent la 3D (M5) et la capture d'écran. |
| 2026-11-21 | M4+ | Le jeu va du titre jusqu'au monde (chambre du héros) sans plantage : 6 bugs « 64 bits » corrigés (blocs de symboles DS, prototype `u8`/`u16`, tailles en dur, zone de rendu trop petite, files de tâches). Mode turbo et scripts d'entrées pour les tests. L'écran du haut reste noir dans le monde tant que la 3D (M5) n'est pas faite. |
| 2026-11-21 | M5 ✅ | 3D logicielle : intro, Ho-Oh de l'écran titre et monde du jeu affichés, le joueur se déplace. Commandes 3D captées via les macros de registres (port + valeur différée), copies et DMA. Corrigé : résultats périmés du diviseur matériel lus à l'adresse brute (projection fausse, `FX_Inv`). |
| 2026-11-21 | M6 ✅ | Son : pilote ARM7 réécrit (séquenceur SSEQ, instruments, ADPCM, enveloppes) dans le fil audio SDL. Musique de l'intro audible. Corrigés : pointeur 32 bits de la FAT de l'archive son, table volume→dB non placée, lecteur préparé à signaler actif, période des ondes. |
| 2026-11-21 | Jeu | Début de l'aventure joué de bout en bout jusqu'au **premier combat sauvage gagné** (Route 29). Corrigés : 6 bugs 64 bits (animations de décor, sauts de scripts, miroir de textures 3D, gestionnaire de transferts de cellules, pointeurs du pilote son). Aussi : capture d'écran `DISPCAPCNT`, polygones d'ombre, veille (F10), dispositions d'écran et plein écran, `config.ini`. |
| 2026-11-22 | Jeu | Aventure poursuivie jusqu'au **combat contre le rival gagné** (Ville Griotte) : visite guidée, Pokédex, appel du Pokématos, IA des dresseurs. Corrigé : alias de données de l'IA des dresseurs. Outils : carte ASCII, rencontres désactivables. |
| 2026-11-22 | Jeu | Retour au labo (nom du rival), tutoriel de capture, **première capture** (Hoothoot) et **Centre Pokémon** OK. Corrigé : textes du Pokédex en anglais dans le jeu français (langue US en dur). |
| 2026-11-22 | Jeu | Arrivée à **Mauville (Violette)** ; menus du terrain testés (Pokédex, Carte, Pokémon, Sac, Options, Pokématos). Corrigé : plantage du Pokédex (pas de 4 octets pour des pointeurs). |
| 2026-11-22 | Jeu | **Premier badge** (Badge Zéphyr, Albert) après la Tour Chétiflor. Aucun nouveau bug du jeu sur ce tronçon. Aides de test : niveau de l'équipe, téléportation. |
| 2026-11-22 | Jeu | **Évolution** vérifiée (Germignon → Macronium), Œuf de l'assistant reçu. Pas de bug du jeu : un faux bug venait de mes réponses « non » automatiques. |
| 2026-11-22 | Jeu | Vague de tests des systèmes (boutique, PC, CT, vélo, Surf, Vol, pêche, œufs, combats doubles, jour/nuit). Corrigés : PC de stockage (états `void`), Surf/reflets (zone de travail des effets), durée de jeu (minuteur 3). |
| 2026-11-22 | Jeu | 21 lieux visités sans plantage. Corrigé : Pokéathlon (table d'états réduite à 1 entrée par le convertisseur de données). |
| 2026-11-22 | Jeu | Modes spéciaux testés : Usine de Combat, Parc Safari, Concours d'insectes, puzzle des Ruines d'Alpha. Aucun nouveau bug du jeu. |
| 2026-11-22 | M7 | Paquet Windows + extraction de la ROM au 1er lancement, README/guide de modding, filtres d'affichage (F7), contours/AA/fil de fer 3D, radio sans fil vide, Linux/macOS + CI (non testés localement). Corrigés : 32 prototypes aux largeurs d'entiers fausses, Pokéathlon (Capture Drapeau et autres tables lues au-delà de leur objet, unions d'icônes). |
| 2026-11-22 | Jeu | **Zone de Combat complète** : Tour, Usine, Salle, Castel, Arcade — inscription, sauvegarde, combat. Corrigés : Castel (menu lu au-delà d'une table → plantage), Tour (sélection d'équipe lue à des décalages DS). Pokéathlon : épreuve 2 (Capture Drapeau) jouée jusqu'aux résultats. |
| 2026-11-22 | M7 | **Linux et macOS compilés et testés en CI** (dépôt GitHub privé, sans aucune donnée du jeu) : compilation, autotest et paquet OK sur Windows, Ubuntu et macOS Intel. Le jeu complet avec la ROM reste à lancer sous Linux (WSL) et sur un vrai Mac. |
| 2026-11-22 | M7 | **Jeu testé sous Linux avec la ROM** (WSL2) : Pokéathlon, Zone de Combat, sauvegarde/rechargement OK. Corrigé : pile du jeu placée sous 4 Gio sous Linux/macOS (adresses de pile tronquées à 32 bits → plantage de la sauvegarde de la Tour de Combat). |
| 2026-11-23 | M7 | **Build DS vérifié** au niveau des sources (`check_ds_sources.py` : 0 fichier différent de la décompilation d'origine, code PC retiré). **Scanner de bugs 64 bits** (`find_ds_offsets.py`, libclang) : 64 alertes revues, corrigés le moteur 3D (textures envmap/prjmap), la VRAM des textures, PalPad, Dress Up, l'Échange de Données, le Terminal Global et du code en ligne. |
| 2026-11-23 | Jeu | **Histoire testée en accéléré** : les 19 dresseurs majeurs battus sans plantage (16 champions de Johto et Kanto, Conseil des 4, Peter, Blue ; Red joué jusqu'au bout), puis Panthéon, générique, retour au titre et sauvegarde d'après-partie. Nouveaux crochets de test : `PHG_TEST_TRAINER=ID[:IMAGE],…` (combat de dresseur direct, contourne les verrous d'histoire), `PHG_TEST_SPECIES=N` (espèce de l'équipe) ; `PHG_TEST_MOVES` règle aussi le style « Défini » et le texte rapide. Aucun bug du port trouvé sur ce parcours. |
| 2026-11-24 | Jeu | **SoulSilver** : `-DPHG_GAME_VERSION=SOULSILVER` donne `pokesoulsilver` (code compilé avec `SOULSILVER`, en-tête simulé « POKEMON SS » / IPG). Jeu à part : n'accepte que sa ROM (code IPG ; la bonne ROM est choisie si les deux sont dans le dossier), données `pc_data_ss/`, mods `mods_ss/`, sauvegarde `save_ss.bin`. Paquet `PokemonSoulSilver-PC-…`, CI 2 versions × 3 systèmes. Vérifié : autotest 831/0 ; avec les mêmes données, l'intro passe de Ho-Oh au coucher du soleil (HG) à Lugia sous la lune (SS). Reste : une ROM SoulSilver pour jouer réellement. `PHG_PREBUILT_HOSTTOOLS` réutilise les outils d'un autre build (contourne aussi un antivirus qui retient les outils neufs). |
| 2026-11-25 | Jeu | **Choix de langue HG/SS** (anglais avec une ROM française) : couche `pc_data/en` / `pc_data_ss/en` construite depuis le dépôt (`tools/pc_port/build_lang_en.py`, cible `phg_lang_en`) — archive de textes `a/0/2/7` refaite depuis les 829 banques `msgenc` (BTNF de la ROM, alignement 4) + 44 fichiers localisés de `files/` ; couches de fichiers : langue choisie puis données de la ROM ; `config.ini [options] language = auto|fr|en` (sinon `--lang`, `PHG_LANG`). Vérifié : tutoriel tactile en FR et EN sur HG et SS (texte et bouton dessiné), HG anglais 9000 images sans plantage, autotest 1103/0. Limites : logo du titre, quelques images Pokégear, tri alphabétique du Pokédex restent dans la langue de la ROM. Le choix se fera dans le launcher de l'utilisateur : voir `docs/pc_port/LAUNCHER.md`. |
