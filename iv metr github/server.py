import csv, io, os, threading
from datetime import datetime, timezone
from flask import Flask, Response, jsonify, request, send_from_directory

BASE = os.path.dirname(os.path.abspath(__file__))
app = Flask(__name__, static_folder=None)

# Kunci rahasia agar hanya ESP32-mu yang boleh mengirim data.
# Set lewat environment variable API_KEY di hosting.
API_KEY = os.environ.get("API_KEY", "ganti-kunci-rahasia")

lock = threading.Lock()
latest = None   # pengukuran terbaru dari ESP32
points = []     # titik kurva yang disimpan lewat tombol "Tambah titik kurva"


def now():
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


@app.get("/")
def index():
    return send_from_directory(BASE, "index.html")


# ---------- dipanggil ESP32 ----------
@app.post("/api/data")
def receive():
    global latest
    if request.headers.get("X-API-Key") != API_KEY:
        return jsonify(error="unauthorized"), 401
    try:
        j = request.get_json(force=True)
        v, i = float(j["voltage"]), float(j["current"])
    except Exception:
        return jsonify(error="format salah, kirim {voltage, current}"), 400
    with lock:
        latest = {"voltage": v, "current": i, "power": v * i, "timestamp": now()}
    return jsonify(ok=True)


# ---------- dipanggil web ----------
@app.get("/api/latest")
def get_latest():
    return jsonify(latest)


@app.get("/api/points")
def get_points():
    return jsonify(points)


@app.post("/api/point")
def add_point():
    with lock:
        if latest is None:
            return "Belum ada data dari ESP32", 400
        points.append(dict(latest))
    return jsonify(ok=True)


@app.delete("/api/points")
def clear_points():
    with lock:
        points.clear()
    return jsonify(ok=True)


@app.get("/export.csv")
def export_csv():
    buf = io.StringIO()
    w = csv.writer(buf)
    w.writerow(["timestamp_utc", "voltage_V", "current_A", "power_W"])
    for p in points:
        w.writerow([p["timestamp"], p["voltage"], p["current"], p["power"]])
    return Response("\ufeff" + buf.getvalue(), mimetype="text/csv",
                    headers={"Content-Disposition": "attachment; filename=iv_data.csv"})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.environ.get("PORT", 5000)))
