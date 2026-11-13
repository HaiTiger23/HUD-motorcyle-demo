import json, os, sys, urllib.request
key = open(os.path.expanduser("~/.elevenlabs_key")).read().strip()
text = "Giới thiệu HUD Connect. Màn hình nhỏ trên tay lái, kết nối thẳng với iPhone. Chỉ đường từng ngã rẽ, tốc độ luôn trong tầm mắt."
for name, vid in [("liam","TX3LPaxmHKxFdv7VOQHJ"),("brian","nPczCjzI2devNBz1zQrb"),("eric","cjVigY5qzO86Huf0OWal"),("sarah","EXAVITQu4vr4xnSDxMaL")]:
    body = {"text": text, "model_id": "eleven_v4", "language_code": "vi"}
    req = urllib.request.Request(f"https://api.elevenlabs.io/v1/text-to-speech/{vid}?output_format=mp3_44100_128",
        data=json.dumps(body).encode(), headers={"xi-api-key": key, "Content-Type": "application/json"})
    try: open(f"audio/sample_{name}.mp3","wb").write(urllib.request.urlopen(req, timeout=120).read()); print(name, "ok")
    except urllib.error.HTTPError as e: print(name, "ERR", e.code, e.read()[:300])
