# Kontrak data Smart Home SMA v2

Firmware dan dashboard menggunakan project `smart-home-sma` dengan dua device:
`smarthome-telemetry` dan `smarthome-commands`. Nama dapat dikonfigurasi di
`secrets.h` firmware dan `.env` proksi, dan harus cocok.

## Telemetry

ESP32 mengirim kondisi aktual setiap 10 detik pada koneksi normal. Payload Antares
memakai maksimal dua tingkat object:

```json
{
  "ts": "2026-10-08T18:30:00+07:00",
  "sensor": { "temperature": 29.5, "humidity": 67, "active_power": 125.4 },
  "contact": { "window": "CLOSED" },
  "actuator": { "lamp": "ON", "blower": "OFF", "door_lock": "LOCK" },
  "lamp_config": { "mode": 1, "on": "18:00", "off": "06:00" },
  "blower_config": { "mode": 0, "threshold": 33, "hysteresis": 1 },
  "system": {
    "wifi": "CONNECTED",
    "rssi": -61,
    "rtc_valid": 1,
    "uptime": 600,
    "last_command_id": "config-v2",
    "last_command_result": "applied"
  }
}
```

| Field | Arti / nilai |
|---|---|
| `ts` | Waktu RTC ISO 8601 WIB; kosong jika RTC belum valid |
| `sensor.temperature`, `humidity`, `active_power` | Angka dalam °C, %, dan W |
| `contact.window` | `OPEN` / `CLOSED`, debounce 50 ms |
| `actuator.lamp`, `blower` | `ON` / `OFF`, state relay firmware |
| `actuator.door_lock` | `LOCK` / `UNLOCK`, state servo; bukan posisi fisik pintu |
| `lamp_config.mode` | Integer 0=manual, 1=jadwal |
| `lamp_config.on`, `off` | HH:MM valid, berbeda; default 18:00 / 06:00 |
| `blower_config.mode` | Integer 0=manual, 1=suhu |
| `blower_config.threshold`, `hysteresis` | Angka °C; default 33 / 1 |
| `system.wifi`, `rssi`, `uptime` | Status Wi-Fi, angka dBm, detik sejak boot |
| `system.rtc_valid` | Integer 0 / 1 |
| `system.last_command_id`, `last_command_result` | ID dan hasil terakhir; kosong sebelum ada command |

Suhu/kelembaban mempertahankan nilai valid terakhir. Sebelum ada bacaan valid,
telemetry mengirim 0 sementara OLED menampilkan `--.-`.

Antares menyimpan payload sebagai string JSON pada `m2m:cin.con` dan memberikan
waktu pembuatan `m2m:cin.ct`. Proksi mengembalikan `{ "payload": {...}, "createdAt": "..." }`
dari GET `/api/telemetry`. Dashboard mengutamakan waktu cloud (format Antares
project ini dalam WIB); fallback ke `ts`. Kontrol membutuhkan data berumur maksimal
20 detik, dengan toleransi waktu masa depan 30 detik. Kontrol lampu juga membutuhkan
status, mode, dan jadwal kontrak baru yang valid.

## Command konfigurasi

POST `/api/commands` menerima JSON command langsung. Proksi meneruskannya ke Antares
sebagai `{"m2m:cin":{"con":"<string JSON command>"}}`.

```json
{
  "id": "config-v2",
  "type": "config",
  "lamp": { "mode": 1, "on": "22:00", "off": "05:00" },
  "blower": { "mode": 1, "threshold": 35, "hysteresis": 1 }
}
```

Root yang diizinkan: `id`, `type`, `lamp`, `blower`. Field lampu: `mode`, `on`,
`off`; field blower: `mode`, `threshold`, `hysteresis`. Object dan field konfigurasi
dapat dikirim sebagian. Mode harus 0/1. Jadwal HH:MM 00:00–23:59, ON dan OFF berbeda,
termasuk saat mengubah hanya satu waktu terhadap konfigurasi aktif firmware.
Threshold 10–60°C; hysteresis >0, ≤10°C, dan lebih kecil dari threshold aktif.

Firmware memfilter nilai invalid pada field yang dikenal, menerapkan field valid,
dan menyimpan nilai yang berubah. Jika tidak ada field valid, hasil `rejected`;
nilai valid yang sama tetap `applied` tanpa menulis ulang konfigurasi. Proksi
memvalidasi tipe/rentang field yang dikirim, sedangkan firmware memeriksa hubungan
dengan konfigurasi aktif. Field asing pada seluruh payload selalu ditolak sebelum
konfigurasi diterapkan, sekalipun field lainnya valid.

## Command kontrol

```json
{ "id": "control-v2", "type": "control", "lamp": "ON", "blower": "NONE", "door_lock": "NONE" }
```

Root yang diizinkan: `id`, `type`, `lamp`, `blower`, `door_lock`. Field aktuator
yang tidak dikirim atau bernilai null dianggap `NONE`.

| Aktuator | Aksi | Kapan diterapkan |
|---|---|---|
| Lampu | `NONE`, `ON`, `OFF` | ON/OFF hanya saat mode manual |
| Blower | `NONE`, `ON`, `OFF` | ON/OFF hanya saat mode manual |
| Kunci servo | `NONE`, `LOCK`, `UNLOCK` | LOCK/UNLOCK pada semua mode |

Aksi invalid atau field asing menolak seluruh command sebelum perubahan aktuator.
Command lampu ON maupun OFF pada mode jadwal menghasilkan `ignored` bila tidak ada
aksi lain yang diterapkan. Servo/RFID tidak memengaruhi lampu.

## ID dan konfirmasi

- `id` adalah string tidak kosong, maksimal 63 karakter; `type` hanya config/control.
- JSON rusak atau ID invalid diabaikan firmware karena tidak dapat dikonfirmasi.
- ID sama dengan ID terakhir diabaikan, termasuk setelah restart.
- `applied`: ada konfigurasi valid atau minimal satu aksi kontrol yang diizinkan.
- `ignored`: aksi valid, tetapi semuanya NONE atau dilarang mode saat ini.
- `rejected`: type, bentuk payload, nama field, atau aksi tidak valid; ID/hasil
  tetap dicatat dan dilaporkan melalui telemetry.
- POST proksi mengembalikan HTTP 202 dan `{ "id": "..." }`; validasi proksi gagal
  menghasilkan HTTP 400 tanpa diteruskan ke Antares.
- Respons POST bukan konfirmasi aktuator. Dashboard menunggu ID yang cocok pada
  telemetry sebelum membuka pengiriman berikutnya. Antares hanya memberikan pesan
  terbaru; beberapa dashboard yang menulis bersamaan dapat saling menimpa.

## Perubahan dari v1

Field `contact.door`, `actuator.lamp1`, `actuator.lamp2`, `lamp1_config`, dan
`lamp2_config` dihapus. Command `lamp1`/`lamp2` tidak menjadi alias `lamp`:
payload campuran baru/lama pun ditolak sebelum aksi/config diterapkan.
Kunci pintu tetap menggunakan `door_lock`. Migrasi NVS dan urutan pembaruan
dijelaskan di [README](../README.md).
