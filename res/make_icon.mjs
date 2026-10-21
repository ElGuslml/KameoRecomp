// Builds res/icon.ico from its vector sources: res/icon_small.svg for 16-32 px (fewer details, a bigger
// medallion and orb) and res/icon.svg from 40 px up. The entries are 32-bit DIBs up to 64 px and a PNG at
// 256 px, the layout Explorer and llvm-rc expect.
//
// Needs @resvg/resvg-js, installed anywhere outside the repository:
//   npm install --prefix <dir> @resvg/resvg-js@2.6.2
//   node res/make_icon.mjs <dir>
import { createRequire } from 'node:module';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

if (process.argv.length < 3) {
  console.error('usage: node res/make_icon.mjs <npm prefix with @resvg/resvg-js>');
  process.exit(1);
}
const { Resvg } = createRequire(path.resolve(process.argv[2], 'package.json'))('@resvg/resvg-js');

const res = path.dirname(fileURLToPath(import.meta.url));
const large = fs.readFileSync(path.join(res, 'icon.svg'), 'utf8');
const small = fs.readFileSync(path.join(res, 'icon_small.svg'), 'utf8');

const SIZES = [16, 20, 24, 32, 40, 48, 64, 256];
const SMALL_MAX = 32;

function render(svg, size) {
  const image = new Resvg(svg, { fitTo: { mode: 'width', value: size } }).render();
  return { png: Buffer.from(image.asPng()), rgba: Buffer.from(image.pixels), size };
}

// BITMAPINFOHEADER (height doubled for the AND mask), BGRA rows bottom-up, then the 1-bit AND mask.
function dib({ rgba, size }) {
  const header = Buffer.alloc(40);
  header.writeUInt32LE(40, 0);
  header.writeInt32LE(size, 4);
  header.writeInt32LE(size * 2, 8);
  header.writeUInt16LE(1, 12);
  header.writeUInt16LE(32, 14);
  const pixels = Buffer.alloc(size * size * 4);
  const maskStride = Math.ceil(size / 32) * 4;
  const mask = Buffer.alloc(maskStride * size);
  for (let y = 0; y < size; y++) {
    const row = size - 1 - y;
    for (let x = 0; x < size; x++) {
      const s = (y * size + x) * 4;
      const d = (row * size + x) * 4;
      pixels[d] = rgba[s + 2];
      pixels[d + 1] = rgba[s + 1];
      pixels[d + 2] = rgba[s];
      pixels[d + 3] = rgba[s + 3];
      if (rgba[s + 3] === 0) mask[row * maskStride + (x >> 3)] |= 0x80 >> (x & 7);
    }
  }
  header.writeUInt32LE(pixels.length + mask.length, 20);
  return Buffer.concat([header, pixels, mask]);
}

const entries = SIZES.map((size) => {
  const image = render(size <= SMALL_MAX ? small : large, size);
  return { size, data: size >= 256 ? image.png : dib(image) };
});

const directory = Buffer.alloc(6 + 16 * entries.length);
directory.writeUInt16LE(1, 2);
directory.writeUInt16LE(entries.length, 4);
let offset = directory.length;
entries.forEach(({ size, data }, i) => {
  const e = 6 + 16 * i;
  directory.writeUInt8(size >= 256 ? 0 : size, e);
  directory.writeUInt8(size >= 256 ? 0 : size, e + 1);
  directory.writeUInt16LE(1, e + 4);
  directory.writeUInt16LE(32, e + 6);
  directory.writeUInt32LE(data.length, e + 8);
  directory.writeUInt32LE(offset, e + 12);
  offset += data.length;
});

const out = path.join(res, 'icon.ico');
fs.writeFileSync(out, Buffer.concat([directory, ...entries.map((e) => e.data)]));
console.log(`${out}: ${SIZES.join(', ')} px, ${offset} bytes`);
