'use strict';
/* HUD Connect launch film. Every pixel is a pure function of t inside seek(t). */

// ───────────────────────── math ─────────────────────────
const clamp = (x, a = 0, b = 1) => (x < a ? a : x > b ? b : x);
const lerp = (a, b, x) => a + (b - a) * x;
function stepR(tau, { w, z }) {
  if (tau <= 0) return 0;
  if (z >= 1) { const x = w * tau; return 1 - Math.exp(-x) * (1 + x); }
  const wd = w * Math.sqrt(1 - z * z);
  return 1 - Math.exp(-z * w * tau) * (Math.cos(wd * tau) + (z * w / wd) * Math.sin(wd * tau));
}
class Track {
  constructor(v0) { this.v0 = v0; this.cur = v0; this.ev = []; }
  to(t, v, sp) { if (v !== this.cur) { this.ev.push([t, v - this.cur, sp]); this.cur = v; } return this; }
  at(t) { let v = this.v0; for (const [ti, d, sp] of this.ev) v += d * stepR(t - ti, sp); return v; }
}
function vis(t, tin, tout, dOut = 0.14) {
  if (t < tin) return { o: 0, b: 10, dy: 16, e: 0 };
  const e = stepR(t - tin, { w: 24, z: 1 });
  const x = tout == null ? 0 : clamp((t - tout) / dOut), xo = x * x;
  return { o: e * (1 - xo), b: (1 - e) * 10 + xo * 10, dy: (1 - e) * 16 - xo * 10, e };
}
const SP = {
  shape: { w: 15, z: 0.82 }, soft: { w: 9, z: 0.9 }, cam: { w: 6.5, z: 1 }, text: { w: 24, z: 1 },
  pop: { w: 21, z: 0.72 }, slow: { w: 3.2, z: 1 }, fast: { w: 30, z: 0.85 },
};
const hex = (h) => [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16)];
const ease = (x) => (x <= 0 ? 0 : x >= 1 ? 1 : x * x * (3 - 2 * x));
function rng(seed) { let s = seed >>> 0; return () => ((s = (s * 1664525 + 1013904223) >>> 0) / 4294967296); }

// ───────────────────────── voice timing ─────────────────────────
const LS = [0.5, 4.36, 11.0, 17.5, 22.0, 26.0, 30.5, 35.5, 41.0];
const END = 47.0;
const norm = (s) => s.toLowerCase().replace(/[.,?!…]/g, '');
function cw(line, word, n = 0) {
  const ws = VO[String(line).padStart(2, '0')].words; let k = 0;
  for (const w of ws) if (norm(w.w) === word && k++ === n) return +(LS[line - 1] + w.start).toFixed(3);
  throw new Error(`word ${line} ${word}`);
}
const T = {
  n1: cw(1, 'máy'), n2: cw(1, 'cúi'), n3: cw(1, 'điện'),
  hud: cw(2, 'hud'), connect: cw(2, 'connect'), man: cw(2, 'màn'), tay: cw(2, 'tay'), ket: cw(2, 'kết'), iphone: cw(2, 'iphone'),
  chi: cw(3, 'chỉ'), re: cw(3, 'rẽ'), xoay: cw(3, 'xoay'), toc: cw(3, 'tốc'),
  muon: cw(4, 'muốn'), noi: cw(4, 'nói'),
  tin: cw(5, 'tin'), gon: cw(5, 'gọn'), rut: cw(5, 'rút'),
  sap: cw(6, 'sắp'), mua: cw(6, 'mưa'), bao: cw(6, 'báo'),
  mat: cw(7, 'mặt'), ho: cw(7, 'hồ'), anh: cw(7, 'ảnh'), nen: cw(7, 'nền'), cach: cw(7, 'cách'), ban: cw(7, 'bạn'),
  roi: cw(8, 'rời'), xe: cw(8, 'xe'), khoa: cw(8, 'khóa'), nho: cw(8, 'nhớ'), do: cw(8, 'đỗ'),
  hud2: cw(9, 'hud'), connect2: cw(9, 'connect'), mat2: cw(9, 'mắt'), moi: cw(9, 'mọi'),
};
const S2 = T.hud - 0.42, S3 = T.chi - 0.62, S4 = LS[3] - 0.32, S5 = LS[4] - 0.45, S6 = LS[5] - 0.42,
  S7 = LS[6] - 0.42, S8 = LS[7] - 0.32, S9 = LS[8] - 0.45;
const TT = T.xoay + 0.05;             // the right turn on the map
const TYPE0 = T.noi + 0.16, QUERY = 'Trống Đồng Palace', TYPE_DT = 0.052;
const RES = TYPE0 + QUERY.length * TYPE_DT + 0.12;   // results open
const CLICK_ROW = RES + 1.0, CLICK_PILL = T.gon, CLICK_CLOSE = T.rut + 0.42;
const SWA = T.cach + 0.12, SWB = T.ban + 0.42;
const CAR = [S7 + 0.42, T.ho - 0.04, T.anh - 0.06, SWB + 0.75];  // carousel steps → idx 0,1,2,3

// ───────────────────────── cues (one source of truth for SFX) ─────────────────────────
const CUES = [];
const cue = (t, type, g = 1, x = {}) => CUES.push({ t: +t.toFixed(3), type, g, ...x });

// ───────────────────────── DOM ─────────────────────────
const W = document.getElementById('world');
const $ = (html) => { const d = document.createElement('div'); d.innerHTML = html.trim(); return d.firstElementChild; };
const add = (html, parent = W) => { const e = $(html); parent.appendChild(e); return e; };
const css = (e, o) => { for (const k in o) e.style[k] = o[k]; };

const ICON = {
  right: (c = '#04121A', s = 64) => `<svg width="${s}" height="${s}" viewBox="0 0 64 64"><path d="M20 56V34a8 8 0 0 1 8-8h14" fill="none" stroke="${c}" stroke-width="9" stroke-linecap="round"/><path d="M40 12l18 14-18 14z" fill="${c}" stroke="${c}" stroke-width="3" stroke-linejoin="round"/></svg>`,
  straight: (c = '#04121A', s = 64) => `<svg width="${s}" height="${s}" viewBox="0 0 64 64"><path d="M32 58V22" fill="none" stroke="${c}" stroke-width="9" stroke-linecap="round"/><path d="M16 26L32 8l16 18z" fill="${c}" stroke="${c}" stroke-width="3" stroke-linejoin="round"/></svg>`,
  mic: (c = '#04121A', s = 48) => `<svg width="${s}" height="${s}" viewBox="0 0 48 48"><rect x="16" y="4" width="16" height="26" rx="8" fill="${c}"/><path d="M9 22a15 15 0 0 0 30 0M24 37v7" fill="none" stroke="${c}" stroke-width="4" stroke-linecap="round"/></svg>`,
  search: (c = '#8EA3B1', s = 34) => `<svg width="${s}" height="${s}" viewBox="0 0 34 34"><circle cx="14" cy="14" r="10" fill="none" stroke="${c}" stroke-width="4"/><path d="M22 22l8 8" stroke="${c}" stroke-width="4" stroke-linecap="round"/></svg>`,
  batt: (p = 86, c = '#C9D6DF') => `<svg width="40" height="22" viewBox="0 0 40 22"><rect x="1.5" y="1.5" width="33" height="19" rx="5" fill="none" stroke="${c}" stroke-width="2.5" opacity=".7"/><rect x="5" y="5" width="${(26 * p) / 100}" height="12" rx="2.5" fill="${c}"/><rect x="36" y="7" width="3" height="8" rx="1.5" fill="${c}" opacity=".7"/></svg>`,
  cloud: (s = 150) => `<svg width="${s}" height="${s}" viewBox="0 0 150 150"><path d="M42 98h70a24 24 0 0 0 0-48 34 34 0 0 0-64-6A27 27 0 0 0 42 98z" fill="#DCE8F0"/><g stroke="#3DDCF5" stroke-width="8" stroke-linecap="round"><path class="dr" d="M52 112l-8 18"/><path class="dr" d="M78 112l-8 18"/><path class="dr" d="M104 112l-8 18"/></g></svg>`,
  umb: (c = '#04121A') => `<svg width="46" height="46" viewBox="0 0 46 46"><path d="M4 22a19 19 0 0 1 38 0z" fill="${c}"/><path d="M23 22v14a5 5 0 0 1-10 0" fill="none" stroke="${c}" stroke-width="4" stroke-linecap="round"/></svg>`,
  chat: () => `<svg width="34" height="34" viewBox="0 0 34 34"><path d="M17 4C9 4 3 9.5 3 16c0 3.6 1.8 6.8 4.7 9L6 31l7-3.4c1.3.3 2.6.4 4 .4 8 0 14-5.5 14-12S25 4 17 4z" fill="#fff"/></svg>`,
  arrowUp: () => `<svg width="70" height="70" viewBox="0 0 70 70"><circle cx="35" cy="35" r="33" fill="#fff"/><circle cx="35" cy="35" r="27" fill="#3DDCF5"/><path d="M35 15l14 32-14-8-14 8z" fill="#fff"/></svg>`,
  app: (s = 300) => `<svg width="${s}" height="${s}" viewBox="0 0 300 300"><rect x="58" y="56" width="184" height="130" rx="30" fill="#07111A" stroke="#3DDCF5" stroke-width="12"/><rect x="80" y="76" width="24" height="10" rx="5" fill="#4ADE80"/><rect x="112" y="76" width="14" height="10" rx="5" fill="#3DDCF5"/><path d="M128 162v-30a8 8 0 0 1 8-8h22" fill="none" stroke="#fff" stroke-width="16" stroke-linecap="round"/><path d="M156 100l30 24-30 24z" fill="#fff"/><rect x="141" y="186" width="18" height="24" fill="#C5D0DB"/><path d="M40 238q110-34 220 0" fill="none" stroke="#C5D0DB" stroke-width="11" stroke-linecap="round"/><rect x="134" y="206" width="32" height="16" rx="5" fill="#96A4B2"/><path d="M34 241l32-10M234 231l32 10" stroke="#3E4C59" stroke-width="20" stroke-linecap="round"/></svg>`,
  lock: () => `<svg width="80" height="100" viewBox="0 0 80 100"><path class="sh" d="M20 46V30a20 20 0 0 1 40 0v16" fill="none" stroke="#fff" stroke-width="9" stroke-linecap="round"/><rect x="6" y="44" width="68" height="52" rx="14" fill="#fff"/><circle cx="40" cy="66" r="6" fill="#04080C"/><rect x="37" y="68" width="6" height="14" rx="3" fill="#04080C"/></svg>`,
  check: (c = '#4ADE80') => `<svg width="26" height="26" viewBox="0 0 26 26"><circle cx="13" cy="13" r="12" fill="${c}"/><path d="M7.5 13.5l3.6 3.6 7.4-8" fill="none" stroke="#04121A" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/></svg>`,
};

// faces (also used inside the carousel)
const faceClassic = (time = '08:42') => `<div class="face classic"><div class="fs-top"><span class="dot"></span><span class="ok">HUD</span><span class="sp"></span>${ICON.batt(86)}&nbsp;86%</div><div class="clock num">${time}</div><div class="date">Thứ Sáu, 13 tháng 11</div><div class="meta">Hôm nay 12,4 km · 24°</div></div>`;
const ring = (p, c = 'var(--acc)', s = 84) => `<svg width="${s}" height="${s}" viewBox="0 0 84 84"><circle cx="42" cy="42" r="34" fill="none" stroke="#1B2C38" stroke-width="10"/><circle cx="42" cy="42" r="34" fill="none" stroke="${c}" stroke-width="10" stroke-linecap="round" stroke-dasharray="${(2 * Math.PI * 34 * p).toFixed(1)} 400" transform="rotate(-90 42 42)"/></svg>`;
const faceInfo = () => `<div class="face info"><div class="cx" style="left:34px;top:30px">${ring(0.86)}<div>86%<small>Pin HUD</small></div></div><div class="cx" style="right:34px;top:30px;text-align:right"><div>24°<small>Mưa 17:40</small></div>${ICON.cloud(70)}</div><div class="clock num">08:50</div><div class="day">Thứ Sáu · 13/11</div><div class="cx" style="left:34px;bottom:30px">${ring(0.62, '#4ADE80')}<div>12,4 km<small>Hôm nay</small></div></div><div class="cx" style="right:34px;bottom:30px;text-align:right"><div>3 tin<small>Thông báo</small></div><div class="av" style="width:70px;height:70px;background:var(--acc)">${ICON.chat()}</div></div></div>`;
const wallpaper = () => `<div class="wall"><div style="position:absolute;inset:0;background:linear-gradient(180deg,#1C2E52 0%,#5B4A7A 38%,#E38A6A 66%,#F3B27A 74%)"></div><div style="position:absolute;left:390px;top:262px;width:120px;height:120px;border-radius:50%;background:#FFD9A0;opacity:.9"></div><div style="position:absolute;left:-40px;right:-40px;bottom:0;height:230px;background:#2D2A4E;clip-path:polygon(0 60%,14% 22%,27% 48%,41% 8%,57% 46%,70% 26%,86% 52%,100% 30%,100% 100%,0 100%)"></div><div style="position:absolute;left:-40px;right:-40px;bottom:0;height:150px;background:#1A1B33;clip-path:polygon(0 40%,20% 10%,36% 46%,55% 18%,72% 50%,90% 20%,100% 36%,100% 100%,0 100%)"></div><div style="position:absolute;inset:0;background:linear-gradient(180deg,rgba(0,0,0,.18),rgba(0,0,0,0) 40%,rgba(0,0,0,.35))"></div></div>`;
const faceIph = () => `<div class="face iph"><div class="wall-host"></div><div class="date">Thứ Sáu, 13 tháng 11</div><div class="clock num">08:50</div><div class="meta">Hôm nay 12,4 km · ${ICON.batt(86, '#fff')} 86%</div></div>`;
const faceRide = (speed = '42', time = '08:50 · 86%', dist = '350 m', st = 'Láng Hạ') => `<div class="face ride"><div class="fs-top" style="top:30px;left:44px;right:44px"><span class="dot" style="background:var(--gr)"></span>Đang dẫn đường<span class="sp"></span><span class="num">${time}</span></div><div class="speed num">${speed}</div><div class="unit">km/h</div><div class="turn"><div class="ti">${ICON.right('#04121A', 52)}</div><b class="num">${dist}</b><span>${st}</span></div></div>`;

// background
const BG = document.getElementById('bg');
// canvas
const FX = document.getElementById('fx');
const ctx = FX.getContext('2d');

// phone (scene 1)
const phone = add(`<div id="phone" class="abs"><div class="island"></div><div class="sb"><span>08:41</span><span>${ICON.batt(12, '#FF6B6B')}</span></div></div>`);
const bannerHTML = (ic, bg, t1, t2) => `<div class="banner"><div class="ic" style="background:${bg}">${ic}</div><div><div class="t1">${t1}</div><div class="t2">${t2}</div></div></div>`;
const n2 = add(`<div class="abs">${bannerHTML(ICON.chat(), '#2F7BF6', 'TIN NHẮN · BÂY GIỜ', 'Minh: Tối nay đi ăn không?')}</div>`);
const n3 = add(`<div class="abs">${bannerHTML(ICON.batt(12, '#fff'), '#E5484D', 'PIN · BÂY GIỜ', 'Pin 12% · Hãy sạc pin')}</div>`);

// handlebar
const bar = add(`<svg class="abs" width="1100" height="270" viewBox="0 0 1100 270"><rect x="538" y="0" width="24" height="96" fill="#AEBBC8"/><rect x="508" y="86" width="84" height="40" rx="10" fill="#8A99A8"/><path d="M70 236Q550 70 1030 236" fill="none" stroke="#C5D0DB" stroke-width="24" stroke-linecap="round"/><path d="M58 242l130-40M912 202l130 40" stroke="#36434F" stroke-width="54" stroke-linecap="round"/></svg>`);

// title (scene 2)
const title = add(`<div class="abs"><div class="title">HUD <span style="color:var(--cy)">Connect</span></div></div>`);
const subt = add(`<div class="abs sub">Màn hình nhỏ trên tay lái</div>`);

// iPhone (scene 2 + 8)
const iph = add(`<div id="iph" class="abs"><div class="island"></div><div class="ap"><div style="width:120px;height:120px;border-radius:30px;background:#0B2230;overflow:hidden">${ICON.app(120)}</div>HUD Connect<div class="okchip">${ICON.check()}Đã kết nối</div></div></div>`);
const iphOk = iph.querySelector('.okchip');
const ldots = []; for (let i = 0; i < 9; i++) ldots.push(add(`<div class="ldot"></div>`));
const linkChip = add(`<div class="abs okchip" style="background:#0E2A1C">${ICON.check()}Bluetooth</div>`);

// THE shape
const shape = add(`<div id="shape"></div>`);
const ringEl = add(`<div id="ring"></div>`);
const lay = (html) => add(`<div class="lay">${html}</div>`, shape);

const Lban = lay(bannerHTML(ICON.right('#fff', 40), '#16A34A', 'MAPS · BÂY GIỜ', 'Rẽ phải sau 200 m'));
const Lhud = lay(`<div style="width:640px;height:480px;position:relative">${faceClassic('08:42')}</div>`);
const hudDot = Lhud.querySelector('.dot');

// map layer
function buildMap() {
  const R = rng(7); let s = '';
  s += `<rect width="3200" height="3200" fill="#0A1620"/>`;
  s += `<path d="M-100 900C600 1100 900 500 1700 760S2900 1300 3300 1000L3300 1240C2900 1540 2300 1000 1700 1000S600 1360 -100 1140z" fill="#0D2B3B"/>`;
  for (let i = 0; i < 26; i++) { const x = R() * 3000, y = R() * 3000, w = 120 + R() * 220, h = 100 + R() * 200; s += `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="18" fill="${R() < 0.5 ? '#0E2420' : '#0C1C27'}"/>`; }
  const roads = [];
  for (let x = 140; x < 3200; x += 230 + Math.floor(R() * 120)) roads.push(`M${x} -50V3250`);
  for (let y = 120; y < 3200; y += 220 + Math.floor(R() * 130)) roads.push(`M-50 ${y}H3250`);
  s += `<path d="${roads.join('')}" stroke="#16283A" stroke-width="16" fill="none"/>`;
  s += `<path d="M-50 3000L3250 400M-50 1500L1600 3250" stroke="#1C3346" stroke-width="30" fill="none"/>`;
  s += `<path d="M1400 -50V3250M-50 1700H3250" stroke="#22394C" stroke-width="44" fill="none"/>`;
  s += `<text x="1440" y="2060" fill="#5E7486" font-size="30" font-weight="600" transform="rotate(-90 1440 2060)">Trần Duy Hưng</text>`;
  s += `<text x="1900" y="1662" fill="#5E7486" font-size="30" font-weight="600">Láng Hạ</text>`;
  s += `<path d="M1400 2400V1700H2700" stroke="#0B3A4A" stroke-width="46" fill="none" stroke-linejoin="round"/>`;
  s += `<path class="route" pathLength="1" d="M1400 2400V1700H2700" stroke="#3DDCF5" stroke-width="26" fill="none" stroke-linejoin="round" stroke-linecap="round"/>`;
  s += `<circle cx="2700" cy="1700" r="26" fill="#fff"/><circle cx="2700" cy="1700" r="15" fill="#E5484D"/>`;
  return `<svg width="3200" height="3200" viewBox="0 0 3200 3200">${s}</svg>`;
}
const Lmap = lay(`<div class="map" style="position:relative">
  <div class="mapworld">${buildMap()}</div>
  <div class="abs" style="left:585px;top:565px">${ICON.arrowUp()}</div>
  <div class="glass turncard"><div class="tl t-a"><div class="ti">${ICON.right('#04121A', 70)}</div><div><b class="num dist">200 m</b><span>vào Láng Hạ</span></div></div>
    <div class="tl t-b"><div class="ti">${ICON.straight('#04121A', 70)}</div><div><b class="num">1,2 km</b><span>Đi thẳng · Láng Hạ</span></div></div></div>
  <div class="glass stat num">08:42 · ${ICON.batt(86)} 86%</div>
  <div class="glass eta">08:56 <span>· 4,8 km</span></div>
  <div class="glass speedb"><div class="odo num"><div class="slot"><div class="strip"></div></div><div class="slot"><div class="strip"></div></div></div><small>km/h</small></div>
</div>`);
const mapWorld = Lmap.querySelector('.mapworld'), route = Lmap.querySelector('.route');
const distEl = Lmap.querySelector('.dist'), tA = Lmap.querySelector('.t-a'), tB = Lmap.querySelector('.t-b');
const tAIcon = tA.querySelector('.ti'), speedB = Lmap.querySelector('.speedb');
const odoStrips = [...Lmap.querySelectorAll('.strip')];
odoStrips.forEach((s) => { s.innerHTML = [0,1,2,3,4,5,6,7,8,9,0].map((d) => `<div style="height:96px">${d}</div>`).join(''); });

// mic + results
const Lmic = lay(`<div class="mic"><div class="mb">${ICON.mic()}</div><div class="q"><div class="qa" style="color:#8EA3B1">Muốn đi đâu?</div><div class="qb"></div></div><div class="wave">${'<i></i>'.repeat(9)}</div></div>`);
const qa = Lmic.querySelector('.qa'), qb = Lmic.querySelector('.qb'), waveBars = [...Lmic.querySelectorAll('.wave i')], micB = Lmic.querySelector('.mb');
const RESROWS = [['Trống Đồng Palace', 'Cầu Giấy, Hà Nội', '2,1 km'], ['Trống Đồng Palace', 'Hoàn Kiếm, Hà Nội', '4,3 km'], ['Trống Đồng Palace', 'Đống Đa, Hà Nội', '5,0 km']];
const Lres = lay(`<div class="res" style="position:relative"><div class="hd">${ICON.search()}Trống Đồng Palace</div>${RESROWS.map((r, i) => `<div class="row" style="top:${140 + i * 132}px"><div class="n">${i + 1}</div><div class="tx"><b>${r[0]}</b><span>${r[1]}</span></div><em>${r[2]}</em></div>`).join('')}</div>`);
const rows = [...Lres.querySelectorAll('.row')];

// ride HUD (scene 5) with the notification pill
const Lride = lay(`<div style="width:800px;height:600px;position:relative">${faceRide('38', '08:47 · 85%', '350 m', 'Láng Hạ')}
  <div class="pill"><div class="pc"><div class="av">M</div><b>Minh</b><span>Tối nay đi ăn không?</span></div>
    <div class="msg"><div class="h"><div class="av">M</div><div><b>Minh</b><span>Tin nhắn · 08:47</span></div></div><p>Tối nay đi ăn không? 7 giờ ở chỗ cũ nhé.</p><div class="close">Đóng</div></div></div></div>`);
const pill = Lride.querySelector('.pill'), pc = pill.querySelector('.pc'), msg = pill.querySelector('.msg'), rideSpeed = Lride.querySelector('.speed');

// weather
const HOURS = [['15h', 10], ['16h', 25], ['17h', 80], ['18h', 90], ['19h', 45], ['20h', 20]];
const Lwx = lay(`<div class="wx" style="position:relative"><div class="hd"><span>Hà Nội · Chiều nay</span><span>24°</span></div>
  <div class="main">${ICON.cloud(150)}<div><b>Mưa lúc 17:40</b><span>Khả năng mưa 80%</span></div></div>
  <div class="bars">${HOURS.map(([h, p]) => `<div class="bar"><div class="pct">${p}%</div><div class="col ${p >= 60 ? 'hot' : ''}" style="height:0px"></div><div class="hr">${h}</div></div>`).join('')}</div></div>`);
const wxCols = [...Lwx.querySelectorAll('.col')], wxDrops = [...Lwx.querySelectorAll('.dr')];
const chip = add(`<div class="abs chip">${ICON.umb()}Mang áo mưa trước khi đi nhé</div>`);

// faces in the shape (hand-offs with the carousel)
const Lclassic = lay(`<div style="width:640px;height:480px;position:relative">${faceClassic('08:50')}</div>`);
const Lride2 = lay(`<div style="width:640px;height:480px;position:relative;overflow:hidden"><div style="position:absolute;left:0;top:0;transform:scale(.8);transform-origin:0 0">${faceRide()}</div></div>`);

// lock + parking
const Llock = lay(`<div class="lock" style="width:640px;height:480px;position:relative"><div class="fs-top"><span class="dot"></span>HUD<span class="sp"></span><span class="num">08:52 · 84%</span></div><div class="lk">${ICON.lock()}</div><div class="clock num">08:52</div><div class="lab">Đã khóa · iPhone ở xa</div></div>`);
const shackle = Llock.querySelector('.sh'), lockDot = Llock.querySelector('.dot');
function buildMini() {
  const R = rng(3); let s = `<rect width="440" height="500" fill="#0A1620"/>`;
  for (let i = 0; i < 9; i++) s += `<rect x="${R() * 400}" y="${R() * 460}" width="${60 + R() * 90}" height="${50 + R() * 80}" rx="10" fill="${R() < 0.5 ? '#0E2420' : '#0C1C27'}"/>`;
  s += `<path d="M110 -10V510M300 -10V510M-10 150H450M-10 360H450" stroke="#16283A" stroke-width="14"/>`;
  s += `<path d="M-10 470L450 60" stroke="#1C3346" stroke-width="22"/>`;
  s += `<path class="walk" pathLength="1" d="M110 470V360H300V250" stroke="#3DDCF5" stroke-width="10" stroke-dasharray="0.02 0.03" stroke-linecap="round" fill="none"/>`;
  s += `<circle cx="110" cy="470" r="14" fill="#fff"/><circle cx="110" cy="470" r="8" fill="#2F7BF6"/>`;
  return `<svg width="440" height="500" viewBox="0 0 440 500">${s}</svg>`;
}
const Lpark = lay(`<div class="park" style="position:relative"><div class="pm">${buildMini()}<div class="pin" style="left:258px;top:166px"><b>P</b></div></div>
  <div class="pt"><small>Vị trí đỗ xe</small><b>Xe của bạn</b><span>Đỗ lúc 08:52</span><span class="c">Cách 120 m · 2 phút đi bộ</span></div><div class="go">Chỉ đường về xe</div></div>`);
const pin = Lpark.querySelector('.pin'), walk = Lpark.querySelector('.walk');

const Licon = lay(`<div style="width:300px;height:300px">${ICON.app(300)}</div>`);

// carousel
const FACES = [faceClassic('08:50'), faceInfo(), faceIph(), `<div style="position:absolute;left:0;top:0;transform:scale(.8);transform-origin:0 0">${faceRide()}</div>`];
const cards = FACES.map((f) => add(`<div class="card">${f}<div class="rg"></div></div>`));
const iphWall = cards[2].querySelector('.wall-host'); iphWall.innerHTML = wallpaper(); const wallEl = iphWall.firstElementChild;
const SWC = ['#FFFFFF', '#3DDCF5', '#FF9F43', '#FF6B9A', '#4ADE80'];
const swatches = SWC.map((c) => add(`<div class="sw" style="background:${c}"></div>`));

// wordmark + tagline
const wHUD = add(`<div class="abs word">HUD</div>`), wCon = add(`<div class="abs word" style="color:var(--cy)">Connect</div>`);
const tag1 = add(`<div class="abs tag">Mắt nhìn đường.</div>`), tag2 = add(`<div class="abs tag" style="color:var(--cy)">Mọi thứ trong tầm mắt.</div>`);

// cursor
const ripple = add(`<div class="ripple"></div>`);
const cur = add(`<div id="cur"><svg width="44" height="52" viewBox="0 0 44 52"><path d="M6 4l32 24-15 2-8 14z" fill="#fff" stroke="#04121A" stroke-width="3" stroke-linejoin="round"/></svg><div class="nm">Bạn</div></div>`);

// ───────────────────────── timeline ─────────────────────────
const C = { x: new Track(960), y: new Track(520), lz: new Track(0) };
const Sh = { x: new Track(960), y: new Track(690), w: new Track(150), h: new Track(44), r: new Track(22),
  cr: new Track(23), cg: new Track(36), cb: new Track(46), ring: new Track(0) };
function shapeTo(t, s, sp = SP.shape) {
  for (const k of ['x', 'y', 'w', 'h', 'r', 'ring']) if (s[k] != null) Sh[k].to(t, s[k], sp);
  if (s.bg) { const [r, g, b] = hex(s.bg); Sh.cr.to(t, r, sp); Sh.cg.to(t, g, sp); Sh.cb.to(t, b, sp); }
}

// scene 1 — the problem
const PH = { top: new Track(1180) };
PH.top.to(0.12, 560, { w: 7, z: 0.86 }).to(S2 - 0.2, 1500, { w: 6, z: 1 });
const slot = (k) => 560 + 168 + k * 112;
shapeTo(T.n1 - 0.06, { x: 960, y: slot(0), w: 400, h: 96, r: 30, bg: '#17242E' }, SP.pop);
Sh.y.to(T.n2 - 0.05, slot(1), SP.shape).to(T.n3 - 0.05, slot(2), SP.shape);
const N2y = new Track(slot(0)).to(T.n3 - 0.05, slot(1), SP.shape);
const SHAKES = [T.n1, T.n2, T.n3];
SHAKES.forEach((t) => { cue(t, 'buzz', 0.9); cue(t, 'note', 0.5); });
C.y.to(T.n2 - 0.4, 820, SP.cam); C.lz.to(T.n2 - 0.4, Math.log(1.32), SP.cam); cue(T.n2 - 0.35, 'whoosh', 0.5);
C.y.to(S2 - 0.28, 540, SP.cam); C.lz.to(S2 - 0.28, 0, SP.cam); cue(S2 - 0.25, 'whoosh', 0.8);

// scene 2 — the reveal
shapeTo(S2, { x: 1260, y: 450, w: 640, h: 480, r: 48, ring: 10, bg: '#04080C' }, { w: 13, z: 0.84 });
cue(T.hud, 'hit', 1); cue(T.hud, 'riserEnd', 1);
Sh.x.to(T.ket - 0.12, 690, SP.shape); cue(T.ket - 0.1, 'whoosh', 0.55);
const IPX = new Track(2300).to(T.ket - 0.05, 1460, SP.soft).to(S3 - 0.15, 2350, SP.soft);
cue(T.iphone, 'link', 0.9);
const BAR = { d: new Track(0) }; BAR.d.to(T.tay - 0.12, 1, SP.soft).to(S3 - 0.1, 0, SP.cam).to(S8 + 0.16, 1, SP.soft).to(T.nho - 0.3, 0, SP.cam);
cue(T.tay - 0.06, 'thump', 0.5);

// scene 3 — the map
shapeTo(S3, { x: 960, y: 540, w: 1240, h: 900, r: 56, ring: 8, bg: '#0A1620' }, { w: 11, z: 0.9 });
cue(S3, 'whoosh', 1);
C.lz.to(S3 + 0.3, Math.log(1.045), SP.slow);
const camTo = (t, z, y) => { C.lz.to(t, Math.log(z), SP.cam); C.y.to(t, y, SP.cam); };
camTo(S4 - 0.05, 1.55, 540); camTo(RES - 0.08, 1.24, 540); camTo(S5 - 0.05, 1.22, 520);
camTo(S6 - 0.05, 1.14, 530); camTo(S7 - 0.05, 1.0, 540); camTo(S8 - 0.05, 1.18, 575);
camTo(T.nho - 0.3, 1.24, 540); camTo(S9 - 0.05, 1.0, 540);
const SPEED_T = T.toc;
cue(T.re, 'pop', 0.8); cue(SPEED_T, 'pop', 0.8); cue(TT - 0.1, 'swish', 0.7);
for (let m = 190; m >= 0; m -= 10) { /* distance ticks */ }

// scene 4 — voice search
shapeTo(S4, { x: 960, y: 540, w: 780, h: 132, r: 66, ring: 3, bg: '#0C1A24' }, { w: 13, z: 0.82 });
cue(S4, 'whoosh', 0.8); cue(T.noi, 'blip', 0.8);
for (let i = 0; i < QUERY.length; i++) if (QUERY[i] !== ' ') cue(TYPE0 + i * TYPE_DT, 'key', 0.55);
shapeTo(RES - 0.05, { w: 820, h: 560, r: 40 }, { w: 14, z: 0.84 }); cue(RES, 'swish', 0.7);
[0, 1, 2].forEach((i) => cue(RES + 0.2 + i * 0.125, 'tick', 0.6));

// scene 5 — notification on the ride face
shapeTo(S5, { x: 960, y: 520, w: 800, h: 600, r: 48, ring: 10, bg: '#04080C' }, { w: 13, z: 0.86 });
cue(S5, 'whoosh', 0.8); cue(T.tin, 'chime', 1);
const P = { y: new Track(-130), w: new Track(600), h: new Track(96), r: new Track(48), x: new Track(100) };
P.y.to(T.tin - 0.12, 26, SP.pop);
const OPEN = CLICK_PILL + 0.08, SHUT = CLICK_CLOSE + 0.08;
P.w.to(OPEN, 700, SP.shape).to(SHUT, 600, SP.shape); P.h.to(OPEN, 340, SP.shape).to(SHUT, 96, SP.shape);
P.r.to(OPEN, 40, SP.shape).to(SHUT, 48, SP.shape); P.x.to(OPEN, 50, SP.shape).to(SHUT, 100, SP.shape); P.y.to(OPEN, 60, SP.shape).to(SHUT, 26, SP.shape);
P.y.to(SHUT + 0.42, -140, SP.soft);
cue(OPEN, 'swish', 0.6); cue(SHUT, 'swish', 0.5);

// scene 6 — rain
shapeTo(S6, { x: 960, y: 470, w: 800, h: 520, r: 44, ring: 0, bg: '#0E2436' }, { w: 13, z: 0.86 });
cue(S6, 'whoosh', 0.8); cue(T.bao, 'pop', 0.9);
HOURS.forEach((_, i) => cue(T.mua + 0.12 + i * 0.125, 'tick', 0.5));
{ const R = rng(11); for (let t = T.mua; t < S7 + 0.2; t += 0.07 + R() * 0.12) cue(t, 'drop', 0.25 + R() * 0.3); }

// scene 7 — faces carousel
shapeTo(S7, { x: 960, y: 500, w: 640, h: 480, r: 40, ring: 6, bg: '#04080C' }, { w: 13, z: 0.86 });
cue(S7, 'whoosh', 0.8);
const IDX = new Track(0); CAR.forEach((t, i) => { if (i) { IDX.to(t - 0.12, i, { w: 13, z: 0.86 }); cue(t - 0.1, 'swish', 0.6); } });
const CAR_IN = CAR[0], CAR_OUT = S8;      // carousel owns the frame between these
const SIDE = new Track(0).to(CAR_IN, 1, SP.soft).to(CAR_OUT - 0.05, 0, SP.cam);
const ACC = { r: new Track(61), g: new Track(220), b: new Track(245) };
[[SWA, '#FF9F43'], [SWB, '#4ADE80']].forEach(([t, c]) => { const [r, g, b] = hex(c); ACC.r.to(t + 0.04, r, SP.text); ACC.g.to(t + 0.04, g, SP.text); ACC.b.to(t + 0.04, b, SP.text); });
cue(T.nen, 'shimmer', 0.7);

// scene 8 — lock + parking
shapeTo(S8, { x: 960, y: 440, w: 640, h: 480, r: 48, ring: 10, bg: '#04080C' }, { w: 13, z: 0.86 });
cue(S8, 'whoosh', 0.7); cue(T.khoa, 'lock', 1);
const IPX2 = new Track(1500);
IPX2.to(T.xe - 0.1, 1980, { w: 2.2, z: 1 });
const SNAP = T.xe + 0.75; cue(SNAP, 'snap', 0.7);
shapeTo(T.nho - 0.28, { x: 960, y: 540, w: 860, h: 500, r: 40, ring: 3, bg: '#0C1A24' }, { w: 13, z: 0.86 });
cue(T.nho - 0.25, 'whoosh', 0.6); cue(T.do + 0.12, 'pin', 1);

// scene 9 — logo
shapeTo(S9, { x: 960, y: 540, w: 300, h: 300, r: 72, ring: 0, bg: '#0B2230' }, { w: 12, z: 0.86 });
cue(S9, 'whoosh', 0.8);
Sh.x.to(T.hud2 - 0.06, 455, SP.shape); Sh.y.to(T.hud2 - 0.06, 470, SP.shape);
cue(T.hud2, 'hit', 1); cue(T.connect2, 'slam', 0.8); cue(T.mat2, 'tick', 0.4); cue(T.moi, 'tick', 0.4);

// cursor
const CU = { x: new Track(1560), y: new Track(1000) };
const CLICKS = [CLICK_ROW, CLICK_PILL, CLICK_CLOSE, SWA, SWB];
CU.x.to(RES + 0.35, 760, SP.soft); CU.y.to(RES + 0.35, 420, SP.soft);
CU.x.to(CLICK_PILL - 0.5, 1010, SP.soft); CU.y.to(CLICK_PILL - 0.5, 300, SP.soft);
CU.x.to(CLICK_CLOSE - 0.45, 1210, SP.soft); CU.y.to(CLICK_CLOSE - 0.45, 552, SP.soft);
CU.x.to(CLICK_CLOSE + 0.4, 1500, SP.soft); CU.y.to(CLICK_CLOSE + 0.4, 820, SP.soft);
const swX = (i) => 960 + (i - 2) * 100, SWY = 836;
CU.x.to(SWA - 0.42, swX(2) + 6, SP.soft); CU.y.to(SWA - 0.42, SWY + 8, SP.soft);
CU.x.to(SWB - 0.4, swX(4) + 6, SP.soft); CU.y.to(SWB - 0.4, SWY + 8, SP.soft);
CU.x.to(SWB + 0.5, 1560, SP.soft); CU.y.to(SWB + 0.5, 1000, SP.soft);
const CUR_WIN = [[RES + 0.1, S5 + 0.1], [CLICK_PILL - 0.75, CLICK_CLOSE + 0.55], [SWA - 0.75, SWB + 0.6]];
CLICKS.forEach((t) => cue(t, 'click', 0.9));

// ───────────────────────── draw helpers ─────────────────────────
const layerStyle = (el, v, extra = '') => {
  el.style.opacity = v.o.toFixed(3);
  el.style.filter = v.b > 0.05 ? `blur(${v.b.toFixed(2)}px)` : 'none';
  el.style.transform = `translate(-50%,-50%) translateY(${v.dy.toFixed(2)}px) ${extra}`;
  el.style.visibility = v.o < 0.003 ? 'hidden' : 'visible';
};
const elStyle = (el, v, x, y, extra = '') => {
  el.style.opacity = v.o.toFixed(3);
  el.style.filter = v.b > 0.05 ? `blur(${v.b.toFixed(2)}px)` : 'none';
  el.style.transform = `translate(${x.toFixed(2)}px,${(y + v.dy).toFixed(2)}px) ${extra}`;
  el.style.visibility = v.o < 0.003 ? 'hidden' : 'visible';
};
const shakeAt = (t) => SHAKES.reduce((a, ts) => (t < ts ? a : a + 9 * Math.exp(-(t - ts) * 9) * Math.sin((t - ts) * 2 * Math.PI * 22)), 0);
const press = (t) => CLICKS.reduce((a, tc) => a * (1 - 0.18 * Math.exp(-(((t - tc) / 0.05) ** 2))), 1);
const kick = (t, at, amp, k = 7) => (t < at ? 0 : amp * Math.exp(-(t - at) * k));

// route geometry
const RA = [1400, 2400], RT = [1400, 1700];
const routeLen = 700 + 1000;
const SPEED_U = 210;           // units per second on the map
const sAt = (t) => { const s0 = 700 - (TT - S3 - 0.1) * SPEED_U; return s0 + Math.max(0, t - S3 - 0.1) * SPEED_U; };
const posAt = (s) => (s <= 700 ? [1400, 2400 - s] : [1400 + (s - 700), 1700]);
const HEAD = new Track(0).to(TT - 0.28, 90, { w: 5.5, z: 1 });

// odometer
function odo(strips, v) {
  const n = strips.length;
  for (let p = 0; p < n; p++) {
    const pw = 10 ** p, d = (Math.floor(v / pw) % 10) + Math.max(0, (v % pw) - (pw - 1));
    const el = strips[n - 1 - p];
    el.style.transform = `translateY(${(-d * 96).toFixed(2)}px)`;
    el.parentElement.style.opacity = p === 0 || v >= pw ? 1 : clamp((v - pw * 0.6) / (pw * 0.4));
  }
}

// ───────────────────────── road + rain ─────────────────────────
function drawFX(t, cam) {
  ctx.setTransform(1, 0, 0, 1, 0, 0); ctx.clearRect(0, 0, 1920, 1080);
  ctx.setTransform(cam.z, 0, 0, cam.z, 960 - cam.z * cam.x, 540 - cam.z * cam.y);
  const roadA = t < S3 ? 1 : 0.14;
  const HY = 400, proj = (lat, Z) => [960 + (lat * 640) / Z, HY + 1200 / Z];
  ctx.globalAlpha = roadA;
  // edges
  ctx.strokeStyle = 'rgba(61,220,245,.35)'; ctx.lineWidth = 3;
  for (const lat of [-6, 6]) { ctx.beginPath(); const a = proj(lat, 1.4), b = proj(lat, 120); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke(); }
  // dashes
  const spd = 14, off = (t * spd) % 9;
  ctx.fillStyle = 'rgba(220,235,245,.55)';
  for (let k = 0; k < 16; k++) {
    const z0 = 1.4 + k * 9 - off, z1 = z0 + 3.2; if (z1 < 1.4) continue;
    const a = proj(-0.12, Math.max(z0, 1.4)), b = proj(0.12, Math.max(z0, 1.4)), c = proj(0.12, z1), d = proj(-0.12, z1);
    ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.lineTo(c[0], c[1]); ctx.lineTo(d[0], d[1]); ctx.fill();
  }
  // street lights
  const loff = (t * spd) % 22;
  for (let k = 0; k < 8; k++) {
    const Z = 2 + k * 22 - loff; if (Z < 2) continue;
    for (const lat of [-10, 10]) {
      const [x, y] = proj(lat, Z), r = Math.min(26, 90 / Z);
      ctx.fillStyle = `rgba(255,196,120,${(0.5 * Math.min(1, Z / 8)).toFixed(3)})`;
      ctx.beginPath(); ctx.arc(x, y - 5200 / Z, r, 0, 7); ctx.fill();
    }
  }
  ctx.globalAlpha = 1;
  // rain
  const rv = vis(t, T.mua - 0.15, S7 + 0.1, 0.5).o;
  if (rv > 0.01) {
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    const R = rng(5); ctx.strokeStyle = `rgba(160,220,245,${(0.5 * rv).toFixed(3)})`; ctx.lineWidth = 2.5; ctx.lineCap = 'round';
    for (let i = 0; i < 160; i++) {
      const x0 = R() * 2200, sp = 1500 + R() * 900, ph = R(), len = 30 + R() * 34;
      const y = ((ph + t * sp / 1300) % 1) * 1300 - 110, x = x0 - y * 0.18;
      ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(x - len * 0.18, y + len); ctx.stroke();
    }
  }
}

// ───────────────────────── seek ─────────────────────────
function seek(t) {
  // camera
  const cam = { x: C.x.at(t), y: C.y.at(t), z: Math.exp(C.lz.at(t) + kick(t, T.hud2, 0.03) + kick(t, T.hud, 0.035)) };
  W.style.transform = `translate(960px,540px) scale(${cam.z.toFixed(5)}) translate(${-cam.x.toFixed(2)}px,${-cam.y.toFixed(2)}px)`;
  BG.style.background = `radial-gradient(900px 700px at ${(22 + 6 * Math.sin(t * 0.25)).toFixed(2)}% 18%, rgba(61,220,245,.13), transparent 70%),
    radial-gradient(1000px 800px at ${(84 - 5 * Math.sin(t * 0.2)).toFixed(2)}% 92%, rgba(26,120,140,.16), transparent 70%), linear-gradient(180deg,#071722 0%,#050C11 55%,#04090D 100%)`;
  drawFX(t, cam);

  // phone
  const shake = shakeAt(t);
  const pv = { o: t < S2 + 0.4 ? 1 : 0, b: 0, dy: 0 };
  elStyle(phone, pv, 740 + shake, PH.top.at(t), `rotate(${(shake * 0.12).toFixed(3)}deg)`);
  elStyle(n2, vis(t, T.n2 - 0.04, S2 - 0.3, 0.2), 760 + shake, N2y.at(t) - 48, `scale(${(0.86 + 0.14 * stepR(t - T.n2 + 0.04, SP.pop)).toFixed(4)})`);
  elStyle(n3, vis(t, T.n3 - 0.04, S2 - 0.3, 0.2), 760 + shake, slot(0) - 48, `scale(${(0.86 + 0.14 * stepR(t - T.n3 + 0.04, SP.pop)).toFixed(4)})`);

  // shape
  const sx = Sh.x.at(t) + (t < S2 ? shake : 0), sy = Sh.y.at(t), sw = Math.max(1, Sh.w.at(t)), sh = Math.max(1, Sh.h.at(t)), sr = Sh.r.at(t);
  const shapeOn = t >= T.n1 - 0.06 && !(t >= CAR_IN && t < CAR_OUT);
  const sv = vis(t, T.n1 - 0.06, null);
  const box = { left: `${(sx - sw / 2).toFixed(2)}px`, top: `${(sy - sh / 2).toFixed(2)}px`, width: `${sw.toFixed(2)}px`, height: `${sh.toFixed(2)}px`, borderRadius: `${Math.max(0, sr).toFixed(2)}px` };
  css(shape, { ...box, background: `rgb(${Sh.cr.at(t) | 0},${Sh.cg.at(t) | 0},${Sh.cb.at(t) | 0})`, opacity: shapeOn ? sv.o.toFixed(3) : 0, visibility: shapeOn ? 'visible' : 'hidden' });
  const rw = Math.max(0, Sh.ring.at(t));
  css(ringEl, { ...box, borderWidth: `${rw.toFixed(2)}px`, opacity: shapeOn && rw > 0.2 ? Math.min(1, rw / 3).toFixed(3) : 0 });

  // layers
  layerStyle(Lban, vis(t, T.n1, S2 + 0.05, 0.16));
  layerStyle(Lhud, vis(t, T.hud - 0.08, S3 - 0.08, 0.14));
  hudDot.style.background = t > T.iphone ? '#4ADE80' : '#5B6B77';

  // handlebar (follows the shape)
  const bd = BAR.d.at(t);
  bar.style.opacity = clamp(bd).toFixed(3);
  bar.style.transform = `translate(${(sx - 550).toFixed(2)}px,${(sy + sh / 2 - 8 + (1 - bd) * 40).toFixed(2)}px) scaleX(${(0.3 + 0.7 * clamp(bd)).toFixed(4)})`;
  bar.style.transformOrigin = '550px 0';
  bar.style.visibility = bd < 0.01 ? 'hidden' : 'visible';

  // title + iPhone
  elStyle(title, vis(t, T.hud + 0.02, T.ket - 0.2), 190, 345);
  elStyle(subt, vis(t, T.man - 0.04, T.ket - 0.2), 196, 480);
  const ix = IPX.at(t) + (t > S8 - 0.5 ? 0 : 0);
  let iphX = ix, iphO = t < S3 + 0.4 ? 1 : 0;
  if (t > S8 - 0.2) { iphX = IPX2.at(t); iphO = vis(t, S8 + 0.1, SNAP + 0.25, 0.35).o; }
  elStyle(iph, { o: iphO, b: 0, dy: 0 }, iphX - 150, (t > S8 - 0.2 ? 440 : 450) - 310);
  iphOk.style.opacity = vis(t, T.iphone, null).o.toFixed(3);
  iphOk.style.display = t > S8 - 0.2 ? 'none' : 'flex';
  // link dots
  let a = sx + sw / 2 + 26, b = iphX - 150 - 26;
  const linkV = t < S8 - 0.2 ? vis(t, T.ket + 0.25, S3 - 0.2) : vis(t, S8 + 0.25, SNAP, 0.08);
  const ok = t > T.iphone && t < S8 - 0.2;
  ldots.forEach((d, i) => {
    const f = i / (ldots.length - 1);
    let x = lerp(a, b, f), y = (t > S8 - 0.2 ? 440 : 450);
    if (t > SNAP) { const k = (t - SNAP); x += (f - 0.5) * 60 * k; y += Math.sin(i * 1.7) * 220 * k * k; }
    const pulse = 0.35 + 0.65 * Math.max(0, Math.cos(((t * 1.4 - f) % 1) * Math.PI * 2));
    const o = linkV.o * (ok ? 1 : pulse) * (t > SNAP ? clamp(1 - (t - SNAP) / 0.5) : 1);
    css(d, { transform: `translate(${(x - 7).toFixed(2)}px,${(y - 7).toFixed(2)}px)`, opacity: o.toFixed(3), background: ok ? '#4ADE80' : '#3DDCF5', visibility: o < 0.003 ? 'hidden' : 'visible' });
  });
  const lc = vis(t, T.iphone + 0.05, S3 - 0.2);
  elStyle(linkChip, lc, (a + b) / 2 - 95, 450 - 100, `scale(${(0.8 + 0.2 * stepR(t - T.iphone - 0.05, SP.pop)).toFixed(4)})`);

  // map
  const mv = vis(t, S3 + 0.18, S4 - 0.02, 0.16);
  layerStyle(Lmap, mv);
  if (mv.o > 0) {
    const s = sAt(t), [px, py] = posAt(Math.max(0, s));
    const hd = HEAD.at(t);
    mapWorld.style.transform = `translate(620px,600px) rotate(${(-hd).toFixed(3)}deg) translate(${-px.toFixed(2)}px,${-py.toFixed(2)}px)`;
    route.setAttribute('stroke-dasharray', `${(ease((t - S3 - 0.25) / 0.7)).toFixed(4)} 2`);
    const rem = Math.max(0, (700 - s) * 0.45 - 6);
    distEl.textContent = rem < 15 ? 'Rẽ ngay' : `${Math.round(rem / 10) * 10} m`;
    const va = vis(t, S3 + 0.3, TT + 0.12, 0.12), vb = vis(t, TT + 0.26, null);
    tA.style.opacity = va.o; tA.style.filter = `blur(${va.b.toFixed(2)}px)`;
    tB.style.opacity = vb.o; tB.style.filter = `blur(${vb.b.toFixed(2)}px)`;
    tAIcon.style.transform = `scale(${(1 + 0.16 * Math.sin(Math.PI * clamp((t - T.re) / 0.3))).toFixed(4)})`;
    const sb = stepR(t - SPEED_T, SP.pop);
    speedB.style.transform = `scale(${(0.4 + 0.6 * sb).toFixed(4)})`; speedB.style.opacity = clamp(sb * 1.4).toFixed(3);
    const sv2 = 42 * ease((t - SPEED_T - 0.05) / 0.8) + (t > SPEED_T + 1 ? 2 * Math.sin((t - SPEED_T - 1) * 1.3) : 0);
    { const v0 = Math.max(0, sv2), fl = Math.floor(v0); odo(odoStrips, fl + ease((v0 - fl - 0.6) / 0.4)); }
  }

  // mic
  const micv = vis(t, S4 + 0.22, RES - 0.1, 0.12);
  layerStyle(Lmic, micv);
  if (micv.o > 0) {
    const n = clamp(Math.floor((t - TYPE0) / TYPE_DT) + 1, 0, QUERY.length);
    qb.textContent = QUERY.slice(0, n);
    const qav = vis(t, S4 + 0.22, TYPE0 - 0.1, 0.1); qa.style.opacity = qav.o; qa.style.filter = `blur(${qav.b}px)`;
    qb.style.opacity = t > TYPE0 ? 1 : 0;
    const live = vis(t, T.noi - 0.05, RES - 0.25, 0.15).o;
    waveBars.forEach((bEl, i) => { const h = 10 + live * 44 * Math.abs(Math.sin(t * 9 + i * 1.3) * Math.sin(t * 4.3 + i * 0.7)); bEl.style.height = `${h.toFixed(1)}px`; bEl.style.opacity = (0.3 + 0.7 * live).toFixed(3); });
    micB.style.transform = `scale(${(1 + 0.08 * live * Math.sin(t * 12)).toFixed(4)})`;
  }
  // results
  const rv = vis(t, RES + 0.08, S5 - 0.02, 0.14);
  layerStyle(Lres, rv);
  rows.forEach((r, i) => {
    const v = vis(t, RES + 0.2 + i * 0.125, null);
    r.style.opacity = v.o; r.style.transform = `translateY(${v.dy * 1.5}px)`; r.style.filter = v.b > 0.05 ? `blur(${v.b}px)` : 'none';
    const hi = i === 0 ? stepR(t - CLICK_ROW, SP.text) : 0;
    r.style.background = `rgba(61,220,245,${(0.14 * hi).toFixed(3)})`;
  });

  // ride + notification
  const ridev = vis(t, S5 + 0.22, S6 - 0.02, 0.14);
  layerStyle(Lride, ridev);
  if (ridev.o > 0) {
    rideSpeed.textContent = String(Math.round(38 + 2 * Math.sin((t - S5) * 1.1)));
    css(pill, { left: `${P.x.at(t).toFixed(2)}px`, top: `${P.y.at(t).toFixed(2)}px`, width: `${P.w.at(t).toFixed(2)}px`, height: `${P.h.at(t).toFixed(2)}px`, borderRadius: `${P.r.at(t).toFixed(2)}px` });
    const pcv = vis(t, T.tin - 0.05, OPEN - 0.06, 0.1), pcv2 = vis(t, SHUT + 0.12, null);
    const pco = t < SHUT ? pcv.o : pcv2.o, pcb = t < SHUT ? pcv.b : pcv2.b;
    pc.style.opacity = pco.toFixed(3); pc.style.filter = pcb > 0.05 ? `blur(${pcb}px)` : 'none';
    const mgv = vis(t, OPEN + 0.08, SHUT - 0.06, 0.1);
    msg.style.opacity = mgv.o.toFixed(3); msg.style.filter = mgv.b > 0.05 ? `blur(${mgv.b}px)` : 'none';
  }

  // weather
  const wv = vis(t, S6 + 0.26, S7 - 0.02, 0.14);
  layerStyle(Lwx, wv);
  if (wv.o > 0) {
    wxCols.forEach((c, i) => { const h = HOURS[i][1] * 1.2 * stepR(t - T.mua - 0.12 - i * 0.125, SP.pop); c.style.height = `${Math.max(0, h).toFixed(2)}px`; });
    wxDrops.forEach((d, i) => { const ph = ((t * 1.6 + i * 0.33) % 1); d.style.transform = `translateY(${(ph * 14).toFixed(2)}px)`; d.style.opacity = (1 - ph).toFixed(3); });
  }
  const cv = vis(t, T.bao - 0.03, S7 - 0.1, 0.14);
  elStyle(chip, cv, 960 - 300, 470 + 260 + 40, `scale(${(0.7 + 0.3 * stepR(t - T.bao + 0.03, SP.pop)).toFixed(4)})`);

  // faces in the shape + carousel
  layerStyle(Lclassic, vis(t, S7 + 0.2, CAR_IN + 0.3, 0.01));
  layerStyle(Lride2, vis(t, CAR_OUT - 0.3, S8 + 0.1, 0.14));
  const idx = IDX.at(t), side = SIDE.at(t);
  const carOn = t >= CAR_IN && t < CAR_OUT;
  const accent = `rgb(${ACC.r.at(t) | 0},${ACC.g.at(t) | 0},${ACC.b.at(t) | 0})`;
  document.documentElement.style.setProperty('--acc', t > S7 ? accent : '#3DDCF5');
  cards.forEach((c, i) => {
    const d = i - idx, ad = Math.abs(d), sg = Math.sign(d);
    const x = 960 + sg * (ad <= 1 ? ad * 640 : 640 + (ad - 1) * 420) * side;
    const sc = 1 - Math.min(ad, 2) * 0.17;
    const o = carOn ? clamp(1 - ad * 0.38) * (ad < 0.02 ? 1 : side) : 0;
    css(c, { transform: `translate(${(x - 320).toFixed(2)}px,${(500 - 240).toFixed(2)}px) scale(${sc.toFixed(4)})`, opacity: o.toFixed(3), visibility: o < 0.003 ? 'hidden' : 'visible', zIndex: String(10 - Math.round(ad * 2)) });
    c.querySelector('.rg').style.opacity = clamp(1 - ad * 1.2).toFixed(3);
  });
  wallEl.style.opacity = stepR(t - T.nen + 0.06, SP.text).toFixed(3);
  swatches.forEach((s, i) => {
    const v = vis(t, T.cach - 0.2 + i * 0.05, CAR_OUT - 0.25, 0.14);
    const sel = i === 1 ? (t < SWA ? 1 : 0) : i === 2 ? (t >= SWA && t < SWB ? 1 : 0) : i === 4 ? (t >= SWB ? 1 : 0) : 0;
    s.style.border = sel ? '5px solid #fff' : '0 solid #fff';
    s.style.boxShadow = sel ? '0 0 0 4px #04121A inset' : 'none';
    elStyle(s, v, swX(i) - 32, SWY - 32, `scale(${(sel ? 1.12 : 1).toFixed(3)})`);
  });

  // lock + parking
  const lv = vis(t, S8 + 0.18, T.nho - 0.32, 0.14);
  layerStyle(Llock, lv);
  if (lv.o > 0) {
    const cl = stepR(t - T.khoa + 0.04, SP.pop);
    shackle.setAttribute('transform', `translate(0 ${(-14 * (1 - cl)).toFixed(2)})`);
    lockDot.style.background = t > SNAP ? '#E5484D' : '#4ADE80';
  }
  const pkv = vis(t, T.nho - 0.05, S9 - 0.02, 0.14);
  layerStyle(Lpark, pkv);
  if (pkv.o > 0) {
    const pd = stepR(t - T.do + 0.1, { w: 16, z: 0.62 });
    pin.style.transform = `translateY(${(-160 * (1 - pd)).toFixed(2)}px) rotate(-45deg) scale(${clamp(pd * 3).toFixed(3)})`;
    pin.style.opacity = t > T.do - 0.1 ? 1 : 0;
    walk.setAttribute('stroke-dashoffset', (-(t * 0.25) % 0.05).toFixed(4));
    walk.style.opacity = vis(t, T.do + 0.4, null).o;
  }

  // logo
  layerStyle(Licon, vis(t, S9 + 0.22, null), `scale(${(1 - kick(t, T.hud2, 0.06, 10)).toFixed(4)})`);
  const wh = vis(t, T.hud2 - 0.02, null), wc = vis(t, T.connect2 - 0.02, null);
  elStyle(wHUD, wh, 670, 395, `scale(${(1.12 - 0.12 * stepR(t - T.hud2 + 0.02, SP.pop)).toFixed(4)})`);
  elStyle(wCon, wc, 670 + 365, 395, `scale(${(1.12 - 0.12 * stepR(t - T.connect2 + 0.02, SP.pop)).toFixed(4)})`);
  elStyle(tag1, vis(t, T.mat2 - 0.03, null), 673, 610);
  elStyle(tag2, vis(t, T.moi - 0.03, null), 673 + 392, 610);

  // cursor
  const cwin = CUR_WIN.find(([a2, b2]) => t >= a2 - 0.3 && t <= b2 + 0.3);
  const co = cwin ? vis(t, cwin[0], cwin[1], 0.2).o : 0;
  const cx = CU.x.at(t), cy = CU.y.at(t);
  const vx = (CU.x.at(t) - CU.x.at(t - 1 / 60)) * 60;
  cur.style.opacity = co.toFixed(3); cur.style.visibility = co < 0.003 ? 'hidden' : 'visible';
  cur.style.transform = `translate(${cx.toFixed(2)}px,${cy.toFixed(2)}px) rotate(${clamp(-vx * 0.0045, -14, 14).toFixed(2)}deg) scale(${press(t).toFixed(4)})`;
  const lastClick = CLICKS.filter((c2) => c2 <= t).pop();
  const rp = lastClick != null ? (t - lastClick) / 0.42 : 2;
  if (rp >= 0 && rp < 1 && co > 0.05) {
    const rr = 14 + 46 * ease(rp);
    css(ripple, { left: `${(cx - rr).toFixed(2)}px`, top: `${(cy - rr).toFixed(2)}px`, width: `${(2 * rr).toFixed(2)}px`, height: `${(2 * rr).toFixed(2)}px`, opacity: ((1 - rp) * 0.8).toFixed(3), visibility: 'visible' });
  } else ripple.style.visibility = 'hidden';
}

window.seek = seek;
window.FILM = { END, CUES, T, LS, S: { S2, S3, S4, S5, S6, S7, S8, S9 } };
document.fonts.ready.then(() => { seek(0); window.READY = true; });
const q = new URLSearchParams(location.search);
if (q.has('t')) document.fonts.ready.then(() => seek(+q.get('t')));
