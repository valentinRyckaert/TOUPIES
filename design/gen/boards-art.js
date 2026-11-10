'use strict';
// Planches : direction artistique, design system, toupies, pièces, stadiums, coups spéciaux.
const L = require('./lib');
const Ar = require('./art');
const { C, TYPE, F, Doc, g, rect, circle, ellipse, line, path, chamfer, skew, T, D, r2 } = L;

const OUT = process.argv[2] || `${__dirname}/../svg`;
const card = (d, x, y, w, h, o = {}) => path(chamfer(x, y, w, h, o.c ?? [18, 0, 18, 0]), {
  fill: o.fill || d.lin([[0, '#17124A', 0.95], [1, '#0C0A2A', 0.95]], 100), stroke: o.stroke || C.line, 'stroke-width': o.sw || 1.2,
});
const label = (x, y, s, col = C.muted) => T(x, y, s.toUpperCase(), { size: 13, ls: 2, fill: col, weight: 700 });
const wrap = (x, y, s, o = {}) => { // retour à la ligne simple, un <text> par ligne
  const max = o.max || 40, words = s.split(' '), lines = []; let cur = '';
  for (const w of words) { if ((cur + ' ' + w).trim().length > max) { lines.push(cur.trim()); cur = w; } else cur += ' ' + w; }
  if (cur.trim()) lines.push(cur.trim());
  return lines.map((l, i) => T(x, y + i * (o.lh || 22), l, o)).join('');
};

// ============================== 00 DIRECTION ARTISTIQUE ==============================
function boardDA() {
  const W = 2400, H = 1820, d = new Doc(W, H, '00-direction-artistique');
  d.add(L.background(d, W, H, { floor: false }));
  d.add(L.boardHeader(d, 120, 130, '00 · Direction artistique', 'Holo Combat', 'Holographique bleu/violet, accents vert fluo, bords cassants : une arène e-sport sous néons.'));
  d.add(L.logo(d, 1500, 230, 0.9));

  // Piliers
  const pillars = [
    ['Holographique', C.cyan, 'Dégradés irisés bleu → violet → magenta, reflets de foil, halos néon. La lumière vient des toupies et de l’arène, jamais d’un fond plat.'],
    ['Cassant', C.green, 'Coins coupés à 45°, parallélogrammes inclinés, barres obliques « /// ». Toujours asymétrique : on coupe haut-gauche et bas-droite.'],
    ['Compétitif', C.magenta, 'Lisible en une demi-seconde : chiffres gros, couleurs de type fixes, contraste fort sur fond quasi noir. Le vert fluo = action.'],
  ];
  pillars.forEach(([t, col, txt], i) => {
    const x = 120 + i * 720, y = 300;
    d.add(g(`Pilier ${t}`, [card(d, x, y, 680, 230), path(skew(x + 30, y + 30, 46, 8, 4), { fill: col }),
      D(x + 30, y + 84, t.toUpperCase(), { size: 32, ls: 3, fill: col }), wrap(x + 30, y + 124, txt, { size: 19, fill: C.text, weight: 500, max: 62, lh: 26 }),
      D(x + 640, y + 50, `0${i + 1}`, { size: 22, fill: C.dim, anchor: 'end' })]));
  });

  // Palette
  d.add(D(120, 620, 'PALETTE', { size: 26, ls: 4 }), L.slashes(260, 600, 3, 22, C.green, 11));
  const groups = [
    ['Socle', [['Void', C.void, 'Fond'], ['Abyss', C.abyss, 'Fond écran'], ['Deep', C.deep, 'Surface'], ['Panel', C.panel2, 'Panneaux'], ['Line', C.line, 'Bordures']]],
    ['Holo', [['Holo Blue', C.blue, 'Principal'], ['Cyan', C.cyan, 'Info, focus'], ['Violet', C.violet, 'Secondaire'], ['Magenta', C.magenta, 'Spécial']]],
    ['Fluo', [['Plasma', C.green, 'Action / CTA'], ['Acid', C.lime, 'Dégradé CTA']]],
    ['Retour', [['Alerte', C.danger, 'Danger, < 25 %'], ['Ambre', C.warn, 'Attention']]],
  ];
  let gx = 120;
  groups.forEach(([gname, sw]) => {
    d.add(label(gx, 670, gname, C.cyan));
    const items = sw.map(([n, col, use], i) => {
      const x = gx + i * 150, y = 690;
      return g(`Couleur ${n}`, [path(chamfer(x, y, 138, 120, [16, 0, 16, 0]), { fill: col, stroke: '#FFFFFF', 'stroke-opacity': 0.15 }),
        D(x, y + 158, n, { size: 17 }), T(x, y + 180, col.toUpperCase(), { size: 15, fill: C.muted, weight: 600, font: F.display }), T(x, y + 200, use, { size: 15, fill: C.dim, weight: 600 })]);
    });
    d.add(g(`Palette ${gname}`, items));
    gx += sw.length * 150 + 40;
  });
  // Types
  d.add(label(120, 950, 'Couleurs de type (fixes dans tout le jeu)', C.cyan));
  Object.entries(TYPE).forEach(([t, col], i) => {
    const x = 120 + i * 300;
    d.add(g(`Type ${t}`, [path(skew(x, 970, 280, 64, 18), { fill: col, 'fill-opacity': 0.15, stroke: col, 'stroke-width': 1.5 }),
      L.icon(['sword', 'shield', 'infinity', 'balance'][i], x + 44, 1002, 26, col), D(x + 72, 1010, t.toUpperCase(), { size: 20, fill: col, ls: 2 }), T(x + 18, 1060, col, { size: 15, fill: C.muted, font: F.display })]));
  });
  // Dégradé holo
  d.add(label(1360, 950, 'Dégradé holographique', C.cyan));
  d.add(path(chamfer(1360, 970, 920, 64, [18, 0, 18, 0]), { fill: d.lin([[0, C.blue], [0.35, C.violet], [0.6, C.magenta], [0.8, C.cyan], [1, C.green]], 0), id: 'Dégradé holo' }));

  // Typographie
  d.add(D(120, 1130, 'TYPOGRAPHIE', { size: 26, ls: 4 }), L.slashes(340, 1110, 3, 22, C.green, 11));
  d.add(card(d, 120, 1160, 1060, 600));
  d.add(label(160, 1205, 'Chakra Petch — titres, chiffres, boutons', C.green));
  d.add(D(160, 1300, 'VICTOIRE !', { size: 88, ls: 4, italic: true }));
  d.add(D(160, 1370, 'PRÉPARER LE COMBAT', { size: 40, ls: 3 }), T(860, 1370, 'H2 · 40 · +3', { size: 15, fill: C.dim, font: F.display }));
  d.add(D(160, 1420, 'COUP SPÉCIAL — LANCE D’ACIER', { size: 24, ls: 2.5, fill: C.cyan }), T(860, 1420, 'H3 · 24 · +2.5', { size: 15, fill: C.dim, font: F.display }));
  d.add(D(160, 1460, 'BOUTON / ÉTIQUETTE', { size: 15, ls: 2, fill: C.muted }), T(860, 1460, 'Label · 15 · +2', { size: 15, fill: C.dim, font: F.display }));
  d.add(label(160, 1530, 'Rajdhani — texte courant, descriptions', C.green));
  d.add(wrap(160, 1572, 'Une toupie est l’assemblage de quatre pièces : boulon, roue métallique, partie intermédiaire et pointe. Le type d’une toupie est sa stat la plus élevée.', { size: 22, weight: 500, max: 80, lh: 30 }));
  d.add(T(160, 1700, 'Corps · 22 / 30 · Medium', { size: 15, fill: C.dim, font: F.display }));
  d.add(D(1140, 1300, '23', { size: 120, fill: C.green, anchor: 'end' }), T(1140, 1330, 'PTS / BUDGET', { size: 14, fill: C.muted, anchor: 'end', ls: 2, weight: 700 }));

  // Langage de forme
  d.add(D(1260, 1130, 'LANGAGE DE FORME', { size: 26, ls: 4 }), L.slashes(1580, 1110, 3, 22, C.green, 11));
  d.add(card(d, 1260, 1160, 1020, 600));
  // 1. Chanfrein
  d.add(g('Règle chanfrein', [path(chamfer(1310, 1220, 260, 140, [36, 0, 36, 0]), { fill: C.panel2, stroke: C.cyan, 'stroke-width': 2 }),
    path('M1310,1256 L1346,1220', { stroke: C.green, 'stroke-width': 3 }), T(1352, 1250, '45°', { size: 16, fill: C.green, font: F.display, weight: 700 }),
    label(1310, 1395, 'Chanfrein 45° · haut-G + bas-D', C.text), T(1310, 1418, 'Panneau 22 px · bouton = hauteur × 0,32', { size: 15, fill: C.muted })]));
  // 2. Inclinaison
  d.add(g('Règle inclinaison', [path(skew(1630, 1250, 260, 70, 20), { fill: d.lin([[0, C.green], [1, C.lime]], 0) }), D(1760, 1295, 'SKEW 16°', { size: 18, fill: '#03140A', anchor: 'middle' }),
    label(1630, 1395, 'Parallélogrammes', C.text), T(1630, 1418, 'Tags, jauges, segments de stats', { size: 15, fill: C.muted })]));
  // 3. Slashes + crochets
  d.add(g('Règle accents', [L.slashes(1960, 1250, 3, 60, C.green, 26), L.brackets(2080, 1225, 150, 130, C.cyan, 26, 3), D(2155, 1298, 'FOCUS', { size: 16, fill: C.cyan, anchor: 'middle', ls: 2 }),
    label(1960, 1395, 'Slash ×3 · crochets d’angle', C.text), T(1960, 1418, 'Rythme, focus, sélection', { size: 15, fill: C.muted })]));
  // Effets
  d.add(label(1310, 1480, 'Effets', C.green));
  const fx = [
    ['Glow néon', [path(chamfer(1310, 1510, 200, 70, [16, 0, 16, 0]), { fill: C.green, opacity: 0.5, filter: d.blur(14) }), path(chamfer(1310, 1510, 200, 70, [16, 0, 16, 0]), { fill: 'none', stroke: C.green, 'stroke-width': 2.5 })]],
    ['Contour holo', [path(chamfer(1550, 1510, 200, 70, [16, 0, 16, 0]), { fill: C.panel, stroke: d.lin([[0, C.cyan], [0.5, C.violet], [1, C.magenta]], 0), 'stroke-width': 3 })]],
    ['Aberration', [D(1795, 1562, 'K.O.', { size: 44, fill: C.magenta, opacity: 0.8 }), D(1801, 1564, 'K.O.', { size: 44, fill: C.cyan, opacity: 0.8 }), D(1798, 1562, 'K.O.', { size: 44, fill: '#FFFFFF' })]],
    ['Scanlines', [rect(2030, 1510, 200, 70, { fill: C.deep }), path(Array.from({ length: 18 }, (_, i) => `M2030,${1512 + i * 4} H2230`).join(' '), { stroke: C.cyan, opacity: 0.25 })]],
  ];
  fx.forEach(([n, els], i) => d.add(g(`Effet ${n}`, [...els, T(1310 + i * 240, 1610, n, { size: 16, fill: C.muted })])));
  d.add(wrap(1310, 1680, 'À éviter : coins arrondis, ombres grises, fonds en aplat clair, plus de deux couleurs vives dans un même composant.', { size: 17, fill: C.danger, weight: 600, max: 95, lh: 24 }));
  return d;
}

// ============================== 01 DESIGN SYSTEM ==============================
function boardDS() {
  const W = 2400, H = 2000, d = new Doc(W, H, '01-design-system');
  d.add(L.background(d, W, H, { floor: false }));
  d.add(L.boardHeader(d, 120, 130, '01 · Design system', 'Composants', 'Tous les éléments d’interface, avec leurs états. Base 8 px, coins coupés, verts fluo réservés aux actions.'));
  const sec = (x, y, t) => d.add(D(x, y, t.toUpperCase(), { size: 22, ls: 3.5 }), L.slashes(x - 40, y - 18, 3, 18, C.green, 9));

  // Boutons
  sec(160, 320, 'Boutons');
  const variants = ['primary', 'secondary', 'holo', 'danger', 'ghost'], vnames = ['Primaire', 'Secondaire', 'Holo', 'Danger', 'Fantôme'];
  const states = [['default', 'Normal'], ['hover', 'Survol'], ['disabled', 'Désactivé']];
  states.forEach(([, n], j) => d.add(label(380 + j * 330, 360, n)));
  variants.forEach((v, i) => {
    d.add(label(120, 405 + i * 84, vnames[i], C.text));
    states.forEach(([s], j) => d.add(L.button(d, 380 + j * 330, 380 + i * 84, 290, 56, ['Combattre !', 'Commandes…', 'Coup spécial', 'Abandonner', 'Retour'][i], { variant: v, state: s, id: `Bouton/${vnames[i]}/${states[j][1]}` })));
  });
  d.add(label(1400, 360, 'Bouton de menu (icône + touche)'));
  d.add(L.button(d, 1400, 380, 520, 64, 'Jouer contre le bot', { variant: 'primary', icon: 'play', align: 'left', key: 'Entrée', keyW: 58 }));
  d.add(L.button(d, 1400, 464, 520, 64, 'Multijoueur réseau local', { icon: 'lan', align: 'left' }));
  d.add(L.button(d, 1400, 548, 520, 64, 'Tournois', { icon: 'trophy', align: 'left', state: 'hover' }));
  d.add(label(1960, 360, 'Touches'));
  [['E'], ['Espace'], ['Échap'], ['R'], ['Entrée'], ['↑'], ['←'], ['Clic']].forEach(([k], i) => d.add(L.keycap(d, 1960 + (i % 4) * 90, 380 + Math.floor(i / 4) * 44, k)));

  // Champs
  sec(160, 870, 'Champs & réglages');
  d.add(field(d, 120, 900, 420, 'Nom de la toupie', 'Cyclone Furieux', true));
  d.add(field(d, 580, 900, 420, 'Stadium', 'Classique · Noyau Holo', false, true));
  d.add(field(d, 1040, 900, 420, 'Adresse IP', '192.168.1.', false, false, true));
  d.add(segmented(d, 120, 1020, 560, 'Difficulté du bot', ['Facile', 'Normal', 'Difficile'], 1));
  d.add(slider(d, 740, 1020, 520, 'Vitesse', 0.55, '×1,10'));
  d.add(toggle(d, 1320, 1020, 'Coups spéciaux', true), toggle(d, 1320, 1080, 'Limite de temps', false));

  // Badges & stats
  sec(1760, 870, 'Types & stats');
  Object.keys(TYPE).forEach((t, i) => d.add(L.badge(d, 1720 + (i % 2) * 190, 900 + Math.floor(i / 2) * 38, t)));
  d.add(L.statBar(d, 1720, 1010, 400, 'Attaque', 14, 15, TYPE.Attaque), L.statBar(d, 1720, 1060, 400, 'Défense', 4, 15, TYPE['Défense']), L.statBar(d, 1720, 1110, 400, 'Endurance', 5, 15, TYPE.Endurance));
  d.add(L.badge(d, 2110, 900, C.cyan, { label: '★ Perso' }), L.badge(d, 2110, 938, C.muted, { label: 'Pré-assemblée' }));

  // Jauges
  sec(160, 1220, 'Jauge d’endurance — états');
  const gauges = [['Pleine', 100, C.green, C.lime], ['En combat', 62, C.green, C.lime], ['Vacille (< 25 %)', 18, C.danger, C.warn], ['Gelée', 54, '#8FF3FF', C.cyan], ['Sonnée', 40, C.lime, C.lime]];
  gauges.forEach(([n, p, c1, c2], i) => {
    const x = 120 + i * 360;
    d.add(g(`Endurance/${n}`, [label(x, 1265, n, C.text), D(x + 330, 1266, String(p), { size: 22, fill: c1, anchor: 'end' }),
      L.enduranceBar(d, x, 1280, 330, 22, p, c1, { color2: c2 }),
      n === 'Gelée' ? path(Array.from({ length: 6 }, (_, k) => `M${x + 14 + k * 28},1280 l10,22`).join(' '), { stroke: '#FFFFFF', opacity: 0.5 }) : '',
      n === 'Sonnée' ? L.icon('lock', x + 320, 1328, 16, C.lime) : '',
      T(x, 1330, ['Dégradé plasma → acid', 'Repère rouge à 25 %', 'Clignote 4 Hz, rouge', 'Givre hachuré, 3 s', 'Ni dash ni coup, 6 s'][i], { size: 15, fill: C.muted })]));
  });

  // Statuts dash & coup spécial
  sec(160, 1420, 'Dash & coup spécial');
  const dashes = [['Dash prêt', C.green, 'Espace'], ['Dash dans 1,2 s', C.muted, null], ['DASH !', C.cyan, null], ['Dash impossible', C.danger, null]];
  dashes.forEach(([n, col, k], i) => d.add(dashChip(d, 120 + i * 300, 1450, 280, n, col, k, i === 1 ? 0.4 : 1)));
  d.add(specialButton(d, 120, 1540, 540, 'lance', 'ready'), specialButton(d, 700, 1540, 540, 'lance', 'used'), specialButton(d, 1280, 1540, 540, 'seisme', 'locked'));

  // Panneau, bulle, bannière
  sec(160, 1720, 'Panneaux & messages');
  d.add(L.panel(d, 120, 1750, 520, 200, { title: 'Votre toupie', index: '01', accent: C.cyan }), T(160, 1840, 'Panneau à chanfrein, crochets d’angle,', { size: 17, fill: C.muted }), T(160, 1864, 'en-tête en capitales + index.', { size: 17, fill: C.muted }));
  d.add(tooltip(d, 700, 1760, 520, 'DASH', 'Ruée courte vers l’adversaire le plus proche. Appuyez sur', 'Espace'));
  d.add(banner(d, 1280, 1770, 900, 'PARTEZ !', C.green));
  // grille d'icônes
  sec(1960, 1220, 'Icônes');
  Object.keys(L.ICONS).forEach((k, i) => {
    const x = 1960 + (i % 7) * 52, y = 1250 + Math.floor(i / 7) * 52;
    d.add(g(`Icône ${k}`, [path(chamfer(x, y, 44, 44, [8, 0, 8, 0]), { fill: C.panel, stroke: C.line }), L.icon(k, x + 22, y + 22, 22, C.cyan)]));
  });
  return d;
}

function field(d, x, y, w, lab, val, focus, select, placeholder) {
  return g(`Champ ${lab}`, [label(x, y + 14, lab),
    path(chamfer(x, y + 26, w, 50, [12, 0, 12, 0]), { fill: '#07061A', stroke: focus ? C.cyan : C.line, 'stroke-width': focus ? 2 : 1.2 }),
    focus ? path(chamfer(x, y + 26, w, 50, [12, 0, 12, 0]), { fill: 'none', stroke: C.cyan, 'stroke-width': 6, opacity: 0.25, filter: d.blur(4) }) : '',
    T(x + 22, y + 58, val, { size: 20, fill: placeholder ? C.dim : C.text, weight: 600 }),
    focus ? rect(x + 22 + val.length * 9.6, y + 40, 2, 24, { fill: C.cyan }) : '',
    select ? L.icon('next', x + w - 28, y + 51, 18, C.cyan) : '',
  ]);
}
function segmented(d, x, y, w, lab, opts, sel) {
  const sw = w / opts.length;
  return g(`Segment ${lab}`, [label(x, y + 14, lab), ...opts.map((o, i) => {
    const on = i === sel;
    return g(`Option ${o}`, [path(skew(x + i * sw, y + 26, sw - 6, 46, 12), { fill: on ? d.lin([[0, C.green], [1, C.lime]], 0) : '#07061A', stroke: on ? C.green : C.line }),
      D(x + i * sw + sw / 2 - 3, y + 55, o.toUpperCase(), { size: 15, ls: 2, fill: on ? '#03140A' : C.muted, anchor: 'middle' })]);
  })]);
}
function slider(d, x, y, w, lab, v, val) {
  const kx = x + w * v;
  return g(`Curseur ${lab}`, [label(x, y + 14, lab), D(x + w, y + 15, val, { size: 17, fill: C.cyan, anchor: 'end' }),
    path(skew(x, y + 44, w, 8, 4), { fill: '#07061A', stroke: C.line }), path(skew(x, y + 44, w * v, 8, 4), { fill: d.lin([[0, C.blue], [1, C.cyan]], 0) }),
    rect(x, y + 46, w * v, 4, { fill: C.cyan, opacity: 0.6, filter: d.blur(4) }),
    path(chamfer(kx - 11, y + 34, 22, 28, [6, 0, 6, 0]), { fill: C.text, stroke: C.cyan, 'stroke-width': 2 })]);
}
function toggle(d, x, y, lab, on) {
  return g(`Interrupteur ${lab}`, [path(skew(x, y + 30, 76, 32, 10), { fill: on ? C.green : '#07061A', 'fill-opacity': on ? 0.25 : 1, stroke: on ? C.green : C.line, 'stroke-width': 1.5 }),
    path(skew(on ? x + 40 : x + 6, y + 35, 30, 22, 7), { fill: on ? C.green : C.dim }),
    T(x + 96, y + 53, lab, { size: 19, fill: on ? C.text : C.muted, weight: 600 }), D(x + 360, y + 53, on ? 'ON' : 'OFF', { size: 14, fill: on ? C.green : C.dim, anchor: 'end', ls: 2 })]);
}
function dashChip(d, x, y, w, txt, col, key, op = 1) {
  return g(`Dash/${txt}`, [path(skew(x, y, w, 46, 14), { fill: col, 'fill-opacity': 0.12, stroke: col, 'stroke-width': 1.5, opacity: op < 1 ? 0.6 : 1 }),
    op < 1 ? path(skew(x, y + 40, w * op, 6, 2), { fill: col }) : '',
    L.icon('dash', x + 34, y + 23, 20, col), D(x + 56, y + 29, txt.toUpperCase(), { size: 15, ls: 1.6, fill: col }),
    key ? L.keycap(d, x + w - 84, y + 10, key) : '']);
}
function specialButton(d, x, y, w, key, state) {
  const mv = Ar.MOVES[key], col = state === 'ready' ? mv.color : C.dim, h = 84, out = [];
  if (state === 'ready') out.push(path(chamfer(x, y + 4, w, h, [24, 0, 24, 0]), { fill: C.magenta, opacity: 0.4, filter: d.blur(16) }));
  out.push(path(chamfer(x, y, w, h, [24, 0, 24, 0]), { fill: state === 'ready' ? d.lin([[0, '#2A1680'], [0.6, '#4A1A9A'], [1, '#7A1FA0']], 0) : '#0D0B2A', stroke: state === 'ready' ? d.lin([[0, C.cyan], [0.5, C.violet], [1, C.magenta]], 0) : C.line, 'stroke-width': 2 }));
  out.push(path(chamfer(x + 14, y + 14, 56, 56, [12, 0, 12, 0]), { fill: '#07061A', stroke: col, 'stroke-width': 1.5 }), Ar.glyph(key, x + 42, y + 42, 18, state === 'ready' ? '#FFFFFF' : C.dim, 2));
  out.push(label(x + 88, y + 34, state === 'used' ? 'Coup spécial — utilisé' : state === 'locked' ? 'Bloqué par séisme · 4,2 s' : 'Coup spécial · coûte ¼ de l’endurance', state === 'locked' ? C.lime : C.muted));
  out.push(D(x + 88, y + 64, mv.name.toUpperCase(), { size: 22, ls: 2, fill: state === 'ready' ? C.white : C.dim }));
  if (state === 'ready') out.push(L.keycap(d, x + w - 64, y + 29, 'E'));
  if (state === 'used') out.push(L.icon('check', x + w - 46, y + 42, 22, C.dim));
  if (state === 'locked') out.push(L.icon('lock', x + w - 46, y + 42, 22, C.lime));
  return g(`Coup spécial/${state}`, out);
}
function tooltip(d, x, y, w, title, txt, key) {
  return g('Bulle tutoriel', [path(chamfer(x, y, w, 120, [18, 0, 18, 0]), { fill: '#0B0930', stroke: C.cyan, 'stroke-width': 1.5 }),
    path(`M${x + 60},${y + 120} l20,22 l20,-22 Z`, { fill: '#0B0930', stroke: C.cyan, 'stroke-width': 1.5 }), rect(x + 61, y + 118, 38, 4, { fill: '#0B0930' }),
    rect(x, y + 18, 4, 40, { fill: C.cyan }), D(x + 28, y + 42, title, { size: 18, ls: 2.5, fill: C.cyan }), T(x + 28, y + 76, txt, { size: 18 }), L.keycap(d, x + 28, y + 86, key),
    T(x + 120, y + 105, 'pour dasher — le jeu est en pause.', { size: 18 }), T(x + w - 20, y + 42, '3 / 7', { size: 14, fill: C.muted, anchor: 'end', font: F.display })]);
}
function banner(d, x, y, w, txt, col) {
  return g(`Bannière ${txt}`, [path(skew(x, y, w, 110, 40), { fill: d.lin([[0, col, 0], [0.2, col, 0.25], [0.8, col, 0.25], [1, col, 0]], 0) }),
    line(x + 40, y, x + w, y, { stroke: col, 'stroke-width': 2 }), line(x, y + 110, x + w - 40, y + 110, { stroke: col, 'stroke-width': 2 }),
    D(x + w / 2 - 5, y + 74, txt, { size: 64, fill: C.magenta, anchor: 'middle', italic: true, ls: 6, opacity: 0.7 }),
    D(x + w / 2 + 5, y + 76, txt, { size: 64, fill: C.cyan, anchor: 'middle', italic: true, ls: 6, opacity: 0.7 }),
    D(x + w / 2, y + 74, txt, { size: 64, fill: '#FFFFFF', anchor: 'middle', italic: true, ls: 6 })]);
}

// ============================== 10 TOUPIES ==============================
function boardTops() {
  const W = 2400, H = 2260, d = new Doc(W, H, '10-toupies');
  d.add(L.background(d, W, H, { floor: false }));
  d.add(L.boardHeader(d, 120, 130, '10 · Toupies', 'Les 8 toupies pré-assemblées', 'Chaque roue a une silhouette unique (profil polaire), une teinte propre et un boulon-emblème qui annonce son coup spécial.'));
  Ar.PRESETS.forEach((p, i) => {
    const x = 120 + (i % 4) * 545, y = 290 + Math.floor(i / 4) * 980;
    d.add(topCard(d, x, y, 515, 940, p, i));
  });
  return d;
}
function topCard(d, x, y, w, h, p, i) {
  const st = Ar.sumStats(p.parts), col = TYPE[p.type], [wk, mk, tk, bk] = p.parts, mv = Ar.MOVES[Ar.BOLTS[bk]], out = [];
  out.push(card(d, x, y, w, h, { c: [28, 0, 28, 0] }));
  out.push(path(chamfer(x, y, w, 470, [28, 0, 0, 0]), { fill: d.rad([[0, col, 0.28], [0.7, col, 0.04], [1, col, 0]], 0.5, 0.5, 0.6) }));
  // anneau holo + toupie
  out.push(circle(x + w / 2, y + 250, 190, { fill: 'none', stroke: col, 'stroke-width': 1, opacity: 0.35, 'stroke-dasharray': '2 8' }));
  out.push(circle(x + w / 2, y + 250, 205, { fill: 'none', stroke: d.lin([[0, C.cyan], [0.5, C.violet], [1, col]], 30), 'stroke-width': 1.5, opacity: 0.6 }));
  out.push(Ar.top(d, x + w / 2, y + 250, 150, p, { angle: i * 0.7, aura: true }));
  out.push(D(x + 30, y + 54, `0${i + 1}`, { size: 18, fill: col, ls: 2 }), L.badge(d, x + w - 170, y + 30, p.type, { w: 140 }));
  out.push(L.brackets(x + 20, y + 20, w - 40, 450, col, 22, 2));
  // identité
  out.push(D(x + 30, y + 520, p.name.toUpperCase(), { size: 34, ls: 2.5 }));
  out.push(T(x + 30, y + 552, p.lore, { size: 17, fill: C.muted, weight: 500 }));
  // stats
  out.push(L.statBar(d, x + 30, y + 600, w - 60, 'Attaque', st[0], 15, TYPE.Attaque), L.statBar(d, x + 30, y + 646, w - 60, 'Défense', st[1], 15, TYPE['Défense']), L.statBar(d, x + 30, y + 692, w - 60, 'Endurance', st[2], 15, TYPE.Endurance));
  // pièces + profil
  out.push(Ar.sideView(d, x + 92, y + 742, 140, p, { exploded: true, id: `Profil ${p.name}` }));
  const rows = [['Boulon', mv.bolt], ['Roue', Ar.WHEELS[wk].name + (Ar.WHEELS[wk].spin === 'G' ? ' ↺' : ' ↻')], ['Intermédiaire', Ar.MIDDLES[mk].name], ['Pointe', Ar.TIPS[tk].name]];
  rows.forEach(([k, v], j) => out.push(label(x + 190, y + 752 + j * 34, k), T(x + w - 30, y + 752 + j * 34, v, { size: 18, anchor: 'end', weight: 700 })));
  out.push(path(chamfer(x + 190, y + 880, w - 220, 40, [10, 0, 10, 0]), { fill: mv.color, 'fill-opacity': 0.12, stroke: mv.color }), Ar.glyph(Ar.BOLTS[bk], x + 214, y + 900, 10, mv.color, 1.6), D(x + 236, y + 906, mv.name.toUpperCase(), { size: 15, fill: mv.color, ls: 1.5 }), T(x + w - 46, y + 906, mv.form, { size: 13, fill: mv.color, anchor: 'end', font: F.display, weight: 700, ls: 2 }));
  return g(`Fiche ${p.name}`, out);
}

// ============================== 11 PIÈCES ==============================
function boardParts() {
  const W = 2400, H = 2100, d = new Doc(W, H, '11-pieces');
  d.add(L.background(d, W, H, { floor: false }));
  d.add(L.boardHeader(d, 120, 130, '11 · Pièces', 'Catalogue des pièces', '7 boulons · 14 roues · 10 parties intermédiaires · 10 pointes. Stats A / D / E sous chaque pièce ; ↻ horaire, ↺ antihoraire.'));
  const sec = (y, t, n) => d.add(D(160, y, t.toUpperCase(), { size: 24, ls: 3.5 }), L.slashes(120, y - 20, 3, 20, C.green, 10), T(160 + t.length * 18 + 30, y, n, { size: 18, fill: C.muted }));
  const stats = (x, y, s) => g('Stats', [['A', TYPE.Attaque], ['D', TYPE['Défense']], ['E', TYPE.Endurance]].map(([k, c], i) => [T(x + i * 44, y, k, { size: 13, fill: C.muted, weight: 700 }), D(x + i * 44 + 14, y, String(s[i]), { size: 15, fill: c })]).flat());
  // Boulons
  sec(320, 'Boulons', '— coup spécial · 0 pt');
  Object.entries(Ar.BOLTS).forEach(([id, k], i) => {
    const x = 120 + i * 310, mv = Ar.MOVES[k];
    d.add(g(`Pièce ${id}`, [card(d, x, 350, 290, 240), circle(x + 145, 450, 66, { fill: mv.color, opacity: 0.18, filter: d.blur(18) }), Ar.boltArt(d, k, x + 145, 450, 58, 0),
      D(x + 20, 38 + 350, id, { size: 13, fill: C.dim }), D(x + 145, 555, mv.bolt.toUpperCase(), { size: 18, anchor: 'middle', ls: 2 }), T(x + 145, 578, mv.name, { size: 16, fill: mv.color, anchor: 'middle' })]));
  });
  // Roues
  sec(680, 'Roues métalliques', '— silhouette, sens de rotation · 10 pts');
  Object.entries(Ar.WHEELS).forEach(([id, w], i) => {
    const x = 120 + (i % 7) * 310, y = 710 + Math.floor(i / 7) * 300;
    d.add(g(`Pièce ${id}`, [card(d, x, y, 290, 280), Ar.wheelArt(d, id, x + 145, y + 120, 92), D(x + 20, y + 30, id, { size: 13, fill: C.dim }),
      T(x + 270, y + 30, w.spin === 'G' ? '↺' : '↻', { size: 20, fill: C.cyan, anchor: 'end', weight: 700 }),
      D(x + 20, y + 244, w.name.toUpperCase(), { size: w.name.length > 9 ? 14 : 17, ls: 1.2 }), stats(x + 150, y + 245, w.stats), rect(x + 20, y + 256, 250, 2, { fill: TYPE[w.kind], opacity: 0.7 })]));
  });
  // Intermédiaires
  sec(1370, 'Parties intermédiaires', '— 6 pts');
  Object.entries(Ar.MIDDLES).forEach(([id, m], i) => {
    const x = 120 + (i % 10) * 216, y = 1400;
    d.add(g(`Pièce ${id}`, [card(d, x, y, 200, 240), Ar.middleArt(d, id, x + 100, y + 100, 66, -Math.PI / 2), D(x + 16, y + 28, id, { size: 12, fill: C.dim }),
      D(x + 100, y + 200, m.name.toUpperCase(), { size: 15, anchor: 'middle', ls: 1.2 }), stats(x + 40, y + 226, m.stats)]));
  });
  // Pointes
  sec(1720, 'Pointes', '— profil · 7 pts');
  Object.entries(Ar.TIPS).forEach(([id, t], i) => {
    const x = 120 + (i % 10) * 216, y = 1750;
    d.add(g(`Pièce ${id}`, [card(d, x, y, 200, 260), rect(x + 30, y + 196, 140, 2, { fill: C.line }), Ar.tipArt(d, id, x + 100, y + 70, 120), D(x + 16, y + 28, id, { size: 12, fill: C.dim }),
      D(x + 100, y + 226, t.name.toUpperCase(), { size: 15, anchor: 'middle', ls: 1.2 }), stats(x + 40, y + 250, t.stats)]));
  });
  return d;
}

// ============================== 12 STADIUMS ==============================
function boardStadiums() {
  const W = 2400, H = 1900, d = new Doc(W, H, '12-stadiums');
  d.add(L.background(d, W, H, { floor: false }));
  d.add(L.boardHeader(d, 120, 130, '12 · Stadiums', 'Six arènes, six identités', 'Chaque stadium garde son nom en jeu et reçoit un nom de code, une matière et une couleur signature qui traduisent ses modificateurs.'));
  Ar.STADIUMS.forEach((s, i) => {
    const x = 120 + (i % 3) * 730, y = 290 + Math.floor(i / 3) * 800, w = 700, h = 770, out = [];
    out.push(card(d, x, y, w, h, { c: [28, 0, 28, 0] }));
    out.push(path(chamfer(x, y, w, 540, [28, 0, 0, 0]), { fill: d.rad([[0, s.color, 0.18], [1, s.color, 0]], 0.5, 0.5, 0.55) }));
    out.push(Ar.stadium(d, s.key, x + w / 2, y + 280, s.key === 'volcan' ? 170 : 215));
    out.push(L.brackets(x + 20, y + 20, w - 40, 520, s.color, 26, 2));
    out.push(D(x + 40, y + 60, `0${i + 1}`, { size: 18, fill: s.color }));
    out.push(D(x + 40, y + 600, s.name.toUpperCase(), { size: 40, ls: 3 }), D(x + 40 + s.name.length * 28 + 30, y + 600, `// ${s.code}`, { size: 20, fill: s.color, ls: 3 }));
    out.push(L.badge(d, x + 40, y + 625, s.color, { label: s.shape }), L.badge(d, x + 54 + L.badgeW(s.shape.toUpperCase()), y + 625, s.key === 'terre' || s.key === 'volcan' ? C.danger : C.muted, { label: s.rim }));
    s.mods.forEach((m, j) => {
      const xx = x + 40 + (j % 2) * 320, yy = y + 690 + Math.floor(j / 2) * 34;
      out.push(path(skew(xx, yy - 13, 10, 14, 4), { fill: s.color }), T(xx + 20, yy, m, { size: 19, weight: 600 }));
    });
    d.add(g(`Fiche stadium ${s.name}`, out));
  });
  return d;
}

// ============================== 13 COUPS SPÉCIAUX ==============================
function boardMoves() {
  const W = 2400, H = 1880, d = new Doc(W, H, '13-coups-speciaux');
  d.add(L.background(d, W, H, { floor: false }));
  d.add(L.boardHeader(d, 120, 130, '13 · Coups spéciaux', 'Sept signatures visuelles', 'Vague = projectile qui file vers l’adversaire le plus proche (esquivable). Anneau = onde qui s’élargit autour du lanceur.'));
  const keys = Object.keys(Ar.MOVES);
  keys.forEach((k, i) => {
    const mv = Ar.MOVES[k], col = i < 4 ? 4 : 3, cw = i < 4 ? 520 : 700, x = 120 + (i < 4 ? i * 545 : (i - 4) * 725), y = i < 4 ? 290 : 1060, w = cw, h = 740, out = [];
    out.push(card(d, x, y, w, h, { c: [28, 0, 28, 0] }));
    out.push(path(chamfer(x + 16, y + 16, w - 32, 340, [20, 0, 0, 0]), { fill: '#06051A', stroke: C.line }));
    out.push(path(chamfer(x + 16, y + 16, w - 32, 340, [20, 0, 0, 0]), { fill: d.rad([[0, mv.color, 0.14], [1, mv.color, 0]], 0.45, 0.5, 0.6) }));
    out.push(Ar.moveFx(d, k, x + 16, y + 16, w - 32, 340));
    out.push(path(chamfer(x + 30, y + 384, 76, 76, [16, 0, 16, 0]), { fill: '#07061A', stroke: mv.color, 'stroke-width': 2 }), path(chamfer(x + 30, y + 384, 76, 76, [16, 0, 16, 0]), { fill: mv.color, opacity: 0.25, filter: d.blur(10) }), Ar.glyph(k, x + 68, y + 422, 24, mv.color, 2.4));
    out.push(label(x + 126, y + 404, `Boulon ${mv.bolt}`), D(x + 126, y + 442, mv.name.toUpperCase(), { size: 30, ls: 2 }));
    out.push(L.badge(d, x + w - 140, y + 386, mv.color, { label: mv.form, w: 110 }));
    out.push(path(skew(x + 30, y + 486, 10, 14, 4), { fill: C.green }), wrap(x + 52, y + 499, mv.plus, { size: 19, max: cw > 600 ? 58 : 42, lh: 24 }));
    out.push(path(skew(x + 30, y + 548, 10, 14, 4), { fill: C.danger }), wrap(x + 52, y + 561, mv.minus, { size: 19, fill: C.muted, max: cw > 600 ? 58 : 42, lh: 24 }));
    [['Dégâts', mv.dmg, 24, TYPE.Attaque], ['Recul', mv.kb, 5, C.cyan], ['Portée', mv.range, 4, C.violet]].forEach(([n, v, m, c], j) => {
      out.push(L.statBar(d, x + 30 + j * ((w - 60) / 3), y + 660, (w - 60) / 3 - 24, n, v, m, c, { segs: 8 }));
    });
    out.push(T(x + 30, y + 712, 'Coûte ¼ de l’endurance du moment · une fois par partie', { size: 15, fill: C.dim }));
    d.add(g(`Fiche ${mv.name}`, out));
  });
  return d;
}

if (require.main === module) for (const b of [boardDA, boardDS, boardTops, boardParts, boardStadiums, boardMoves]) b().save(OUT);
module.exports = { field, segmented, slider, toggle, dashChip, specialButton, tooltip, banner, topCard, card, label, wrap };
