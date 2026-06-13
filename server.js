// =========================================
// Smart Monitoring System - Backend Server
// Node.js + Express — Render Deployment
// =========================================

const express = require('express');
const cors    = require('cors');
const path    = require('path');

const app  = express();
const PORT = process.env.PORT || 3000;

// ===== MIDDLEWARE =====
app.use(cors());
app.use(express.json());

// ===== IN-MEMORY STORAGE =====
// Render filesystem bersifat ephemeral — gunakan in-memory
// Data hilang saat server restart (normal untuk demo/tugas)
const MAX_RECORDS = 1000;
let sensorData = [];

// ===== KEEP-ALIVE PING (PENTING untuk Render gratis) =====
// Render free tier sleep setelah 15 menit idle
// Untuk demo: gunakan UptimeRobot.com (gratis) untuk ping tiap 14 menit
// Atau aktifkan self-ping di bawah ini:
const SELF_PING = process.env.SELF_PING_URL || null; // Set di Render env var
if (SELF_PING) {
  setInterval(async () => {
    try {
      await fetch(SELF_PING + '/health');
      console.log('[Keep-alive] Ping sent');
    } catch (e) { /* ignore */ }
  }, 14 * 60 * 1000); // Ping setiap 14 menit
}

// ===== ROUTES =====
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'index.html'));
});

// Health check — Render butuh endpoint ini
app.get('/health', (req, res) => {
  res.json({
    status:  'ok',
    uptime:  Math.floor(process.uptime()),
    records: sensorData.length,
    memory:  Math.round(process.memoryUsage().heapUsed / 1024 / 1024) + ' MB',
    time:    new Date().toISOString(),
  });
});

// POST /api/data — Terima data dari ESP32
app.post('/api/data', (req, res) => {
  const body = req.body;

  if (body.temperature === undefined || body.humidity === undefined) {
    return res.status(400).json({ error: 'Data tidak valid: temperature dan humidity wajib ada' });
  }

  const record = {
    id:          Date.now(),
    timestamp:   new Date().toISOString(),
    device_id:   body.device_id   || 'ESP32-001',
    temperature: parseFloat(body.temperature),
    humidity:    parseFloat(body.humidity),
    heat_index:  body.heat_index  ? parseFloat(body.heat_index) : null,
    status:      body.status      || 'normal',
    alert:       body.alert       || 'none',
    wifi_rssi:   body.wifi_rssi   || null,
    uptime:      body.uptime      || null,
  };

  sensorData.push(record);
  if (sensorData.length > MAX_RECORDS) sensorData.shift();

  const time      = new Date().toLocaleTimeString('id-ID');
  const alertInfo = record.alert !== 'none' ? ` | ⚠️ ${record.alert}` : '';
  console.log(`[${time}] ${record.device_id} → Suhu: ${record.temperature}°C | Lembab: ${record.humidity}% | ${record.status}${alertInfo}`);

  res.status(201).json({ success: true, id: record.id, total: sensorData.length });
});

// GET /api/data — Ambil data untuk dashboard
app.get('/api/data', (req, res) => {
  const limit = Math.min(parseInt(req.query.limit) || 100, MAX_RECORDS);
  res.json(sensorData.slice(-limit));
});

// GET /api/latest — Data terbaru saja
app.get('/api/latest', (req, res) => {
  if (sensorData.length === 0) return res.json(null);
  res.json(sensorData[sensorData.length - 1]);
});

// GET /api/stats — Statistik ringkasan
app.get('/api/stats', (req, res) => {
  if (sensorData.length === 0) return res.json({ count: 0 });

  const temps  = sensorData.map(d => d.temperature);
  const hums   = sensorData.map(d => d.humidity);
  const alerts = sensorData.filter(d => d.alert !== 'none');

  res.json({
    count: sensorData.length,
    temp: {
      min: Math.min(...temps).toFixed(1),
      max: Math.max(...temps).toFixed(1),
      avg: (temps.reduce((a, b) => a + b, 0) / temps.length).toFixed(1),
    },
    humidity: {
      min: Math.min(...hums).toFixed(1),
      max: Math.max(...hums).toFixed(1),
      avg: (hums.reduce((a, b) => a + b, 0) / hums.length).toFixed(1),
    },
    alerts_count: alerts.length,
    last_update:  sensorData[sensorData.length - 1].timestamp,
  });
});

// GET /api/alerts — Riwayat alert
app.get('/api/alerts', (req, res) => {
  res.json(sensorData.filter(d => d.alert !== 'none').slice(-50));
});

// GET /api/export — Download CSV
app.get('/api/export', (req, res) => {
  const header = 'id,timestamp,device_id,temperature,humidity,heat_index,status,alert\n';
  const rows   = sensorData.map(d =>
    `${d.id},"${d.timestamp}","${d.device_id}",${d.temperature},${d.humidity},${d.heat_index ?? ''},"${d.status}","${d.alert}"`
  ).join('\n');

  res.setHeader('Content-Type', 'text/csv');
  res.setHeader('Content-Disposition', `attachment; filename="sensor_data_${Date.now()}.csv"`);
  res.send(header + rows);
});

// DELETE /api/data — Reset data
app.delete('/api/data', (req, res) => {
  sensorData = [];
  console.log('[DB] Data direset');
  res.json({ success: true, message: 'Semua data berhasil dihapus' });
});

// ===== START SERVER =====
app.listen(PORT, '0.0.0.0', () => {
  console.log('====================================');
  console.log('  Smart Monitoring System - Render  ');
  console.log('====================================');
  console.log(`[Server] Port    : ${PORT}`);
  console.log(`[Server] Node.js : ${process.version}`);
  console.log('[Server] Status  : Running ✓');
  console.log('[Server] Menunggu data dari ESP32...\n');
});
