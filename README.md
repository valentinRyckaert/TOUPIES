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
|        | `E` ou bouton « Coup spécial » : coup spécial (une fois par partie, coûte la moitié de l'endurance) |
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

Chaque toupie peut lancer le coup spécial de son boulon **une seule fois par partie**, et ce
lancer lui coûte **la moitié de son endurance du moment** : à 100 d'endurance on en perd 50,
à 20 on n'en perd que 10. Lancer son coup dès le début est donc rarement payant ; il vaut
mieux le garder pour un moment décisif (cible au bord, adversaire affaibli, fin de partie).

Le coup part sous forme d'une **vague** qui file vers l'adversaire le plus proche (sans le
suivre : on peut l'esquiver en pilotant) ou d'un **anneau** qui s'élargit et touche tous les
adversaires jusqu'à sa portée. La défense de la cible réduit dégâts et recul.

| Boulon    | Coup             | Forme  | Point fort | Contrepartie |
|-----------|------------------|--------|------------|--------------|
| Tempête   | Onde de choc     | anneau | gros recul : éjecte une cible déjà au bord | portée courte |
| Perçant   | Lance d'acier    | vague  | très lourds dégâts | presque aucun recul, esquivable de loin |
| Polaire   | Souffle de givre | vague  | ralentit et bloque le pilotage 3 s | dégâts moyens |
| Abysse    | Vortex           | anneau | aspire tout le monde vers le centre | dégâts modérés |
| Spectral  | Distorsion       | vague  | blesse et téléporte la cible près du bord | lente, facile à esquiver |
| Vampire   | Siphon           | vague  | rend toute l'endurance volée | dégâts moyens, peu de recul |
| Tellurique| Séisme           | anneau | blesse et bloque dash + coup spécial 4,5 s | portée très courte |

### Personnalisation

Dans la préparation du combat (solo) et dans le lobby (réglée par l'hôte, visible par tous) :
vitesse, rebonds après un choc, usure, dégâts, puissance des dashs, pilotage (0 = désactivé),
coups spéciaux (activés ou non), endurance max et temps maximal (aucun par défaut).

### Stadiums

| Stadium   | Forme        | Rebord     | Caractéristiques |
|-----------|--------------|------------|------------------|
| Classique | cercle       | oui        | référence, aucun modificateur |
| Béton     | octogone     | très haut  | sol lisse : vitesse +5 %, dégâts +10 %, recul −10 %, usure +10 % |
| Terre     | cercle       | non        | le sol accroche : usure +20 %, dégâts +40 %, dashs −5 % |
| Glace     | ellipse      | très haut  | pentes douces, usure −25 %, recul −17 %, dégâts +19 % |
| Sable     | hexagone     | haut       | frottement +30 %, usure +30 %, dégâts +30 %, dash plus court mais 2× plus puissant |
| Volcan    | petit cercle | non        | pentes très raides, dégâts +14 %, usure +10 %, dash plus puissant |

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
./build/tools/toupie_balance_sim -n 0 -r 6000                  # types et coups spéciaux
./build/tools/toupie_balance_sim -n 0 -r 3000 -s all           # résumé par stadium
./build/tools/toupie_balance_sim -n 0 -r 1500 -s all -p 4      # mêlées à 4
./build/tools/toupie_balance_sim -n 0 -r 3000 -d difficile -d2 normal  # niveaux de bot
./build/tools/toupie_balance_sim -n 0 -r 3000 --sans-speciaux-2        # intérêt du coup spécial
./build/tools/toupie_balance_sim -n 0 -r 3000 move.lance.damage=40 stadium.glace.friction=0.9
```

**Duels, bots normaux** — taux de victoire global par type :

| Stadium   | Attaque | Défense | Endurance | Équilibre | Éjections | Durée moy. |
|-----------|---------|---------|-----------|-----------|-----------|------------|
| Classique | 50,3 %  | 52,1 %  | 48,1 %    | 48,6 %    | 38 %      | 14 s       |
| Béton     | 48,6 %  | 50,4 %  | 50,6 %    | 51,3 %    | 29 %      | 15 s       |
| Terre     | 51,1 %  | 49,0 %  | 50,1 %    | 49,5 %    | 30 %      | 12 s       |
| Glace     | 47,9 %  | 52,7 %  | 47,9 %    | 53,3 %    | 33 %      | 17 s       |
| Sable     | 52,2 %  | 49,4 %  | 48,1 %    | 49,8 %    | 31 %      | 13 s       |
| Volcan    | 56,3 %  | 45,8 %  | 43,3 %    | 57,4 %    | 26 %      | 12 s       |

**Coups spéciaux** (duels entre boulons différents, Classique, 8 000 combats) : de 47,2 %
(Distorsion) à 53,4 % (Siphon). **Intérêt** : un bot difficile qui sait utiliser son coup gagne
~66 % de ses duels contre un bot difficile qui s'en prive (~56 % pour le bot normal, qui le
lance moins bien).

**Mêlées à 4** — part des victoires rapportée à une part équitable (100 %) : Attaque 86–100 %,
Défense 112–123 %, Endurance 84–96 %, Équilibre 88–107 % selon le stadium.

Limites connues : entre bots difficiles, le type Équilibre (qui alterne chasse et esquive
selon la course à l'usure) monte à ~57 % ; entre bots faciles, qui lancent leur coup sans
calculer, l'Endurance (peu défensive) s'effondre à ~28 %. Les toupies les plus spécialisées
subissent des contres plus marqués.

## Le bot

À chaque décision, le bot analyse la partie (adversaires, course à l'usure, distance au bord)
et choisit un **plan** selon sa toupie : *chasser* (aller au contact), *tenir* (garder le centre
et contrer) ou *esquiver* (garder ses distances loin du bord pour gagner à l'usure). Les trois
niveaux diffèrent par leurs réflexes et surtout par la qualité de leurs décisions :

| Niveau    | Comportement |
|-----------|--------------|
| Facile    | débutant impulsif : dashe dès que possible, lance son coup spécial dès que l'adversaire est à portée, esquive rarement les vagues |
| Normal    | règles de jeu correctes selon son type, coup spécial lancé quand il semble rentable |
| Difficile | **chasseur** (attaque) qui se place côté centre de sa cible pour la pousser vers le bord ; **enclume** (défense) qui tient le centre ; **esquive** (endurance) qui fuit les attaquants et frappe en fin de partie ; équilibre qui choisit selon la course à l'usure. Il estime la valeur nette de chaque **dash** (dégâts infligés − subis − coût, éjection probable, risque de s'éjecter) et de son **coup spécial** (avec une marge de sécurité : il attend que le coût de la moitié de l'endurance devienne faible ou que l'effet soit décisif), enchaîne des combos (givre ou distorsion puis dash, onde sur une cible au bord), anticipe les déplacements, esquive 80 % des vagues et adapte force et angle de lancement au stadium et à la mêlée |

Mesures (3 000 duels, toupies au hasard, stadium Classique) : le difficile bat le normal dans
~70 % des cas et le facile dans ~80 % ; le normal bat le facile dans ~80 % des cas. En mêlée à
4, un bot difficile gagne ~42 % des parties contre trois bots normaux (part équitable 25 %).

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
  clients envoient leur visée (angle, force), leur pilotage, leurs dashs et coups spéciaux
  (protocole v3 : une ancienne version ne peut pas rejoindre un lobby récent).
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
| `tst_battle` | formules, dash, pilotage, recentrage, force de lancer, coups spéciaux (une fois par partie, coût) et effets, règles, stadiums, mêlées, abandons, déterminisme, niveaux de bot ordonnés |
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
