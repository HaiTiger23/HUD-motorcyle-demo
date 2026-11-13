#!/bin/zsh
# usage: verify.sh film.mp4
S=/private/tmp/claude-501/-Users-macbook-Project-HUD-motorcyle-demo/806c2322-bffa-4944-9577-3a3c2c3eaba1/scratchpad
FF=$($S/venv/bin/python -c "import imageio_ffmpeg as f; print(f.get_ffmpeg_exe())")
rm -rf vsb && mkdir vsb
$FF -loglevel error -i $1 -vf "fps=1,scale=480:-1" vsb/s%03d.png
$S/venv/bin/python sheet.py vsb storyboard.png 6 320
# frame-difference scan at 60 fps, low res
$FF -loglevel error -i $1 -vf "scale=192:108,format=gray" -f rawvideo - | $S/venv/bin/python -c "
import sys, numpy as np
a = np.frombuffer(sys.stdin.buffer.read(), np.uint8).reshape(-1, 108, 192).astype(np.float32)
d = np.abs(np.diff(a, axis=0)).mean(axis=(1, 2))
print('frames', len(a), 'median diff', round(float(np.median(d)), 2))
for i in np.argsort(d)[::-1][:12]: print(f'  t={(i+1)/60:6.2f}s diff={d[i]:.1f}')
"
$FF -hide_banner -nostats -i $1 -af ebur128=peak=true -f null - 2>&1 | grep -A14 Summary | grep -E "I:|Peak:"
