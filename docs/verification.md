# Checklist verifikasi v2

## Verifikasi pada komputer

- [x] Kompilasi sketch: `arduino-cli compile --fqbn esp32:esp32:esp32 --warnings none .`.
- [x] Dashboard: `npm test` (36 tes) dan `npm run build`.
- [x] Tes React: satu Lampu, satu kontak Jendela, GPIO17, tanpa kontrol override
  pada jadwal; perubahan mode/jadwal memakai field baru.
- [x] Tes proksi: command lampu baru, blower/servo, dan penolakan field lama,
  payload campuran, serta nested field asing sebelum diteruskan ke cloud.
- [x] Tes status: konfirmasi ID, hasil command, data kedaluwarsa, serta lampu
  dinonaktifkan pada telemetry lama/tidak lengkap/invalid.

Pengujian di atas tidak menjalankan ESP32 atau menghubungi Antares nyata.
Checklist perangkat berikut masih memerlukan ESP32 dengan port dan wiring yang
telah diidentifikasi. Pakai project/device uji untuk skenario NVS dan command.

## Firmware dan wiring

- [ ] Boot: lampu GPIO17 OFF, blower OFF, servo LOCK, relay daya ON; tidak ada
  inisialisasi output lampu GPIO16 atau kontak GPIO35.
- [ ] Window GPIO33: buka/tutup, periksa log dan `contact.window`; gangguan kontak
  kurang dari 50 ms tidak menjadi perubahan stabil.
- [ ] Telemetry memiliki seluruh field [kontrak v2](data-contract.md), tanpa field
  kontak pintu/lampu kedua; daya, suhu/kelembaban, RSSI, dan state servo terbaca.
- [ ] Manual: kirim lamp ON/OFF dengan ID berbeda; relay dan telemetry sesuai,
  `last_command_result=applied`.
- [ ] Schedule 18:00–06:00: ON pada 18:00, tetap ON setelah tengah malam,
  OFF pada 06:00. Ulangi interval siang, misalnya 08:00–10:00.
- [ ] Boot saat interval jadwal aktif mengevaluasi ON dengan RTC valid.
- [ ] RTC tidak ada/invalid: boot aman, jadwal menunggu; NTP membuat RTC valid lalu
  jadwal dievaluasi. Jangan mengubah clock mesin dashboard untuk tes RTC.
- [ ] Pada SCHEDULE, kirim lamp OFF ketika ON dan ON ketika OFF: status tetap,
  hasil ignored. Ubah ke MANUAL sebelum mencoba ON/OFF lagi.
- [ ] Membuka jendela atau mengubah servo/RFID tidak mengubah lampu.

## Kontrak dan konfirmasi firmware

- [ ] Kirim command dengan field v1 `lamp1`/`lamp2`: rejected.
- [ ] Kirim campuran `lamp:"ON"`, `lamp2:"OFF"`, `door_lock:"UNLOCK"`: rejected;
  lampu, blower, dan servo tidak berubah.
- [ ] Kirim config lamp valid dengan nested field asing pada blower: rejected,
  tidak ada perubahan mode/jadwal/threshold.
- [ ] Aksi invalid menolak seluruh control sebelum perubahan aktuator.
- [ ] Semua NONE atau kontrol dilarang mode: ignored; command servo valid: applied.
- [ ] Ulangi ID yang sama pada control/config, lalu restart dan poll kembali:
  tidak diterapkan ulang; ID/hasil terakhir tetap tersedia.
- [ ] Bandingkan pending dashboard dengan telemetry: status berubah setelah
  konfirmasi perangkat, bukan setelah POST 202 saja.

## Migrasi dan persistence

- [ ] NVS v1 valid: `lamp1_mode=1`, `lamp1_on=22:00`, `lamp1_off=05:00`,
  threshold blower=35, dan ID terakhir terisi. Boot v2 mempertahankan nilai dan
  menyimpan key `lamp_mode`, `lamp_on`, `lamp_off`.
- [ ] Ubah jadwal/mode melalui v2, restart: key baru diutamakan walau key lama
  berbeda; konfigurasi blower dan riwayat command tetap ada.
- [ ] Konfigurasi lampu kedua tidak memengaruhi lampu GPIO17.
- [ ] Mode/jadwal legacy invalid memakai default manual dan 18:00–06:00.
- [ ] Boot ulang setelah migrasi tidak menulis ulang key yang sudah ada.
- [ ] Jika NVS gagal dibuka, firmware memakai default dan tidak mencoba menulis
  config. Bila penulisan key baru gagal, key yang belum ada dicoba pada boot berikutnya.

## Fungsi lain dan offline

- [ ] Blower manual ON/OFF; mode suhu: ON ≥33°C, tetap ON pada 32.6°C,
  OFF ≤32°C. Bacaan DHT gagal tidak mengubah blower.
- [ ] RFID terotorisasi men-toggle LOCK/UNLOCK, yang tidak terotorisasi tidak
  mengubah servo; OLED menampilkan pesan sementara yang sesuai.
- [ ] OLED menampilkan suhu, kelembaban, daya dan pesan RFID; servo dapat
  dikontrol dari dashboard melalui LOCK/UNLOCK.
- [ ] Putuskan Wi-Fi: RTC/jadwal, sensor jendela, RFID, blower, dan OLED tetap
  bekerja. Dashboard menonaktifkan kontrol setelah data berumur >20 detik.
- [ ] Hubungkan ulang: Wi-Fi reconnect, NTP sinkron, telemetry/command kembali
  bekerja dengan kontrak baru.

## Tampilan

- [x] Browser pada viewport 1280px: tiga kartu perangkat sejajar; satu kontak jendela.
- [x] Browser pada viewport 820px: dua kolom, kartu kunci memenuhi baris terakhir.
- [x] Browser pada viewport 390px: satu kolom perangkat tanpa overflow horizontal.

Ukuran dan posisi elemen diverifikasi melalui DOM browser menggunakan telemetry
contoh lokal; pemeriksaan ini tidak mengirim command ke Antares.
