# Top Arena — maquette « Holo Combat »

Planches SVG éditables destinées au fichier Figma « Toupie Arena — Holo Combat Design ».

## Importer dans Figma

1. Ouvrir le fichier Figma, créer les pages voulues (ex. `Direction`, `Écrans`, `Art`).
2. Glisser-déposer les fichiers de `svg/` sur le canevas : chaque SVG devient un cadre, avec des
   calques nommés (groupes = `id`) et des **textes éditables**.
3. Polices : **Chakra Petch** (titres, chiffres, boutons) et **Rajdhani** (texte courant),
   toutes deux disponibles dans Figma via Google Fonts.

| Fichier | Contenu |
|---|---|
| `00-direction-artistique` | piliers, palette, couleurs de type, typographie, langage de forme, effets |
| `01-design-system` | boutons (5 variantes × 3 états), champs, segments, curseur, interrupteurs, badges, stats, jauges d'endurance (états), dash, coup spécial, bulle, bannière, icônes |
| `02` → `09` | écrans 1920×1080 : menu, préparation, atelier, combat, duel, résultat, lobby LAN, tournoi |
| `10-toupies` | les 8 toupies pré-assemblées (vue de dessus, vue éclatée de profil, stats, pièces, coup) |
| `11-pieces` | catalogue complet : 7 boulons, 14 roues, 10 intermédiaires, 10 pointes |
| `12-stadiums` | les 6 stadiums redessinés, avec nom de code et modificateurs |
| `13-coups-speciaux` | les 7 coups spéciaux : illustration de l'effet, icône, forces/faiblesses |

## Régénérer

```sh
node gen/boards-art.js svg && node gen/boards-screens.js svg
```

Les toupies, stadiums et effets sont procéduraux (`gen/art.js`) : la silhouette de chaque roue
est un profil polaire `r(θ)`, sa teinte vient du catalogue du jeu (`src/core/PartCatalog.cpp`).
