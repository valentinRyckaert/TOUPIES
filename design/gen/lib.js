// Primitives SVG pensées pour l'import Figma : attributs de présentation uniquement (pas de CSS),
// chaque groupe porte un id (= nom du calque dans Figma), textes en <text> (= calques texte éditables).
'use strict';
const fs = require('fs');

const C = {
  void: '#05040F', abyss: '#0A0820', deep: '#110E33', panel: '#15113F', panel2: '#1D1858',
  line: '#3A2F8F', lineHi: '#5B4BD1',
  blue: '#2F6BFF', cyan: '#34D6FF', violet: '#7B3CFF', magenta: '#C23BFF',
  green: '#3DFF8B', lime: '#B8FF2E',
  text: '#EAF0FF', muted: '#9296D0', dim: '#585C99', danger: '#FF3B6B', warn: '#FFC23D', white: '#FFFFFF',
};
const TYPE = { Attaque: '#FF3D6E', 'Défense': '#2F7BFF', Endurance: '#3DFF8B', 'Équilibre': '#FFC23D' };
const F = { display: 'Chakra Petch', body: 'Rajdhani' };

const esc = s => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
const r2 = n => Math.round(n * 100) / 100;
const A = o => Object.entries(o).filter(([, v]) => v !== undefined && v !== null && v !== false)
  .map(([k, v]) => `${k}="${esc(v)}"`).join(' ');

let uid = 0;
class Doc {
  constructor(w, h, name) { this.w = w; this.h = h; this.name = name; this.defs = []; this.body = []; this.n = 0; }
  id(p) { return `${p}${++this.n}`; }
  // angle en degrés : 0 = gauche→droite, 90 = haut→bas
  lin(stops, angle = 90) {
    const id = this.id('lg'), a = angle * Math.PI / 180;
    const x = Math.cos(a) / 2, y = Math.sin(a) / 2;
    this.defs.push(`<linearGradient id="${id}" x1="${r2(0.5 - x)}" y1="${r2(0.5 - y)}" x2="${r2(0.5 + x)}" y2="${r2(0.5 + y)}">${stopsXml(stops)}</linearGradient>`);
    return `url(#${id})`;
  }
  rad(stops, cx = 0.5, cy = 0.5, r = 0.5) {
    const id = this.id('rg');
    this.defs.push(`<radialGradient id="${id}" cx="${cx}" cy="${cy}" r="${r}">${stopsXml(stops)}</radialGradient>`);
    return `url(#${id})`;
  }
  blur(std) {
    const id = this.id('bl');
    this.defs.push(`<filter id="${id}" x="-60%" y="-60%" width="220%" height="220%"><feGaussianBlur stdDeviation="${std}"/></filter>`);
    return `url(#${id})`;
  }
  shadow(dy, std, color = '#000', op = 0.6) {
    const id = this.id('sh');
    this.defs.push(`<filter id="${id}" x="-30%" y="-30%" width="160%" height="160%"><feDropShadow dx="0" dy="${dy}" stdDeviation="${std}" flood-color="${color}" flood-opacity="${op}"/></filter>`);
    return `url(#${id})`;
  }
  clip(inner) {
    const id = this.id('cp');
    this.defs.push(`<clipPath id="${id}">${inner}</clipPath>`);
    return `url(#${id})`;
  }
  add(...s) { this.body.push(...s.flat()); return this; }
  toString() {
    return `<svg xmlns="http://www.w3.org/2000/svg" width="${this.w}" height="${this.h}" viewBox="0 0 ${this.w} ${this.h}">\n` +
      `<defs>${this.defs.join('\n')}</defs>\n${this.body.join('\n')}\n</svg>\n`;
  }
  save(dir) { fs.mkdirSync(dir, { recursive: true }); fs.writeFileSync(`${dir}/${this.name}.svg`, this.toString()); }
}
const stopsXml = stops => stops.map(s => Array.isArray(s)
  ? `<stop offset="${s[0]}" stop-color="${s[1]}"${s[2] !== undefined ? ` stop-opacity="${s[2]}"` : ''}/>`
  : '').join('');

// ---------- formes ----------
const g = (id, inner, attrs = {}) => `<g ${A({ id, ...attrs })}>${[inner].flat().join('')}</g>`;
const rect = (x, y, w, h, a = {}) => `<rect ${A({ x: r2(x), y: r2(y), width: r2(w), height: r2(h), ...a })}/>`;
const circle = (cx, cy, r, a = {}) => `<circle ${A({ cx: r2(cx), cy: r2(cy), r: r2(r), ...a })}/>`;
const ellipse = (cx, cy, rx, ry, a = {}) => `<ellipse ${A({ cx: r2(cx), cy: r2(cy), rx: r2(rx), ry: r2(ry), ...a })}/>`;
const line = (x1, y1, x2, y2, a = {}) => `<line ${A({ x1: r2(x1), y1: r2(y1), x2: r2(x2), y2: r2(y2), ...a })}/>`;
const path = (d, a = {}) => `<path ${A({ d, ...a })}/>`;
const polyD = pts => 'M' + pts.map(p => `${r2(p[0])},${r2(p[1])}`).join(' L') + ' Z';

// Rectangle aux coins coupés à 45° : c = rayon unique ou [hautG, hautD, basD, basG]
function chamfer(x, y, w, h, c = 12) {
  const [a, b, cc, d] = Array.isArray(c) ? c : [c, c, c, c];
  return polyD([[x + a, y], [x + w - b, y], [x + w, y + b], [x + w, y + h - cc], [x + w - cc, y + h], [x + d, y + h], [x, y + h - d], [x, y + a]]);
}
// Parallélogramme incliné (boutons, segments, tags)
function skew(x, y, w, h, s = 10) { return polyD([[x + s, y], [x + w, y], [x + w - s, y + h], [x, y + h]]); }

function T(x, y, str, o = {}) {
  const a = {
    x: r2(x), y: r2(y), 'font-family': o.font || F.body, 'font-size': o.size || 16,
    'font-weight': o.weight || 600, fill: o.fill || C.text, 'text-anchor': o.anchor,
    'letter-spacing': o.ls, opacity: o.opacity, 'font-style': o.italic ? 'italic' : undefined,
    transform: o.transform, id: o.id,
  };
  return `<text ${A(a)}>${esc(str)}</text>`;
}
const D = (x, y, s, o = {}) => T(x, y, s, { font: F.display, weight: 700, ...o });

// ---------- décor ----------
function background(d, w, h, o = {}) {
  const out = [];
  out.push(rect(0, 0, w, h, { fill: C.void }));
  out.push(rect(0, 0, w, h, { fill: d.rad([[0, o.core || '#1B1260', 0.95], [0.55, '#0C0A2C', 0.9], [1, C.void, 1]], o.cx ?? 0.5, o.cy ?? 0.35, 0.75) }));
  // halos de couleur
  out.push(circle(w * (o.h1x ?? 0.12), h * (o.h1y ?? 0.1), Math.max(w, h) * 0.32, { fill: C.blue, opacity: 0.16, filter: d.blur(120) }));
  out.push(circle(w * (o.h2x ?? 0.9), h * (o.h2y ?? 0.95), Math.max(w, h) * 0.28, { fill: C.magenta, opacity: 0.14, filter: d.blur(120) }));
  out.push(circle(w * (o.h3x ?? 0.75), h * (o.h3y ?? 0.15), Math.max(w, h) * 0.12, { fill: C.green, opacity: 0.07, filter: d.blur(90) }));
  // grille en perspective au sol
  if (o.floor !== false) {
    const hz = h * (o.horizon ?? 0.62), vx = w / 2; let p = '';
    for (let i = -24; i <= 24; i++) p += `M${r2(vx + i * 18)},${r2(hz)} L${r2(vx + i * w / 10)},${h} `;
    for (let k = 1; k <= 12; k++) { const yy = hz + (h - hz) * Math.pow(k / 12, 2.2); p += `M0,${r2(yy)} L${w},${r2(yy)} `; }
    out.push(path(p, { stroke: C.violet, 'stroke-width': 1, opacity: 0.22, fill: 'none' }));
    out.push(rect(0, hz - 2, w, 4, { fill: C.cyan, opacity: 0.25, filter: d.blur(3) }));
  }
  // lignes de balayage (un seul calque)
  let s = ''; for (let yy = 0; yy < h; yy += 4) s += `M0,${yy} H${w} `;
  out.push(path(s, { stroke: '#FFFFFF', 'stroke-width': 1, opacity: 0.025, fill: 'none' }));
  return g('Fond', out);
}

// Crochets d'angle holographiques
function brackets(x, y, w, h, color = C.cyan, len = 18, sw = 2) {
  const p = `M${x},${y + len} V${y} H${x + len} M${x + w - len},${y} H${x + w} V${y + len} ` +
    `M${x + w},${y + h - len} V${y + h} H${x + w - len} M${x + len},${y + h} H${x} V${y + h - len}`;
  return path(p, { stroke: color, 'stroke-width': sw, fill: 'none' });
}
// "///" décoratif
function slashes(x, y, n = 3, h = 14, color = C.green, gap = 7, op = 1) {
  let out = ''; for (let i = 0; i < n; i++) out += path(skew(x + i * gap, y, 4 + h * 0.35, h, h * 0.35), { fill: color, opacity: op * (1 - i * 0.22) });
  return out;
}

// ---------- composants ----------
function panel(d, x, y, w, h, o = {}) {
  const c = o.c ?? 22, acc = o.accent || C.cyan, out = [];
  out.push(path(chamfer(x, y, w, h, [c, 0, c, 0]), { fill: d.lin([[0, '#1A1556', 0.92], [1, '#0D0A2E', 0.92]], 100), stroke: C.line, 'stroke-width': 1.2 }));
  out.push(path(chamfer(x + 1, y + 1, w - 2, h - 2, [c, 0, c, 0]), { fill: 'none', stroke: d.lin([[0, acc, 0.9], [0.35, acc, 0], [0.7, C.violet, 0], [1, C.violet, 0.7]], 30), 'stroke-width': 1.5 }));
  out.push(path(`M${x},${y + c + 26} V${y + c} L${x + c},${y} H${x + c + 60}`, { stroke: acc, 'stroke-width': 3, fill: 'none' }));
  out.push(path(`M${x + w},${y + h - c - 26} V${y + h - c} L${x + w - c},${y + h} H${x + w - c - 60}`, { stroke: C.violet, 'stroke-width': 3, fill: 'none' }));
  if (o.title) {
    out.push(D(x + c + 14, y + 34, o.title.toUpperCase(), { size: 17, ls: 2.5, fill: C.text }));
    if (o.index) out.push(T(x + w - 24, y + 33, o.index, { font: F.display, size: 12, fill: acc, anchor: 'end', ls: 2 }));
    out.push(line(x + c + 14, y + 48, x + w - 24, y + 48, { stroke: C.line, 'stroke-width': 1 }));
    out.push(slashes(x + c + 14, y + 45, 3, 6, acc, 9));
  }
  return g(o.id || `Panneau ${o.title || ''}`.trim(), out);
}

const BTN = {
  primary: { fill: [C.green, C.lime], text: '#03140A', stroke: '#B9FFD6' },
  secondary: { fill: ['#231C6B', '#151049'], text: C.text, stroke: C.lineHi },
  ghost: { fill: null, text: C.muted, stroke: C.line },
  danger: { fill: ['#3A0F2A', '#220820'], text: '#FFC3D2', stroke: C.danger },
  holo: { fill: [C.blue, C.violet, C.magenta], text: C.white, stroke: '#C9B8FF' },
};
function button(d, x, y, w, h, label, o = {}) {
  const v = BTN[o.variant || 'secondary'], c = h * 0.32, out = [];
  const disabled = o.state === 'disabled', hover = o.state === 'hover';
  if (v.fill && (o.variant === 'primary' || o.variant === 'holo' || hover))
    out.push(path(chamfer(x, y + 6, w, h, [c, 0, c, 0]), { fill: o.variant === 'holo' ? C.violet : C.green, opacity: hover ? 0.55 : 0.35, filter: d.blur(14) }));
  const fill = v.fill ? d.lin(v.fill.map((col, i) => [i / Math.max(1, v.fill.length - 1), col]), o.variant === 'primary' ? 0 : 20) : C.void;
  out.push(path(chamfer(x, y, w, h, [c, 0, c, 0]), {
    fill, 'fill-opacity': v.fill ? 1 : 0.4, stroke: hover ? C.green : v.stroke, 'stroke-width': hover ? 2 : 1.3,
    opacity: disabled ? 0.38 : 1,
  }));
  if (o.variant === 'primary') out.push(path(chamfer(x + 3, y + 3, w - 6, h * 0.42, [c - 2, 0, 0, 0]), { fill: '#FFFFFF', opacity: 0.18 }));
  const tx = o.align === 'left' ? x + c + 18 + (o.icon ? 30 : 0) : x + w / 2;
  if (o.icon) out.push(icon(o.icon, o.align === 'left' ? x + c + 18 : x + w / 2 - textW(label, h) / 2 - 24, y + h / 2, 18, hover ? C.green : (o.variant === 'primary' ? v.text : C.cyan)));
  out.push(D(tx, y + h / 2 + (h > 50 ? 7 : 5.5), label.toUpperCase(), {
    size: h > 50 ? 20 : 15, ls: h > 50 ? 2.5 : 1.8, fill: hover ? C.white : v.text, anchor: o.align === 'left' ? undefined : 'middle',
    opacity: disabled ? 0.5 : 1,
  }));
  if (o.key) out.push(keycap(d, x + w - c - 46 + (o.keyW ? 30 - o.keyW : 0), y + h / 2 - 13, o.key, { dark: o.variant === 'primary', w: o.keyW }));
  else if (o.slashes !== false && o.align === 'left') out.push(slashes(x + w - c - 40, y + h / 2 - 7, 3, 14, hover ? C.green : C.lineHi, 8, 0.9));
  return g(o.id || `Bouton ${label}`, out);
}
const textW = (s, h) => s.length * (h > 50 ? 15.5 : 11.5);

function keycap(d, x, y, label, o = {}) {
  const w = o.w || Math.max(26, label.length * 9 + 14), h = 26;
  return g(`Touche ${label}`, [
    path(chamfer(x, y, w, h, [5, 0, 5, 0]), { fill: o.dark ? '#03140A' : '#0A0826', 'fill-opacity': o.dark ? 0.35 : 0.9, stroke: o.dark ? '#03140A' : C.lineHi, 'stroke-width': 1 }),
    D(x + w / 2, y + 18, label, { size: 12.5, fill: o.dark ? '#03140A' : C.cyan, anchor: 'middle', ls: 1 }),
  ]);
}

const badgeW = label => Math.round(label.length * 10.4 + 34);
function badge(d, x, y, type, o = {}) {
  const col = TYPE[type] || type, label = (o.label || type).toUpperCase(), w = o.w || badgeW(label), h = o.h || 24;
  return g(`Badge ${label}`, [
    path(skew(x, y, w, h, 8), { fill: col, 'fill-opacity': 0.16, stroke: col, 'stroke-width': 1.2 }),
    path(skew(x + 4, y + 5, 10, h - 10, 4), { fill: col }),
    D(x + 20, y + h / 2 + 5, label, { size: 12.5, fill: col, ls: 1.8 }),
  ]);
}

// Barre segmentée (stat 0..max)
function statBar(d, x, y, w, label, value, max = 15, color = C.cyan, o = {}) {
  const segs = o.segs || max, gap = 3, sw = (w - (segs - 1) * gap) / segs, h = o.h || 10, out = [];
  if (label) {
    out.push(T(x, y - 8, label.toUpperCase(), { size: 13, ls: 1.6, fill: C.muted, weight: 700 }));
    out.push(D(x + w, y - 7, String(value), { size: 15, fill: color, anchor: 'end' }));
  }
  for (let i = 0; i < segs; i++) {
    const on = i < Math.round(value / max * segs);
    out.push(path(skew(x + i * (sw + gap), y, sw + 3, h, 3), { fill: on ? color : C.line, opacity: on ? 1 - (segs - i) * 0 : 0.5 }));
  }
  if (o.glow !== false) out.push(rect(x, y + h / 2 - 2, (sw + gap) * Math.round(value / max * segs), 4, { fill: color, opacity: 0.6, filter: d.blur(5) }));
  return g(`Stat ${label || ''}`.trim(), out);
}

// Jauge d'endurance (vacille sous 25 %)
function enduranceBar(d, x, y, w, h, pct, color = C.green, o = {}) {
  const out = [], fw = Math.max(0, w * pct / 100);
  out.push(path(skew(x, y, w, h, h * 0.6), { fill: '#07061A', stroke: C.line, 'stroke-width': 1 }));
  if (fw > 0) {
    out.push(path(skew(x, y, fw, h, h * 0.6), { fill: color, opacity: 0.4, filter: d.blur(8) }));
    out.push(path(skew(x, y, fw, h, h * 0.6), { fill: d.lin([[0, color], [1, o.color2 || C.lime]], 0) }));
    out.push(path(skew(x + 2, y + 2, Math.max(0, fw - 4), h * 0.35, h * 0.2), { fill: '#FFFFFF', opacity: 0.28 }));
  }
  // repères tous les 10 %, gros repère à 25 %
  let t = ''; for (let i = 1; i < 10; i++) { const xx = x + w * i / 10; t += `M${r2(xx)},${y + h - 4} L${r2(xx + 1)},${y + h} `; }
  out.push(path(t, { stroke: '#07061A', 'stroke-width': 1.5, opacity: 0.8 }));
  out.push(path(`M${x + w * 0.25 + h * 0.3},${y - 4} L${x + w * 0.25 - h * 0.3},${y + h + 4}`, { stroke: C.danger, 'stroke-width': 2 }));
  return g(o.id || 'Jauge endurance', out);
}

// ---------- icônes (traits 1.8, grille 24) ----------
const ICONS = {
  play: 'M7,4 L19,12 L7,20 Z',
  lan: 'M3,8 H15 L12,5 M21,16 H9 L12,19',
  trophy: 'M7,4 H17 V9 C17,12 15,14 12,14 C9,14 7,12 7,9 Z M7,6 H4 V8 C4,10 5.5,11 7,11 M17,6 H20 V8 C20,10 18.5,11 17,11 M12,14 V18 M8,20 H16 L15,18 H9 Z',
  build: 'M12,3 L20,7.5 V16.5 L12,21 L4,16.5 V7.5 Z M12,12 L20,7.5 M12,12 L4,7.5 M12,12 V21',
  tuto: 'M2,9 L12,4 L22,9 L12,14 Z M6,11 V16 C8,18 16,18 18,16 V11 M22,9 V15',
  gear: 'M12,8.5 A3.5,3.5 0 1 0 12,15.5 A3.5,3.5 0 1 0 12,8.5 Z M10.5,2 H13.5 L14,5 L16.5,6.2 L19,4.5 L21,6.5 L19.5,9 L20.5,11.5 L23,12 V14 L20.5,14.6 L19.5,17 L21,19.5 L19,21.5 L16.5,20 L14,21 L13.5,23 H10.5 L10,21 L7.5,20 L5,21.5 L3,19.5 L4.5,17 L3.5,14.6 L1,14 V12 L3.5,11.5 L4.5,9 L3,6.5 L5,4.5 L7.5,6.2 L10,5 Z',
  help: 'M9,9 C9,6 15,6 15,9.5 C15,12 12,12 12,15 M12,18.5 V19.5',
  update: 'M20,12 A8,8 0 1 1 17.6,6.3 M20,4 V8 H16',
  quit: 'M6,6 L18,18 M18,6 L6,18',
  back: 'M15,5 L8,12 L15,19',
  next: 'M9,5 L16,12 L9,19',
  dash: 'M3,12 H14 M10,7 L16,12 L10,17 M18,6 V18 M21,8 V16',
  bolt: 'M13,2 L5,13 H11 L10,22 L19,10 H13 Z',
  user: 'M12,4 A4,4 0 1 0 12,12 A4,4 0 1 0 12,4 Z M4,21 C5,16 19,16 20,21',
  bot: 'M5,8 H19 V19 H5 Z M12,4 V8 M9,13 V14 M15,13 V14 M9,17 H15',
  lock: 'M6,11 H18 V21 H6 Z M8.5,11 V7.5 A3.5,3.5 0 0 1 15.5,7.5 V11',
  wifi: 'M2,9 C8,3 16,3 22,9 M5.5,12.5 C9.5,8.5 14.5,8.5 18.5,12.5 M9,16 C10.8,14.4 13.2,14.4 15,16 M12,19.5 V19.6',
  star: 'M12,3 L14.6,9 L21,9.5 L16,13.6 L17.6,20 L12,16.6 L6.4,20 L8,13.6 L3,9.5 L9.4,9 Z',
  check: 'M4,12.5 L9.5,18 L20,6',
  timer: 'M12,6 A7.5,7.5 0 1 0 12,21 A7.5,7.5 0 1 0 12,6 Z M12,9.5 V13.5 L14.5,15 M9.5,2.5 H14.5',
  stadium: 'M2,12 A10,5 0 1 0 22,12 A10,5 0 1 0 2,12 Z M6,12 A6,2.6 0 1 0 18,12 A6,2.6 0 1 0 6,12 Z',
  sliders: 'M4,6 H20 M4,12 H20 M4,18 H20 M9,4 V8 M15,10 V14 M7,16 V20',
  shield: 'M12,2.5 L20,5.5 V11 C20,16 16.5,19.5 12,21.5 C7.5,19.5 4,16 4,11 V5.5 Z',
  sword: 'M19,3 L21,5 L10,16 L8,14 Z M7,13 L11,17 M6,18 L4,20 M5.5,15.5 L8.5,18.5',
  infinity: 'M12,12 C9,7 4,7 4,12 C4,17 9,17 12,12 C15,7 20,7 20,12 C20,17 15,17 12,12 Z',
  balance: 'M12,3 V20 M5,6 H19 M5,6 L2,13 C2,15 8,15 8,13 Z M19,6 L16,13 C16,15 22,15 22,13 Z M8,20 H16',
  plus: 'M12,5 V19 M5,12 H19',
  trash: 'M4,7 H20 M9,7 V4 H15 V7 M6,7 L7,21 H17 L18,7',
  save: 'M5,3 H16 L20,7 V21 H5 Z M8,3 V8 H15 V3 M8,21 V14 H17 V21',
  random: 'M3,7 H7 C12,7 12,17 17,17 H21 M18,14 L21,17 L18,20 M3,17 H7 C9,17 10,15.5 11,13.5 M14,8.2 C15,7.4 16,7 17,7 H21 M18,4 L21,7 L18,10',
};
function icon(name, cx, cy, size = 24, color = C.cyan, sw = 1.8) {
  const s = size / 24;
  return path(ICONS[name], {
    id: `icône/${name}`, fill: name === 'play' ? color : 'none', stroke: color, 'stroke-width': sw / s,
    'stroke-linejoin': 'miter', 'stroke-linecap': 'square', transform: `translate(${r2(cx - size / 2)},${r2(cy - size / 2)}) scale(${r2(s)})`,
  });
}

// En-tête de planche (titre de section)
function boardHeader(d, x, y, kicker, title, sub) {
  return g('En-tête planche', [
    slashes(x, y - 15, 3, 16, C.green, 9),
    D(x + 40, y, kicker.toUpperCase(), { size: 15, ls: 4, fill: C.green }),
    D(x, y + 58, title.toUpperCase(), { size: 54, ls: 3, fill: C.text }),
    sub ? T(x, y + 96, sub, { size: 21, fill: C.muted, weight: 500 }) : '',
  ]);
}

// Logo : texte éditable + décalage chromatique
function logo(d, x, y, scale = 1) {
  const s = n => n * scale, out = [];
  const big = { size: s(118), ls: s(4), italic: true, font: F.display, weight: 700 };
  out.push(T(x - s(4), y, 'TOUPIE', { ...big, fill: C.magenta, opacity: 0.75 }));
  out.push(T(x + s(4), y + s(2), 'TOUPIE', { ...big, fill: C.cyan, opacity: 0.75 }));
  out.push(T(x, y, 'TOUPIE', { ...big, fill: d.lin([[0, '#FFFFFF'], [0.45, '#BFD3FF'], [0.7, '#C9A5FF'], [1, '#9DFFCB']], 70), id: 'Logo TOUPIE' }));
  out.push(path(skew(x + s(6), y + s(18), s(360), s(46), s(16)), { fill: d.lin([[0, C.green], [1, C.lime]], 0) }));
  out.push(D(x + s(32), y + s(52), 'ARENA', { size: s(36), ls: s(18), fill: '#03140A', italic: true }));
  out.push(slashes(x + s(296), y + s(26), 3, s(30), '#03140A', s(12)));
  out.push(path(`M${x + s(380)},${y + s(41)} H${x + s(560)}`, { stroke: C.cyan, 'stroke-width': s(2) }));
  out.push(T(x + s(384), y + s(62), 'COMBAT DE TOUPIES', { size: s(15), ls: s(5), fill: C.cyan, weight: 700 }));
  return g('Logo', out);
}

module.exports = { C, TYPE, F, Doc, g, rect, circle, ellipse, line, path, polyD, chamfer, skew, T, D, A, r2, esc,
  badgeW, background, brackets, slashes, panel, button, keycap, badge, statBar, enduranceBar, icon, ICONS, boardHeader, logo };
