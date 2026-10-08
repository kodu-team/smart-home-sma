# Smart Home SMA — Firmware v2

Firmware Arduino untuk ESP32: satu lampu, blower, kontak jendela, kunci servo/RFID,
DHT11, HLW8012, RTC DS3231, dan OLED SSD1306. Dashboard React berada di
[repo dashboard](../smart-home-sma-dashboard).

## Hardware dan pinout

| Perangkat | GPIO |
|---|---|
| Lampu, relay | 17 |
| Blower, relay | 12 |
| Kontak magnet jendela, INPUT_PULLUP | 33 |
| DHT11 | 4 |
| Servo kunci SG90 | 25 |
| RFID MFRC522: SS / RST / SCK / MISO / MOSI | 5 / 27 / 18 / 19 / 23 |
| OLED dan DS3231: SDA / SCL | 21 / 22 |
| HLW8012: CF / CF1 / SEL | 34 / 13 / 26 |
| Relay jalur pengukuran daya | 32 |

Satu reed switch dipakai untuk jendela. Modul relay yang ada tetap dapat digunakan
dengan dua output untuk lampu/blower. Polaritas firmware terbaru dipertahankan:
ON=HIGH, OFF=LOW; relay pengukuran daya selalu HIGH. Cocokkan wiring/polaritas dengan
hardware. GPIO12 dan GPIO5 berkaitan dengan boot ESP32; lihat catatan pada [PRD](PRD.md).

## Persiapan dan kompilasi

1. Pasang ESP32 Arduino core serta library Antares ESP HTTP, ArduinoJson, DHT sensor
   library, Adafruit Unified Sensor, HLW8012, MFRC522, RTClib, ESP32Servo,
   Adafruit GFX, Adafruit SSD1306, dan Adafruit BusIO.
2. Salin `secrets.example.h` menjadi `secrets.h`; isi Wi-Fi, access key, project,
   dan kedua device Antares. File `secrets.h` diabaikan Git.
3. Isi `AUTHORIZED_UIDS` pada sketch dengan UID kapital tanpa spasi. Daftar kosong
   menolak semua kartu. Kalibrasi konstanta HLW8012 menggunakan beban acuan.
4. Pilih ESP32 Dev Module dan kompilasi:

   ```powershell
   arduino-cli compile --fqbn esp32:esp32:esp32 --warnings none .
   ```

   Pada Arduino IDE Windows, CLI juga tersedia di
   `C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe`.

Kompilasi revisi v2 diverifikasi dengan ESP32 core 3.3.10 dan ArduinoJson 7.4.2
menggunakan library lokal yang terpasang. Dependensi dan catatan library dijelaskan
di PRD. Upload menggunakan port ESP32 yang sudah diidentifikasi, lalu buka Serial
Monitor 115200 baud.

## Perilaku

- Boot: relay lampu/blower OFF, kunci LOCK, jalur pengukuran daya ON.
- Lampu MANUAL (mode 0) menerima ON/OFF dari dashboard. SCHEDULE (mode 1) memakai
  DS3231 dalam WIB; ON/OFF manual diabaikan. Default jadwal 18:00–06:00.
- Jadwal dievaluasi saat boot dengan RTC valid, perubahan mode/jadwal, dan RTC baru
  valid; selanjutnya mengikuti event ON/OFF. Waktu nyala/mati harus berbeda.
- Blower dapat manual atau otomatis berdasarkan suhu, default threshold 33°C dan
  hysteresis 1°C. Bacaan DHT gagal tidak mengubah blower.
- Kontak jendela memakai debounce 50 ms dan hanya untuk monitoring. Kartu RFID
  terotorisasi men-toggle servo LOCK/UNLOCK; kartu lain tidak mengubah kunci.
- OLED menampilkan suhu, kelembaban, daya, dan pesan RFID. DS3231 dan otomasi lokal
  tetap bekerja saat Wi-Fi terputus. Request HTTPS Antares masih sinkron sehingga
  jaringan lambat dapat menunda loop.
- Poll command tiap 5 detik dan kirim telemetry tiap 10 detik pada koneksi normal.
  Dashboard mengambil data tiap 5 detik dan menunggu konfirmasi ID dari ESP32.

## Kontrak dan pembaruan dari v1

Kontrak aktif memakai `actuator.lamp`, `lamp_config`, dan field command `lamp`.
Lihat [kontrak data](docs/data-contract.md) untuk payload, validasi, dan konfirmasi.

Dalam transisi v1 → v2, sensor pintu GPIO35 dan lampu GPIO16 dilepas dari pemakaian.
Otomasi pintu → lampu serta tombol “Matikan Sekarang” dihapus. Field lama
`contact.door`, `lamp1`, `lamp2`, `lamp1_config`, dan `lamp2_config` bukan bagian
kontrak v2. Kunci servo/RFID tetap tersedia melalui `door_lock`.

Saat key NVS baru belum ada, konfigurasi valid `lamp1_mode`, `lamp1_on`, dan
`lamp1_off` dimigrasikan ke `lamp_mode`, `lamp_on`, dan `lamp_off`. Key baru
diutamakan pada boot berikutnya; nilai invalid memakai default. Key yang gagal
tersimpan dicoba kembali. Konfigurasi lampu kedua diabaikan, dan key lama tidak
dihapus. Konfigurasi blower serta ID/hasil command terakhir tetap disimpan.

Urutan pembaruan:

1. Hentikan dashboard/proksi lama agar tidak mengirim command v1.
2. Pasang firmware v2 dengan wiring lampu GPIO17 dan sensor jendela GPIO33.
3. Pastikan telemetry terbaru memiliki field `lamp`/`lamp_config` dan kontak jendela.
4. Build dan jalankan dashboard/proksi v2, lalu muat ulang browser.
5. Periksa mode/jadwal hasil migrasi, kontrol manual, dan konfirmasi command.

Project dan dua device Antares tetap sama; riwayat cloud tidak perlu dihapus.
Dashboard v2 tidak menerjemahkan telemetry lama menjadi status lampu baru.
Command dengan field lama atau asing ditolak, termasuk payload campuran.

## Verifikasi

[Checklist pengujian](docs/verification.md) mencakup jadwal, offline, migrasi NVS,
kontrak, dan perangkat fisik. [PRD v2](PRD.md) menjadi spesifikasi produk dan
acceptance criteria yang disimpan bersama source. Uji perangkat/Antares nyata
memerlukan ESP32 dan wiring yang telah diidentifikasi.
