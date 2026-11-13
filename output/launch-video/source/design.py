import json, os, sys, base64, urllib.request
key = open(os.path.expanduser("~/.elevenlabs_key")).read().strip()
def call(path, body):
    req = urllib.request.Request("https://api.elevenlabs.io"+path, data=json.dumps(body).encode(),
        headers={"xi-api-key": key, "Content-Type": "application/json"})
    try: return json.load(urllib.request.urlopen(req, timeout=180))
    except urllib.error.HTTPError as e: print("ERR", e.code, e.read()[:500]); sys.exit(1)
desc = ("A native Vietnamese man in his late twenties with a clear northern Hanoi accent. Confident, warm and energetic, "
        "a premium tech product launch narrator, crisp studio recording, modern and upbeat, natural intonation.")
text = ("Đi xe máy mà cứ phải cúi xuống nhìn điện thoại? Giới thiệu HUD Connect, màn hình nhỏ trên tay lái, "
        "kết nối thẳng với iPhone. Chỉ đường từng ngã rẽ, tốc độ luôn trong tầm mắt.")
d = call("/v1/text-to-voice/design?output_format=mp3_44100_128", {"voice_description": desc, "model_id": "eleven_ttv_v3", "text": text, "guidance_scale": 5})
for i, p in enumerate(d["previews"]):
    open(f"audio/design_{i}.mp3", "wb").write(base64.b64decode(p["audio_base_64"]))
    print(i, p["generated_voice_id"], p.get("duration_secs"), p.get("language"))
json.dump([p["generated_voice_id"] for p in d["previews"]], open("audio/design_ids.json", "w"))
