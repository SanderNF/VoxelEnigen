"""
Streamlined FastAPI FPS App
- POST /ingest: store numeric FPS values
- Keeps last 500 rows + global min + global max
- GET /chart-data: return series + stats
- POST /clear: delete all data
- / : modern minimal UI (Tailwind + Chart.js) with clear button
"""

from fastapi import FastAPI
from fastapi.responses import HTMLResponse, JSONResponse
from pydantic import BaseModel
from datetime import datetime
import sqlalchemy as sa
from sqlalchemy.orm import sessionmaker, declarative_base

# --- DB Setup ---
DB_URL = "sqlite:///./fps.db"
engine = sa.create_engine(DB_URL, connect_args={"check_same_thread": False})
Session = sessionmaker(bind=engine)
Base = declarative_base()

class FPS(Base):
    __tablename__ = "fps"
    id = sa.Column(sa.Integer, primary_key=True)
    value = sa.Column(sa.Float, nullable=False)
    ts = sa.Column(sa.DateTime, default=datetime.utcnow, index=True)

Base.metadata.create_all(bind=engine)

app = FastAPI()

class Ingest(BaseModel):
    value: float

# --- Helpers ---
def keep_rows(db):
    min_row = db.query(FPS).order_by(FPS.value.asc(), FPS.ts.desc()).first()
    max_row = db.query(FPS).order_by(FPS.value.desc(), FPS.ts.desc()).first()
    recent = db.query(FPS).order_by(FPS.ts.desc()).limit(500).all()
    keep = {r.id for r in recent}
    if min_row: keep.add(min_row.id)
    if max_row: keep.add(max_row.id)
    db.query(FPS).filter(~FPS.id.in_(keep)).delete(synchronize_session=False)
    db.commit()

# --- API ---
@app.post("/ingest")
def ingest(data: Ingest):
    db = Session()
    row = FPS(value=data.value)
    db.add(row)
    db.commit()
    keep_rows(db)
    db.close()
    return {"status": "ok"}

@app.post("/clear")
def clear_data():
    db = Session()
    db.query(FPS).delete()
    db.commit()
    db.close()
    return {"status": "cleared"}

@app.get("/chart-data")
def chart_data():
    db = Session()
    rows = db.query(FPS).order_by(FPS.ts.asc()).all()
    values = [r.value for r in rows]

    stats = {
        "count": len(values),
        "min": min(values) if values else None,
        "max": max(values) if values else None,
        "avg": (sum(values) / len(values)) if values else None,
    }

    series = [{"t": r.ts.isoformat(), "v": r.value} for r in rows]
    db.close()
    return JSONResponse({"series": series, **stats})

# --- UI ---
@app.get("/", response_class=HTMLResponse)
def index():
    return """
<!doctype html>
<html>
<head>
<meta charset='utf-8' />
<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
<script src="https://cdn.tailwindcss.com"></script>
<title>FPS Dashboard</title>
<style>body { max-width: 900px; margin: auto; }</style>
</head>
<body class="bg-white text-gray-900 p-4">
  <div class="flex items-center justify-between mb-3">
    <h1 class="text-xl font-semibold">FPS Dashboard</h1>
    <button onclick="clearData()" class="px-3 py-1 border border-red-400 text-red-600 rounded-md text-sm">Clear All</button>
  </div>

  <div class="border border-gray-300 p-3 rounded-lg mb-3">
    <canvas id="chart" height="140"></canvas>
  </div>

  <div class="grid grid-cols-4 gap-2 text-center border border-gray-300 p-3 rounded-lg">
    <div><div class="text-xs text-gray-500">Count</div><div id="count" class="text-lg font-medium text-blue-600">-</div></div>
    <div><div class="text-xs text-gray-500">Min</div><div id="min" class="text-lg font-medium text-green-600">-</div></div>
    <div><div class="text-xs text-gray-500">Max</div><div id="max" class="text-lg font-medium text-red-600">-</div></div>
    <div><div class="text-xs text-gray-500">Avg</div><div id="avg" class="text-lg font-medium text-indigo-600">-</div></div>
  </div>

<script>
const ctx = document.getElementById('chart').getContext('2d');
const chart = new Chart(ctx, {
  type: 'line',
  data: { labels: [], datasets: [{ label: 'FPS', data: [], borderWidth: 2, borderColor: '#2563eb', pointRadius: 0, tension: 0.2 }] },
  options: { responsive: true, maintainAspectRatio: false, scales: { x: { ticks: { color: '#555', maxRotation: 0, autoSkip: true } }, y: { ticks: { color: '#555' } } }, plugins: { legend: { display: false } } }
});

async function refresh() {
  const r = await fetch('/chart-data');
  const j = await r.json();
  chart.data.labels = j.series.map(p => p.t);
  chart.data.datasets[0].data = j.series.map(p => p.v);
  chart.update();
  document.getElementById('count').textContent = j.count;
  document.getElementById('min').textContent = j.min;
  document.getElementById('max').textContent = j.max;
  document.getElementById('avg').textContent = j.avg ? j.avg.toFixed(3) : '-';
}

async function clearData() {
  await fetch('/clear', { method: 'POST' });
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
"""