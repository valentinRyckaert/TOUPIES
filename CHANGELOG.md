# Changelog

Toutes les évolutions notables de Top Arena (anciennement Toupie Arena). Le format suit
[Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le projet respecte le
[versionnage sémantique](https://semver.org/lang/fr/).

## [Non publié]

## [2.0.0] — 2026-11-14

Toupie Arena devient **Top Arena** : refonte visuelle complète « Holo Combat », mêlées contre
plusieurs bots, traduction anglaise, trois types de duel, tournois sauvegardés et coups
spéciaux rééquilibrés. Les toupies enregistrées, les paramètres et les mises à jour
automatiques des versions 1.x sont conservés.

### Nouveautés

- **Le jeu s'appelle désormais Top Arena** et son dépôt **Top-Arena**. Les identifiants internes,
  l'exécutable (`toupie_arena`) et le dossier de données ne changent pas : les toupies
  sauvegardées et les paramètres sont repris, et les versions déjà installées trouvent cette
  mise à jour (l'ancienne adresse du dépôt redirige vers la nouvelle).
- **Charte « Holo Combat »** sur tout le jeu, d'après les maquettes de `design/` :
  - fond holographique, boutons, champs, listes et cadres à coins coupés, icônes vectorielles ;
  - polices Chakra Petch et Rajdhani embarquées (licence SIL OFL) ;
  - toupies redessinées : une silhouette par roue métallique (lames tournées dans le sens de
    rotation), motifs des parties intermédiaires, emblème du coup spécial sur le boulon ;
  - stadiums détaillés, chacun avec sa matière, son nom de code et sa couleur ;
  - effet visuel propre à chaque coup spécial ; toupies gelées ou sonnées signalées sur
    l'arène.
- **Écrans refaits** :
  - **Menu** : un vrai combat entre deux bots tourne en fond, sur un stadium au hasard, avec
    les mêmes effets qu'en jeu ; les toupies sont tirées équitablement parmi les toupies
    prédéfinies et les vôtres.
  - **Préparation du combat** en cartes : camps, VS, choix du stadium.
  - **Combat** : boutons Menu (Échap) et Changer de toupies sur l'arène, stadium et limite de
    temps en haut à droite, manche et score (ou chrono) au centre, fiches des toupies en
    miroir (endurance, état, « DASH ! » pendant un dash), carte du coup spécial cliquable,
    rappel des touches en bas.
  - **Atelier** en trois panneaux : vos toupies illustrées ; l'établi avec une carte par pièce
    (flèches ‹ › ou liste illustrée de toutes les pièces), une vue éclatée et un aperçu qui
    tourne ; les statistiques (radar, type obtenu, barres).
  - **Coups spéciaux** : une page qui détaille les sept coups (illustration de l'effet,
    emblème, point fort, faiblesse, jauges de dégâts, de recul et de portée), ouverte depuis
    l'atelier.
  - **Tournoi** : tableau d'élimination directe complet dès le premier tour (scores, chemin du
    joueur en vert, coupe du champion), qui défile pour 16 ou 32 participants ; carte « Votre
    match » ; classement de ligue avec podium, miniatures et différence de buts.
- **Solo contre 1 à 3 bots** : avec 2 ou 3 bots, le combat devient une mêlée à 3 ou 4 toupies.
  Chaque bot a sa toupie (choisie ou au hasard) ; ils partagent la difficulté.
- **Trois types de duel** quand deux dashs se croisent :
  - **Timing** (comme avant) : appuyer quand le curseur traverse la zone verte ;
  - **Rafale** : tir à la corde, chaque appui pousse le curseur vers l'adversaire ; au bout de
    3,5 s le plus avancé gagne, 12 appuis d'avance gagnent aussitôt, pile au milieu c'est nul ;
  - **Sans duel** : les toupies se repoussent comme lors d'un choc ordinaire.

  Le type par défaut se choisit dans les Paramètres ; la personnalisation d'un combat, d'un
  lobby ou d'un tournoi peut en imposer un autre. Les bots appuient plus ou moins vite en
  rafale selon leur niveau ; le tutoriel garde le duel de timing qu'il enseigne.
- **Tournois solo sauvegardés** : quitter propose « Sauvegarder et quitter », et fermer le jeu
  en plein tournoi le sauvegarde. L'écran des tournois affiche alors une carte « Reprendre »
  (format, joueurs, tour) ; un match en cours au moment de la sauvegarde est rejoué.
- **Traduction anglaise** : langue au choix dans les Paramètres (par défaut celle du système),
  le jeu redémarre pour l'appliquer. Pièces, toupies, stadiums et coups sont traduits ; le nom
  du jeu ne l'est jamais.
- **Plein écran** au démarrage ; F11 bascule entre plein écran et fenêtre.

### Modifications

- **Stadiums** : tous ont désormais la surface du stadium Classique (l'octogone, l'hexagone et
  l'ellipse étaient inscrits dans son cercle et le Volcan faisait 85 % de sa taille), et ils
  apparaissent à la même échelle à l'écran.
- **Rebord** : le percuter fait perdre un peu d'endurance (jusqu'à environ 1,7 point, selon la
  vitesse, amorti par la défense), en jetant des étincelles ; le longer ne coûte rien.
- **Vortex** : l'anneau balaie tout le stadium (portée 2,2, vitesse 1,8) et aspire vraiment ses
  cibles pendant 0,9 s jusqu'au centre, sans le dépasser ; elles ne peuvent ni piloter ni
  dasher pendant l'aspiration. 20 dégâts (au lieu de 17).
- **Onde de choc** : recul plus fort (1,4 au lieu de 1,25), dégâts réduits (6 au lieu de 12,5)
  et portée un peu plus courte (0,65 au lieu de 0,75) : c'est un coup qui éjecte, pas qui use.
- **Duels** : maintenir la touche de dash ne répète plus les appuis.
- **Atelier** : la jauge de budget disparaît (toutes les toupies totalisent 23 points).
- **Tournoi** : « Quitter » au lieu d'« Abandonner » ; quitter un tournoi en réseau local
  demande confirmation, puisque Échap le déclenche.

### Corrections

- **Coups spéciaux** : une toupie en plein dash n'était pas repoussée (le dash effaçait le
  recul), et une toupie qui fonçait sur le lanceur amortissait le recul de sa propre vitesse.
  Le choc stoppe désormais son élan vers le lanceur avant de la repousser.
- Le Vortex projetait ses cibles de l'autre côté du stadium, parfois dehors, au lieu de les
  ramener au centre.
- L'emblème du boulon tourne avec la toupie ; les lames des roues sont orientées dans le sens
  de rotation.
- Titres de fin de combat lisibles (sans effet chromatique), fissures des stadiums Terre et
  Volcan plus discrètes, courbure de la cuvette visible sur les stadiums Béton et Sable.
- Le sélecteur de langue et celui du nombre de participants d'un tournoi affichent leur texte
  en entier.
- **Windows** : la 1.2.1 n'avait pas pu être publiée pour Windows (un test réseau dépassait son
  délai sur cette plateforme). La 2.0.0 est de nouveau disponible pour Linux, Windows et macOS.

### Compatibilité

- Le multijoueur en réseau local garde le protocole 4 : une version 1.x peut encore rejoindre
  un lobby 2.0.0 (et l'application mobile aussi). L'hôte simule le combat, donc ce sont ses
  règles qui s'appliquent ; mieux vaut que tout le monde passe en 2.0.0, car une version 1.x
  ne connaît pas les nouveaux types de duel.

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

[Non publié]: https://github.com/valentinRyckaert/Top-Arena/compare/v2.0.0...HEAD
[2.0.0]: https://github.com/valentinRyckaert/Top-Arena/compare/v1.2.1...v2.0.0
[1.2.1]: https://github.com/valentinRyckaert/Top-Arena/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/valentinRyckaert/Top-Arena/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/valentinRyckaert/Top-Arena/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/valentinRyckaert/Top-Arena/releases/tag/v1.0.0
