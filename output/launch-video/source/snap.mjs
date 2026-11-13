import { chromium } from 'playwright';
const [dir, ...ts] = process.argv.slice(2);
const b = await chromium.launch(); const p = await b.newPage({ viewport: { width: 1920, height: 1080 } });
p.on('pageerror', (e) => console.log('PAGEERR', e.message)); p.on('console', (m) => { if (m.type() === 'error') console.log('CONSOLE', m.text()); });
await p.goto('http://127.0.0.1:8931/index.html'); await p.waitForFunction(() => window.READY === true, null, { timeout: 15000 });
if (ts[0] === 'info') { console.log(JSON.stringify(await p.evaluate(() => ({ T: FILM.T, S: FILM.S, n: FILM.CUES.length })))); }
else for (const t of ts) { await p.evaluate((t) => seek(t), +t); await p.screenshot({ path: `${dir}/t${(+t).toFixed(2).padStart(6, '0')}.png` }); }
await b.close(); process.exit(0);
