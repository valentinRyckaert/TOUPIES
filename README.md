# Toupie Arena

[![CI](https://github.com/valentinRyckaert/TOUPIES/actions/workflows/ci.yml/badge.svg)](https://github.com/valentinRyckaert/TOUPIES/actions/workflows/ci.yml)
[![Release](https://github.com/valentinRyckaert/TOUPIES/actions/workflows/release.yml/badge.svg)](https://github.com/valentinRyckaert/TOUPIES/actions/workflows/release.yml)

Jeu de combat de toupies en 2D, vu du dessus, écrit en C++17 avec Qt 6 (Widgets).

Fonctionnalités :

- **Jouer contre le bot** : choisissez votre toupie, celle du bot (ou au hasard), la
  difficulté, le stadium et les règles, réglez la force et la direction du lancer, puis
  pilotez, dashez et déclenchez votre coup spécial au bon moment.
- **Multijoueur en réseau local** (2 à 4 joueurs) : créez un lobby ou rejoignez-en un depuis
  la liste des lobbys détectés, par son nom ou par adresse IP ; chacun choisit sa toupie parmi
  ses propres toupies.
- **Assembler une toupie** : un boulon (coup spécial), une roue métallique, une partie
  intermédiaire et une pointe.
- **Six stadiums** aux formes et propriétés différentes, **sept coups spéciaux** et une
  section **Personnalisation** pour régler vitesse, rebonds, usure, endurance, durée…

## Compilation

Prérequis : CMake ≥ 3.21, un compilateur C++17, Qt ≥ 6.4 (modules Core, Gui, Network,
Widgets, Test).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/src/toupie_arena          # le jeu
ctest --test-dir build            # tests unitaires, d'intégration et d'interface
./build/tools/toupie_balance_sim  # le simulateur d'équilibrage
```

Options CMake : `TOUPIE_WERROR` (avertissements = erreurs), `TOUPIE_BUILD_TOOLS`,
`BUILD_TESTING`. `toupie_arena --smoke-test` démarre le jeu puis le ferme (vérification des
paquets).

Les toupies assemblées sont enregistrées en JSON dans le dossier de données de l'application
(`~/.local/share/ToupieArena/ToupieArena/tops.json` sous Linux).

## Commandes

| Phase  | Action |
|--------|--------|
| Visée  | souris : direction, et **force** selon la distance du curseur (flèche plus ou moins longue) ; ou `←` `→` pour la direction et `↑` `↓` pour la force ; clic ou `Entrée` pour lancer |
| Combat | **flèches** (ou `Z` `Q` `S` `D`) maintenues : pilotage, légère correction de trajectoire |
|        | `Espace` ou clic : dash |
|        | `E` ou bouton « Coup spécial » : coup spécial (jauge pleine) |
| Fin    | `R` : revanche (solo) ou retour au lobby (hôte) |
| Partout| `Échap` : retour au menu / quitter le lobby |

## Règles

### Toupies et pièces

Une toupie est l'assemblage de quatre pièces, de haut en bas : **boulon**, **roue
métallique**, **partie intermédiaire** et **pointe**. Chaque pièce (sauf le boulon) a un
niveau d'attaque, de défense et d'endurance ; les stats de la toupie sont leur somme. La roue
fixe le sens de rotation, le boulon le coup spécial.

| nom          | attaque | défense | endurance |
|--------------|---------|---------|-----------|
| Rempart (RM1)| 3       | 5       | 2         |
| Noyau (PI1)  | 2       | 3       | 1         |
| Pivot (P1)   | 1       | 4       | 2         |
| **Toupie**   | **6**   | **12**  | **5**     |

Le **type** d'une toupie est sa stat strictement la plus élevée (ici : Défense). Si deux stats
ou plus sont à égalité au sommet, la toupie est de type **Équilibre**.

**Budget de points** : toutes les pièces d'un même emplacement totalisent le même nombre de
points (roue 10, intermédiaire 6, pointe 7, boulon 0). Toutes les toupies valent donc
23 points : assembler consiste à *répartir*. Le catalogue compte 7 boulons, 14 roues,
10 parties intermédiaires et 10 pointes.

### Le combat

Le stadium est une cuvette vue du dessus (distances en rayons de stadium). Les 2 à 4 toupies
partent à égale distance du centre, dans la direction et avec la force choisies.

- **Déplacement** : la pente et une légère attraction ramènent les toupies vers le centre ; leur
  rotation les propulse **dans le sens inverse de leur rotation**. Sans action contraire, une
  toupie finit donc par se rapprocher du centre.
- **Pilotage** : maintenir une flèche applique une petite accélération dans cette direction,
  assez pour sortir d'une orbite et aller chercher (ou éviter) l'adversaire, pas pour se
  téléporter.
- **Force de lancer** : de 50 % à 150 % de la vitesse de lancement normale.
- **Endurance** : chaque toupie démarre à 100 (réglable) et en perd en continu, d'autant moins
  que sa stat d'endurance est élevée et d'autant plus qu'elle va vite. Sous 25 %, elle vacille.
- **Chocs** : chaque toupie perd de l'endurance selon l'attaque adverse et sa propre défense,
  et recule d'autant moins que sa défense est élevée.
- **Dash** : ruée courte vers l'adversaire le plus proche, avec un bonus d'attaque, contre un
  peu d'endurance.
- **Rebord** : une toupie qui atteint lentement un rebord rebondit ; trop vite, elle le
  franchit. Sans rebord, rien ne la retient.
- **Défaite** : éjectée ou à court d'endurance. La dernière toupie en jeu gagne. Par défaut il
  n'y a **pas de limite de temps** ; si une limite est choisie, la toupie qui a le plus
  d'endurance à la fin gagne.
- **Mêlée (3-4 toupies)** : éliminer un adversaire frappé dans les 4 dernières secondes rend
  35 points d'endurance.

### Coups spéciaux

Une jauge se remplit avec le temps et à chaque choc. Pleine, elle permet de lancer le coup
spécial du boulon, sous forme d'une **vague** qui file vers l'adversaire le plus proche
(sans le suivre : on peut l'esquiver en pilotant) ou d'un **anneau** qui s'élargit et touche
tous les adversaires jusqu'à sa portée. La défense de la cible réduit dégâts et recul.

| Boulon    | Coup             | Forme  | Point fort | Contrepartie |
|-----------|------------------|--------|------------|--------------|
| Tempête   | Onde de choc     | anneau | gros recul, pour éjecter | peu de dégâts, portée courte |
| Perçant   | Lance d'acier    | vague  | gros dégâts, très rapide | presque aucun recul, trait fin |
| Polaire   | Souffle de givre | vague  | ralentit et bloque le pilotage 3,5 s | dégâts faibles |
| Abysse    | Vortex           | anneau | aspire tout le monde vers le centre | presque aucun dégât |
| Spectral  | Distorsion       | vague  | téléporte la cible près du bord | aucun dégât, esquivable |
| Vampire   | Siphon           | vague  | rend une partie de l'endurance volée | dégâts moyens, peu de recul |
| Tellurique| Séisme           | anneau | blesse et bloque dash + coup spécial 3 s | portée très courte |

### Personnalisation

Dans la préparation du combat (solo) et dans le lobby (réglée par l'hôte, visible par tous) :
vitesse, rebonds après un choc, usure, dégâts, puissance des dashs, pilotage (0 = désactivé),
charge des coups spéciaux (0 = désactivés), endurance max et temps maximal (aucun par défaut).

### Stadiums

| Stadium   | Forme        | Rebord     | Caractéristiques |
|-----------|--------------|------------|------------------|
| Classique | cercle       | oui        | référence, aucun modificateur |
| Béton     | octogone     | très haut  | sol lisse : vitesse +5 %, dégâts +10 %, recul −10 %, usure +10 % |
| Terre     | cercle       | non        | le sol accroche : usure +20 %, dégâts +40 %, dashs −5 % |
| Glace     | ellipse      | haut       | usure −30 %, recul −15 %, dégâts −10 % |
| Sable     | hexagone     | haut       | frottement +30 %, usure +30 %, dégâts +30 %, dash plus court mais 2× plus puissant |
| Volcan    | petit cercle | non        | pentes très raides, dégâts +10 %, dash plus puissant |

### Formules

Avec `A`, `D`, `E` les stats, `v` la vitesse et `ratio` l'endurance restante / maximum :

```
attaque effective   = A × (bonus de dash si actif) × (0,6 + 0,4 × ratio)
perte passive / s   = base × max(0,15 ; 1 − kE × E) × (1 + drain × v)
dégâts d'un choc    = base × attaqueEffectiveAdverse × impact × max(0,15 ; 1 − kD × D)
recul d'un choc     = (base + kA × attaqueAdverse + kM × DéfenseAdverse + kV × vRapprochement)
                      × max(0,15 ; 1 − kR × D)
dégâts d'un coup    = dégâtsDuCoup × échelle × max(0,15 ; 1 − kD × D)
```

Les toupies de type Équilibre subissent un peu moins d'usure, de dégâts et de recul. Toutes
les constantes sont commentées dans [`src/game/BattleConfig.h`](src/game/BattleConfig.h), les
coups dans [`src/game/SpecialMove.cpp`](src/game/SpecialMove.cpp) et les stadiums dans
[`src/game/Stadium.cpp`](src/game/Stadium.cpp).

### Équilibrage

Les effets des stats sont linéaires et les types forment un pierre-feuille-ciseaux
(Attaque > Endurance > Défense > Attaque, Équilibre sans point fort ni faiblesse marqués). Les
constantes ont été réglées avec `toupie_balance_sim`, qui fait s'affronter des bots (qui
pilotent, esquivent et utilisent leur coup spécial) sur des milliers de combats entre toupies
tirées au hasard parmi toutes les combinaisons :

```sh
./build/tools/toupie_balance_sim -n 0 -r 6000              # types et coups spéciaux
./build/tools/toupie_balance_sim -n 0 -r 3000 -s all       # résumé par stadium
./build/tools/toupie_balance_sim -n 0 -r 1500 -s all -p 4  # mêlées à 4
./build/tools/toupie_balance_sim -n 200 -r 0 dashCost=8    # presets, avec une surcharge
```

**Duels, bots normaux** — taux de victoire global par type :

| Stadium   | Attaque | Défense | Endurance | Équilibre | Éjections | Durée moy. |
|-----------|---------|---------|-----------|-----------|-----------|------------|
| Classique | 47,3 %  | 51,2 %  | 52,6 %    | 48,5 %    | 41 %      | 19 s       |
| Béton     | 40,6 %  | 52,6 %  | 57,1 %    | 52,0 %    | 29 %      | 20 s       |
| Terre     | 41,7 %  | 51,6 %  | 57,3 %    | 51,3 %    | 30 %      | 18 s       |
| Glace     | 50,7 %  | 59,6 %  | 39,1 %    | 50,1 %    | 49 %      | 21 s       |
| Sable     | 44,7 %  | 50,5 %  | 55,3 %    | 50,5 %    | 32 %      | 17 s       |
| Volcan    | 41,0 %  | 58,8 %  | 46,6 %    | 59,2 %    | 33 %      | 18 s       |

**Coups spéciaux** (duels entre boulons différents, Classique) : de 45,6 % (Distorsion,
Givre) à 55,7 % (Lance d'acier).

**Mêlées à 4** — part des victoires rapportée à une part équitable (100 %) : Attaque 81–103 %,
Défense 116–127 %, Endurance 74–105 %, Équilibre 98–109 % selon le stadium.

Limites connues : les écarts dépendent de la difficulté des bots (en « difficile », Attaque
~39 % et Endurance ~62 % sur le stadium Classique ; en « facile », de 41 % à 58 %), et les
toupies les plus spécialisées subissent des contres plus marqués.

## Le bot

Le bot choisit son angle et sa force de lancement, pilote, dashe, esquive les vagues et
utilise son coup spécial selon le type de sa toupie :

- **Attaque** : lancer puissant, va chercher le contact, dashe dès qu'il est à portée ; en
  mêlée, il cible les proies fragiles ;
- **Défense** : tient le centre et contre-attaque quand l'adversaire arrive ou approche du bord ;
- **Endurance** : lancer doux, évite les chocs, ne dashe que pour éjecter ;
- **Équilibre** : opportuniste.

Il passe à l'offensive s'il sait qu'il perdra à l'usure, évite de s'éjecter lui-même et
n'utilise un coup que s'il est utile (Distorsion sur une cible proche du centre, Onde de choc
sur une cible déjà écartée…). La difficulté règle son temps de réaction, sa précision et la
part de vagues qu'il esquive (20 % / 60 % / 75 %).

## Multijoueur en réseau local

Depuis le menu **Multijoueur en réseau local** :

- **Créer un lobby** : nom, nombre de joueurs (2 à 4) et stadium. L'écran du lobby affiche
  les adresses à communiquer ; l'hôte y règle aussi la personnalisation.
- **Rejoindre un lobby** : dans la liste des lobbys détectés, en tapant le **nom du lobby**, ou
  une **adresse IP** (`192.168.1.20` ou `192.168.1.20:45455`).
- Chaque joueur choisit sa toupie **parmi ses propres toupies** et se déclare prêt ; l'hôte
  lance la partie quand le lobby est complet. Après le combat, l'hôte ramène tout le monde au
  lobby. Un joueur déconnecté en cours de partie est éliminé (abandon).

Fonctionnement :

- **L'hôte fait autorité** : il simule le combat et diffuse l'état 30 fois par seconde ; les
  clients envoient leur visée (angle, force), leur pilotage, leurs dashs et coups spéciaux.
- **Découverte des lobbys** : les hôtes diffusent une annonce UDP par seconde (port 45454).
  Comme ces annonces sont souvent bloquées (pare-feu qui n'autorise que le port TCP du jeu,
  Wi-Fi qui filtre les diffusions), le jeu sonde aussi les sous-réseaux locaux en TCP
  (le /24 de la machine puis jusqu'à un /20, en quelques secondes, toutes les 30 s ou avec
  « Actualiser »). Un lobby joignable par IP est donc aussi listé.
- **Ports** : TCP 45455 (ou le suivant libre jusqu'à 45464) pour le lobby, UDP 45454 pour les
  annonces. Avec firewalld par exemple :
  `sudo firewall-cmd --permanent --add-port=45455/tcp --add-port=45454/udp && sudo firewall-cmd --reload`.
- **Compatibilité** : le client envoie la version du protocole et une empreinte du catalogue
  de pièces ; l'hôte refuse les versions différentes.

## Intégration et livraison continues

- **CI** ([`ci.yml`](.github/workflows/ci.yml), à chaque push sur `main` et pull request) :
  - Linux avec le Qt 6.4 de la distribution (version minimale), Debug et Release, avertissements
    traités comme des erreurs : compilation, tests unitaires, d'intégration réseau et
    d'interface (plateforme `offscreen`), démarrage de l'application, simulateur ;
  - Linux avec AddressSanitizer et UndefinedBehaviorSanitizer ;
  - Linux, Windows (MSVC) et macOS avec Qt 6.8 LTS : compilation, tests, binaires en artefacts.
- **CD** ([`release.yml`](.github/workflows/release.yml), à chaque tag `vX.Y.Z`) : compilation
  et tests sur les trois systèmes, paquets AppImage (Linux), zip (Windows) et dmg (macOS),
  vérification du démarrage de chaque paquet, publication dans une release GitHub.

```sh
git tag v1.0.0 && git push origin v1.0.0   # déclenche la publication
```

Tests (`ctest`) :

| Suite        | Contenu |
|--------------|---------|
| `tst_core`   | stats, types, catalogue, boulons, persistance JSON |
| `tst_battle` | formules, dash, pilotage, recentrage, force de lancer, coups spéciaux et effets, règles, stadiums, mêlées, abandons, déterminisme |
| `tst_net`    | lobby (connexion, refus, déconnexion), découverte UDP et par balayage TCP, encodage de l'état, partie réseau de bout en bout |
| `tst_gui`    | navigation, partie solo complète au clavier, atelier, lobby entre deux fenêtres avec règles de l'hôte |

## Architecture

```
src/
  core/     modèle : Stats, Part (dont boulons), Top, PartCatalog, TopRepository
  game/     moteur déterministe : Battle, BattleConfig, Stadium, SpecialMove, MatchRules,
            BotController
  session/  MatchSession (état affiché + actions du joueur local), LocalSession (solo)
  net/      réseau local : Protocol, LobbyServer, LobbyClient, LobbyBrowser + LobbyScanner
            (découverte), HostSession / ClientSession, MatchCodec
  ui/       interface (bibliothèque toupie_ui) : MainWindow et pages, ArenaWidget,
            RulesWidget, StadiumPainter, StatsWidget, TopPainter, Theme
  main.cpp  exécutable toupie_arena
tests/      Qt Test : modèle, moteur, réseau, interface
tools/      simulateur d'équilibrage
packaging/  fichier .desktop et icône
```

`toupie_core` (modèle, moteur, sessions) ne dépend que de Qt Core, `toupie_net` ajoute
Qt Network et `toupie_ui` Qt Widgets.
