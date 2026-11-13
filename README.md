# Top Arena

[![CI](https://github.com/valentinRyckaert/Top-Arena/actions/workflows/ci.yml/badge.svg)](https://github.com/valentinRyckaert/Top-Arena/actions/workflows/ci.yml)
[![Release](https://github.com/valentinRyckaert/Top-Arena/actions/workflows/release.yml/badge.svg)](https://github.com/valentinRyckaert/Top-Arena/actions/workflows/release.yml)

Jeu de combat de toupies en 2D, vu du dessus, écrit en C++17 avec Qt 6 (Widgets).

Fonctionnalités :

- **Jouer contre les bots** : choisissez votre toupie, de 1 à 3 bots (leur toupie, ou au
  hasard), leur difficulté, le stadium et les règles, réglez la force et la direction du
  lancer, puis pilotez, dashez et déclenchez votre coup spécial au bon moment. Avec 2 ou 3
  bots, le combat devient une mêlée à 3 ou 4 toupies : la dernière en jeu gagne.
- **Multijoueur en réseau local** (2 à 4 joueurs) : créez un lobby ou rejoignez-en un depuis
  la liste des lobbys détectés, par son nom ou par adresse IP ; chacun choisit sa toupie parmi
  ses propres toupies.
- **Tournois** (4 à 32 participants) : ligue ou élimination directe, en solo contre des bots ou
  en réseau local avec 2 à 4 joueurs humains, matchs express (une toupie, 2 manches gagnantes)
  ou classiques (trois toupies, 4 points).
- **Duels** : deux toupies qui dashent l'une vers l'autre au même moment s'affrontent dans un
  duel de timing.
- **Tutoriel** : un combat scénarisé qui présente chaque action pas à pas.
- **Paramètres et aide** : langue du jeu (français ou anglais), son (musique des menus,
  musique des combats et effets sonores, activables et réglables séparément), touches du
  clavier et boutons de la souris configurables, et page d'aide qui récapitule les commandes.
- **Musique et effets sonores** : une boucle pour les menus, une autre pendant les combats
  (coupée à la fin de la partie) ; sons des boutons, du compte à rebours, des chocs, du dash,
  des duels, des éjections, de la victoire et de la défaite, et un son par coup spécial.
- **Charte « Holo Combat »** : reflets holographiques bleu / violet, vert fluo pour les actions
  et coins coupés ; chaque roue a sa silhouette, chaque stadium sa matière et chaque coup
  spécial son effet visuel. Maquettes de référence dans [`design/`](design/README.md).
- **Mises à jour** : le jeu vérifie s'il existe une version plus récente, la télécharge,
  remplace l'ancienne et redémarre dessus.
- **Assembler une toupie** : un boulon (coup spécial), une roue métallique, une partie
  intermédiaire et une pointe.
- **Six stadiums** aux formes et propriétés différentes, **sept coups spéciaux** et une
  section **Personnalisation** pour régler vitesse, rebonds, usure, endurance, durée…

## Compilation

Prérequis : CMake ≥ 3.21, un compilateur C++17, Qt ≥ 6.4 (modules Core, Gui, Network,
Widgets, Multimedia, Test et les outils Linguist ; sous Ubuntu : `qt6-base-dev`,
`qt6-multimedia-dev`, `qt6-tools-dev`, `qt6-tools-dev-tools`, `qt6-l10n-tools`).

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

### Traductions

Le français est la langue source ; les traductions vivent dans `src/i18n/` (format Qt
Linguist) et sont compilées dans l'exécutable. Après avoir ajouté ou modifié des textes :

```sh
lupdate src -ts src/i18n/toparena_en.ts -locations none   # puis traduire (Qt Linguist)
```

Le nom du jeu, « Top Arena », n'est jamais traduit. Changer de langue (Paramètres) relance le
jeu.

Les toupies assemblées sont enregistrées en JSON dans le dossier de données de l'application
(`~/.local/share/ToupieArena/ToupieArena/tops.json` sous Linux). Ce dossier, l'exécutable
(`toupie_arena`) et les identifiants internes gardent l'ancien nom du jeu, pour conserver les
toupies et les paramètres et pour que les mises à jour automatiques continuent de fonctionner.

### Sons

Les musiques (`src/audio/menu.wav`, `src/audio/battle.wav`, WAV 16 bits) et les effets
sonores sont compilés dans l'exécutable. Les effets sont synthétisés par
`tools/make_sounds.py` (Python, bibliothèque standard seule : sons originaux, sans licence) ;
après avoir modifié un réglage du script :

```sh
python3 tools/make_sounds.py   # réécrit les effets dans src/audio/
```

## Commandes

Touches par défaut ci-dessous. Toutes (sauf `Échap`) se changent dans **Paramètres** (menu
principal) : chaque action accepte deux entrées, touche du clavier ou bouton de la souris. La page
**Aide** récapitule les commandes avec les touches actuellement configurées.

| Phase  | Action |
|--------|--------|
| Visée  | souris : direction, et **force** selon la distance du curseur (flèche plus ou moins longue) ; ou `←` `→` pour la direction et `↑` `↓` pour la force ; clic ou `Entrée` pour lancer |
| Combat | **flèches** (ou `Z` `Q` `S` `D`) maintenues : pilotage, légère correction de trajectoire |
|        | `Espace` ou clic : dash ; pendant un **duel**, appui quand le curseur est dans le vert |
|        | `E` ou bouton « Coup spécial » : coup spécial (une fois par partie, coûte le quart de l'endurance) |
| Fin    | `R` : revanche (solo) ou retour au lobby (hôte) |
| Partout| `Échap` : retour au menu / quitter le lobby |

## Tutoriel

Le bouton **Tutoriel** du menu lance un combat scénarisé contre une toupie d'entraînement dont la
trajectoire est prévue à l'avance (lancer fixe, puis approche lente vers vous). Une bulle présente
chaque action : visée (à la souris en cliquant, ou au clavier), force, lancer, pilotage, dash,
coup spécial et duel (la toupie d'entraînement répond à votre dash au même instant). Pendant le
combat, chaque bulle **met le jeu en pause** jusqu'à ce que vous appuyiez sur la touche présentée,
qui exécute l'action ; les autres touches sont ignorées. Les bulles affichent les touches
configurées dans les paramètres.

Le duel est guidé : une bulle l'explique dès qu'il commence, et chaque appui hors de la zone verte
ouvre une bulle qui explique l'erreur sans la compter. Vous gagnez donc toujours le duel ; la toupie
d'entraînement est alors éjectée du stadium et le tutoriel se termine sur votre victoire.

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
- **Duel** : si deux toupies se percutent **en plein dash toutes les deux**, elles restent au
  contact (elles tremblent et des étincelles jaillissent du point de friction) et chaque
  joueur voit apparaître une barre rouge avec une zone verte au centre et un curseur qui fait
  des allers-retours (la sienne en bas de l'écran, celle de l'adversaire juste au-dessus, en
  plus petit). Il faut appuyer (`Espace` ou clic) quand le curseur est dans le vert ; la zone
  rétrécit à chaque réussite jusqu'à une taille minimale et le curseur accélère un peu. Le
  premier qui rate (ou qui n'appuie pas pendant 3 s) perd le duel et subit **1,5× les dégâts**
  du dash adverse, avec le recul correspondant. Si les deux réussissent **3 appuis d'affilée à
  la taille minimale**, c'est un nul : les toupies se repoussent sans dégâts.
- **Rebord** : une toupie qui atteint lentement un rebord rebondit ; trop vite, elle le
  franchit. Sans rebord, rien ne la retient.
- **Défaite** : éjectée ou à court d'endurance. La dernière toupie en jeu gagne. Par défaut il
  n'y a **pas de limite de temps** ; si une limite est choisie, la toupie qui a le plus
  d'endurance à la fin gagne.
- **Mêlée (3-4 toupies)** : éliminer un adversaire frappé dans les 4 dernières secondes rend
  35 points d'endurance (sauf pour l'élimination qui termine le combat).

### Coups spéciaux

Chaque toupie peut lancer le coup spécial de son boulon **une seule fois par partie**, et ce
lancer lui coûte **le quart de son endurance du moment** : à 100 d'endurance on en perd 25, à
40 on n'en perd que 10. Le lancer dès le début reste coûteux, mais il est rentable en milieu
de partie, quand son **effet** compte : geler une toupie qui fuit, téléporter un adversaire
près du bord, le désarmer avant qu'il ne dashe… Les dégâts sont volontairement modérés, pour
que le coup ne serve pas qu'à achever un adversaire en fin de combat.

Le coup part sous forme d'une **vague** qui file vers l'adversaire le plus proche (sans le
suivre : on peut l'esquiver en pilotant) ou d'un **anneau** qui s'élargit et touche tous les
adversaires jusqu'à sa portée. La défense de la cible réduit dégâts et recul.

| Boulon    | Coup             | Forme  | Point fort | Contrepartie |
|-----------|------------------|--------|------------|--------------|
| Tempête   | Onde de choc     | anneau | gros recul : éjecte une cible déjà au bord | portée courte |
| Perçant   | Lance d'acier    | vague  | les plus lourds dégâts (24) | aucun effet, presque aucun recul, esquivable de loin |
| Polaire   | Souffle de givre | vague  | 3 s gelée : ralentie, sans pilotage, et usée trois fois plus vite | dégâts moyens |
| Abysse    | Vortex           | anneau | balaie tout le stadium et aspire tout le monde vers le centre (pas de dash pendant l'aspiration) | dégâts modérés, éloigne les cibles du bord |
| Spectral  | Distorsion       | vague  | téléporte la cible près du bord, lancée vers l'extérieur (risque d'éjection) | lente, facile à esquiver |
| Vampire   | Siphon           | vague  | rend 60 % de l'endurance volée | dégâts modérés, peu de recul |
| Tellurique| Séisme           | anneau | blesse et bloque dash + coup spécial 6 s | portée très courte |

### Personnalisation

Bouton « Personnalisation… » de la préparation du combat (solo), du tournoi solo et du panneau
de réglages du lobby (réglée par l'hôte, visible par tous) :
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
| Classique | 51,5 %  | 49,4 %  | 48,2 %    | 51,4 %    | 35 %      | 18 s       |
| Béton     | 50,4 %  | 49,0 %  | 50,0 %    | 51,1 %    | 28 %      | 18 s       |
| Terre     | 51,4 %  | 48,7 %  | 49,7 %    | 50,1 %    | 29 %      | 15 s       |
| Glace     | 50,7 %  | 50,6 %  | 47,8 %    | 51,6 %    | 33 %      | 21 s       |
| Sable     | 53,4 %  | 46,5 %  | 49,3 %    | 51,1 %    | 27 %      | 16 s       |
| Volcan    | 54,8 %  | 46,9 %  | 43,6 %    | 58,0 %    | 23 %      | 15 s       |

**Coups spéciaux** — le simulateur mesure aussi quand ils sont lancés et s'ils servent surtout
à achever (« coup de grâce » : cible éliminée dans la seconde qui suit la touche) :

| Réglage | Endurance du lanceur au lancer | Moment du lancer (part du combat) | Coups de grâce | Intérêt (bot difficile) |
|---------|--------------------------------|-----------------------------------|----------------|-------------------------|
| Avant : coût ½, dégâts forts | 22 % | 95 % | 45 % | 67 % |
| Maintenant : coût ¼, dégâts modérés, effets renforcés | 42 % | 69 % | 8 % | 65 % |

(Intérêt : victoires d'un bot qui utilise son coup contre le même bot qui s'en prive.) Avec un
coût de ½, le coup ne coûtait presque plus rien en fin de partie et servait surtout à achever ;
au quart, il est lancé en milieu de partie, quand ses effets ont le temps de compter (les
éjections entre bots difficiles passent de 1 % à 9 %). Taux de victoire par coup : de 37 %
(Séisme) à 54 % entre bots normaux, de 39 % (Distorsion) à 65 % (Lance) entre bots
difficiles, qui gagnent à l'usure et profitent donc surtout des dégâts.

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
| Difficile | **chasseur** (attaque) qui se place côté centre de sa cible pour la pousser vers le bord ; **enclume** (défense) qui tient le centre ; **esquive** (endurance) qui fuit les attaquants et frappe en fin de partie ; équilibre qui choisit selon la course à l'usure. Il estime la valeur nette de chaque **dash** (dégâts infligés − subis − coût, éjection probable, risque de s'éjecter) et de son **coup spécial** (avec une marge de sécurité : il attend que le coût de la moitié de l'endurance devienne faible ou que l'effet soit décisif), enchaîne des combos (givre ou distorsion puis dash, onde sur une cible au bord), anticipe les déplacements, esquive 80 % des vagues et adapte force et angle de lancement au stadium et à la mêlée. Il **contre-dashe** par réflexe quand un adversaire fonce sur lui, pour provoquer un duel |

En **duel**, chaque bot appuie quand son curseur passe au centre de la zone verte, avec une
erreur de timing selon son niveau (écart-type de 75 ms en facile, 45 ms en normal, 25 ms en
difficile) : plus la zone rétrécit et plus le curseur accélère, plus les bots faibles ratent.

Mesures (3 000 combats, toupies au hasard, stadium Classique) : le difficile bat le normal dans
~75-80 % des cas et le facile dans ~97 % ; le normal bat le facile dans ~80 % des cas. En mêlée
à 4, un bot difficile gagne ~49 % des parties contre trois bots normaux (part équitable 25 %).

## Tournois

Le bouton **Tournois** du menu ouvre la section des tournois : on y choisit de jouer **seul
contre des bots** ou **en multijoueur** (réseau local), puis :

- **Format** : **ligue** (chacun rencontre chacun une fois, avec une exemption par journée si le
  nombre de participants est impair) ou **élimination directe** (tableau complet tiré au sort).
- **Participants** : de 4 à 32 en ligue, 4, 8, 16 ou 32 en élimination directe ; dont 1 joueur
  humain en solo ou 2 à 4 en réseau. Les autres sont des bots dont on règle le **niveau un par
  un** (ou tous d'un coup) dans la fenêtre « Bots ».
- **Type de match**, le même pour tout le tournoi :
  - **Express** : une toupie pour tout le tournoi ; le premier à gagner **2 manches** remporte le
    match (une manche nulle ne compte pas) ;
  - **Classique** : trois toupies pour le tournoi ; avant chaque match, chaque joueur choisit
    l'**ordre** dans lequel il les jouera (sans voir celui de l'adversaire ; après la 3e on
    reprend la 1re). Le premier à **4 points** gagne : arrêter la toupie adverse rapporte
    **1 point**, l'éjecter **2 points**.
- **Classement de la ligue** : chaque joueur ajoute ce qu'il a gagné dans le match, vainqueur
  comme perdant. En express ce sont les manches (A bat B 2-1 : A marque 2 points, B 1 point), en
  classique les points du match. Départage : victoires, puis différence.
- Le stadium et la personnalisation sont communs à tout le tournoi.

Le tableau de bord du tournoi montre les tours et les résultats, le classement (ligue) et
votre prochain match (les tours se replient dans un panneau latéral). En express, **Prêt** lance
le match ; en classique, une fenêtre permet de ranger ses toupies (Monter / Descendre) avant de
valider. Les manches
s'enchaînent automatiquement, le score du match est affiché pendant la manche et à la fin de
chacune. « Abandonner le match » le donne à l'adversaire. Les matchs entre bots sont simulés
instantanément.

**En réseau local**, l'hôte crée un lobby de tournoi (les réglages du tournoi sont modifiables
dans le lobby) ; chaque joueur humain y choisit sa ou ses toupies parmi les siennes. Les matchs
humains d'un même tour se jouent **en parallèle** (l'hôte les simule tous) ; un joueur qui
attend voit le tournoi avancer. Un joueur qui se déconnecte est remplacé par un bot normal pour
la suite du tournoi. À la fin, l'hôte ramène tout le monde au lobby.

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
  clients envoient leur visée (angle, force), leur pilotage, leurs dashs, coups spéciaux et
  appuis en duel (avec la position du curseur qu'ils voyaient, pour ne pas être pénalisés par
  la latence). En tournoi, l'hôte diffuse l'état du tournoi et n'envoie l'état d'un combat
  qu'à ses participants (protocole v4 : une ancienne version ne peut pas rejoindre un lobby
  récent).
- **Découverte des lobbys** : les hôtes diffusent une annonce UDP par seconde (port 45454), et
  les clients diffusent une question toutes les 2 s à laquelle les hôtes répondent directement :
  une réponse passe souvent un pare-feu qui bloque les annonces entrantes (celui de Windows en
  réseau « public » notamment). En secours, le jeu sonde les sous-réseaux locaux en TCP, dans
  un fil séparé pour ne jamais ralentir l'interface : le /24 de la machine toutes les 4 s, le
  reste du sous-réseau (jusqu'à un /20) toutes les 30 s ou avec « Actualiser ». Les cartes
  réseau virtuelles (Docker, Hyper-V/WSL, VirtualBox, VMware…) sont ignorées. Un lobby
  joignable par IP est donc aussi listé.
- **Ports** : TCP 45455 (ou le suivant libre jusqu'à 45464) pour le lobby, UDP 45454 pour les
  annonces. Avec firewalld par exemple :
  `sudo firewall-cmd --permanent --add-port=45455/tcp --add-port=45454/udp && sudo firewall-cmd --reload`.
- **Compatibilité** : le client envoie la version du protocole et une empreinte du catalogue
  de pièces ; l'hôte refuse les versions différentes.

## Mises à jour

Le bouton **Mises à jour** du menu interroge la dernière release GitHub du projet et compare
son tag à la version du jeu (la CI de release compile chaque paquet avec la version de son tag).
Si une version plus récente existe, le jeu propose de télécharger le paquet de la plateforme,
remplace l'ancienne version puis redémarre sur la nouvelle :

| Système | Paquet | Remplacement |
|---------|--------|--------------|
| Linux   | AppImage | la nouvelle AppImage est téléchargée à côté de l'ancienne puis la remplace (renommage atomique) et se lance |
| Windows | zip | un script PowerShell attend la fermeture du jeu, écrase les fichiers du dossier avec ceux du zip et relance le jeu |
| macOS   | dmg | un script attend la fermeture du jeu, copie le nouveau `.app` depuis le dmg à côté de l'ancien, le remplace (l'ancien n'est supprimé que si la copie a réussi) et le relance |

Une installation qui ne peut pas se remplacer seule (jeu compilé depuis les sources, paquet de
distribution, dossier en lecture seule) propose d'ouvrir la page de téléchargement.

## Intégration et livraison continues

- **CI** ([`ci.yml`](.github/workflows/ci.yml), à chaque push sur `main` et pull request) :
  - Linux avec le Qt 6.4 de la distribution (version minimale), Debug et Release, avertissements
    traités comme des erreurs : compilation, tests unitaires, d'intégration réseau et
    d'interface (plateforme `offscreen`), démarrage de l'application, simulateur ;
  - Linux avec AddressSanitizer et UndefinedBehaviorSanitizer ;
  - Linux, Windows (MSVC) et macOS avec Qt 6.8 LTS : compilation, tests, binaires en artefacts.
- **CD** ([`release.yml`](.github/workflows/release.yml), à chaque tag `vX.Y.Z`) : compilation
  et tests sur les trois systèmes, paquets AppImage (Linux), zip (Windows) et dmg (macOS),
  vérification du démarrage de chaque paquet, publication dans une release GitHub dont les notes
  reprennent la section du tag dans [`CHANGELOG.md`](CHANGELOG.md).

```sh
git tag v1.2.1 && git push origin v1.2.1   # déclenche la publication
```

Tests (`ctest`) :

| Suite        | Contenu |
|--------------|---------|
| `tst_core`   | stats, types, catalogue, boulons, persistance JSON |
| `tst_battle` | formules, dash, pilotage, recentrage, force de lancer, coups spéciaux (une fois par partie, coût) et effets, duels (déclenchement, raté à 1,5× les dégâts, nul, inactivité, bots), règles, stadiums, mêlées, abandons, déterminisme, niveaux de bot ordonnés |
| `tst_tournament` | score express et classique, ordre des toupies, calendrier de ligue (chacun rencontre chacun), classement, tableau à élimination directe avec exemptions, JSON, match entre bots |
| `tst_net`    | lobby (connexion, refus, déconnexion), découverte UDP et par balayage TCP, encodage de l'état (duel, score, bots), partie réseau de bout en bout, tournoi solo et tournoi en réseau, recherche de mise à jour, remplacement et relance d'une AppImage |
| `tst_gui`    | navigation, partie solo complète au clavier, atelier, lobby entre deux fenêtres avec règles de l'hôte, ouverture d'un match de tournoi, touches configurables (sauvegarde, touche reprise à une autre action, touche réassignée utilisée en combat), tutoriel joué en entier (clic pour la visée, pauses, touches ignorées, raté de duel non compté, victoire par éjection) |

## Architecture

```
src/
  core/     modèle : Stats, Part (dont boulons), Top, PartCatalog, TopRepository
  game/     moteur déterministe : Battle (dont duels), BattleConfig, Stadium, SpecialMove,
            MatchRules, BotController, Tournament (ligue, élimination, score des matchs)
  session/  MatchSession (état affiché + actions du joueur local), LocalSession (solo)
  net/      réseau local : Protocol, LobbyServer, LobbyClient, LobbyBrowser + LobbyScanner
            (découverte), HostSession / ClientSession, MatchCodec, TournamentSession
            (directeur du tournoi en solo ou chez l'hôte, suivi chez les clients)
  ui/       interface (bibliothèque toupie_ui) : MainWindow et pages, ArenaWidget,
            RulesWidget, TournamentSettingsWidget, Controls (touches), StadiumPainter, StatsWidget,
            TopPainter, Theme, Audio (musiques, effets et réglages ; BattleSounds : sons d'un combat)
  audio/    musiques et effets sonores (WAV)
  main.cpp  exécutable toupie_arena
tests/      Qt Test : modèle, moteur, réseau, interface
tools/      simulateur d'équilibrage, générateur des effets sonores (make_sounds.py)
packaging/  fichier .desktop et icône
```

`toupie_core` (modèle, moteur, sessions) ne dépend que de Qt Core, `toupie_net` ajoute
Qt Network et `toupie_ui` Qt Widgets et Qt Multimedia.
