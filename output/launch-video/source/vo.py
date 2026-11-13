import json, os, sys, base64, urllib.request
key = open(os.path.expanduser("~/.elevenlabs_key")).read().strip()
lines = [
 "Đi xe máy... mà cứ phải cúi xuống nhìn điện thoại?",
 "Giới thiệu HUD Connect. Màn hình nhỏ trên tay lái, kết nối thẳng với iPhone.",
 "Chỉ đường từng ngã rẽ. Bản đồ xoay theo hướng đi, tốc độ luôn trong tầm mắt.",
 "Muốn đi đâu? Chỉ cần nói.",
 "Tin nhắn hiện gọn gàng, không phải rút máy.",
 "Sắp mưa? HUD báo trước khi bạn lên đường.",
 "Mặt đồng hồ, ảnh nền, theo cách của bạn.",
 "Rời xe, HUD tự khóa, và nhớ chỗ đỗ.",
 "HUD Connect. Mắt nhìn đường, mọi thứ trong tầm mắt.",
]
voice, model, tag = sys.argv[1], sys.argv[2], sys.argv[3]
out = {}
for i, text in enumerate(lines):
    body = {"text": text, "model_id": model, "language_code": "vi",
            "voice_settings": {"stability": 0.45, "similarity_boost": 0.8, "style": 0.35, "speed": 1.0}}
    if i > 0: body["previous_text"] = " ".join(lines[:i])
    if i < len(lines)-1: body["next_text"] = " ".join(lines[i+1:])
    req = urllib.request.Request(f"https://api.elevenlabs.io/v1/text-to-speech/{voice}/with-timestamps?output_format=mp3_44100_128",
        data=json.dumps(body).encode(), headers={"xi-api-key": key, "Content-Type": "application/json"})
    try:
        d = json.load(urllib.request.urlopen(req, timeout=120))
    except urllib.error.HTTPError as e:
        print("ERR", e.code, e.read()[:400]); sys.exit(1)
    fn = f"audio/{tag}_{i+1:02d}.mp3"
    open(fn, "wb").write(base64.b64decode(d["audio_base64"]))
    out[f"{i+1:02d}"] = {"text": text, "file": fn, "alignment": d.get("normalized_alignment") or d.get("alignment")}
    print(fn, "ok")
json.dump(out, open(f"audio/{tag}_align.json", "w"), ensure_ascii=False)
