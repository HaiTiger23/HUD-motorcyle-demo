const fs = require('fs');
const path = require('path');

const source = '/Users/macbook/Downloads/Việt_Nam_Hub_—_Thiết_kế_UI_và_prototype_–_Prototype_—_bấm_thử_luồng.html';
const outDir = '/Users/macbook/Project/HUD-motorcyle-demo/tmp/pdfs/prototype-html';
fs.mkdirSync(outDir, { recursive: true });
const original = fs.readFileSync(source, 'utf8');
const screens = [
  ['u01', ''], ['u02', ''], ['u03', ''], ['u04', ''],
  ['u05', ''], ['u05', 'empty'], ['u05', 'filtered'], ['u08', ''],
  ['u09', ''], ['u10', ''], ['u11', ''], ['u12', ''],
  ['u12p', ''], ['u13', ''], ['u14', ''], ['u15', '']
];
for (let i = 0; i < screens.length; i++) {
  const [start, variant] = screens[i];
  let page = original.replace('&quot;default&quot;:&quot;u01&quot;},&quot;variant&quot;', `&quot;default&quot;:&quot;${start}&quot;},&quot;variant&quot;`);
  if (variant) page = page.replace('&quot;variant&quot;:{&quot;editor&quot;:&quot;text&quot;,&quot;default&quot;:&quot;&quot;}', `&quot;variant&quot;:{&quot;editor&quot;:&quot;text&quot;,&quot;default&quot;:&quot;${variant}&quot;}`);
  fs.writeFileSync(path.join(outDir, `screen-${String(i + 1).padStart(2, '0')}.html`), page);
}
