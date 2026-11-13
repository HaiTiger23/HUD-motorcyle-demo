// usage: node render.mjs <outdir> <fps> <sub> <workers> [t0] [t1] [scale]
import { chromium } from 'playwright'; import { spawn } from 'node:child_process'; import { mkdirSync, writeFileSync } from 'node:fs';
import { FF } from './ffpath.mjs';
const [out, fpsS, subS, wS, t0S, t1S, scS] = process.argv.slice(2);
const FPS = +fpsS, SUB = +subS, WORKERS = +wS, SC = +(scS || 1);
mkdirSync(out, { recursive: true });
const b0 = await chromium.launch();
const p0 = await b0.newPage(); await p0.goto('http://127.0.0.1:8931/index.html'); await p0.waitForFunction(() => window.READY === true);
const END = await p0.evaluate(() => FILM.END); await b0.close();
const T0 = +(t0S || 0), T1 = +(t1S || END);
const CH = 4;                                     // seconds per chunk
const chunks = []; for (let t = T0; t < T1 - 1e-6; t += CH) chunks.push([t, Math.min(T1, t + CH)]);
const W = Math.round(1920 * SC), H = Math.round(1080 * SC);
async function renderChunk(page, [a, b], k) {
  const n0 = Math.round(a * FPS), n1 = Math.round(b * FPS);
  const vf = SUB > 1 ? `tmix=frames=${SUB}:weights='${Array(SUB).fill(1).join(' ')}',select='eq(mod(n\\,${SUB})\\,${SUB - 1})',setpts=N/(${FPS}*TB),` : '';
  const ff = spawn(FF, ['-y', '-loglevel', 'error', '-f', 'image2pipe', '-framerate', String(FPS * SUB), '-i', '-',
    '-vf', `${vf}scale=${W}:${H}:out_color_matrix=bt709:out_range=tv,format=yuv420p`, '-r', String(FPS),
    '-c:v', 'libx264', '-preset', 'medium', '-crf', '16', '-tune', 'film',
    '-color_primaries', 'bt709', '-color_trc', 'bt709', '-colorspace', 'bt709', `${out}/c${String(k).padStart(3, '0')}.mp4`], { stdio: ['pipe', 'inherit', 'inherit'] });
  for (let f = n0; f < n1; f++) for (let s = 0; s < SUB; s++) {
    const t = f / FPS + (SUB > 1 ? (s - (SUB - 1) / 2) / (FPS * SUB) : 0);
    await page.evaluate((t) => seek(t), Math.max(0, t));
    const buf = await page.screenshot({ type: 'jpeg', quality: 94 });
    if (!ff.stdin.write(buf)) await new Promise((r) => ff.stdin.once('drain', r));
  }
  ff.stdin.end(); await new Promise((r) => ff.on('close', r));
}
let next = 0; const t00 = Date.now();
async function worker(id) {
  const br = await chromium.launch(); const page = await br.newPage({ viewport: { width: 1920, height: 1080 }, deviceScaleFactor: 1 });
  await page.goto('http://127.0.0.1:8931/index.html'); await page.waitForFunction(() => window.READY === true);
  while (next < chunks.length) { const k = next++; await renderChunk(page, chunks[k], k); console.log(`chunk ${k + 1}/${chunks.length} done ${((Date.now() - t00) / 1000).toFixed(0)}s`); }
  await br.close();
}
await Promise.all(Array.from({ length: WORKERS }, (_, i) => worker(i)));
writeFileSync(`${out}/list.txt`, chunks.map((_, k) => `file 'c${String(k).padStart(3, '0')}.mp4'`).join('\n'));
process.exit(0);
