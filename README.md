# Smart Monitoring System
## ESP32 + DHT11 — Deploy ke Render.com

---

## 📁 Struktur Folder (di komputer kamu)

```
smart-monitoring/          ← folder project (nama bebas)
├── server.js
├── package.json
└── public/
    └── index.html
```

File Arduino (.ino) terpisah — dibuka di Arduino IDE.

---

## 🚀 TUTORIAL DEPLOY KE RENDER — STEP BY STEP

### ══ TAHAP 1: Persiapan GitHub ══

> Render deploy dari GitHub, jadi kita perlu push project ke sana dulu.

**Langkah 1.1 — Buat akun GitHub**
- Buka https://github.com → Sign up (gratis)

**Langkah 1.2 — Buat repository baru**
- Klik tombol **"+"** di pojok kanan atas → **"New repository"**
- Repository name: `smart-monitoring` (nama bebas)
- Pilih **Public**
- Klik **"Create repository"**

**Langkah 1.3 — Upload file project**
- Di halaman repo yang baru dibuat, klik **"uploading an existing file"**
- Upload file-file berikut:
  - `server.js`
  - `package.json`
  - Buat folder `public` → upload `index.html` ke dalamnya
- Klik **"Commit changes"**

> Alternatif via terminal (jika sudah install Git):
> ```bash
> git init
> git add .
> git commit -m "Initial commit"
> git remote add origin https://github.com/USERNAME/smart-monitoring.git
> git push -u origin main
> ```

---

### ══ TAHAP 2: Deploy ke Render ══

**Langkah 2.1 — Buat akun Render**
- Buka https://render.com → **"Get Started for Free"**
- Daftar dengan akun **GitHub** (lebih mudah, langsung terhubung)

**Langkah 2.2 — Buat Web Service baru**
- Di dashboard Render → klik **"New +"** → pilih **"Web Service"**
- Klik **"Connect a repository"**
- Pilih repository `smart-monitoring` yang tadi dibuat
- Klik **"Connect"**

**Langkah 2.3 — Konfigurasi deployment**

Isi form dengan setting berikut:

| Field | Nilai |
|-------|-------|
| **Name** | `smart-monitoring` (nama bebas) |
| **Region** | Singapore (paling dekat Indonesia) |
| **Branch** | `main` |
| **Runtime** | `Node` |
| **Build Command** | `npm install` |
| **Start Command** | `node server.js` |
| **Instance Type** | `Free` |

**Langkah 2.4 — Deploy!**
- Scroll ke bawah → klik **"Create Web Service"**
- Tunggu proses deploy selesai (~2-5 menit)
- Lihat log di bagian bawah, tunggu muncul:
  ```
  Smart Monitoring System - Render
  Server Running ✓
  ```

**Langkah 2.5 — Catat URL kamu**
- URL akan muncul di bagian atas halaman
- Format: `https://smart-monitoring-xxxx.onrender.com`
- **Simpan URL ini** — akan dipakai di kode Arduino

---

### ══ TAHAP 3: Setup Arduino ══

**Langkah 3.1 — Install Arduino IDE**
- Download dari https://www.arduino.cc/en/software

**Langkah 3.2 — Install ESP32 Board**
- Buka Arduino IDE → **File → Preferences**
- Di "Additional boards manager URLs", tambahkan:
  ```
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  ```
- Klik OK
- Buka **Tools → Board → Boards Manager**
- Cari "esp32" → Install **"esp32 by Espressif Systems"**

**Langkah 3.3 — Install Library**
- Buka **Tools → Manage Libraries**
- Install satu per satu:
  1. **DHT sensor library** by Adafruit
  2. **Adafruit Unified Sensor** by Adafruit
  3. **ArduinoJson** by Benoit Blanchon

**Langkah 3.4 — Edit kode Arduino**
Buka `esp32_dht11.ino` dan ganti 3 baris ini:

```cpp
const char* WIFI_SSID     = "NAMA_WIFI_KAMU";    // ← WiFi name
const char* WIFI_PASSWORD = "PASSWORD_WIFI";      // ← WiFi password
const char* SERVER_URL    = "https://smart-monitoring-xxxx.onrender.com/api/data";
//                                                    ↑ URL Render kamu
```

**Langkah 3.5 — Pilih board dan upload**
- **Tools → Board → ESP32 Arduino → ESP32 Dev Module**
- **Tools → Port** → pilih port COM ESP32 (cek Device Manager Windows jika tidak muncul)
- Klik tombol **Upload** (→)
- Tunggu hingga muncul "Done uploading"

**Langkah 3.6 — Cek Serial Monitor**
- Buka **Tools → Serial Monitor**
- Set baud rate ke **115200**
- Harus muncul:
  ```
  [WiFi] TERHUBUNG!
  [HTTPS] Mengirim ke Render... SUKSES (HTTP 201)
  ```

---

### ══ TAHAP 4: Cek Dashboard ══

- Buka browser → `https://smart-monitoring-xxxx.onrender.com`
- Dashboard akan menampilkan data real-time dari ESP32

---

## ⚠️ Masalah Umum & Solusi

### Server Sleep (Render Free Tier)
**Gejala**: Pengiriman pertama gagal atau timeout ~30 detik
**Solusi**: 
- Tunggu 30-60 detik saat pertama kali. Render "membangunkan" server.
- Daftarkan URL ke https://uptimerobot.com (gratis) untuk ping otomatis setiap 14 menit agar server tidak sleep.

### "Gagal koneksi" di Serial Monitor
**Cek urutan ini**:
1. Apakah URL Render sudah benar? (copy-paste dari dashboard Render)
2. Apakah WiFi ESP32 terhubung? (cek baris [WiFi] TERHUBUNG)
3. Apakah Render menampilkan status "Live" (bukan "Suspended")?
4. Coba buka URL /health di browser: `https://xxx.onrender.com/health`

### Sensor baca NAN
**Penyebab**: Wiring salah atau kurang pull-up resistor
**Solusi**: Pasang resistor 10kΩ antara DATA dan VCC DHT11

### Upload Arduino gagal
**Solusi**: Tekan dan tahan tombol BOOT di ESP32 saat muncul "Connecting..."

---

## 📡 Endpoint API

| Method | URL | Fungsi |
|--------|-----|--------|
| POST | /api/data | Terima data dari ESP32 |
| GET | /api/data | Ambil semua data |
| GET | /api/latest | Data terbaru saja |
| GET | /api/stats | Statistik min/max/avg |
| GET | /api/alerts | Riwayat alert |
| GET | /api/export | Download CSV |
| GET | /health | Cek status server |
| DELETE | /api/data | Reset semua data |

---

## 🔌 Wiring ESP32 + DHT11

```
DHT11 Pin    →    ESP32 Pin
─────────────────────────────
VCC (pin 1)  →   3.3V
DATA (pin 2) →   GPIO 4  (+ resistor 10kΩ ke 3.3V)
NC  (pin 3)  →   (tidak digunakan)
GND (pin 4)  →   GND
```

---

## 🔔 Tips Supaya Tidak Sleep (UptimeRobot)

1. Buka https://uptimerobot.com → daftar gratis
2. Klik **"Add New Monitor"**
3. Monitor Type: **HTTP(S)**
4. Friendly Name: `Smart Monitoring`
5. URL: `https://smart-monitoring-xxxx.onrender.com/health`
6. Monitoring Interval: **5 minutes**
7. Klik **"Create Monitor"**

Server tidak akan sleep selama UptimeRobot aktif melakukan ping!
