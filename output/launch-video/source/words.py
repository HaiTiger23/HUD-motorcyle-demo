import json, sys
a = json.load(open(f"audio/{sys.argv[1]}_align.json"))
out = {}
for k, v in a.items():
    al = v["alignment"]; chars, st, en = al["characters"], al["character_start_times_seconds"], al["character_end_times_seconds"]
    words, cur = [], None
    for c, s, e in zip(chars, st, en):
        if c.isspace():
            if cur: words.append(cur); cur = None
            continue
        if cur is None: cur = {"w": "", "start": s, "end": e}
        cur["w"] += c; cur["end"] = e
    if cur: words.append(cur)
    out[k] = {"text": v["text"], "end": en[-1], "words": [{"w": w["w"], "start": round(w["start"], 3), "end": round(w["end"], 3)} for w in words]}
    print(k, f"{en[-1]:.2f}s", " ".join(f'{w["w"]}@{w["start"]:.2f}' for w in words))
json.dump(out, open(f"audio/{sys.argv[1]}_words.json", "w"), ensure_ascii=False, indent=1)
