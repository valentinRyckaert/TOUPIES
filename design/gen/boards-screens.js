'use strict';
// Écrans du jeu en 1920 × 1080.
const L = require('./lib');
const Ar = require('./art');
const B = require('./boards-art');
const { C, TYPE, F, Doc, g, rect, circle, ellipse, line, path, chamfer, skew, T, D, r2 } = L;

const OUT = process.argv[2] || `${__dirname}/../svg`;
const W = 1920, H = 1080;
const P = Object.fromEntries(Ar.PRESETS.map(p => [p.name, p]));
const { label, card } = B;

function topBar(d, title, o = {}) {
  const back = o.back || 'Retour', bw = 170 + back.length * 11.5, tx = 40 + bw + 78;
  const out = [L.button(d, 40, 32, bw, 46, back, { variant: 'ghost', icon: 'back', key: 'Échap', keyW: 50, align: 'left' })];
  out.push(L.slashes(tx - 46, 42, 3, 26, C.green, 12), D(tx, 68, title.toUpperCase(), { size: 32, ls: 3.5 }));
  if (o.sub) out.push(T(tx + title.length * 24 + 30, 66, o.sub, { size: 19, fill: C.muted, weight: 500 }));
  out.push(line(40, 104, W - 40, 104, { stroke: C.line }), line(40, 104, 420, 104, { stroke: C.cyan, 'stroke-width': 2 }));
  return g('Barre du haut', out);
}
const chipW = (txt, ic) => txt.length * 10.6 + (ic ? 62 : 38);
const chips = (d, x, y, list) => list.map(([t, c, ic]) => { const el = chip(d, x, y, t, c, ic); x += chipW(t, ic) + 10; return el; });
function chip(d, x, y, txt, col = C.cyan, ic) {
  const w = chipW(txt, ic);
  return g(`Puce ${txt}`, [path(skew(x, y, w, 34, 10), { fill: col, 'fill-opacity': 0.1, stroke: col, 'stroke-opacity': 0.7 }),
    ic ? L.icon(ic, x + 30, y + 17, 16, col) : '', D(x + (ic ? 46 : 20), y + 22, txt.toUpperCase(), { size: 13, ls: 1.6, fill: col })]);
}
const vignette = d => rect(0, 0, W, H, { fill: d.rad([[0, '#000', 0], [0.65, '#000', 0.15], [1, '#000', 0.8]], 0.5, 0.5, 0.75), id: 'Vignette' });
const chroma = (x, y, s, o) => [D(x - 5, y, s, { ...o, fill: C.magenta, opacity: 0.75 }), D(x + 5, y + 2, s, { ...o, fill: C.cyan, opacity: 0.75 }), D(x, y, s, { ...o, fill: '#FFFFFF' })];

// ============================== 02 MENU ==============================
function menu() {
  const d = new Doc(W, H, '02-ecran-menu');
  d.add(L.background(d, W, H, { horizon: 0.7, cx: 0.7, cy: 0.45, h3x: 0.72, h3y: 0.45 }));
  // héros
  const hx = 1370, hy = 500;
  d.add(g('Héros', [
    ellipse(hx, hy + 290, 420, 70, { fill: C.cyan, opacity: 0.25, filter: d.blur(30) }),
    ellipse(hx, hy + 290, 330, 46, { fill: 'none', stroke: C.cyan, 'stroke-width': 2, opacity: 0.8 }),
    ellipse(hx, hy + 290, 420, 64, { fill: 'none', stroke: C.violet, 'stroke-width': 1.5, 'stroke-dasharray': '4 10' }),
    circle(hx, hy, 330, { fill: 'none', stroke: d.lin([[0, C.cyan], [0.5, C.violet], [1, C.green]], 30), 'stroke-width': 1.5, opacity: 0.5 }),
    circle(hx, hy, 300, { fill: 'none', stroke: C.violet, 'stroke-width': 1, 'stroke-dasharray': '2 12', opacity: 0.6 }),
    path(`M${hx - 380},${hy + 120} A400,400 0 0 1 ${hx - 120},${hy - 380}`, { fill: 'none', stroke: C.green, 'stroke-width': 3, opacity: 0.7, 'stroke-linecap': 'round' }),
    Ar.top(d, hx, hy, 230, P['Cyclone Furieux'], { angle: 0.3, aura: true, id: 'Toupie héros' }),
    Ar.top(d, 1760, 250, 70, P['Forteresse'], { angle: 1, aura: true }),
    path('M1690,230 Q1600,200 1560,250', { fill: 'none', stroke: TYPE['Défense'], 'stroke-width': 3, opacity: 0.6, 'stroke-linecap': 'round' }),
    Ar.top(d, 1000, 860, 56, P['Éternelle'], { angle: 2, aura: true }),
    ...Array.from({ length: 14 }, (_, i) => { const a = i * 0.45 - 2.4, r = 360 + (i % 3) * 40; return line(hx + Math.cos(a) * r, hy + Math.sin(a) * r, hx + Math.cos(a) * (r + 30 + (i % 4) * 12), hy + Math.sin(a) * (r + 30 + (i % 4) * 12), { stroke: i % 2 ? C.cyan : C.green, 'stroke-width': 2, opacity: 0.7 }); }),
  ]));
  d.add(L.logo(d, 130, 250, 1));
  d.add(T(134, 362, 'Assemblez votre toupie · affrontez le bot ou vos amis en réseau local', { size: 21, fill: C.muted, weight: 500 }));
  const items = [['Jouer contre le bot', 'play', 'primary', 'Entrée'], ['Multijoueur en réseau local', 'lan'], ['Tournois', 'trophy', 'secondary', null, 'hover'], ['Assembler une toupie', 'build'], ['Tutoriel', 'tuto']];
  items.forEach(([t, ic, v, k, st], i) => d.add(L.button(d, 130, 408 + i * 78, 580, 64, t, { variant: v || 'secondary', icon: ic, align: 'left', key: k, keyW: 58, state: st })));
  ['Paramètres', 'Aide', 'Mises à jour', 'Quitter'].forEach((t, i) => d.add(L.button(d, 130 + i * 148, 810, 136, 46, t, { variant: 'ghost' })));
  d.add(chip(d, 1540, 40, 'v1.2.1 · à jour', C.green, 'check'), chip(d, 1540, 84, '2 lobbys en réseau local', C.cyan, 'wifi'));
  // toupie équipée
  d.add(g('Toupie équipée', [card(d, 1440, 930, 440, 110), Ar.top(d, 1500, 985, 36, P['Cyclone Furieux'], { angle: 1 }),
    label(1556, 968, 'Dernière toupie'), D(1556, 1000, 'CYCLONE FURIEUX', { size: 22, ls: 2 }), L.badge(d, 1556, 1010, 'Attaque', { h: 20 }), L.icon('next', 1850, 985, 20, C.cyan)]));
  d.add(g('Aide clavier', [L.keycap(d, 130, 1010, '↑'), L.keycap(d, 160, 1010, '↓'), T(200, 1029, 'Naviguer', { size: 17, fill: C.muted }),
    L.keycap(d, 296, 1010, 'Entrée'), T(372, 1029, 'Valider', { size: 17, fill: C.muted }), L.keycap(d, 452, 1010, 'Échap'), T(526, 1029, 'Quitter', { size: 17, fill: C.muted })]));
  return d;
}

// ============================== 03 PRÉPARATION ==============================
function topSummary(d, x, y, w, p, o = {}) {
  const st = Ar.sumStats(p.parts), mv = Ar.MOVES[Ar.BOLTS[p.parts[3]]], out = [];
  out.push(L.badge(d, x, y, p.type));
  out.push(L.statBar(d, x, y + 76, w, 'Attaque', st[0], 15, TYPE.Attaque), L.statBar(d, x, y + 126, w, 'Défense', st[1], 15, TYPE['Défense']), L.statBar(d, x, y + 176, w, 'Endurance', st[2], 15, TYPE.Endurance));
  out.push(path(chamfer(x, y + 214, w, 62, [12, 0, 12, 0]), { fill: mv.color, 'fill-opacity': 0.1, stroke: mv.color, 'stroke-opacity': 0.8 }),
    Ar.glyph(Ar.BOLTS[p.parts[3]], x + 30, y + 245, 13, mv.color, 1.8), label(x + 56, y + 238, `Coup spécial · ${mv.form}`), D(x + 56, y + 263, mv.name.toUpperCase(), { size: 18, fill: mv.color, ls: 1.5 }));
  return g(`Résumé ${p.name}`, out);
}
function setup() {
  const d = new Doc(W, H, '03-ecran-preparation');
  d.add(L.background(d, W, H, { floor: false }), topBar(d, 'Préparer le combat', { back: 'Menu', sub: 'Solo contre le bot' }));
  const side = (x, title, idx, p, sel, acc) => {
    d.add(L.panel(d, x, 130, 720, 600, { title, index: idx, accent: acc }));
    d.add(B.field(d, x + 40, 186, 640, 'Toupie', sel, false, true));
    d.add(circle(x + 200, 470, 170, { fill: TYPE[p.type], opacity: 0.12, filter: d.blur(30) }), circle(x + 200, 470, 168, { fill: 'none', stroke: TYPE[p.type], 'stroke-dasharray': '2 8', opacity: 0.6 }));
    d.add(Ar.top(d, x + 200, 470, 135, p, { angle: x / 300, aura: true }));
    d.add(topSummary(d, x + 400, 300, 280, p));
  };
  side(60, 'Votre toupie', 'J1', P['Cyclone Furieux'], 'Cyclone Furieux  [Attaque]', C.green);
  side(1140, 'Toupie du bot', 'BOT', P['Forteresse'], 'Forteresse  [Défense]', C.danger);
  d.add(B.segmented(d, 1180, 640, 640, '', ['Facile', 'Normal', 'Difficile'], 1));
  d.add(label(1180, 650, 'Difficulté du bot'));
  // VS
  d.add(g('VS', [line(960, 160, 960, 330, { stroke: C.line }), line(960, 530, 960, 700, { stroke: C.line }),
    circle(960, 430, 90, { fill: C.violet, opacity: 0.35, filter: d.blur(30) }), path(chamfer(890, 370, 140, 120, [30, 0, 30, 0]), { fill: '#0A0826', stroke: d.lin([[0, C.cyan], [1, C.magenta]], 0), 'stroke-width': 2 }),
    ...chroma(960, 455, 'VS', { size: 64, anchor: 'middle', italic: true }), L.slashes(925, 500, 3, 12, C.green, 8)]));
  // stadiums
  d.add(D(60, 784, 'STADIUM', { size: 18, ls: 3 }), T(170, 784, 'Classique · référence, aucun modificateur', { size: 18, fill: C.muted }));
  Ar.STADIUMS.forEach((s, i) => {
    const x = 60 + i * 302, y = 800, on = i === 0;
    d.add(g(`Choix stadium ${s.name}`, [path(chamfer(x, y, 286, 150, [18, 0, 18, 0]), { fill: on ? '#16214F' : '#0E0B30', stroke: on ? C.green : C.line, 'stroke-width': on ? 2.5 : 1.2 }),
      on ? path(chamfer(x, y, 286, 150, [18, 0, 18, 0]), { fill: 'none', stroke: C.green, 'stroke-width': 6, opacity: 0.3, filter: d.blur(6) }) : '',
      Ar.stadium(d, s.key, x + 78, y + 75, s.key === 'volcan' ? 46 : 54),
      D(x + 150, y + 62, s.name.toUpperCase(), { size: 18, ls: 1.5 }), T(x + 150, y + 86, s.code, { size: 14, fill: s.color, weight: 700, ls: 1.5 }), T(x + 150, y + 112, s.shape, { size: 15, fill: C.muted }),
      on ? L.icon('check', x + 262, y + 26, 18, C.green) : '']));
  });
  d.add(L.button(d, 60, 990, 360, 60, 'Personnalisation…', { variant: 'secondary', icon: 'sliders', align: 'left', slashes: false }));
  d.add(L.button(d, 436, 990, 290, 60, 'Commandes…', { variant: 'ghost', icon: 'help', align: 'left', slashes: false }));
  d.add(T(760, 1027, 'Règles par défaut · endurance 100 · pas de limite de temps', { size: 18, fill: C.dim }));
  d.add(L.button(d, 1440, 978, 420, 74, 'Combattre !', { variant: 'primary', icon: 'play', key: 'Entrée', keyW: 58 }));
  return d;
}

// ============================== 04 ATELIER ==============================
function builder() {
  const d = new Doc(W, H, '04-ecran-atelier');
  d.add(L.background(d, W, H, { floor: false }), topBar(d, 'Assembler une toupie', { back: 'Menu', sub: 'Atelier' }));
  // liste
  d.add(L.panel(d, 40, 130, 380, 910, { title: 'Mes toupies', index: '6', accent: C.cyan }));
  const list = [['Cyclone Furieux', 'Attaque', false], ['Forteresse', 'Défense', false], ['Éternelle', 'Endurance', false], ['Juste Milieu', 'Équilibre', false], ['Comète', 'Attaque', true], ['Brise Éternelle', 'Endurance', true]];
  list.forEach(([n, t, perso], i) => {
    const y = 200 + i * 92, on = i === 0;
    d.add(g(`Ligne ${n}`, [path(chamfer(64, y, 332, 80, [14, 0, 14, 0]), { fill: on ? '#1B2A60' : '#0E0B30', stroke: on ? C.cyan : C.line, 'stroke-width': on ? 2 : 1 }),
      on ? rect(64, y + 16, 4, 48, { fill: C.cyan }) : '', Ar.top(d, 112, y + 40, 28, P[n], { angle: i }),
      D(156, y + 36, n.toUpperCase(), { size: 16, ls: 1 }), L.badge(d, 156, y + 46, t, { h: 20 }), perso ? D(380, y + 30, '★', { size: 16, fill: C.cyan, anchor: 'end' }) : '']));
  });
  d.add(L.button(d, 64, 968, 160, 50, 'Nouvelle', { variant: 'secondary', icon: 'plus' }), L.button(d, 236, 968, 160, 50, 'Supprimer', { variant: 'danger', icon: 'trash' }));
  // atelier
  const p = P['Cyclone Furieux'], cx = 880;
  d.add(L.panel(d, 450, 130, 860, 910, { title: 'Établi', index: '23 PTS', accent: C.green }));
  d.add(B.field(d, 490, 186, 780, 'Nom', 'Cyclone Furieux', true));
  d.add(rect(cx - 1, 290, 2, 420, { fill: C.cyan, opacity: 0.25 }), circle(cx, 480, 200, { fill: C.violet, opacity: 0.15, filter: d.blur(40) }));
  d.add(Ar.sideView(d, cx, 300, 320, p, { exploded: true, id: 'Vue éclatée' }));
  const slot = (x, y, kind, id, name, stats, col, art, ax, ay) => {
    const lx = x < cx ? x + 220 : x;
    d.add(g(`Emplacement ${kind}`, [path(`M${lx},${y + 40} H${(lx + ax) / 2} L${ax},${ay}`, { fill: 'none', stroke: col, 'stroke-width': 1.5, 'stroke-dasharray': '4 4' }), circle(ax, ay, 4, { fill: col }),
      path(chamfer(x, y, 220, 150, [14, 0, 14, 0]), { fill: '#0B0930', stroke: col, 'stroke-width': 1.5 }), label(x + 16, y + 26, kind, col), D(x + 204, y + 26, id, { size: 12, fill: C.dim, anchor: 'end' }),
      art, D(x + 16, y + 112, name.toUpperCase(), { size: 16, ls: 1 }),
      T(x + 16, y + 136, stats, { size: 15, fill: C.muted, weight: 600 }), L.icon('back', x + 160, y + 128, 16, C.cyan), L.icon('next', x + 196, y + 128, 16, C.cyan)]));
  };
  slot(490, 270, 'Boulon', 'B2', 'Perçant', "Lance d'acier · vague", '#C9D8FF', Ar.boltArt(d, 'lance', 540, 330, 26, 0), cx, 310);
  slot(1060, 270, 'Roue', 'RM2', 'Lame Cyclone ↻', 'A 6 · D 2 · E 2', TYPE.Attaque, Ar.wheelArt(d, 'RM2', 1110, 330, 32), cx + 120, 390);
  slot(490, 500, 'Intermédiaire', 'PI2', 'Griffe', 'A 4 · D 1 · E 1', C.warn, Ar.middleArt(d, 'PI2', 540, 562, 30, 0), cx - 90, 470);
  slot(1060, 500, 'Pointe', 'P2', 'Gomme', 'A 4 · D 1 · E 2', C.magenta, Ar.tipArt(d, 'P2', 1110, 535, 60), cx + 20, 560);
  d.add(label(490, 728, 'Aperçu en rotation'), line(490, 740, 1270, 740, { stroke: C.line }));
  d.add(Ar.top(d, 640, 880, 110, p, { angle: 0.8, aura: true }));
  d.add(B.wrap(800, 820, 'Lame Cyclone : trois crochets d’acier inclinés dans le sens horaire. Avec la Gomme, la toupie accroche le sol et charge fort, mais s’use vite.', { size: 19, fill: C.muted, weight: 500, max: 44, lh: 26 }));
  d.add(T(800, 980, '« Cyclone Furieux » mise à jour.', { size: 18, fill: C.green, weight: 700 }));
  // stats
  d.add(L.panel(d, 1340, 130, 540, 910, { title: 'Statistiques', index: 'TOTAL', accent: C.magenta }));
  const st = Ar.sumStats(p.parts), rc = [1610, 400], R = 175, ang = [-90, 30, 150].map(a => a * Math.PI / 180);
  const pt = (k, v) => [rc[0] + Math.cos(ang[k]) * R * v, rc[1] + Math.sin(ang[k]) * R * v];
  d.add(g('Radar', [[1, 0.66, 0.33].map(k => path(L.polyD([0, 1, 2].map(i => pt(i, k))), { fill: 'none', stroke: C.line })).join(''),
    ...[0, 1, 2].map(i => line(rc[0], rc[1], ...pt(i, 1), { stroke: C.line })),
    path(L.polyD([0, 1, 2].map(i => pt(i, st[i] / 15))), { fill: TYPE.Attaque, 'fill-opacity': 0.25, stroke: TYPE.Attaque, 'stroke-width': 2 }),
    path(L.polyD([0, 1, 2].map(i => pt(i, st[i] / 15))), { fill: 'none', stroke: TYPE.Attaque, 'stroke-width': 6, opacity: 0.4, filter: d.blur(6) }),
    ...[0, 1, 2].map(i => circle(...pt(i, st[i] / 15), 5, { fill: '#FFFFFF' })),
    D(rc[0], rc[1] - R - 16, `ATT ${st[0]}`, { size: 16, fill: TYPE.Attaque, anchor: 'middle' }), D(pt(1, 1)[0] + 6, pt(1, 1)[1] + 30, `DÉF ${st[1]}`, { size: 16, fill: TYPE['Défense'], anchor: 'middle' }), D(pt(2, 1)[0] - 6, pt(2, 1)[1] + 30, `END ${st[2]}`, { size: 16, fill: TYPE.Endurance, anchor: 'middle' })]));
  d.add(label(1380, 600, 'Type obtenu'), D(1380, 640, 'ATTAQUE', { size: 38, fill: TYPE.Attaque, ls: 3 }), T(1380, 666, 'Stat la plus élevée : Attaque 14', { size: 17, fill: C.muted }));
  d.add(L.statBar(d, 1380, 716, 460, 'Attaque', st[0], 15, TYPE.Attaque), L.statBar(d, 1380, 764, 460, 'Défense', st[1], 15, TYPE['Défense']), L.statBar(d, 1380, 812, 460, 'Endurance', st[2], 15, TYPE.Endurance));
  d.add(label(1380, 866, 'Budget'), D(1840, 867, '23 / 23 PTS', { size: 16, fill: C.green, anchor: 'end' }), L.statBar(d, 1380, 878, 460, null, 23, 23, C.green, { segs: 23, h: 8 }));
  d.add(L.button(d, 1380, 958, 460, 62, 'Enregistrer', { variant: 'primary', icon: 'save' }));
  return d;
}

// ============================== 05 COMBAT ==============================
function hudCard(d, x, y, w, who, p, pct, o = {}) {
  const col = TYPE[p.type], right = o.right, out = [];
  out.push(path(chamfer(x, y, w, 150, right ? [0, 26, 0, 26] : [26, 0, 26, 0]), { fill: '#0B0930', 'fill-opacity': 0.88, stroke: o.local ? C.green : C.line, 'stroke-width': 1.5 }));
  out.push(Ar.top(d, right ? x + w - 60 : x + 60, y + 58, 36, p, { angle: 1 }));
  const tx = right ? x + w - 116 : x + 116, an = right ? 'end' : undefined;
  out.push(D(tx, y + 40, who, { size: 22, ls: 2.5, fill: o.local ? C.green : C.danger, anchor: an }), T(tx, y + 66, `${p.name} · ${p.type}`, { size: 17, fill: C.muted, anchor: an }));
  out.push(D(right ? x + 30 : x + w - 30, y + 46, String(pct), { size: 40, fill: pct < 25 ? C.danger : C.text, anchor: right ? undefined : 'end' }));
  out.push(L.enduranceBar(d, x + 24, y + 96, w - 48, 20, pct, pct < 50 && !o.local ? C.warn : C.green, { color2: pct < 50 && !o.local ? C.danger : C.lime }));
  out.push(label(x + 24, y + 138, 'Endurance'), T(x + w - 24, y + 138, o.status || '', { size: 15, fill: C.muted, anchor: 'end', weight: 700 }));
  return g(`HUD ${who}`, out);
}
function arenaScene(d, R = 370, cx = 960, cy = 495) {
  return [Ar.stadium(d, 'classique', cx, cy, R)];
}
function battle() {
  const d = new Doc(W, H, '05-ecran-combat');
  d.add(L.background(d, W, H, { floor: false, core: '#130E48' }));
  d.add(...arenaScene(d));
  const a = [800, 410], b = [1110, 580];
  // traînée de dash
  d.add(g('Traînée dash', [path(L.polyD([[a[0] - 230, a[1] - 140], [a[0] - 10, a[1] - 34], [a[0] + 10, a[1] + 34], [a[0] - 250, a[1] - 100]]), { fill: d.lin([[0, C.green, 0], [1, C.green, 0.6]], 25), filter: d.blur(4) }),
    ...[0, 1, 2].map(i => line(a[0] - 200 + i * 30, a[1] - 140 + i * 30 + 20, a[0] - 60 + i * 10, a[1] - 50 + i * 20, { stroke: C.lime, 'stroke-width': 2, opacity: 0.8 }))]));
  d.add(g('Orbite bot', [path(`M${b[0] - 60},${b[1] + 70} A200,200 0 0 0 ${b[0] + 150},${b[1] - 60}`, { fill: 'none', stroke: TYPE['Défense'], 'stroke-width': 3, opacity: 0.5, 'stroke-dasharray': '2 8' })]));
  d.add(Ar.top(d, ...a, 46, P['Cyclone Furieux'], { angle: 0.5, aura: true }), Ar.top(d, ...b, 46, P['Forteresse'], { angle: 2, aura: true }));
  // étiquettes
  d.add(g('Étiquette VOUS', [path(skew(a[0] - 40, a[1] - 96, 80, 24, 8), { fill: C.green }), D(a[0], a[1] - 78, 'VOUS', { size: 13, fill: '#03140A', anchor: 'middle', ls: 2 }), path(`M${a[0] - 6},${a[1] - 72} l6,8 l6,-8 Z`, { fill: C.green })]));
  d.add(g('Étiquette BOT', [path(skew(b[0] - 40, b[1] - 96, 80, 24, 8), { fill: C.danger }), D(b[0], b[1] - 78, 'BOT', { size: 13, fill: '#FFFFFF', anchor: 'middle', ls: 2 }), path(`M${b[0] - 6},${b[1] - 72} l6,8 l6,-8 Z`, { fill: C.danger })]));
  // haut
  d.add(L.button(d, 40, 32, 180, 44, 'Menu', { variant: 'ghost', key: 'Échap', keyW: 50, align: 'left', slashes: false }), L.button(d, 232, 32, 250, 44, 'Changer de toupies', { variant: 'ghost' }));
  d.add(g('Manche', [path(chamfer(760, 22, 400, 70, [0, 0, 24, 24]), { fill: '#0B0930', stroke: C.lineHi }), D(960, 52, 'MANCHE 1', { size: 15, ls: 3, fill: C.muted, anchor: 'middle' }),
    D(900, 82, '0', { size: 30, fill: C.green, anchor: 'middle' }), D(960, 80, '–', { size: 24, fill: C.dim, anchor: 'middle' }), D(1020, 82, '0', { size: 30, fill: C.danger, anchor: 'middle' }),
    T(960, 118, 'Victoires 3  ·  Défaites 1  ·  Nuls 0', { size: 16, fill: C.dim, anchor: 'middle' })]));
  d.add(D(1880, 56, 'CLASSIQUE', { size: 20, anchor: 'end', ls: 2.5 }), T(1880, 80, 'NOYAU HOLO · sans limite de temps', { size: 16, fill: C.cyan, anchor: 'end', weight: 700 }));
  // bas
  d.add(hudCard(d, 40, 890, 560, 'VOUS', P['Cyclone Furieux'], 72, { local: true, status: 'Pilotage actif' }));
  d.add(hudCard(d, 1320, 890, 560, 'BOT · NORMAL', P['Forteresse'], 41, { right: true, status: 'Onde de choc prête' }));
  d.add(B.dashChip(d, 40, 830, 320, 'DASH !', C.cyan, null));
  d.add(B.specialButton(d, 660, 940, 600, 'lance', 'ready'));
  d.add(T(960, 1062, '↑ ↓ ← →  piloter   ·   Espace  dash   ·   E  coup spécial', { size: 16, fill: C.dim, anchor: 'middle', weight: 600 }));
  return d;
}

// ============================== 06 DUEL ==============================
function duelBar(d, x, y, w, h, cursor, zone, o = {}) {
  const out = [], zx = x + w * (0.5 - zone / 2), zw = w * zone;
  out.push(path(skew(x, y, w, h, h * 0.5), { fill: d.lin([[0, '#5A0F2A'], [0.5, C.danger], [1, '#5A0F2A']], 0), stroke: '#FF7A9B', 'stroke-width': 1.2 }));
  out.push(path(skew(zx, y, zw, h, h * 0.5), { fill: C.green, opacity: 0.5, filter: d.blur(8) }), path(skew(zx, y, zw, h, h * 0.5), { fill: d.lin([[0, C.green], [1, C.lime]], 0) }));
  const cx = x + w * cursor;
  out.push(path(L.polyD([[cx - 10, y - 14], [cx + 10, y - 14], [cx + 4, y + h + 14], [cx - 4, y + h + 14]]), { fill: '#FFFFFF', stroke: '#05040F', 'stroke-width': 1.5 }));
  out.push(rect(cx - 2, y - 14, 4, h + 28, { fill: '#FFFFFF', opacity: 0.8, filter: d.blur(6) }));
  return g(o.id || 'Barre de duel', out);
}
function duel() {
  const d = new Doc(W, H, '06-ecran-duel');
  d.add(L.background(d, W, H, { floor: false }));
  d.add(g('Arène zoomée', arenaScene(d, 720, 960, 520), { 'clip-path': d.clip(rect(0, 0, W, H)) }));
  d.add(vignette(d));
  const ax = 870, bx = 1050, y = 470;
  d.add(Ar.top(d, ax, y, 96, P['Cyclone Furieux'], { angle: 0.2, aura: true }), Ar.top(d, bx, y, 96, P['Forteresse'], { angle: 1.4, aura: true }));
  // étincelles au point de friction
  const sp = []; for (let i = 0; i < 26; i++) { const a = (i / 26) * Math.PI * 2, r0 = 12, r1 = 40 + (i * 37) % 90; sp.push(line(960 + Math.cos(a) * r0, y + Math.sin(a) * r0 * 1.6, 960 + Math.cos(a) * r1 * 0.6, y + Math.sin(a) * r1 * 1.4, { stroke: i % 3 ? C.warn : '#FFFFFF', 'stroke-width': i % 2 ? 2.5 : 1.4, 'stroke-linecap': 'round' })); }
  d.add(g('Étincelles', [ellipse(960, y, 50, 110, { fill: C.warn, opacity: 0.5, filter: d.blur(26) }), ellipse(960, y, 16, 50, { fill: '#FFFFFF', opacity: 0.9, filter: d.blur(6) }), ...sp]));
  d.add(B.banner(d, 510, 60, 900, 'DUEL !', C.magenta));
  // barres
  d.add(label(660, 726, 'Bot'), D(1260, 727, 'SÉRIE 1', { size: 14, fill: C.muted, anchor: 'end' }), duelBar(d, 660, 740, 600, 22, 0.71, 0.24, { id: 'Barre duel adversaire' }));
  d.add(g('Consigne', [T(770, 830, 'Appuyez sur', { size: 24, anchor: 'end', weight: 600 }), L.keycap(d, 784, 810, 'Espace', { w: 80 }), T(878, 830, 'quand le curseur est dans le vert', { size: 24, weight: 600 })]));
  d.add(duelBar(d, 460, 870, 1000, 56, 0.47, 0.2, { id: 'Barre duel joueur' }));
  d.add(g('Série', [label(460, 980, 'Série à taille minimale'), ...[0, 1, 2].map(i => path(L.polyD([[700 + i * 40, 962], [714 + i * 40, 974], [700 + i * 40, 986], [686 + i * 40, 974]]), { fill: i < 2 ? C.green : 'none', stroke: C.green, 'stroke-width': 1.5 })),
    T(1460, 980, 'Le premier qui rate subit 1,5× les dégâts du dash', { size: 17, fill: C.muted, anchor: 'end' })]));
  d.add(g('Mini HUD', [label(40, 50, 'Vous'), L.enduranceBar(d, 40, 62, 320, 14, 68, C.green), label(1880 - 320, 50, 'Bot'), L.enduranceBar(d, 1560, 62, 320, 14, 44, C.warn, { color2: C.danger })]));
  return d;
}

// ============================== 07 RÉSULTAT ==============================
function result() {
  const d = new Doc(W, H, '07-ecran-resultat');
  d.add(L.background(d, W, H, { floor: false, core: '#16205E' }));
  d.add(g('Arène floutée', arenaScene(d, 520, 960, 560), { opacity: 0.25 }), vignette(d));
  // rayons
  const rays = []; for (let i = 0; i < 16; i++) { const a = -Math.PI / 2 + (i - 7.5) * 0.17; rays.push(path(L.polyD([[960, 520], [960 + Math.cos(a - 0.03) * 900, 520 + Math.sin(a - 0.03) * 900], [960 + Math.cos(a + 0.03) * 900, 520 + Math.sin(a + 0.03) * 900]]), { fill: i % 2 ? C.green : C.cyan, opacity: 0.08 })); }
  d.add(g('Rayons', rays));
  d.add(...chroma(960, 220, 'VICTOIRE !', { size: 150, anchor: 'middle', italic: true, ls: 8 }));
  d.add(path(skew(700, 250, 520, 40, 14), { fill: d.lin([[0, C.green], [1, C.lime]], 0) }), D(960, 278, 'ÉJECTION HORS DU STADIUM', { size: 17, ls: 3, fill: '#03140A', anchor: 'middle' }));
  d.add(circle(960, 500, 190, { fill: C.green, opacity: 0.18, filter: d.blur(40) }), Ar.top(d, 960, 500, 150, P['Cyclone Furieux'], { angle: 0.6, aura: true }));
  d.add(D(960, 700, 'CYCLONE FURIEUX', { size: 34, ls: 3, anchor: 'middle' }), T(960, 728, 'Lame Cyclone · Griffe · Gomme · Boulon Perçant', { size: 18, fill: C.muted, anchor: 'middle' }));
  d.add(card(d, 460, 760, 1000, 130, { c: [22, 0, 22, 0] }));
  [['Durée', '00:18', C.text], ['Chocs', '12', C.text], ['Dashs', '4', C.cyan], ['Coup spécial', '24 dég.', C.magenta], ['Endurance', '46', C.green]].forEach(([k, v, c], i) => {
    const x = 520 + i * 192;
    d.add(g(`Stat fin ${k}`, [label(x, 805, k), D(x, 856, v, { size: 38, fill: c }), i ? line(x - 26, 790, x - 26, 866, { stroke: C.line }) : '']));
  });
  d.add(T(960, 930, 'Victoires 4  ·  Défaites 1  ·  Nuls 0', { size: 20, fill: C.muted, anchor: 'middle', weight: 600 }));
  d.add(L.button(d, 520, 960, 340, 70, 'Revanche', { variant: 'primary', key: 'R' }), L.button(d, 880, 960, 300, 70, 'Changer de toupies', { variant: 'secondary' }), L.button(d, 1200, 960, 200, 70, 'Menu', { variant: 'ghost' }));
  return d;
}

// ============================== 08 LOBBY ==============================
function lobby() {
  const d = new Doc(W, H, '08-ecran-lobby');
  d.add(L.background(d, W, H, { floor: false }), topBar(d, 'Lobby · Arène Nova', { back: 'Quitter', sub: '' }));
  d.add(...chips(d, 900, 48, [['Hôte', C.green, 'star'], ['192.168.1.24 · visible en LAN', C.cyan, 'wifi']]));
  d.add(L.panel(d, 60, 130, 1060, 780, { title: 'Joueurs', index: '3 / 4', accent: C.green }));
  const slots = [['Nova', 'Vous · hôte', P['Cyclone Furieux'], 'Prêt', C.green], ['Kaï', '192.168.1.31', P['Colosse'], 'Prêt', C.green], ['Mira', '192.168.1.40', P['Éternelle'], 'Choisit sa toupie…', C.warn], [null]];
  slots.forEach(([n, sub, p, st, col], i) => {
    const y = 200 + i * 170;
    if (!n) {
      d.add(g('Emplacement libre', [path(chamfer(100, y, 980, 150, [20, 0, 20, 0]), { fill: 'none', stroke: C.line, 'stroke-width': 1.5, 'stroke-dasharray': '6 6' }),
        D(590, y + 72, 'EMPLACEMENT LIBRE', { size: 20, ls: 3, fill: C.dim, anchor: 'middle' }), T(590, y + 100, 'En attente d’un joueur sur le réseau local…', { size: 18, fill: C.dim, anchor: 'middle' })]));
      return;
    }
    d.add(g(`Joueur ${n}`, [path(chamfer(100, y, 980, 150, [20, 0, 20, 0]), { fill: '#0E0B30', stroke: i === 0 ? C.green : C.line, 'stroke-width': i === 0 ? 2 : 1.2 }),
      D(130, y + 44, `J${i + 1}`, { size: 18, fill: C.dim }), Ar.top(d, 230, y + 75, 50, p, { angle: i, aura: st === 'Prêt' }),
      D(310, y + 62, n.toUpperCase(), { size: 28, ls: 2.5 }), T(310, y + 90, sub, { size: 17, fill: C.muted }),
      label(600, y + 52, 'Toupie'), D(600, y + 82, p.name.toUpperCase(), { size: 20, ls: 1.5 }), L.badge(d, 600, y + 96, p.type, { h: 20 }),
      path(skew(860, y + 54, 190, 42, 12), { fill: col, 'fill-opacity': 0.14, stroke: col }), D(955, y + 81, st.toUpperCase(), { size: st.length > 6 ? 12 : 16, ls: 1.5, fill: col, anchor: 'middle' })]));
  });
  d.add(L.panel(d, 1150, 130, 710, 780, { title: 'Réglages de l’hôte', index: 'VISIBLES PAR TOUS', accent: C.cyan }));
  d.add(B.field(d, 1190, 190, 630, 'Stadium', 'Volcan · Caldeira', false, true));
  d.add(Ar.stadium(d, 'volcan', 1505, 420, 100));
  d.add(B.segmented(d, 1190, 560, 630, 'Nombre de joueurs', ['2', '3', '4'], 2));
  d.add(B.toggle(d, 1190, 640, 'Coups spéciaux', true), B.toggle(d, 1190, 700, 'Limite de temps', false));
  d.add(L.button(d, 1190, 790, 630, 56, 'Personnalisation…', { variant: 'secondary', icon: 'sliders' }));
  d.add(T(1190, 880, 'Mêlée à 3-4 : éliminer un adversaire touché dans les 4 s rend 35 d’endurance.', { size: 16, fill: C.muted }));
  d.add(L.button(d, 60, 960, 300, 66, 'Fermer le lobby', { variant: 'danger', icon: 'quit' }));
  d.add(T(1150, 1000, '2 joueurs sur 3 prêts', { size: 19, fill: C.warn, weight: 700 }));
  d.add(L.button(d, 1440, 952, 420, 74, 'Lancer la partie', { variant: 'primary', icon: 'play', state: 'disabled' }));
  return d;
}

// ============================== 09 TOURNOI ==============================
function tournament() {
  const d = new Doc(W, H, '09-ecran-tournoi');
  d.add(L.background(d, W, H, { floor: false }), topBar(d, 'Tournoi · Coupe Néon', { back: 'Abandonner', sub: '' }));
  d.add(...chips(d, 1010, 48, [['Élimination directe', C.cyan, 'trophy'], ['8 participants', C.violet, 'user'], ['Express · 2 manches', C.green, 'timer']]));
  d.add(L.panel(d, 40, 130, 1250, 910, { title: 'Tableau', index: 'TOUR 2 / 3', accent: C.cyan }));
  const ppl = [['Vous', 'Cyclone Furieux'], ['Atlas', 'Forteresse'], ['Kira', 'Éternelle'], ['Rook', 'Colosse'], ['Vega', 'Comète'], ['Lyn', 'Juste Milieu'], ['Orion', 'Gardienne'], ['Sol', 'Brise Éternelle']];
  const box = (x, y, a, b, sa, sb, o = {}) => {
    const out = [path(chamfer(x, y, 250, 100, [14, 0, 14, 0]), { fill: '#0E0B30', stroke: o.you ? C.green : C.line, 'stroke-width': o.you ? 2 : 1.2 })];
    [[a, sa], [b, sb]].forEach(([pp, s], k) => {
      const yy = y + 12 + k * 44, win = s !== null && s === Math.max(sa ?? 0, sb ?? 0) && s > 0;
      if (!pp) { out.push(T(x + 20, yy + 26, 'À déterminer', { size: 16, fill: C.dim })); return; }
      out.push(Ar.top(d, x + 30, yy + 18, 16, P[pp[1]], { angle: k, shadow: false }), D(x + 56, yy + 25, pp[0].toUpperCase(), { size: 16, fill: win ? C.text : C.muted, ls: 1 }),
        s !== null ? D(x + 230, yy + 26, String(s), { size: 18, fill: win ? C.green : C.dim, anchor: 'end' }) : '');
    });
    out.push(line(x + 14, y + 52, x + 236, y + 52, { stroke: C.line }));
    if (o.live) out.push(path(skew(x + 160, y - 12, 90, 22, 6), { fill: C.green }), D(x + 205, y + 4, 'À JOUER', { size: 11, fill: '#03140A', anchor: 'middle', ls: 1.5 }));
    return g(`Match ${a ? a[0] : '?'} – ${b ? b[0] : '?'}`, out);
  };
  const cols = [90, 470, 850], q = [220, 430, 640, 850];
  ['Quarts de finale', 'Demi-finales', 'Finale'].forEach((t, i) => d.add(label(cols[i], 206, t, i === 1 ? C.green : C.muted)));
  const qs = [[0, 1, 2, 0], [2, 3, 2, 1], [4, 5, 0, 2], [6, 7, 2, 1]];
  qs.forEach(([a, b, sa, sb], i) => d.add(box(cols[0], q[i], ppl[a], ppl[b], sa, sb, { you: i === 0 })));
  const s1 = (q[0] + q[1]) / 2, s2 = (q[2] + q[3]) / 2, f = (s1 + s2) / 2;
  d.add(box(cols[1], s1, ppl[0], ppl[2], 0, 0, { you: true, live: true }), box(cols[1], s2, ppl[5], ppl[6], null, null), box(cols[2], f, null, null, null, null));
  let con = ''; const link = (x1, y1, y2, x2, ym) => { const m = (x1 + x2) / 2; return `M${x1},${y1} H${m} V${y2} H${x1} M${m},${ym} H${x2} `; };
  con += link(cols[0] + 250, q[0] + 50, q[1] + 50, cols[1], s1 + 50) + link(cols[0] + 250, q[2] + 50, q[3] + 50, cols[1], s2 + 50) + link(cols[1] + 250, s1 + 50, s2 + 50, cols[2], f + 50);
  d.add(path(con, { fill: 'none', stroke: C.lineHi, 'stroke-width': 1.5 }), path(`M${cols[0] + 250},${q[0] + 50} H${(cols[0] + 250 + cols[1]) / 2} V${s1 + 50} H${cols[1]}`, { fill: 'none', stroke: C.green, 'stroke-width': 3 }));
  d.add(g('Champion', [circle(1180, f + 50, 70, { fill: C.warn, opacity: 0.15, filter: d.blur(20) }), path(chamfer(1130, f, 100, 100, [20, 0, 20, 0]), { fill: '#0E0B30', stroke: C.warn, 'stroke-width': 2 }), L.icon('trophy', 1180, f + 50, 48, C.warn), D(1180, f + 132, 'CHAMPION', { size: 14, fill: C.warn, anchor: 'middle', ls: 2 })]));
  // votre match
  d.add(L.panel(d, 1320, 130, 560, 520, { title: 'Votre match', index: 'DEMI-FINALE', accent: C.green }));
  d.add(Ar.top(d, 1450, 330, 80, P['Cyclone Furieux'], { angle: 0.4, aura: true }), Ar.top(d, 1750, 330, 80, P['Éternelle'], { angle: 2, aura: true }));
  d.add(...chroma(1600, 350, 'VS', { size: 46, anchor: 'middle', italic: true }));
  d.add(D(1450, 455, 'VOUS', { size: 22, anchor: 'middle', fill: C.green, ls: 2 }), T(1450, 480, 'Cyclone Furieux', { size: 17, anchor: 'middle', fill: C.muted }), D(1750, 455, 'KIRA', { size: 22, anchor: 'middle', fill: C.danger, ls: 2 }), T(1750, 480, 'Éternelle · bot difficile', { size: 17, anchor: 'middle', fill: C.muted }));
  d.add(T(1600, 528, 'Premier à 2 manches · stadium Glace', { size: 18, anchor: 'middle', weight: 600 }));
  d.add(L.button(d, 1360, 560, 480, 66, 'Prêt — jouer le match', { variant: 'primary', icon: 'play' }));
  d.add(L.panel(d, 1320, 680, 560, 360, { title: 'Tours et résultats', index: '4 / 7', accent: C.violet }));
  [['Vous', '2 – 0', 'Atlas', true], ['Kira', '2 – 1', 'Rook'], ['Vega', '0 – 2', 'Lyn'], ['Orion', '2 – 1', 'Sol']].forEach(([a, s, b, you], i) => {
    const y = 760 + i * 64;
    d.add(g(`Résultat ${a}-${b}`, [path(skew(1350, y, 500, 50, 12), { fill: you ? '#12301F' : '#0E0B30', stroke: you ? C.green : C.line }), label(1374, y + 31, 'Quart', C.dim),
      D(1560, y + 32, a.toUpperCase(), { size: 17, anchor: 'end' }), D(1610, y + 32, s, { size: 18, fill: C.cyan, anchor: 'middle' }), D(1660, y + 32, b.toUpperCase(), { size: 17 })]));
  });
  return d;
}

for (const s of [menu, setup, builder, battle, duel, result, lobby, tournament]) s().save(OUT);
