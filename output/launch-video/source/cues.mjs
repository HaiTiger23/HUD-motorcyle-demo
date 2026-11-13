import { chromium } from 'playwright'; import { writeFileSync } from 'node:fs';
const b = await chromium.launch(); const p = await b.newPage();
await p.goto('http://127.0.0.1:8931/index.html'); await p.waitForFunction(() => window.READY === true);
writeFileSync('cues.json', JSON.stringify(await p.evaluate(() => ({ END: FILM.END, LS: FILM.LS, cues: FILM.CUES })), null, 1));
await b.close(); process.exit(0);
