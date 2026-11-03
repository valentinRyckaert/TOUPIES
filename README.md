# Toupie Arena

Jeu de combat de toupies en 2D, vu du dessus, écrit en C++17 avec Qt 6 (Widgets).

Deux fonctionnalités :

- **Jouer une partie** : choisissez votre toupie, celle du bot (ou au hasard) et la
  difficulté, visez, lancez, puis dashez au bon moment.
- **Assembler une toupie** : combinez une roue métallique, une partie intermédiaire et une
  pointe, consultez les statistiques obtenues et sauvegardez le résultat.

## Compilation

Prérequis : CMake ≥ 3.21, un compilateur C++17, Qt ≥ 6.4 (modules Core, Gui, Widgets, Test).

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
| Fin        | `R` : revanche                                    |
| Partout    | `Échap` : retour au menu                          |

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
assembler consiste à *répartir*, jamais à accumuler. Le catalogue compte 8 roues,
6 parties intermédiaires et 6 pointes, soit 288 combinaisons.

### Le combat

Le stadium est un cercle de rayon 1 (toutes les distances sont exprimées en rayons de stadium)
en forme de cuvette. Les deux toupies partent de part et d'autre du centre, dans la direction
choisie par chaque joueur.

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
- **Rebord** : une toupie qui atteint le bord lentement rebondit ; si elle y arrive trop vite,
  elle le franchit.
- **Défaite** : une toupie éjectée du stadium ou à court d'endurance perd. Après 90 s, la
  toupie qui a le plus d'endurance gagne.

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
./build/tools/toupie_balance_sim -n 0 -r 10000            # matrice des types
./build/tools/toupie_balance_sim -n 200 -r 0 dashCost=8   # presets, avec une surcharge
```

Résultats avec les constantes actuelles (10 000 combats aléatoires par difficulté, taux de
victoire global de chaque type) :

| Bots      | Attaque | Défense | Endurance | Équilibre |
|-----------|---------|---------|-----------|-----------|
| Facile    | 43,5 %  | 51,0 %  | 54,7 %    | 50,3 %    |
| Normal    | 45,6 %  | 48,7 %  | 55,0 %    | 49,7 %    |
| Difficile | 44,5 %  | 46,4 %  | 57,1 %    | 50,9 %    |

Aucun type ne descend sous ~43 % ; les confrontations directes restent comprises entre
~28 % et ~72 % (pierre-feuille-ciseaux). Durée moyenne d'un combat : ~25 s, environ 40 %
des combats se terminent par une éjection. Les toupies les plus spécialisées (presets
14/4/5, 4/14/5, 4/4/15) subissent logiquement des contres plus marqués.

## Le bot

Le bot choisit son angle de lancement et le moment de ses dashs selon le type de sa toupie :

- **Attaque** : vise l'adversaire et dashe dès qu'il est à portée ;
- **Défense** : se place au centre et contre-attaque quand l'adversaire arrive ou approche du
  bord ;
- **Endurance** : orbite à l'écart et ne dashe que pour éjecter un adversaire au bord ;
- **Équilibre** : opportuniste.

Quel que soit son style, il passe à l'offensive s'il sait qu'il perdra à l'usure. Il évite
les dashs qui l'enverraient lui-même hors du stadium. La difficulté règle son temps de
réaction (0,5 / 0,25 / 0,1 s), la précision de son lancer et son anticipation.

## Architecture

```
src/
  core/   modèle sans dépendance graphique : Stats, Part, Top, PartCatalog, TopRepository
  game/   moteur de combat déterministe (pas de temps fixe) : Battle, BattleConfig, BotController
  ui/     interface Qt Widgets : MainWindow, pages (menu, assemblage, préparation, combat),
          ArenaWidget (rendu et contrôles), StatsWidget, TopPainter
tests/    tests unitaires Qt Test
tools/    simulateur d'équilibrage
```

Le moteur (`toupie_core`) ne dépend que de Qt Core : il est utilisé à l'identique par le jeu,
les tests et le simulateur.
