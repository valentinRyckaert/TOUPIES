# Changelog

Toutes les évolutions notables de Top Arena (anciennement Toupie Arena). Le format suit
[Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le projet respecte le
[versionnage sémantique](https://semver.org/lang/fr/).

## [Non publié]

### Nouveautés

- **Le jeu s'appelle désormais Top Arena.** Les toupies sauvegardées, les paramètres et les
  mises à jour automatiques ne changent pas (identifiants et exécutable conservés).
- **Musique et effets sonores** : une boucle musicale sur les menus et une autre pendant les
  combats (coupée à la fin de la partie, écran de résultat sans musique). Effets pour les
  boutons, le compte à rebours, les chocs (selon leur violence) et le rebord, le dash, les
  duels (début, appui réussi, gagné, perdu), les éjections, la victoire et la défaite, et un
  son par coup spécial. Dans les Paramètres, la musique des menus, celle des combats et les
  effets s'activent et se règlent séparément.
- **Dépôt renommé « Top-Arena »** : les mises à jour sont cherchées à la nouvelle adresse
  (l'ancienne redirige, les versions déjà installées les trouvent aussi).
- **Solo contre 1 à 3 bots** : avec 2 ou 3 bots, le combat est une mêlée à 3 ou 4 toupies.
  Chaque bot a sa toupie (choisie ou au hasard) ; ils partagent la difficulté.
- **Traduction anglaise** : langue au choix dans les Paramètres (par défaut celle du système),
  le jeu redémarre pour l'appliquer. Noms des pièces, toupies, stadiums et coups traduits.
- **Nouvelle charte « Holo Combat »** sur tout le jeu : fond holographique, coins coupés,
  polices Chakra Petch et Rajdhani (licence OFL, embarquées), menu animé avec le logo,
  préparation du combat en cartes (camps, VS, stadiums), interface de combat refaite (fiches
  des toupies, chrono, manche), toupies redessinées (une silhouette par roue, motifs des
  parties intermédiaires, emblème du coup spécial sur le boulon), stadiums détaillés (matière
  et nom de code propres) et effet visuel propre à chaque coup spécial ; toupies gelées ou
  sonnées signalées sur l'arène.

### Modifications

- **Vortex** : l'anneau balaie tout le stadium (portée 2,2, plus rapide) et aspire vraiment ses
  cibles pendant 0,9 s, jusqu'au centre sans le dépasser ; elles ne peuvent pas dasher pendant
  l'aspiration. 20 dégâts (au lieu de 17).
- **Onde de choc** : recul plus fort (1,4 au lieu de 1,25), dégâts réduits (6 au lieu de 12,5)
  et portée un peu plus courte (0,65 au lieu de 0,75) : c'est un coup qui éjecte, pas qui use.
- **Rebord** : le percuter fait perdre un peu d'endurance (selon la vitesse, amorti par la
  défense), en jetant des étincelles ; le longer ne coûte rien.
- Le combat de bots du menu affiche les mêmes effets qu'en jeu (coups spéciaux, étincelles des
  chocs et des duels).
- Les stadiums Béton et Sable laissent voir la courbure de leur cuvette.
- Écran du tournoi : « Quitter » au lieu d'« Abandonner » (le bouton propose de sauvegarder).

### Corrections

- Le Vortex projetait ses cibles de l'autre côté du stadium, parfois dehors, au lieu de les
  ramener au centre.
- Les coups spéciaux ne repoussaient pas une toupie en plein dash (le dash effaçait le recul),
  et une toupie qui fonçait sur le lanceur amortissait le recul de sa propre vitesse. Le choc
  stoppe désormais son élan vers le lanceur avant de la repousser.

## [1.2.1] — 2026-11-05

### Corrections

- Plantage possible de la recherche des lobbys (et des tests de l'interface sous macOS) : quand
  de nombreuses sondes réseau échouaient immédiatement (limite de fichiers ouverts, adresses
  refusées), elles s'enchaînaient en appels imbriqués jusqu'à déborder la petite pile du fil de
  balayage. Les sondes s'enchaînent désormais sans récursion, et au plus 128 à la fois.

## [1.2.0] — 2026-11-05

### Modifications

- **Coups spéciaux** : ils ne coûtent plus que le quart de l'endurance du moment (au lieu de la
  moitié), et sont rééquilibrés pour que leurs effets comptent plus que leurs dégâts. Ils sont
  désormais lancés en milieu de partie plutôt que pour achever l'adversaire.
  - Lance d'acier : 24 dégâts (au lieu de 42), un peu plus lente ;
  - Souffle de givre : 18,5 dégâts, et la toupie gelée s'use trois fois plus vite ;
  - Vortex : 17 dégâts, aspiration plus forte ;
  - Distorsion : 23 dégâts, la cible repart vers l'extérieur (risque d'éjection) ;
  - Siphon : 12 dégâts, rend 60 % de l'endurance volée ;
  - Séisme : 18 dégâts, désarme 6 secondes ;
  - Onde de choc : 12,5 dégâts.
- **Recherche des lobbys en réseau local** plus rapide : les hôtes répondent directement aux
  questions des clients (ce qui passe le pare-feu de Windows), et le réseau local est sondé
  toutes les 4 s au lieu de 30.

### Corrections

- Le menu de recherche des lobbys ne ralentit plus le jeu (sous Windows notamment) : le
  balayage du réseau tourne dans un fil séparé et ignore les cartes réseau virtuelles
  (Hyper-V/WSL, VirtualBox, VMware…).

## [1.1.0] — 2026-11-04

### Ajouts

- **Mises à jour intégrées** : le bouton « Mises à jour » du menu vérifie la dernière release,
  télécharge le paquet de votre système, remplace l'ancienne version et relance le jeu
  (AppImage sous Linux, zip sous Windows, dmg sous macOS). Une installation qui ne peut pas se
  remplacer seule (compilée depuis les sources, dossier protégé) propose la page de
  téléchargement.
- **Paramètres** : toutes les touches se configurent, avec deux entrées par action (touche du
  clavier ou bouton de la souris). Une touche déjà utilisée est retirée de l'action précédente ;
  Échap reste réservée pour quitter. Bouton pour revenir aux touches par défaut.
- **Aide** : récapitulatif des commandes (visée, combat, duel, fin de combat) avec les touches
  actuellement configurées.
- **Tutoriel** : un combat scénarisé contre une toupie d'entraînement à la trajectoire prévue.
  Des bulles présentent chaque action (visée, force, lancer, pilotage, dash, coup spécial, duel)
  et mettent le jeu en pause jusqu'à ce que vous appuyiez sur la touche présentée. Le duel est
  guidé : une bulle l'explique, chaque erreur est expliquée sans être comptée, et la victoire
  éjecte la toupie adverse.

### Modifications

- Les indications à l'écran (visée, dash, coup spécial, duel, boutons du combat) affichent les
  touches configurées.
- Menu principal : « Tutoriel » rejoint les boutons principaux ; Paramètres, Aide, Mises à jour
  et Quitter forment une rangée compacte en dessous.
- Les paquets de release sont compilés avec la version de leur tag, pour la recherche de mises à
  jour.

### Note de mise à jour

La version 1.0.0 ne sait pas encore se mettre à jour : téléchargez la 1.1.0 une fois depuis la
page des releases. Les versions suivantes pourront s'installer depuis le jeu.

## [1.0.0] — 2026-11-04

Première version publique.

### Ajouts

- **Combat de toupies** en 2D vu du dessus : toupies de quatre pièces (boulon, roue, partie
  intermédiaire, pointe), types Attaque / Défense / Endurance / Équilibre, sens de rotation,
  lancer avec direction et force, pilotage aux flèches, dash, et sept **coups spéciaux** (vagues
  et anneaux à esquiver, une fois par partie, au prix de la moitié de l'endurance).
- **Duels** : deux toupies qui se percutent en plein dash s'affrontent dans un duel de timing
  (zone verte qui rétrécit, 1,5× les dégâts du dash adverse pour le perdant).
- **Six stadiums** aux formes et propriétés différentes, et une section **Personnalisation**
  (vitesse, rebonds, usure, dégâts, endurance, durée, pilotage, coups spéciaux…).
- **Bot** en trois niveaux, avec des tactiques adaptées à sa toupie (dash et coup spécial
  calculés, contre-dash, timing en duel selon le niveau).
- **Multijoueur en réseau local** de 2 à 4 joueurs : création de lobby, découverte automatique
  (UDP et balayage TCP), connexion par nom ou par adresse IP.
- **Tournois** solo ou en réseau local, de 4 à 32 participants : ligue ou élimination directe,
  matchs express (une toupie, 2 manches) ou classiques (trois toupies, 4 points), niveau de chaque
  bot réglable.
- **Atelier** pour assembler et enregistrer ses toupies.
- **Paquets** Linux (AppImage), Windows (zip) et macOS (dmg) publiés par la CI.

[1.2.1]: https://github.com/valentinRyckaert/Top-Arena/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/valentinRyckaert/Top-Arena/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/valentinRyckaert/Top-Arena/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/valentinRyckaert/Top-Arena/releases/tag/v1.0.0
