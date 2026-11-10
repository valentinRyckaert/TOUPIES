'use strict';
const L = require('./lib');
const { C, TYPE, g, rect, circle, ellipse, line, path, polyD, chamfer, skew, T, D, r2 } = L;

const TAU = Math.PI * 2;
const hsl = (h, s, l) => `hsl(${h}, ${s}%, ${l}%)`;
// hsl() n'est pas toujours compris à l'import : conversion en hex
function hex(h, s, l) {
  s /= 100; l /= 100;
  const k = n => (n + h / 30) % 12, a = s * Math.min(l, 1 - l);
  const f = n => l - a * Math.max(-1, Math.min(k(n) - 3, Math.min(9 - k(n), 1)));
  return '#' + [f(0), f(8), f(4)].map(x => Math.round(x * 255).toString(16).padStart(2, '0')).join('');
}
const frac = x => x - Math.floor(x);
const smooth = (a, b, x) => { const t = Math.max(0, Math.min(1, (x - a) / (b - a))); return t * t * (3 - 2 * t); };

// ================= PIÈCES (données du catalogue + forme) =================
// Profil polaire de la roue : r(θ) ∈ [0,1] — chaque roue a une silhouette unique.
const WHEELS = {
  RM1: { name: 'Rempart', hue: 210, stats: [3, 5, 2], spin: 'D', kind: 'Défense',
    r: t => { const u = frac(t * 6 / TAU); return 0.8 + 0.2 * smooth(0.06, 0.16, u) * (1 - smooth(0.84, 0.94, u)); } },
  RM2: { name: 'Lame Cyclone', hue: 0, stats: [6, 2, 2], spin: 'D', kind: 'Attaque',
    r: t => { const u = frac(t * 3 / TAU); return 0.72 + 0.28 * Math.pow(u, 2.4) * (1 - smooth(0.96, 1, u)); } },
  RM3: { name: 'Bastion', hue: 120, stats: [2, 6, 2], spin: 'G', kind: 'Défense',
    r: t => { const u = frac(t * 8 / TAU); return 0.86 + 0.14 * smooth(0.12, 0.25, u) * (1 - smooth(0.75, 0.88, u)); } },
  RM4: { name: 'Éclipse', hue: 275, stats: [2, 2, 6], spin: 'G', kind: 'Endurance',
    r: t => 0.97 - 0.22 * Math.pow(Math.max(0, Math.cos(t - 0.6)), 60) - 0.1 * Math.pow(Math.max(0, Math.cos(t + 2.5)), 60) },
  RM5: { name: 'Prisme', hue: 45, stats: [4, 3, 3], spin: 'D', kind: 'Équilibre',
    r: t => { const n = 6, a = frac(t * n / TAU) * TAU / n - Math.PI / n; return Math.cos(Math.PI / n) / Math.cos(a) * 0.95; } },
  RM6: { name: 'Tonnerre', hue: 25, stats: [5, 3, 2], spin: 'G', kind: 'Attaque',
    r: t => { const u = frac(t * 5 / TAU); return 0.7 + 0.3 * (u < 0.5 ? u * 2 : (u < 0.62 ? 0.55 : 0.55 + (u - 0.62) * 1.18)) * (1 - smooth(0.95, 1, u)); } },
  RM7: { name: 'Ouragan', hue: 190, stats: [2, 3, 5], spin: 'D', kind: 'Endurance',
    r: t => { const u = frac(t * 4 / TAU); return 0.72 + 0.28 * Math.pow(Math.sin(Math.PI * Math.pow(u, 0.6)), 1.5); } },
  RM8: { name: 'Orbite', hue: 320, stats: [4, 2, 4], spin: 'G', kind: 'Équilibre',
    r: t => 0.84 + 0.16 * Math.pow(Math.max(0, Math.cos(2 * t)), 8) },
  RM9: { name: 'Météore', hue: 15, stats: [7, 2, 1], spin: 'D', kind: 'Attaque',
    r: t => { const u = frac(t * 2 / TAU); return 0.7 + 0.3 * Math.pow(u, 3.5) * (1 - smooth(0.97, 1, u)); } },
  RM10: { name: 'Titan', hue: 100, stats: [1, 7, 2], spin: 'G', kind: 'Défense',
    r: t => { const u = frac(t * 4 / TAU); return 0.82 + 0.18 * smooth(0.1, 0.18, u) * (1 - smooth(0.62, 0.7, u)); } },
  RM11: { name: 'Nébuleuse', hue: 250, stats: [1, 2, 7], spin: 'D', kind: 'Endurance',
    r: t => 0.95 + 0.035 * Math.sin(t * 14) + 0.015 * Math.sin(t * 5 + 1) },
  RM12: { name: 'Faucon', hue: 340, stats: [5, 2, 3], spin: 'D', kind: 'Attaque',
    r: t => { const u = frac(t * 3 / TAU); return 0.7 + 0.3 * Math.pow(Math.max(0, Math.sin(Math.PI * Math.pow(u, 2.2))), 3); } },
  RM13: { name: 'Zéphyr', hue: 170, stats: [3, 2, 5], spin: 'G', kind: 'Endurance',
    r: t => { const u = frac(t * 6 / TAU); return 0.8 + 0.2 * Math.pow(u, 1.2) * (1 - smooth(0.9, 1, u)); } },
  RM14: { name: 'Golem', hue: 80, stats: [3, 6, 1], spin: 'D', kind: 'Défense',
    r: t => { const u = frac(t * 7 / TAU), i = Math.floor(t * 7 / TAU); return 0.84 + (0.1 + 0.06 * Math.sin(i * 2.7)) * smooth(0.05, 0.2, u) * (1 - smooth(0.7, 0.92, u)); } },
};
const MIDDLES = {
  PI1: { name: 'Noyau', hue: 200, stats: [2, 3, 1], style: 'noyau' },
  PI2: { name: 'Griffe', hue: 10, stats: [4, 1, 1], style: 'griffe' },
  PI3: { name: 'Carapace', hue: 130, stats: [1, 4, 1], style: 'carapace' },
  PI4: { name: 'Anneau', hue: 280, stats: [1, 1, 4], style: 'anneau' },
  PI5: { name: 'Axe', hue: 50, stats: [2, 2, 2], style: 'axe' },
  PI6: { name: 'Turbine', hue: 30, stats: [3, 1, 2], style: 'turbine' },
  PI7: { name: 'Lame', hue: 0, stats: [3, 2, 1], style: 'lame' },
  PI8: { name: 'Bouclier', hue: 140, stats: [1, 3, 2], style: 'bouclier' },
  PI9: { name: 'Spirale', hue: 300, stats: [2, 1, 3], style: 'spirale' },
  PI10: { name: 'Contrepoids', hue: 230, stats: [1, 2, 3], style: 'contrepoids' },
};
const TIPS = {
  P1: { name: 'Pivot', stats: [1, 4, 2] }, P2: { name: 'Gomme', stats: [4, 1, 2] }, P3: { name: 'Aiguille', stats: [1, 1, 5] },
  P4: { name: 'Bille', stats: [2, 3, 2] }, P5: { name: 'Plateau', stats: [3, 2, 2] }, P6: { name: 'Pointe Fine', stats: [1, 2, 4] },
  P7: { name: 'Crampon', stats: [5, 1, 1] }, P8: { name: 'Dôme', stats: [1, 5, 1] }, P9: { name: 'Rotule', stats: [2, 2, 3] },
  P10: { name: 'Sabot', stats: [3, 3, 1] },
};
const MOVES = {
  onde: { bolt: 'Tempête', name: 'Onde de choc', color: C.cyan, form: 'ANNEAU', plus: 'Gros recul : éjecte une cible déjà au bord', minus: 'Portée courte', dmg: 14, kb: 5, range: 2 },
  lance: { bolt: 'Perçant', name: "Lance d'acier", color: '#C9D8FF', form: 'VAGUE', plus: 'Les plus lourds dégâts (24)', minus: 'Aucun effet, presque aucun recul, esquivable de loin', dmg: 24, kb: 1, range: 4 },
  givre: { bolt: 'Polaire', name: 'Souffle de givre', color: '#8FF3FF', form: 'VAGUE', plus: '3 s gelée : ralentie, sans pilotage, usée ×3', minus: 'Dégâts moyens', dmg: 12, kb: 2, range: 4 },
  vortex: { bolt: 'Abysse', name: 'Vortex', color: C.violet, form: 'ANNEAU', plus: 'Aspire violemment tout le monde vers le centre', minus: 'Dégâts modérés', dmg: 10, kb: 4, range: 4 },
  distorsion: { bolt: 'Spectral', name: 'Distorsion', color: C.magenta, form: 'VAGUE', plus: 'Téléporte la cible près du bord, lancée vers l’extérieur', minus: 'Lente, facile à esquiver', dmg: 8, kb: 4, range: 3 },
  siphon: { bolt: 'Vampire', name: 'Siphon', color: C.danger, form: 'VAGUE', plus: 'Rend 60 % de l’endurance volée', minus: 'Dégâts modérés, peu de recul', dmg: 11, kb: 1, range: 4 },
  seisme: { bolt: 'Tellurique', name: 'Séisme', color: C.lime, form: 'ANNEAU', plus: 'Blesse et bloque dash + coup spécial 6 s', minus: 'Portée très courte', dmg: 13, kb: 2, range: 1 },
};
const BOLTS = { B1: 'onde', B2: 'lance', B3: 'givre', B4: 'vortex', B5: 'distorsion', B6: 'siphon', B7: 'seisme' };
const PRESETS = [
  { name: 'Cyclone Furieux', parts: ['RM2', 'PI2', 'P2', 'B2'], type: 'Attaque', lore: 'Trois lames en crochet qui mordent à chaque rotation.' },
  { name: 'Forteresse', parts: ['RM3', 'PI3', 'P1', 'B1'], type: 'Défense', lore: 'Un mur d’acier à huit bastions. Elle encaisse, puis repousse.' },
  { name: 'Éternelle', parts: ['RM4', 'PI4', 'P3', 'B6'], type: 'Endurance', lore: 'Une lune noire qui tourne plus longtemps que toutes les autres.' },
  { name: 'Juste Milieu', parts: ['RM5', 'PI5', 'P4', 'B3'], type: 'Équilibre', lore: 'Un cristal hexagonal, ni trop dur, ni trop fragile.' },
  { name: 'Gardienne', parts: ['RM1', 'PI1', 'P1', 'B7'], type: 'Défense', lore: 'Six boucliers et un cœur tellurique qui fait trembler l’arène.' },
  { name: 'Comète', parts: ['RM9', 'PI7', 'P7', 'B5'], type: 'Attaque', lore: 'Deux éperons asymétriques et une queue de distorsion.' },
  { name: 'Colosse', parts: ['RM10', 'PI8', 'P8', 'B4'], type: 'Défense', lore: 'Quatre butoirs massifs, un abysse en son centre.' },
  { name: 'Brise Éternelle', parts: ['RM13', 'PI9', 'P9', 'B3'], type: 'Endurance', lore: 'Six ailettes de vent qui glissent sur le givre.' },
];
const sumStats = parts => {
  const [w, m, t] = [WHEELS[parts[0]], MIDDLES[parts[1]], TIPS[parts[2]]];
  return [0, 1, 2].map(i => w.stats[i] + m.stats[i] + t.stats[i]);
};

// ================= GLYPHES DES BOULONS (repère -1..1) =================
const GLYPH = {
  onde: 'M-0.25,0 A0.25,0.25 0 1 0 0.25,0 A0.25,0.25 0 1 0 -0.25,0 Z M-0.62,-0.18 A0.65,0.65 0 0 1 -0.18,-0.62 M0.18,-0.62 A0.65,0.65 0 0 1 0.62,-0.18 M0.62,0.18 A0.65,0.65 0 0 1 0.18,0.62 M-0.18,0.62 A0.65,0.65 0 0 1 -0.62,0.18',
  lance: 'M0,-0.95 L0.3,-0.38 L0.11,-0.42 L0.11,0.62 L0.26,0.9 L-0.26,0.9 L-0.11,0.62 L-0.11,-0.42 L-0.3,-0.38 Z',
  givre: 'M0,-0.9 V0.9 M-0.78,-0.45 L0.78,0.45 M-0.78,0.45 L0.78,-0.45 M-0.22,-0.75 L0,-0.52 L0.22,-0.75 M-0.22,0.75 L0,0.52 L0.22,0.75 M-0.76,-0.18 L-0.45,-0.26 L-0.54,-0.56 M0.76,0.18 L0.45,0.26 L0.54,0.56 M0.76,-0.18 L0.45,-0.26 L0.54,-0.56 M-0.76,0.18 L-0.45,0.26 L-0.54,0.56',
  vortex: 'M0,0 C0.15,-0.05 0.2,0.18 0.05,0.25 C-0.2,0.35 -0.4,0.05 -0.3,-0.2 C-0.15,-0.55 0.35,-0.55 0.52,-0.15 C0.7,0.3 0.35,0.75 -0.1,0.75 C-0.6,0.75 -0.92,0.3 -0.85,-0.2',
  distorsion: 'M-0.9,0 C-0.5,-0.55 0.5,-0.55 0.9,0 C0.5,0.55 -0.5,0.55 -0.9,0 Z M0,-0.22 A0.22,0.22 0 1 0 0,0.22 A0.22,0.22 0 1 0 0,-0.22 Z M-0.6,-0.72 H-0.05 M0.1,-0.72 H0.35 M-0.3,0.72 H0.6',
  siphon: 'M0,-0.9 C0.35,-0.35 0.6,0 0.6,0.3 A0.6,0.6 0 0 1 -0.6,0.3 C-0.6,0 -0.35,-0.35 0,-0.9 Z M-0.25,0.1 L-0.12,0.45 L0,0.1 L0.12,0.45 L0.25,0.1',
  seisme: 'M-0.95,0.55 L-0.45,-0.25 L-0.15,0.15 L0.2,-0.6 L0.95,0.55 Z M-0.55,0.55 L-0.25,0.2 L0.05,0.4 L0.2,0.05',
};
function glyph(key, cx, cy, s, color, sw = 2.2, fill = false) {
  return path(GLYPH[key], {
    id: `Emblème ${MOVES[key].bolt}`, transform: `translate(${r2(cx)},${r2(cy)}) scale(${r2(s)})`,
    fill: fill ? color : 'none', 'fill-opacity': fill ? 0.25 : undefined, stroke: color,
    'stroke-width': r2(sw / s), 'stroke-linejoin': 'round', 'stroke-linecap': 'round',
  });
}

// ================= TOUPIE VUE DU DESSUS =================
function profilePath(fn, cx, cy, R, rot = 0, mirror = false, scale = 1) {
  const pts = [], N = 260;
  for (let i = 0; i < N; i++) {
    const t = i / N * TAU, tt = mirror ? TAU - t : t;
    const r = fn(tt) * R * scale;
    pts.push([cx + Math.cos(t + rot) * r, cy + Math.sin(t + rot) * r]);
  }
  return polyD(pts);
}

function middleArt(d, key, cx, cy, R, rot) {
  const m = MIDDLES[key], col = hex(m.hue, 95, 62), dark = '#0B0926', out = [];
  out.push(circle(cx, cy, R, { fill: d.rad([[0, '#2A2470'], [0.7, '#141038'], [1, '#07061A']]), stroke: '#000', 'stroke-width': 1 }));
  const at = (a, r) => [cx + Math.cos(a + rot) * r, cy + Math.sin(a + rot) * r];
  const P = (pts, a) => path(polyD(pts), a);
  switch (m.style) {
    case 'noyau': for (let i = 0; i < 4; i++) { const a = i * TAU / 4 + 0.78; const [x, y] = at(a, R * 0.72); out.push(circle(x, y, R * 0.13, { fill: '#1B1650', stroke: col, 'stroke-width': 1.5 }), circle(x, y, R * 0.04, { fill: col })); } break;
    case 'griffe': for (let i = 0; i < 3; i++) { const a = i * TAU / 3; out.push(P([at(a - 0.25, R * 0.55), at(a + 0.1, R * 1.02), at(a + 0.32, R * 0.6)], { fill: col, opacity: 0.85 })); } break;
    case 'carapace': for (let i = 0; i < 6; i++) { const a = i * TAU / 6; const pts = []; for (let k = 0; k < 6; k++) { const [x, y] = at(a, R * 0.7); pts.push([x + Math.cos(k * TAU / 6) * R * 0.22, y + Math.sin(k * TAU / 6) * R * 0.22]); } out.push(P(pts, { fill: '#1B1650', stroke: col, 'stroke-width': 1.3 })); } break;
    case 'anneau': [0.95, 0.8, 0.65].forEach((k, i) => out.push(circle(cx, cy, R * k, { fill: 'none', stroke: col, 'stroke-width': i === 0 ? 2 : 1, opacity: 1 - i * 0.25 }))); break;
    case 'axe': for (let i = 0; i < 4; i++) { const a = i * TAU / 4; out.push(P([at(a - 0.14, R * 0.5), at(a - 0.08, R * 0.98), at(a + 0.08, R * 0.98), at(a + 0.14, R * 0.5)], { fill: col, opacity: 0.8 })); } break;
    case 'turbine': for (let i = 0; i < 9; i++) { const a = i * TAU / 9; const [x0, y0] = at(a, R * 0.5), [x1, y1] = at(a + 0.55, R * 0.98), [xc, yc] = at(a + 0.1, R * 0.95); out.push(path(`M${r2(x0)},${r2(y0)} Q${r2(xc)},${r2(yc)} ${r2(x1)},${r2(y1)}`, { fill: 'none', stroke: col, 'stroke-width': 2.2, 'stroke-linecap': 'round' })); } break;
    case 'lame': for (let i = 0; i < 2; i++) { const a = i * Math.PI; out.push(P([at(a - 0.5, R * 0.5), at(a - 0.05, R * 1.04), at(a + 0.6, R * 0.62)], { fill: col })); } break;
    case 'bouclier': for (let i = 0; i < 4; i++) { const a = i * TAU / 4; let p = ''; const [x0, y0] = at(a - 0.6, R * 0.92), [x1, y1] = at(a + 0.6, R * 0.92); p = `M${r2(x0)},${r2(y0)} A${r2(R * 0.92)},${r2(R * 0.92)} 0 0 1 ${r2(x1)},${r2(y1)}`; out.push(path(p, { fill: 'none', stroke: col, 'stroke-width': R * 0.16 })); } break;
    case 'spirale': for (let i = 0; i < 3; i++) { let p = ''; for (let k = 0; k <= 20; k++) { const [x, y] = at(i * TAU / 3 + k * 0.12, R * (0.45 + k * 0.026)); p += (k ? ' L' : 'M') + `${r2(x)},${r2(y)}`; } out.push(path(p, { fill: 'none', stroke: col, 'stroke-width': 2.4, 'stroke-linecap': 'round' })); } break;
    case 'contrepoids': for (let i = 0; i < 2; i++) { const a = i * Math.PI + Math.PI / 2; const [x, y] = at(a, R * 0.7); out.push(ellipse(x, y, R * 0.28, R * 0.18, { fill: col, transform: `rotate(${r2((a + rot) * 180 / Math.PI + 90)} ${r2(x)} ${r2(y)})` })); } break;
  }
  out.push(circle(cx, cy, R, { fill: 'none', stroke: col, 'stroke-width': 1, opacity: 0.45 }));
  return g(`Intermédiaire ${m.name}`, out);
}

function boltArt(d, key, cx, cy, R, rot) {
  const mv = MOVES[key], col = mv.color, out = [];
  // couronne octogonale à angles coupés
  const oct = []; for (let i = 0; i < 8; i++) { const a = i * TAU / 8 + Math.PI / 8 + rot; oct.push([cx + Math.cos(a) * R, cy + Math.sin(a) * R]); }
  out.push(path(polyD(oct), { fill: d.lin([['0', '#E9EEFF'], ['0.5', '#7E86BE'], ['1', '#262A5A']], 60), stroke: '#05040F', 'stroke-width': 1.2 }));
  out.push(circle(cx, cy, R * 0.8, { fill: d.rad([[0, col, 0.95], [0.6, '#1A1650'], [1, '#07061A']], 0.42, 0.38, 0.65), stroke: col, 'stroke-width': 1.3 }));
  out.push(circle(cx, cy, R * 0.8, { fill: col, opacity: 0.25, filter: d.blur(R * 0.25) }));
  out.push(glyph(key, cx, cy, R * 0.5, '#FFFFFF', Math.max(1.2, R * 0.07)));
  out.push(ellipse(cx - R * 0.28, cy - R * 0.36, R * 0.26, R * 0.12, { fill: '#FFFFFF', opacity: 0.35, transform: `rotate(-35 ${r2(cx - R * 0.28)} ${r2(cy - R * 0.36)})` }));
  return g(`Boulon ${mv.bolt}`, out);
}

// spec = { parts:[wheel, middle, tip, bolt], type }
function top(d, cx, cy, R, spec, o = {}) {
  const [wk, mk, , bk] = spec.parts, w = WHEELS[wk], move = BOLTS[bk];
  const rot = o.angle ?? 0, mirror = w.spin === 'G', accent = hex(w.hue, 95, 60), deep = hex(w.hue, 80, 22), out = [];
  const tcol = TYPE[spec.type || w.kind];
  if (o.shadow !== false) out.push(ellipse(cx + R * 0.1, cy + R * 0.16, R * 1.02, R * 0.98, { fill: '#000', opacity: 0.55, filter: d.blur(R * 0.08) }));
  if (o.aura) {
    out.push(circle(cx, cy, R * 1.25, { fill: tcol, opacity: 0.22, filter: d.blur(R * 0.22) }));
    for (let i = 0; i < 3; i++) {
      const a0 = rot + i * TAU / 3, a1 = a0 + 1.4, rr = R * 1.14;
      out.push(path(`M${r2(cx + Math.cos(a0) * rr)},${r2(cy + Math.sin(a0) * rr)} A${r2(rr)},${r2(rr)} 0 0 ${mirror ? 0 : 1} ${r2(cx + Math.cos(a1) * rr)},${r2(cy + Math.sin(a1) * rr)}`, { fill: 'none', stroke: tcol, 'stroke-width': R * 0.035, 'stroke-linecap': 'round', opacity: 0.85 }));
    }
  }
  const sil = profilePath(w.r, cx, cy, R, rot, mirror);
  out.push(path(sil, { fill: d.rad([[0, '#F4F7FF'], [0.45, '#9AA4D8'], [0.8, '#4B5290'], [1, '#1D2150']], 0.35, 0.3, 0.8), stroke: '#05040F', 'stroke-width': Math.max(1, R * 0.015) }));
  out.push(path(sil, { fill: d.lin([[0, accent, 0.85], [0.5, deep, 0.6], [1, accent, 0.9]], 45) }));
  out.push(path(profilePath(w.r, cx, cy, R, rot, mirror, 0.86), { fill: 'none', stroke: '#FFFFFF', 'stroke-width': Math.max(0.8, R * 0.012), opacity: 0.45 }));
  // rainures radiales
  let gr = ''; for (let i = 0; i < 24; i++) { const a = i * TAU / 24 + rot; gr += `M${r2(cx + Math.cos(a) * R * 0.66)},${r2(cy + Math.sin(a) * R * 0.66)} L${r2(cx + Math.cos(a) * R * 0.76)},${r2(cy + Math.sin(a) * R * 0.76)} `; }
  out.push(path(gr, { stroke: '#05040F', 'stroke-width': Math.max(0.8, R * 0.012), opacity: 0.55 }));
  out.push(circle(cx, cy, R * 0.79, { fill: 'none', stroke: accent, 'stroke-width': Math.max(1, R * 0.02), opacity: 0.9 }));
  // reflet métallique
  out.push(path(`M${r2(cx - R * 0.86)},${r2(cy - R * 0.1)} A${r2(R * 0.87)},${r2(R * 0.87)} 0 0 1 ${r2(cx - R * 0.1)},${r2(cy - R * 0.86)}`, { fill: 'none', stroke: '#FFFFFF', 'stroke-width': Math.max(1.2, R * 0.03), opacity: 0.5, 'stroke-linecap': 'round' }));
  out.push(middleArt(d, mk, cx, cy, R * 0.6, rot));
  out.push(boltArt(d, move, cx, cy, R * 0.34, rot));
  return g(o.id || `Toupie ${spec.name || ''}`.trim(), out);
}

// Roue seule (fiche pièces)
function wheelArt(d, key, cx, cy, R) {
  const w = WHEELS[key], accent = hex(w.hue, 95, 60), deep = hex(w.hue, 80, 22), mirror = w.spin === 'G';
  const sil = profilePath(w.r, cx, cy, R, -Math.PI / 2, mirror);
  return g(`Roue ${w.name}`, [
    path(sil, { fill: d.rad([[0, '#F4F7FF'], [0.5, '#9AA4D8'], [1, '#1D2150']], 0.35, 0.3, 0.8), stroke: '#05040F', 'stroke-width': 1.2 }),
    path(sil, { fill: d.lin([[0, accent, 0.85], [0.5, deep, 0.6], [1, accent, 0.9]], 45) }),
    path(profilePath(w.r, cx, cy, R, -Math.PI / 2, mirror, 0.86), { fill: 'none', stroke: '#FFFFFF', 'stroke-width': 1, opacity: 0.45 }),
    circle(cx, cy, R * 0.6, { fill: '#0A0826', stroke: accent, 'stroke-width': 1.5 }),
    circle(cx, cy, R * 0.25, { fill: 'none', stroke: C.line, 'stroke-width': 1, 'stroke-dasharray': '3 3' }),
  ]);
}

// Pointe en profil (vue de côté)
function tipArt(d, key, cx, y, W) {
  const metal = d.lin([[0, '#5C649E'], [0.45, '#EEF2FF'], [1, '#3A3F72']], 0), s = W / 2, out = [];
  const P = (pts, a) => path(polyD(pts.map(([x, yy]) => [cx + x * s, y + yy * s])), a);
  out.push(P([[-1, 0], [1, 0], [0.8, 0.35], [-0.8, 0.35]], { fill: metal, stroke: '#05040F', 'stroke-width': 1 })); // collerette
  const tip = {
    P1: [[-0.55, 0.35], [0.55, 0.35], [0.18, 1.1], [-0.18, 1.1]], P2: [[-0.7, 0.35], [0.7, 0.35], [0.62, 0.95], [-0.62, 0.95]],
    P3: [[-0.35, 0.35], [0.35, 0.35], [0, 1.45]], P5: [[-0.6, 0.35], [0.6, 0.35], [0.85, 0.9], [-0.85, 0.9]],
    P6: [[-0.45, 0.35], [0.45, 0.35], [0.05, 1.3], [-0.05, 1.3]], P10: [[-0.6, 0.35], [0.6, 0.35], [0.5, 0.8], [0.25, 1.05], [-0.25, 1.05], [-0.5, 0.8]],
  }[key];
  const rubber = key === 'P2' || key === 'P7';
  if (tip) out.push(P(tip, { fill: rubber ? d.lin([[0, '#3A0F2A'], [0.5, '#FF3D6E'], [1, '#3A0F2A']], 0) : metal, stroke: '#05040F', 'stroke-width': 1 }));
  if (key === 'P4') out.push(circle(cx, y + s * 0.82, s * 0.5, { fill: d.rad([[0, '#FFFFFF'], [0.5, '#8E98C8'], [1, '#2A2F60']], 0.35, 0.3, 0.7), stroke: '#05040F', 'stroke-width': 1 }));
  if (key === 'P8') out.push(path(`M${cx - s * 0.75},${y + s * 0.35} A${s * 0.75},${s * 0.7} 0 0 0 ${cx + s * 0.75},${y + s * 0.35} Z`, { fill: metal, stroke: '#05040F', 'stroke-width': 1 }));
  if (key === 'P9') out.push(P([[-0.45, 0.35], [0.45, 0.35], [0.3, 0.6], [-0.3, 0.6]], { fill: metal, stroke: '#05040F' }), circle(cx, y + s * 0.85, s * 0.3, { fill: d.rad([[0, '#FFFFFF'], [1, '#2A2F60']], 0.35, 0.3, 0.7), stroke: '#05040F' }));
  if (key === 'P7') { out.push(P([[-0.7, 0.35], [0.7, 0.35], [0.6, 0.8], [-0.6, 0.8]], { fill: d.lin([[0, '#3A0F2A'], [0.5, '#FF3D6E'], [1, '#3A0F2A']], 0), stroke: '#05040F' })); for (let i = -2; i <= 2; i++) out.push(P([[i * 0.25 - 0.08, 0.8], [i * 0.25 + 0.08, 0.8], [i * 0.25, 1.05]], { fill: '#FF3D6E', stroke: '#05040F' })); }
  out.push(line(cx - s * 0.9, y + s * 0.06, cx + s * 0.9, y + s * 0.06, { stroke: '#FFFFFF', opacity: 0.4, 'stroke-width': 1 }));
  return g(`Pointe ${TIPS[key].name}`, out);
}

// Vue éclatée de profil : boulon / roue / intermédiaire / pointe
function sideView(d, cx, y, W, spec, o = {}) {
  const [wk, mk, tk, bk] = spec.parts, w = WHEELS[wk], m = MIDDLES[mk], mv = MOVES[BOLTS[bk]];
  const acc = hex(w.hue, 95, 60), macc = hex(m.hue, 95, 62), gap = o.exploded ? W * 0.16 : 0, out = [];
  const metal = d.lin([[0, '#3A3F72'], [0.35, '#EEF2FF'], [0.6, '#9AA4D8'], [1, '#262A5A']], 0);
  let yy = y;
  // boulon
  out.push(g(`Boulon ${mv.bolt} (profil)`, [
    path(`M${cx - W * 0.16},${yy + W * 0.12} Q${cx - W * 0.15},${yy} ${cx},${yy} Q${cx + W * 0.15},${yy} ${cx + W * 0.16},${yy + W * 0.12} Z`, { fill: metal, stroke: '#05040F' }),
    ellipse(cx, yy + W * 0.05, W * 0.07, W * 0.03, { fill: mv.color }),
    ellipse(cx, yy + W * 0.05, W * 0.07, W * 0.03, { fill: mv.color, opacity: 0.6, filter: d.blur(6) }),
  ]));
  yy += W * 0.12 + gap;
  // roue : bandeau avec dents latérales selon le profil
  const prof = []; const N = 40;
  for (let i = 0; i <= N; i++) { const t = i / N; prof.push(w.r(t * Math.PI)); }
  const rimH = W * 0.14, pts = [];
  pts.push([cx - W * 0.42, yy], [cx + W * 0.42, yy]);
  pts.push([cx + W * 0.5 * Math.max(...prof), yy + rimH * 0.35], [cx + W * 0.5 * Math.min(...prof), yy + rimH], [cx - W * 0.5 * Math.min(...prof), yy + rimH], [cx - W * 0.5 * Math.max(...prof), yy + rimH * 0.35]);
  out.push(g(`Roue ${w.name} (profil)`, [
    path(polyD(pts), { fill: metal, stroke: '#05040F', 'stroke-width': 1.2 }),
    path(polyD(pts), { fill: acc, opacity: 0.45 }),
    line(cx - W * 0.42, yy + 2, cx + W * 0.42, yy + 2, { stroke: '#FFFFFF', opacity: 0.6 }),
    rect(cx - W * 0.3, yy + rimH * 0.45, W * 0.6, rimH * 0.18, { fill: acc, opacity: 0.9 }),
  ]));
  yy += rimH + gap;
  // intermédiaire
  out.push(g(`Intermédiaire ${m.name} (profil)`, [
    path(polyD([[cx - W * 0.3, yy], [cx + W * 0.3, yy], [cx + W * 0.26, yy + W * 0.09], [cx - W * 0.26, yy + W * 0.09]]), { fill: d.lin([[0, '#141038'], [0.5, '#3B3490'], [1, '#141038']], 0), stroke: '#05040F' }),
    rect(cx - W * 0.27, yy + W * 0.035, W * 0.54, W * 0.02, { fill: macc }),
  ]));
  yy += W * 0.09 + gap;
  out.push(tipArt(d, tk, cx, yy, W * 0.24));
  return g(o.id || `Profil ${spec.name || ''}`, out);
}

// ================= STADIUMS =================
const STADIUMS = [
  { key: 'classique', name: 'Classique', code: 'NOYAU HOLO', shape: 'Cercle', rim: 'Rebord', mods: ['Référence', 'Aucun modificateur'], color: C.cyan },
  { key: 'beton', name: 'Béton', code: 'BUNKER-8', shape: 'Octogone', rim: 'Rebord très haut', mods: ['Vitesse +5 %', 'Dégâts +10 %', 'Recul −10 %', 'Usure +10 %'], color: C.green },
  { key: 'terre', name: 'Terre', code: 'FOSSE SAUVAGE', shape: 'Cercle', rim: 'Sans rebord', mods: ['Usure +20 %', 'Dégâts +40 %', 'Dashs −5 %'], color: C.lime },
  { key: 'glace', name: 'Glace', code: 'CRYO-ELLIPSE', shape: 'Ellipse', rim: 'Rebord très haut', mods: ['Usure −25 %', 'Recul −17 %', 'Dégâts +19 %'], color: '#8FF3FF' },
  { key: 'sable', name: 'Sable', code: 'DUNE HEXA', shape: 'Hexagone', rim: 'Rebord haut', mods: ['Frottement +30 %', 'Usure +30 %', 'Dégâts +30 %', 'Dash court ×2'], color: C.warn },
  { key: 'volcan', name: 'Volcan', code: 'CALDEIRA', shape: 'Petit cercle', rim: 'Sans rebord', mods: ['Pentes très raides', 'Dégâts +14 %', 'Usure +10 %', 'Dash renforcé'], color: '#FF6A3D' },
];
function rng(seed) { let s = seed; return () => (s = (s * 16807) % 2147483647) / 2147483647; }

function regular(cx, cy, R, n, rot, sy = 1) { const p = []; for (let i = 0; i < n; i++) { const a = rot + i * TAU / n; p.push([cx + Math.cos(a) * R, cy + Math.sin(a) * R * sy]); } return p; }

function stadium(d, key, cx, cy, R) {
  const out = [], rnd = rng(key.length * 977 + 13);
  const S = STADIUMS.find(s => s.key === key);
  const rings = (n, col, op, sy = 1) => { let p = ''; for (let i = 1; i <= n; i++) { const r = R * i / (n + 1); p += `M${r2(cx - r)},${cy} A${r2(r)},${r2(r * sy)} 0 1 0 ${r2(cx + r)},${cy} A${r2(r)},${r2(r * sy)} 0 1 0 ${r2(cx - r)},${cy} `; } return path(p, { fill: 'none', stroke: col, 'stroke-width': 1, opacity: op }); };
  const spokes = (n, r0, r1, col, op, sy = 1) => { let p = ''; for (let i = 0; i < n; i++) { const a = i * TAU / n; p += `M${r2(cx + Math.cos(a) * r0)},${r2(cy + Math.sin(a) * r0 * sy)} L${r2(cx + Math.cos(a) * r1)},${r2(cy + Math.sin(a) * r1 * sy)} `; } return path(p, { stroke: col, 'stroke-width': 1, opacity: op }); };
  const markers = (r, col, sy = 1) => [0, 1, 2, 3].map(i => { const a = i * TAU / 4 - Math.PI / 4; const x = cx + Math.cos(a) * r, y = cy + Math.sin(a) * r * sy; return g(`Repère départ ${i + 1}`, [path(chamfer(x - 9, y - 9, 18, 18, 5), { fill: 'none', stroke: col, 'stroke-width': 1.5 }), circle(x, y, 2.5, { fill: col })]); });

  if (key === 'classique') {
    out.push(circle(cx, cy, R * 1.08, { fill: C.cyan, opacity: 0.18, filter: d.blur(R * 0.1) }));
    out.push(circle(cx, cy, R * 1.06, { fill: d.lin([[0, '#3B3490'], [1, '#0F0C33']], 60), stroke: C.cyan, 'stroke-width': 2 }));
    for (let i = 0; i < 24; i++) { const a0 = i * TAU / 24 + 0.03, a1 = a0 + TAU / 24 - 0.06, rr = R * 1.03; out.push(path(`M${r2(cx + Math.cos(a0) * rr)},${r2(cy + Math.sin(a0) * rr)} A${r2(rr)},${r2(rr)} 0 0 1 ${r2(cx + Math.cos(a1) * rr)},${r2(cy + Math.sin(a1) * rr)}`, { fill: 'none', stroke: i % 3 ? C.violet : C.cyan, 'stroke-width': R * 0.025, opacity: i % 3 ? 0.7 : 1 })); }
    out.push(circle(cx, cy, R, { fill: d.rad([[0, '#2A3C9E'], [0.55, '#191660'], [1, '#07061A']], 0.5, 0.5, 0.5), stroke: '#05040F', 'stroke-width': 3 }));
    out.push(rings(7, C.cyan, 0.3), spokes(24, R * 0.12, R, C.violet, 0.25));
    out.push(circle(cx, cy, R * 0.55, { fill: 'none', stroke: C.green, 'stroke-width': 1.5, 'stroke-dasharray': '2 10', opacity: 0.8 }));
    out.push(circle(cx, cy, R * 0.12, { fill: C.cyan, opacity: 0.35, filter: d.blur(R * 0.06) }), circle(cx, cy, R * 0.1, { fill: 'none', stroke: C.cyan, 'stroke-width': 2 }));
    out.push(path(polyD(regular(cx, cy, R * 0.07, 6, 0)), { fill: C.cyan, opacity: 0.8 }));
    out.push(...markers(R * 0.72, C.green));
  }
  if (key === 'beton') {
    const oct = (r) => regular(cx, cy, r, 8, Math.PI / 8);
    out.push(path(polyD(oct(R * 1.16)), { fill: d.lin([[0, '#4A4E66'], [0.5, '#22253A'], [1, '#14162A']], 70), stroke: '#05040F', 'stroke-width': 2 }));
    // chevrons de sécurité sur le mur haut
    const o1 = oct(R * 1.14), o0 = oct(R * 1.02);
    for (let i = 0; i < 8; i++) {
      const a = o1[i], b = o1[(i + 1) % 8], a2 = o0[i], b2 = o0[(i + 1) % 8];
      for (let k = 0; k < 6; k++) {
        const t0 = (k + 0.15) / 6, t1 = (k + 0.55) / 6, L2 = (p, q, t) => [p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t];
        out.push(path(polyD([L2(a, b, t0), L2(a, b, t1), L2(a2, b2, t1 - 0.06), L2(a2, b2, t0 - 0.06)]), { fill: C.green, opacity: 0.85 }));
      }
    }
    out.push(path(polyD(oct(R * 1.02)), { fill: 'none', stroke: C.green, 'stroke-width': 3, opacity: 0.4, filter: d.blur(4) }));
    out.push(path(polyD(oct(R)), { fill: d.rad([[0, '#5D6280'], [0.6, '#2C3048'], [1, '#121426']], 0.45, 0.4, 0.6), stroke: '#05040F', 'stroke-width': 3 }));
    // dalles de béton
    let seams = ''; for (let i = -4; i <= 4; i++) { seams += `M${r2(cx + i * R * 0.24)},${r2(cy - R)} V${r2(cy + R)} M${r2(cx - R)},${r2(cy + i * R * 0.24)} H${r2(cx + R)} `; }
    out.push(path(seams, { stroke: '#0B0C18', 'stroke-width': 2, opacity: 0.6, 'clip-path': d.clip(path(polyD(oct(R)))) }));
    for (let i = 0; i < 26; i++) { const x = cx + (rnd() - 0.5) * R * 1.5, y = cy + (rnd() - 0.5) * R * 1.5; out.push(circle(x, y, 1 + rnd() * 2, { fill: '#9AA0C0', opacity: 0.25 })); }
    out.push(ellipse(cx - R * 0.25, cy - R * 0.3, R * 0.5, R * 0.18, { fill: '#FFFFFF', opacity: 0.07, transform: `rotate(-30 ${r2(cx - R * 0.25)} ${r2(cy - R * 0.3)})` }));
    out.push(path(polyD(regular(cx, cy, R * 0.18, 8, Math.PI / 8)), { fill: 'none', stroke: C.green, 'stroke-width': 2 }));
    out.push(T(cx, cy + 6, 'B-8', { font: L.F.display, size: R * 0.09, fill: C.green, anchor: 'middle', weight: 700, ls: 2 }));
    out.push(...markers(R * 0.68, C.green));
  }
  if (key === 'terre') {
    // pas de rebord : le sol se délite dans le vide
    for (let i = 0; i < 40; i++) { const a = rnd() * TAU, r = R * (0.98 + rnd() * 0.16); out.push(path(polyD(regular(cx + Math.cos(a) * r, cy + Math.sin(a) * r, 3 + rnd() * 7, 5, rnd() * 3)), { fill: '#4A3524', opacity: 0.4 + rnd() * 0.5 })); }
    out.push(circle(cx, cy, R, { fill: d.rad([[0, '#6B4A2E'], [0.7, '#3B2716'], [1, '#1A1008']], 0.48, 0.45, 0.55) }));
    out.push(circle(cx, cy, R, { fill: 'none', stroke: C.lime, 'stroke-width': 1.5, 'stroke-dasharray': '6 8', opacity: 0.5 }));
    // failles bioluminescentes
    for (let k = 0; k < 7; k++) {
      let a = k * TAU / 7 + rnd() * 0.4, r = R * 0.12, p = `M${r2(cx + Math.cos(a) * r)},${r2(cy + Math.sin(a) * r)}`;
      while (r < R * 0.92) { r += R * (0.06 + rnd() * 0.08); a += (rnd() - 0.5) * 0.5; p += ` L${r2(cx + Math.cos(a) * r)},${r2(cy + Math.sin(a) * r)}`; }
      out.push(path(p, { fill: 'none', stroke: C.lime, 'stroke-width': 5, opacity: 0.35, filter: d.blur(4) }), path(p, { fill: 'none', stroke: C.lime, 'stroke-width': 1.6, 'stroke-linejoin': 'bevel' }));
    }
    for (let i = 0; i < 18; i++) { const a = rnd() * TAU, r = R * (0.2 + rnd() * 0.7); out.push(path(polyD(regular(cx + Math.cos(a) * r, cy + Math.sin(a) * r, 4 + rnd() * 9, 5 + Math.floor(rnd() * 3), rnd() * 3)), { fill: '#2A1A0E', stroke: '#8A6A44', 'stroke-width': 1, opacity: 0.85 })); }
    out.push(circle(cx, cy, R * 0.14, { fill: C.lime, opacity: 0.25, filter: d.blur(12) }));
    out.push(...markers(R * 0.7, C.lime));
  }
  if (key === 'glace') {
    const sy = 0.72;
    out.push(ellipse(cx, cy, R * 1.12, R * 1.12 * sy, { fill: '#8FF3FF', opacity: 0.16, filter: d.blur(18) }));
    out.push(ellipse(cx, cy, R * 1.1, R * 1.1 * sy + R * 0.04, { fill: d.lin([[0, '#E6FCFF'], [0.5, '#5AB8E0'], [1, '#1A3A7A']], 70), stroke: '#05040F', 'stroke-width': 2 }));
    out.push(ellipse(cx, cy, R, R * sy, { fill: d.rad([[0, '#C9F6FF'], [0.5, '#4FA6E8'], [1, '#18286E']], 0.42, 0.38, 0.62), stroke: '#05040F', 'stroke-width': 3 }));
    // facettes cristallines (triangles depuis des points jitterés)
    const pts = []; for (let i = 0; i < 6; i++) for (let j = 0; j < 12; j++) { const r = R * (i + 0.5) / 6.2, a = j * TAU / 12 + i * 0.26 + (rnd() - 0.5) * 0.2; pts.push([cx + Math.cos(a) * r, cy + Math.sin(a) * r * sy]); }
    let f = ''; for (let i = 0; i < 5; i++) for (let j = 0; j < 12; j++) { const a = pts[i * 12 + j], b = pts[i * 12 + (j + 1) % 12], c = pts[(i + 1) * 12 + j]; f += `M${r2(a[0])},${r2(a[1])} L${r2(b[0])},${r2(b[1])} L${r2(c[0])},${r2(c[1])} Z `; }
    out.push(path(f, { fill: 'none', stroke: '#FFFFFF', 'stroke-width': 0.8, opacity: 0.35 }));
    for (let i = 0; i < 9; i++) { const a = pts[Math.floor(rnd() * 60)], b = pts[Math.floor(rnd() * 60)]; out.push(path(polyD([a, b, [cx, cy]]), { fill: '#FFFFFF', opacity: 0.06 + rnd() * 0.06 })); }
    out.push(ellipse(cx - R * 0.3, cy - R * 0.25, R * 0.35, R * 0.08, { fill: '#FFFFFF', opacity: 0.35, transform: `rotate(-18 ${r2(cx - R * 0.3)} ${r2(cy - R * 0.25)})` }));
    out.push(glyph('givre', cx, cy, R * 0.12, '#FFFFFF', 1.8));
    out.push(...markers(R * 0.66, '#FFFFFF', sy));
  }
  if (key === 'sable') {
    const hx = r => regular(cx, cy, r, 6, 0);
    out.push(path(polyD(hx(R * 1.14)), { fill: d.lin([[0, '#E8B86A'], [0.5, '#9A6A34'], [1, '#4A2E18']], 70), stroke: '#05040F', 'stroke-width': 2 }));
    // glyphes gravés sur le mur
    const h1 = hx(R * 1.07);
    for (let i = 0; i < 6; i++) { const a = h1[i], b = h1[(i + 1) % 6]; for (let k = 1; k < 4; k++) { const x = a[0] + (b[0] - a[0]) * k / 4, y = a[1] + (b[1] - a[1]) * k / 4; out.push(path(polyD(regular(x, y, 5, 3, i + k)), { fill: 'none', stroke: C.violet, 'stroke-width': 1.5, opacity: 0.9 })); } }
    out.push(path(polyD(hx(R)), { fill: d.rad([[0, '#F2C98A'], [0.6, '#C08A4C'], [1, '#6A4422']], 0.45, 0.4, 0.6), stroke: '#05040F', 'stroke-width': 3 }));
    // rides de dunes
    let rip = ''; for (let i = -9; i <= 9; i++) { const y0 = cy + i * R * 0.11; rip += `M${r2(cx - R)},${r2(y0)} `; for (let k = 1; k <= 8; k++) { const x = cx - R + k * R / 4; rip += `Q${r2(x - R / 8)},${r2(y0 + (k % 2 ? -1 : 1) * R * 0.05 + Math.sin(i) * 3)} ${r2(x)},${r2(y0)} `; } }
    out.push(path(rip, { fill: 'none', stroke: '#6A4422', 'stroke-width': 1.3, opacity: 0.5, 'clip-path': d.clip(path(polyD(hx(R)))) }));
    out.push(path(polyD(hx(R * 0.98)), { fill: 'none', stroke: C.violet, 'stroke-width': 2, opacity: 0.6 }));
    out.push(path(polyD(hx(R * 0.2)), { fill: 'none', stroke: C.violet, 'stroke-width': 2 }), path(polyD(regular(cx, cy, R * 0.1, 3, -Math.PI / 2)), { fill: C.violet, opacity: 0.8 }));
    out.push(...markers(R * 0.66, C.violet));
  }
  if (key === 'volcan') {
    out.push(circle(cx, cy, R * 1.25, { fill: '#FF3D1F', opacity: 0.18, filter: d.blur(R * 0.15) }));
    for (let i = 0; i < 30; i++) { const a = rnd() * TAU, r = R * (1.0 + rnd() * 0.18); out.push(path(polyD(regular(cx + Math.cos(a) * r, cy + Math.sin(a) * r, 3 + rnd() * 6, 4, rnd() * 3)), { fill: '#2A0E14', opacity: 0.8 })); }
    out.push(circle(cx, cy, R, { fill: d.rad([[0, '#FF6A3D'], [0.25, '#7A1B2A'], [0.6, '#2A0A1E'], [1, '#12040E']], 0.5, 0.5, 0.5) }));
    // courbes de niveau serrées = pente raide
    let cl = ''; for (let i = 1; i <= 12; i++) { const r = R * Math.pow(i / 13, 0.7); let p = ''; for (let k = 0; k <= 72; k++) { const a = k * TAU / 72, rr = r * (1 + 0.025 * Math.sin(a * 5 + i)); p += (k ? ' L' : 'M') + `${r2(cx + Math.cos(a) * rr)},${r2(cy + Math.sin(a) * rr)}`; } cl += p + ' '; }
    out.push(path(cl, { fill: 'none', stroke: '#FF6A3D', 'stroke-width': 0.9, opacity: 0.35 }));
    for (let k = 0; k < 9; k++) {
      let a = k * TAU / 9 + rnd() * 0.3, r = R * 0.1, p = `M${r2(cx + Math.cos(a) * r)},${r2(cy + Math.sin(a) * r)}`;
      while (r < R * 0.95) { r += R * (0.07 + rnd() * 0.07); a += (rnd() - 0.5) * 0.6; p += ` L${r2(cx + Math.cos(a) * r)},${r2(cy + Math.sin(a) * r)}`; }
      out.push(path(p, { fill: 'none', stroke: '#FF3D1F', 'stroke-width': 6, opacity: 0.45, filter: d.blur(5) }), path(p, { fill: 'none', stroke: '#FFC23D', 'stroke-width': 1.5 }));
    }
    out.push(circle(cx, cy, R * 0.16, { fill: '#FFC23D', opacity: 0.7, filter: d.blur(R * 0.08) }), circle(cx, cy, R * 0.08, { fill: '#FFF1C2' }));
    for (let i = 0; i < 22; i++) { const a = rnd() * TAU, r = R * rnd() * 1.1; out.push(circle(cx + Math.cos(a) * r, cy + Math.sin(a) * r, 1 + rnd() * 2, { fill: '#FFC23D', opacity: 0.4 + rnd() * 0.6 })); }
    out.push(...markers(R * 0.62, '#FFC23D'));
  }
  return g(`Stadium ${S.name}`, out);
}

// ================= EFFETS DES COUPS SPÉCIAUX =================
// Illustration dans un cadre (x,y,w,h) : lanceur à gauche, cible à droite
function moveFx(d, key, x, y, w, h) {
  const mv = MOVES[key], col = mv.color, out = [], cy = y + h / 2, u = h, ax = x + w * 0.2, bx = x + w * 0.78, rnd = rng(key.length * 31 + 7);
  const caster = PRESETS.find(p => BOLTS[p.parts[3]] === key) || PRESETS[0];
  const target = PRESETS[(PRESETS.indexOf(caster) + 3) % PRESETS.length];
  const glowLine = (dd, sw, op = 1) => [path(dd, { fill: 'none', stroke: col, 'stroke-width': sw * 3, opacity: 0.35 * op, filter: d.blur(sw * 2) }), path(dd, { fill: 'none', stroke: col, 'stroke-width': sw, opacity: op, 'stroke-linecap': 'round' })];
  if (mv.form === 'ANNEAU') {
    const cx = x + w * 0.42;
    if (key === 'onde') {
      [0.16, 0.27, 0.38].forEach((k, i) => out.push(...glowLine(`M${cx - u * k},${cy} A${u * k},${u * k} 0 1 0 ${cx + u * k},${cy} A${u * k},${u * k} 0 1 0 ${cx - u * k},${cy}`, 5 - i * 1.4, 1 - i * 0.25)));
      out.push(circle(cx - 4, cy, u * 0.38, { fill: 'none', stroke: C.magenta, 'stroke-width': 1.5, opacity: 0.6 }));
      for (let i = 0; i < 16; i++) { const a = i * TAU / 16 + rnd() * 0.2, r = u * (0.4 + rnd() * 0.05); out.push(path(polyD([[cx + Math.cos(a) * r, cy + Math.sin(a) * r], [cx + Math.cos(a + 0.03) * (r + 14), cy + Math.sin(a + 0.03) * (r + 14)], [cx + Math.cos(a - 0.03) * (r + 10), cy + Math.sin(a - 0.03) * (r + 10)]]), { fill: col })); }
      out.push(top(d, cx, cy, 34, caster, { angle: 0.4 }), top(d, x + w * 0.88, cy - 20, 26, target, { angle: 1 }));
      out.push(...glowLine(`M${x + w * 0.8},${cy - 20} L${x + w * 0.95},${cy - 32}`, 2), ...glowLine(`M${x + w * 0.8},${cy - 10} L${x + w * 0.97},${cy - 14}`, 2));
    }
    if (key === 'vortex') {
      for (let k = 0; k < 4; k++) { let p = ''; for (let i = 0; i <= 40; i++) { const a = k * TAU / 4 + i * 0.13, r = u * 0.46 * (1 - i / 44); p += (i ? ' L' : 'M') + `${r2(cx + Math.cos(a) * r)},${r2(cy + Math.sin(a) * r * 0.9)}`; } out.push(...glowLine(p, 3.2, 0.9)); }
      out.push(circle(cx, cy, u * 0.1, { fill: '#000', stroke: C.violet, 'stroke-width': 2 }), circle(cx, cy, u * 0.14, { fill: C.violet, opacity: 0.4, filter: d.blur(16) }));
      [[x + w * 0.85, cy - 40], [x + w * 0.1, cy + 50]].forEach(([tx, ty]) => { const a = Math.atan2(cy - ty, cx - tx); out.push(path(`M${tx + Math.cos(a) * 34},${ty + Math.sin(a) * 34} l${Math.cos(a) * 36},${Math.sin(a) * 36}`, { stroke: C.violet, 'stroke-width': 3, 'stroke-dasharray': '6 6', fill: 'none' })); });
      out.push(top(d, cx, cy, 30, caster, { angle: 2 }), top(d, x + w * 0.85, cy - 40, 22, target, { angle: 0.3 }), top(d, x + w * 0.1, cy + 50, 22, PRESETS[2], { angle: 1.4 }));
    }
    if (key === 'seisme') {
      for (let k = 0; k < 10; k++) { let a = k * TAU / 10, r = 30, p = `M${r2(cx + Math.cos(a) * r)},${r2(cy + Math.sin(a) * r)}`; while (r < u * 0.4) { r += 10 + rnd() * 14; a += (rnd() - 0.5) * 0.5; p += ` L${r2(cx + Math.cos(a) * r)},${r2(cy + Math.sin(a) * r)}`; } out.push(...glowLine(p, 2, 0.95)); }
      for (let i = 0; i < 12; i++) { const a = rnd() * TAU, r = u * (0.15 + rnd() * 0.22); out.push(path(polyD(regular(cx + Math.cos(a) * r, cy + Math.sin(a) * r, 4 + rnd() * 6, 4, rnd() * 3)), { fill: '#1D2A10', stroke: col, 'stroke-width': 1 })); }
      out.push(circle(cx, cy, u * 0.4, { fill: 'none', stroke: col, 'stroke-width': 1.5, 'stroke-dasharray': '3 6' }));
      out.push(top(d, cx, cy, 32, caster, { angle: 0.9 }), top(d, cx + w * 0.24, cy - 28, 24, target, { angle: 0.2 }));
      out.push(g('Cadenas désarmé', [path(chamfer(cx + w * 0.24 + 14, cy - 76, 34, 30, 6), { fill: '#0A0826', stroke: col, 'stroke-width': 1.5 }), L.icon('lock', cx + w * 0.24 + 31, cy - 61, 18, col)]));
      out.push(D(cx + w * 0.24 + 54, cy - 55, '6 S', { size: 15, fill: col }));
    }
  } else {
    if (key === 'lance') {
      out.push(...glowLine(`M${ax + 40},${cy} L${bx - 30},${cy}`, 4));
      out.push(path(polyD([[bx - 30, cy], [bx - 76, cy - 18], [bx - 64, cy], [bx - 76, cy + 18]]), { fill: '#FFFFFF', stroke: col, 'stroke-width': 2 }));
      for (let i = 0; i < 9; i++) { const yy = cy + (rnd() - 0.5) * 50, x0 = ax + 50 + rnd() * w * 0.3; out.push(line(x0, yy, x0 + 30 + rnd() * 60, yy, { stroke: col, 'stroke-width': 1.3, opacity: 0.6 })); }
      for (let i = 0; i < 12; i++) { const a = (rnd() - 0.5) * 2.4 + Math.PI, r = 18 + rnd() * 30; out.push(line(bx - 24, cy, bx - 24 + Math.cos(a + Math.PI) * r, cy + Math.sin(a + Math.PI) * r, { stroke: C.warn, 'stroke-width': 1.5 })); }
    }
    if (key === 'givre') {
      out.push(path(`M${ax + 30},${cy - 8} L${bx - 10},${cy - 70} L${bx - 10},${cy + 70} L${ax + 30},${cy + 8} Z`, { fill: d.lin([[0, col, 0.55], [1, col, 0.05]], 0) }));
      for (let i = 0; i < 18; i++) { const t = rnd(), xx = ax + 40 + t * (bx - ax - 40), yy = cy + (rnd() - 0.5) * 120 * t; out.push(glyph('givre', xx, yy, 4 + rnd() * 6, '#FFFFFF', 1.1)); }
      out.push(path(polyD(regular(bx, cy, 48, 6, 0)), { fill: col, 'fill-opacity': 0.18, stroke: '#FFFFFF', 'stroke-width': 1.5 }), path(polyD(regular(bx, cy, 40, 6, 0.5)), { fill: 'none', stroke: col, 'stroke-width': 1 }));
    }
    if (key === 'distorsion') {
      for (let i = 0; i < 7; i++) { const yy = cy - 36 + i * 12, off = (rnd() - 0.5) * 40; out.push(rect(ax + 50 + off, yy, w * 0.42, 6, { fill: i % 2 ? C.magenta : C.cyan, opacity: 0.55 + rnd() * 0.4 })); }
      out.push(path(`M${bx - 10},${cy} C${bx},${cy - 60} ${bx + 30},${cy - 70} ${x + w * 0.93},${y + 28}`, { fill: 'none', stroke: C.magenta, 'stroke-width': 2, 'stroke-dasharray': '3 6' }));
      out.push(top(d, x + w * 0.93, y + 30, 20, target, { angle: 2, shadow: false }));
      out.push(circle(x + w * 0.93, y + 30, 26, { fill: 'none', stroke: C.magenta, 'stroke-width': 1.5 }));
    }
    if (key === 'siphon') {
      for (let k = -1; k <= 1; k++) out.push(...glowLine(`M${bx - 30},${cy + k * 6} C${bx - 120},${cy - 70 + k * 30} ${ax + 120},${cy + 70 + k * 30} ${ax + 40},${cy + k * 4}`, k ? 1.4 : 3, k ? 0.6 : 1));
      for (let i = 0; i < 7; i++) { const t = (i + 0.5) / 7, xx = bx - 30 - t * (bx - ax - 70), yy = cy + Math.sin(t * Math.PI * 2) * -26; out.push(path(`M${xx},${yy - 7} C${xx + 4},${yy - 2} ${xx + 5},${yy + 2} ${xx},${yy + 5} C${xx - 5},${yy + 2} ${xx - 4},${yy - 2} ${xx},${yy - 7} Z`, { fill: col })); }
      out.push(D(ax, cy - 52, '+60 %', { size: 17, fill: C.green, anchor: 'middle' }));
    }
    out.push(top(d, ax, cy, 34, caster, { angle: 0.6 }));
    if (key !== 'distorsion') out.push(top(d, bx, cy, 28, target, { angle: 1.7 }));
    else out.push(g('Fantôme cible', [top(d, bx, cy, 28, target, { angle: 1.7, shadow: false })], { opacity: 0.35 }));
  }
  return g(`FX ${mv.name}`, out, { 'clip-path': d.clip(rect(x, y, w, h)) });
}

module.exports = { WHEELS, MIDDLES, TIPS, MOVES, BOLTS, PRESETS, STADIUMS, sumStats, hex, top, wheelArt, middleArt, boltArt, tipArt, sideView, glyph, stadium, moveFx, regular };
