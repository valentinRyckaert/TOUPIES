# Toupie Arena

Jeu de combat de toupies en 2D, vu du dessus, écrit en C++17 avec Qt 6 (Widgets).

Fonctionnalités :

- **Jouer une partie contre le bot** : choisissez votre toupie, celle du bot (ou au hasard),
  la difficulté et le stadium, visez, lancez, puis dashez au bon moment.
- **Multijoueur en réseau local** (2 à 4 joueurs) : créez un lobby ou rejoignez-en un, par
  son nom ou par adresse IP ; chacun choisit sa toupie parmi ses propres toupies.
- **Assembler une toupie** : combinez une roue métallique, une partie intermédiaire et une
  pointe, consultez les statistiques obtenues et sauvegardez le résultat.
- **Six stadiums** aux formes et propriétés différentes (Classique, Béton, Terre, Glace,
  Sable, Volcan).

## Compilation

Prérequis : CMake ≥ 3.21, un compilateur C++17, Qt ≥ 6.4 (modules Core, Gui, Network, Widgets, Test).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/src/toupie_arena          # le jeu
ctest --test-dir build            # les tests unitaires
./build/tools/toupie_balance_sim  # le simulateur d'équilibrage
```

Les toupies assemblées sont enregistrées en JSON dans le dossier de données de l'application
(`~/.local/share/ToupieArena/ToupieArena/tops.json` sous Linux).

## Commandes

| Phase      | Action                                            |
|------------|---------------------------------------------------|
| Visée      | souris ou `←` `→` pour orienter, clic ou `Entrée` pour lancer |
| Combat     | `Espace` ou clic : dash                           |
| Fin        | `R` : revanche (solo) ou retour au lobby (hôte)   |
| Partout    | `Échap` : retour au menu / quitter le lobby       |

## Règles

### Toupies et pièces

Une toupie est l'assemblage de trois pièces : **roue métallique**, **partie intermédiaire** et
**pointe**. Chaque pièce a un niveau d'attaque, de défense et d'endurance ; les stats de la
toupie sont la somme de celles de ses pièces. La roue métallique fixe le sens de rotation.

| nom          | attaque | défense | endurance |
|--------------|---------|---------|-----------|
| Rempart (RM1)| 3       | 5       | 2         |
| Noyau (PI1)  | 2       | 3       | 1         |
| Pivot (P1)   | 1       | 4       | 2         |
| **Toupie**   | **6**   | **12**  | **5**     |

Le **type** d'une toupie est sa stat strictement la plus élevée (ici : Défense). Si deux stats
ou plus sont à égalité au sommet, la toupie est de type **Équilibre**.

**Budget de points** : toutes les pièces d'un même emplacement totalisent le même nombre de
points (roue 10, intermédiaire 6, pointe 7). Toutes les toupies valent donc 23 points :
assembler consiste à *répartir*, jamais à accumuler. Le catalogue compte 14 roues,
10 parties intermédiaires et 10 pointes, soit 1 400 combinaisons possibles.

### Le combat

Le stadium est une cuvette vue du dessus (les distances sont exprimées en rayons de stadium).
Les 2 à 4 toupies partent à égale distance du centre, régulièrement réparties, dans la
direction choisie par chaque joueur.

- **Déplacement** : la pente ramène les toupies vers le centre, un frottement les freine et
  leur rotation les propulse le long du stadium **dans le sens inverse de leur rotation**
  (une toupie qui tourne dans le sens horaire parcourt le stadium dans le sens anti-horaire).
  Une toupie avec plus d'attaque se déplace plus vite.
- **Endurance** : chaque toupie démarre à 100 et perd de l'endurance en continu, d'autant
  moins que sa stat d'endurance est élevée et d'autant plus qu'elle se déplace vite.
  Sous 25 %, elle vacille.
- **Chocs** : au contact, chaque toupie perd de l'endurance selon l'attaque de l'adversaire et
  sa propre défense, et elle est repoussée d'autant moins que sa défense est élevée.
  Deux toupies de même sens de rotation se repoussent plus fort ; deux toupies de sens
  opposés s'usent davantage.
- **Dash** : ruée rapide sur une courte distance (~0,4) vers l'adversaire. Le dash coûte de
  l'endurance, double presque l'attaque pendant un court instant et a un temps de recharge.
- **Rebord** : sur un stadium avec rebord, une toupie qui atteint le bord lentement rebondit ;
  si elle y arrive trop vite, elle le franchit. Sans rebord, rien ne la retient.
- **Défaite** : une toupie éjectée du stadium ou à court d'endurance est éliminée ; la
  dernière toupie en jeu gagne. Après 90 s, la toupie qui a le plus d'endurance gagne.
- **Mêlée (3-4 toupies)** : le dash vise l'adversaire le plus proche ; éliminer un adversaire
  qu'on a frappé dans les 4 dernières secondes rend 35 points d'endurance.

### Stadiums

| Stadium   | Forme     | Rebord   | Caractéristiques                                                    |
|-----------|-----------|----------|---------------------------------------------------------------------|
| Classique | cercle    | oui      | référence, aucun modificateur                                       |
| Béton     | octogone  | haut     | sol lisse : toupies rapides, chocs violents (+20 %), usure +10 %   |
| Terre     | cercle    | non      | sol meuble : frottement +15 %, vitesse −10 %, dash −5 %, usure +20 %, dégâts +15 % |
| Glace     | ellipse   | haut     | glissant : frottement −20 %, usure −30 %, recul −15 %, dégâts −10 % |
| Sable     | hexagone  | très haut| frottement +30 %, usure +30 %, dégâts +30 %, dash plus court mais 2× plus puissant |
| Volcan    | petit cercle | non   | pentes raides (+40 %), dégâts +25 %, dash bien plus puissant        |

Les valeurs exactes sont dans [`src/game/Stadium.cpp`](src/game/Stadium.cpp) ; l'écran de
préparation et le lobby en affichent le résumé.

### Formules

Avec `A`, `D`, `E` les stats, `v` la vitesse et `ratio` l'endurance restante / 100 :

```
attaque effective   = A × (bonus de dash si actif) × (0,6 + 0,4 × ratio)
perte passive / s   = base × max(0,15 ; 1 − kE × E) × (1 + drain × v)
impact              = min(impactMax ; impactMin + vitesseDeRapprochement / impactRef)
dégâts subis        = base × attaqueEffectiveAdverse × impact
                      × max(0,15 ; 1 − kD × D) × (1,2 si sens de rotation opposés)
recul subi          = (base + kA × attaqueEffectiveAdverse + kM × DéfenseAdverse
                       + kV × vitesseDeRapprochement)
                      × max(0,15 ; 1 − kR × D) × (1,1 si même sens de rotation)
```

Les toupies de type Équilibre subissent une usure, des dégâts et un recul légèrement réduits
(bonus de stabilité). Toutes les constantes sont regroupées et commentées dans
[`src/game/BattleConfig.h`](src/game/BattleConfig.h).

### Équilibrage

Les effets des stats sont **linéaires** : investir dans une seule stat n'a pas de rendement
décroissant face à une répartition équilibrée. Les types forment un pierre-feuille-ciseaux :

- **Attaque > Endurance** : une toupie endurance, peu défensive, encaisse mal les chocs et
  se fait éjecter ;
- **Défense > Attaque** : la défense absorbe les chocs pendant que l'attaque s'épuise en
  dashs et en vitesse ;
- **Endurance > Défense** : la défense frappe trop peu fort pour empêcher l'endurance de
  tenir plus longtemps ;
- **Équilibre** : pas de point fort ni de faiblesse marqués, et un bonus de stabilité.

Les constantes ont été réglées avec le simulateur `toupie_balance_sim`, qui fait s'affronter
des bots sur des milliers de combats entre toupies tirées au hasard parmi toutes les
combinaisons possibles et mesure les taux de victoire par type :

```sh
./build/tools/toupie_balance_sim -n 0 -r 10000              # matrice des types (Classique)
./build/tools/toupie_balance_sim -n 0 -r 4000 -s all        # résumé par stadium
./build/tools/toupie_balance_sim -n 0 -r 2000 -s all -p 4   # mêlées à 4
./build/tools/toupie_balance_sim -n 200 -r 0 dashCost=8     # presets, avec une surcharge
```

**Duels** — taux de victoire global de chaque type (4 000 combats par stadium, bots normaux) :

| Stadium   | Attaque | Défense | Endurance | Équilibre | Éjections | Durée moy. |
|-----------|---------|---------|-----------|-----------|-----------|------------|
| Classique | 45,2 %  | 52,6 %  | 53,4 %    | 48,8 %    | 42 %      | 23 s       |
| Béton     | 53,7 %  | 51,5 %  | 45,9 %    | 46,8 %    | 56 %      | 18 s       |
| Terre     | 42,3 %  | 49,7 %  | 60,0 %    | 48,2 %    | 30 %      | 22 s       |
| Glace     | 50,0 %  | 57,7 %  | 41,3 %    | 51,3 %    | 58 %      | 24 s       |
| Sable     | 46,4 %  | 46,0 %  | 61,3 %    | 44,1 %    | 24 %      | 21 s       |
| Volcan    | 53,5 %  | 54,4 %  | 41,3 %    | 50,4 %    | 57 %      | 18 s       |

Chaque stadium avantage légèrement certains types (Béton : attaque ; Terre et Sable :
endurance ; Glace : défense ; Volcan : attaque et défense) sans qu'aucun ne descende sous
~41 % ni ne dépasse ~62 %. Les résultats varient de quelques points selon la difficulté des
bots. Les toupies les plus spécialisées subissent logiquement des contres plus marqués.

**Mêlées à 4** — part des victoires de chaque type rapportée à une part équitable (100 %) :

| Stadium   | Attaque | Défense | Endurance | Équilibre |
|-----------|---------|---------|-----------|-----------|
| Classique | 63 %    | 114 %   | 121 %     | 107 %     |
| Béton     | 70 %    | 122 %   | 107 %     | 102 %     |
| Terre     | 71 %    | 107 %   | 124 %     | 99 %      |
| Glace     | 74 %    | 115 %   | 111 %     | 101 %     |
| Sable     | 79 %    | 114 %   | 107 %     | 101 %     |
| Volcan    | 78 %    | 124 %   | 96 %      | 105 %     |

En mêlée, les toupies d'attaque restent désavantagées : encaissant les chocs de plusieurs
adversaires, elles gagnent environ 1 partie sur 6 au lieu d'1 sur 4. La récompense
d'élimination et le ciblage des proies fragiles par le bot ont réduit l'écart (elles
partaient de ~48 %), sans le combler.

## Le bot

Le bot choisit son angle de lancement, le moment de ses dashs et leur cible selon le type de
sa toupie :

- **Attaque** : vise l'adversaire et dashe dès qu'il est à portée ; en mêlée, il cible les
  proies fragiles (peu de défense, au bord, à court d'endurance) ;
- **Défense** : se place au centre et contre-attaque quand l'adversaire arrive ou approche du
  bord ;
- **Endurance** : orbite à l'écart et ne dashe que pour éjecter un adversaire au bord ;
- **Équilibre** : opportuniste.

Quel que soit son style, il passe à l'offensive s'il sait qu'il perdra à l'usure. Il évite
les dashs qui l'enverraient lui-même hors du stadium (prudence accrue sans rebord). La
difficulté règle son temps de
réaction (0,5 / 0,25 / 0,1 s), la précision de son lancer et son anticipation.

## Multijoueur en réseau local

Depuis le menu **Multijoueur en réseau local** :

- **Créer un lobby** : nom du lobby, nombre de joueurs (2 à 4) et stadium. L'hôte est le
  premier joueur ; l'écran du lobby affiche les adresses à communiquer aux autres. Il peut
  encore changer le stadium et le nombre de joueurs (pas en dessous du nombre de connectés).
- **Rejoindre un lobby** : dans la liste des lobbys détectés sur le réseau, en tapant le
  **nom du lobby**, ou en tapant une **adresse IP** (`192.168.1.20` ou `192.168.1.20:45455`).
- Dans le lobby, chaque joueur choisit sa toupie **parmi ses propres toupies** (presets et
  toupies assemblées sur sa machine) et se déclare prêt. L'hôte lance la partie quand le lobby
  est complet et que tout le monde est prêt. Après le combat, l'hôte ramène tout le monde au
  lobby pour la revanche.
- Un joueur qui se déconnecte en cours de partie est éliminé (abandon). Si l'hôte ferme le
  lobby, les clients en sont avertis.

Fonctionnement :

- **L'hôte fait autorité** : il simule le combat et diffuse l'état 30 fois par seconde ; les
  clients n'envoient que leur angle de lancement et leurs dashs, et prolongent les
  trajectoires entre deux mises à jour. Un joueur qui n'a pas visé après 30 s est lancé vers
  le centre.
- **Ports** : TCP 45455 (ou le suivant libre jusqu'à 45464) pour le lobby, UDP 45454 pour la
  découverte (diffusion d'une annonce par seconde). Le pare-feu doit les autoriser sur le
  réseau local. Plusieurs instances peuvent tourner sur une même machine (tests en local
  avec `127.0.0.1`).
- **Compatibilité** : à la connexion, le client envoie la version du protocole et une
  empreinte du catalogue de pièces ; l'hôte refuse les versions différentes, puisque les
  toupies sont échangées sous forme d'identifiants de pièces.

## Architecture

```
src/
  core/     modèle sans dépendance graphique : Stats, Part, Top, PartCatalog, TopRepository
  game/     moteur de combat déterministe (pas de temps fixe) : Battle (2 à 4 toupies),
            BattleConfig, Stadium (formes et modificateurs), BotController
  session/  MatchSession (état affiché + actions du joueur local), LocalSession (solo)
  net/      réseau local : Protocol, LobbyServer, LobbyClient, LobbyBrowser (découverte UDP),
            HostSession / ClientSession (combat en réseau), MatchCodec
  ui/       interface Qt Widgets : MainWindow, pages (menu, assemblage, préparation, combat,
            multijoueur, lobby), ArenaWidget, StadiumPainter, StatsWidget, TopPainter
tests/      tests unitaires Qt Test (modèle, moteur, réseau)
tools/      simulateur d'équilibrage
```

`toupie_core` (modèle, moteur, sessions) ne dépend que de Qt Core et `toupie_net` ajoute
Qt Network : l'arène affiche de la même façon un combat solo, hébergé ou rejoint.
