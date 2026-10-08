# Product Requirements Document (PRD)
# Smart Home IoT untuk Pembelajaran Tingkat SMA

- **Versi:** 2.0
- **Status:** Revisi satu lampu; firmware mengikuti implementasi `smart-home-sma.ino`
- **Platform Hardware:** ESP32
- **Firmware:** Arduino Framework / C++
- **IoT Platform:** Antares HTTP
- **Dashboard:** React + Vite dengan proksi Node pada origin yang sama
- **Target Pembelajaran:** Siswa SMA/Sederajat

---

## 1. Ringkasan Produk

Smart Home IoT adalah proyek pembelajaran Internet of Things tingkat SMA yang mengintegrasikan sensor, aktuator, otomasi, komunikasi internet, penyimpanan konfigurasi, dan aplikasi dashboard berbasis web.

Sistem menggunakan ESP32 sebagai controller utama dan Antares IoT Platform sebagai perantara komunikasi antara ESP32 dan dashboard.

Arsitektur utama:

```text
Sensor / Aktuator
       │
       ▼
     ESP32
       │
       │ Wi-Fi + HTTPS
       ▼
 Antares IoT Platform
       ▲
       │ HTTPS REST API
       │
 Proksi Node /api
       ▲
       │ HTTP same-origin
       │
 Dashboard React
```

Sistem dirancang agar fungsi lokal tetap dapat berjalan ketika koneksi internet terputus.

---

# 2. Tujuan Produk

## 2.1 Tujuan Utama

Proyek harus memungkinkan siswa memahami konsep:

- pembacaan sensor;
- kontrol aktuator;
- input digital;
- relay;
- servo;
- RFID;
- monitoring penggunaan daya;
- komunikasi I2C;
- komunikasi SPI;
- penyimpanan konfigurasi;
- Wi-Fi;
- HTTP/HTTPS;
- REST API;
- IoT cloud platform;
- dashboard web;
- otomasi berbasis kondisi;
- otomasi berbasis waktu;
- sinkronisasi waktu menggunakan NTP;
- konsep edge/local automation;
- integrasi hardware, firmware, cloud, dan web.

---

## 2.2 Hasil Akhir

Siswa menghasilkan sebuah prototype Smart Home yang dapat:

1. memonitor suhu dan kelembaban;
2. mendeteksi status jendela;
3. memonitor active power;
4. mengontrol satu lampu;
5. mengontrol blower;
6. mengontrol kunci pintu;
7. membuka/mengunci pintu menggunakan RFID;
8. menjalankan otomasi berdasarkan suhu;
9. menjalankan otomasi berdasarkan jadwal;
10. menampilkan informasi pada OLED;
11. mengirim telemetry melalui Antares;
12. menerima konfigurasi dan command dari Antares;
13. dikontrol melalui web dashboard.

---

# 3. Scope Sistem

Sistem terdiri dari ESP32, Antares, dan dashboard React dengan proksi Node:

```text
+------------------------+
|     SMART HOME NODE    |
|         ESP32          |
+------------------------+
 | Sensor
 | Relay
 | Servo
 | RFID
 | RTC
 | OLED
 |
 | Wi-Fi / HTTPS
 v
+------------------------+
|     ANTARES CLOUD      |
|                        |
| Telemetry Device       |
| Commands Device        |
+------------------------+
          ^
          |
          | HTTPS REST API
          |
+------------------------+
|     WEB DASHBOARD      |
| React + Proksi Node    |
+------------------------+
```

---

# 4. Out of Scope

Versi v2 tidak mencakup:

- user login;
- authentication dashboard;
- database sendiri;
- MQTT;
- mobile application;
- OTA firmware update;
- pengelolaan RFID melalui dashboard;
- histori grafik jangka panjang;
- multi-user;
- multi-home;
- push notification;
- email notification;
- kontrol relay sensor daya dari dashboard;
- role-based access control.

Dashboard memakai proksi Node untuk mengakses Antares; tidak ada backend bisnis atau database.

---

# 5. Hardware

## 5.1 Komponen

| Komponen | Fungsi |
|---|---|
| ESP32 | Main controller |
| DHT11 | Sensor suhu dan kelembaban |
| Magnetic Reed Switch | Sensor jendela |
| Custom HLW8012 Power Sensor | Monitoring active power |
| Relay 4 Channel | Kontrol lampu dan blower |
| SG90 Servo | Simulasi kunci pintu |
| MFRC522 | RFID reader |
| DS3231 | Real Time Clock |
| OLED SSD1306 0.96" 128×64 | Local display |

---

# 6. GPIO Mapping

| Perangkat | Fungsi | GPIO |
|---|---|---:|
| DHT11 | Data | GPIO4 |
| Sensor Jendela | Digital Input | GPIO33 |
| Relay lampu | Lampu | GPIO17 |
| Relay blower | Blower | GPIO12 |
| OLED | SDA | GPIO21 |
| OLED | SCL | GPIO22 |
| RTC DS3231 | SDA | GPIO21 |
| RTC DS3231 | SCL | GPIO22 |
| RFID MFRC522 | SCK | GPIO18 |
| RFID MFRC522 | MISO | GPIO19 |
| RFID MFRC522 | MOSI | GPIO23 |
| RFID MFRC522 | RST | GPIO27 |
| RFID MFRC522 | NSS / SS | GPIO5 |
| Servo SG90 | PWM | GPIO25 |
| HLW8012 | CF1 | GPIO13 |
| HLW8012 | SEL | GPIO26 |
| HLW8012 | CF | GPIO34 |
| Power Sensor Relay | Relay | GPIO32 |

---

## 6.1 Catatan GPIO

### GPIO34

GPIO34 merupakan input-only GPIO.

Penggunaan sebagai input CF dari HLW8012 sesuai dengan fungsi GPIO tersebut.

---

### GPIO12

GPIO12 merupakan salah satu strapping pin ESP32.

Karena digunakan untuk relay blower, hardware harus dipastikan tidak memberikan level tegangan pada GPIO12 saat boot yang menyebabkan ESP32 gagal melakukan startup.

Pin tetap dipertahankan sesuai desain hardware.

---

### GPIO5

GPIO5 juga berkaitan dengan proses boot pada beberapa varian ESP32.

RFID MFRC522 tidak boleh memberikan kondisi listrik yang mengganggu proses boot ESP32.

---

# 7. Communication Bus

## 7.1 I2C

OLED dan RTC menggunakan bus I2C yang sama.

```text
ESP32 GPIO21 SDA
        │
        ├──── OLED SSD1306
        │
        └──── RTC DS3231

ESP32 GPIO22 SCL
        │
        ├──── OLED SSD1306
        │
        └──── RTC DS3231
```

Perbedaan I2C address memungkinkan kedua perangkat berada pada bus yang sama.

---

## 7.2 SPI

RFID MFRC522 menggunakan SPI.

```text
SCK  -> GPIO18
MISO -> GPIO19
MOSI -> GPIO23
SS   -> GPIO5
RST  -> GPIO27
```

---

# 8. Functional Requirements — Sensor

## FR-SENSOR-001 — Temperature Monitoring

ESP32 harus membaca suhu dari DHT11.

Data:

```text
temperature
```

Unit:

```text
°C
```

Pembacaan sensor direkomendasikan setiap ±2 detik.

Nilai terbaru digunakan pada telemetry setiap 10 detik.

---

## FR-SENSOR-002 — Humidity Monitoring

ESP32 harus membaca kelembaban dari DHT11.

Data:

```text
humidity
```

Unit:

```text
%
```

---

## FR-SENSOR-004 — Window Monitoring

Sensor magnet jendela menghasilkan:

```text
OPEN
CLOSED
```

Debounce sensor juga harus diterapkan.

---

## FR-SENSOR-005 — Active Power Monitoring

HLW8012 digunakan untuk membaca:

```text
active_power
```

Unit:

```text
Watt
```

Parameter berikut tidak wajib ditampilkan pada versi pertama:

- voltage;
- current;
- power factor;
- energy/kWh.

Kalibrasi HLW8012 harus dilakukan sesuai karakteristik custom power sensor.

Konstanta resistor dan koreksi daya pada firmware saat ini masih nilai awal contoh, belum hasil kalibrasi rangkaian. Pengukuran Watt perlu diverifikasi dengan beban acuan sebelum dipakai sebagai hasil pembelajaran.

---

# 9. Power Sensor Relay

Custom HLW8012 memiliki built-in relay yang dikontrol melalui:

```text
GPIO32
```

Relay ini **bukan fitur yang dapat dikontrol pengguna**.

Saat ESP32 melakukan initialization:

```text
Power Sensor Relay = CONNECTED / ON
```

Tujuannya agar load selalu terhubung dengan jalur measurement.

State relay harus tetap aktif selama sistem beroperasi.

Level GPIO untuk menghubungkan jalur measurement dikonfigurasi sesuai karakteristik hardware:

```cpp
POWER_RELAY_ON_LEVEL
```

Firmware saat ini mengaktifkan relay tersebut saat boot dan tidak menyediakan fungsi untuk mematikannya.

---

# 10. Lampu dan Blower

Lampu dikontrol oleh relay di GPIO17; blower menggunakan relay di GPIO12. Status ON/OFF berasal dari state relay firmware, bukan sensor eksternal. Nomor channel fisik relay mengikuti wiring yang digunakan.

---

# 11. Relay Abstraction

Firmware tidak boleh mengasumsikan relay selalu Active HIGH.

Gunakan abstraction:

```cpp
const uint8_t RELAY_ON_LEVEL;
const uint8_t RELAY_OFF_LEVEL;
```

Contoh fungsi:

```cpp
void setLamp(bool state);
void setBlower(bool state);
```

Seluruh perubahan state relay harus melalui fungsi tersebut.

Nilai awal firmware saat ini adalah `RELAY_ON_LEVEL = HIGH` dan `RELAY_OFF_LEVEL = LOW` untuk dua relay lampu/blower. Polaritas perlu dicocokkan dengan modul yang digunakan sebelum upload.

---

# 12. Mode Lampu

Satu lampu memiliki konfigurasi `lamp.mode`: `0` = MANUAL, `1` = SCHEDULE. Mode dan jadwal aktif berasal dari Preferences dan dilaporkan sebagai `lamp_config` pada telemetry.

---

# 13. Manual Lamp Control

Pada mode MANUAL, dashboard menyediakan ON/OFF dan mengirim command `control` dengan field `lamp`. Pada mode SCHEDULE, ON/OFF manual diabaikan oleh firmware, termasuk OFF.

---

# 14. Schedule Lamp Control

Pada mode SCHEDULE, lampu mengikuti jadwal ON/OFF setiap hari dalam WIB menggunakan DS3231. Format waktu HH:MM (00:00–23:59); ON dan OFF harus berbeda. Default ON 18:00 dan OFF 06:00. Jadwal malam dapat melewati tengah malam.

---

# 15. Schedule Behavior

Schedule harus mendukung interval melewati tengah malam.

Contoh:

```text
ON  = 18:00
OFF = 06:00
```

berarti lampu aktif pada periode malam.

Saat:

- ESP32 boot;
- mode berubah menjadi SCHEDULE;
- schedule diperbarui;

firmware harus menentukan initial state berdasarkan waktu RTC saat itu.

Setelah initial evaluation, event schedule ON/OFF hanya dieksekusi sekali pada waktu yang sesuai.

---

# 16. Aturan Kontrol Lampu

| Kondisi | Perilaku |
|---|---|
| Boot + RTC valid + mode jadwal | Evaluasi interval jadwal |
| Mode/jadwal berubah | Evaluasi konfigurasi jadwal aktif |
| RTC baru valid | Evaluasi interval jadwal |
| Event Schedule ON/OFF | Ubah relay sesuai event |
| Manual ON/OFF dalam mode MANUAL | Terapkan |
| Manual ON/OFF dalam mode SCHEDULE | Abaikan |

Sensor jendela dan servo/RFID tidak mengubah lampu.

---

# 17. Blower Mode

Blower mempunyai dua mode.

```text
config_blower = 0 -> MANUAL
config_blower = 1 -> TEMPERATURE
```

---

# 18. Manual Blower Control

Jika:

```text
config_blower = 0
```

dashboard menampilkan:

```text
Blower

[ ON / OFF ]
```

---

# 19. Temperature Automation

Jika:

```text
config_blower = 1
```

blower dikontrol berdasarkan suhu DHT11.

Default threshold:

```text
33°C
```

Digunakan hysteresis:

```text
ON threshold  = 33°C
OFF threshold = 32°C
```

Rule:

```text
Temperature >= 33°C
        ↓
Blower ON
```

```text
Temperature <= 32°C
        ↓
Blower OFF
```

Ketika suhu berada pada:

```text
32°C < Temperature < 33°C
```

state blower tidak berubah.

---

## 19.1 Dashboard Blower Auto Mode

Dashboard menampilkan:

```text
Mode: Berdasarkan Suhu

Threshold saat ini:
33°C

[ Input Threshold ]
[ Simpan ]
```

Hysteresis default:

```text
1°C
```

Formula:

```text
blower_on  = threshold
blower_off = threshold - hysteresis
```

---

# 20. Door Lock

Servo SG90 digunakan sebagai simulasi kunci pintu.

State:

```text
LOCK   = 0°
UNLOCK = 90°
```

Firmware harus menyediakan abstraction:

```cpp
lockDoor();
unlockDoor();
toggleDoorLock();
```

---

# 21. RFID Authentication

RFID menggunakan MFRC522.

Daftar authorized UID:

```text
hardcoded di firmware
```

Contoh:

```cpp
const char* AUTHORIZED_UIDS[] = {
    "A1B2C3D4",
    "11223344"
};
```

Daftar pada sketch saat ini berisi string kosong, sehingga semua kartu ditolak sampai UID yang benar diisi oleh pengguna. UID ditulis dengan huruf heksadesimal kapital tanpa spasi.

RFID management melalui dashboard tidak termasuk scope MVP.

---

# 22. RFID Toggle Behavior

RFID menggunakan konsep toggle.

```text
Initial State
LOCK

Tap RFID Valid
↓
UNLOCK

Tap RFID Valid
↓
LOCK

Tap RFID Valid
↓
UNLOCK
```

State berikutnya selalu berdasarkan current lock state.

Jika dashboard mengubah state:

```text
Dashboard -> LOCK
```

maka tap RFID berikutnya:

```text
LOCK -> UNLOCK
```

---

# 23. Invalid RFID

Jika kartu tidak terdaftar:

```text
ACCESS DENIED
```

maka:

- servo tidak bergerak;
- door lock state tidak berubah;
- event tidak dikirim ke Antares;
- informasi hanya ditampilkan sementara pada OLED.

---

# 24. Dashboard Door Lock

Dashboard harus menyediakan:

```text
[ LOCK ]
[ UNLOCK ]
```

Kontrol kunci tidak memiliki mode manual/automatic terpisah.

---

# 25. RTC dan NTP

RTC menggunakan DS3231.

Sumber waktu utama setelah initialization:

```text
DS3231
```

Ketika ESP32 memperoleh koneksi Wi-Fi:

```text
Wi-Fi Connected
       ↓
Sync NTP
       ↓
Update DS3231
       ↓
RTC menjadi local time source
```

Timezone:

```text
WIB / UTC+7
```

Implementasi meminta NTP dengan `configTzTime("WIB-7", ...)` saat Wi-Fi tersambung dan setiap 6 jam selama terhubung. Firmware menunggu status `SNTP_SYNC_STATUS_COMPLETED` sebelum menyalin waktu WIB ke DS3231. Jadwal kemudian membaca waktu dari DS3231, termasuk saat offline.

---

# 26. RTC Offline Behavior

Jika internet terputus:

```text
ESP32
 ↓
RTC DS3231
 ↓
Schedule tetap bekerja
```

Dengan demikian schedule tidak bergantung pada koneksi internet secara terus-menerus.

Jika NTP gagal tetapi RTC masih memiliki waktu valid, sistem tetap menggunakan RTC.

Jika RTC invalid dan NTP tidak tersedia:

- manual control tetap bekerja secara lokal;
- temperature automation tetap bekerja;
- RFID tetap bekerja;
- schedule ditandai unavailable sampai waktu valid tersedia.

---

# 27. OLED Display

OLED menggunakan:

```text
SSD1306
128 × 64
I2C
```

Implementasi saat ini memakai alamat I2C `0x3C`, memperbarui tampilan setiap 500 ms, dan mencoba inisialisasi ulang setiap 30 detik bila OLED belum siap.

Informasi ditampilkan pada **satu page**.

Data utama:

```text
Temperature
Humidity
Active Power
RFID Status
```

Contoh:

```text
TEMP : 29.5 C
HUM  : 67 %
POWER: 125.4 W
RFID : READY
```

---

## 27.1 RFID OLED Message

Default:

```text
RFID : READY
```

RFID valid:

```text
RFID : AUTHORIZED
```

RFID invalid:

```text
RFID : DENIED
```

Pesan event dapat ditampilkan selama sekitar:

```text
2–3 detik
```

setelah itu kembali menjadi:

```text
READY
```

---

# 28. Wi-Fi

ESP32 terhubung ke Wi-Fi menggunakan konfigurasi:

```cpp
WIFI_SSID
WIFI_PASSWORD
```

`WIFI_SSID` dan `WIFI_PASSWORD` didefinisikan di `secrets.h` lokal, dengan fallback `secrets.example.h` untuk kompilasi tanpa kredensial. `secrets.h` diabaikan oleh Git. Jika SSID kosong, koneksi tidak dicoba. Ketika terputus, firmware mencoba menyambung ulang setiap 15 detik tanpa menunggu dalam loop koneksi.

Wi-Fi provisioning portal tidak termasuk scope.

---

# 29. Offline-First Behavior

Koneksi internet tidak boleh menjadi dependency untuk fungsi dasar Smart Home.

Jika:

```text
Wi-Fi / Antares DOWN
```

fungsi berikut tetap berjalan:

- DHT11;
- sensor jendela;
- power measurement;
- temperature → blower automation;
- RTC schedule;
- RFID;
- servo lock;
- OLED;
- konfigurasi terakhir dari Preferences.

Yang tidak tersedia:

- remote dashboard;
- cloud telemetry terbaru;
- remote command;
- perubahan remote configuration.

Ketika Wi-Fi terputus, firmware melewati request Antares sehingga pekerjaan lokal tetap berjalan. Satu request HTTPS yang sudah dimulai masih dapat menunda satu putaran `loop()` sampai request selesai atau gagal.

---

# 30. Antares Architecture

Gunakan satu Antares project dengan dua Antares Device.

Recommended structure:

```text
ANTARES PROJECT
smart-home-sma

├── smarthome-telemetry
└── smarthome-commands
```

Nama project dan device diletakkan sebagai konstanta di `secrets.h` atau `secrets.example.h` dan dapat diubah tanpa mengubah business logic.

Contoh:

```cpp
#define ANTARES_PROJECT "smart-home-sma"

#define DEVICE_TELEMETRY "smarthome-telemetry"
#define DEVICE_COMMANDS  "smarthome-commands"
```

---

# 31. Fungsi Antares Device

## `smarthome-telemetry`

Direction:

```text
ESP32 -> Antares -> Dashboard
```

Berisi kondisi aktual sistem, konfigurasi aktif, dan hasil command terakhir.

---

## `smarthome-commands`

Direction:

```text
Dashboard -> Antares -> ESP32
```

Berisi command one-shot bertipe `config` atau `control` dari pengguna. Konfigurasi yang diterima disimpan di Preferences, bukan dibaca ulang sebagai desired state dari Antares.

---

# 32. Communication Interval

## Telemetry

```text
ESP32 -> Antares
Interval: 10 detik
```

---

## ESP32 Command Polling

```text
ESP32 -> Antares GET
Interval: 5 detik
```

Satu timer membaca pesan terbaru dari `smarthome-commands`. `loop()` menjalankan maksimal satu request Antares per putaran, dengan urutan command lalu telemetry. Interval di atas berlaku pada koneksi normal; request HTTPS yang lambat dapat menggeser waktu polling berikutnya.

---

## Dashboard Polling

```text
Dashboard -> Antares
Interval: 5 detik
```

Dashboard mengambil telemetry terbaru, termasuk konfigurasi aktif dan hasil command terakhir.

---

# 33. Antares Transport

Gunakan HTTPS.

Endpoint berdasarkan library Antares ESP HTTP v1.6.0:

```text
https://platform.antares.id:8443
```

Firmware harus menggunakan:

```cpp
AntaresESPHTTP antares(ANTARES_ACCESS_KEY);
```

Upload:

```cpp
antares.add(...);
antares.send(ANTARES_PROJECT, DEVICE_TELEMETRY);
```

Retrieve:

```cpp
String raw = antares.getRaw(ANTARES_PROJECT, DEVICE_COMMANDS);
if (antares.getSuccess()) {
    // Parse dan validasi JSON raw di firmware.
}
```

`getRaw()` dipakai untuk device commands; `get()` serta `getString()` tidak dipakai oleh sketch saat ini.

---

# 34. Telemetry Data Contract

```json
{
  "ts": "2026-10-08T18:30:00+07:00",
  "sensor": {
    "temperature": 29.5,
    "humidity": 67,
    "active_power": 125.4
  },
  "contact": {
    "window": "CLOSED"
  },
  "actuator": {
    "lamp": "ON",
    "blower": "OFF",
    "door_lock": "LOCK"
  },
  "lamp_config": {
    "mode": 1,
    "on": "18:00",
    "off": "06:00"
  },
  "blower_config": {
    "mode": 0,
    "threshold": 33,
    "hysteresis": 1
  },
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

Payload memiliki maksimal dua tingkat nested object. `ts` kosong jika RTC belum valid. `contact` hanya memuat jendela; `actuator.door_lock` adalah state servo dan tidak menunjukkan kondisi fisik buka/tutup pintu. Kontrak lengkap dan contoh command tersedia di [docs/data-contract.md](docs/data-contract.md).

---

# 35. Telemetry Field Definition

| Field | Type | Description |
|---|---|---|
| `ts` | String | Timestamp WIB |
| `sensor.temperature` | Float | Suhu °C |
| `sensor.humidity` | Float | Humidity % |
| `sensor.active_power` | Float | Active Power Watt |
| `contact.window` | String | OPEN/CLOSED |
| `actuator.lamp` | String | ON/OFF |
| `actuator.blower` | String | ON/OFF |
| `actuator.door_lock` | String | LOCK/UNLOCK |
| `lamp_config.mode` | Integer | 0=MANUAL, 1=SCHEDULE |
| `lamp_config.on/off` | String | Jadwal HH:MM yang tersimpan |
| `blower_config.mode` | Integer | 0=MANUAL, 1=TEMPERATURE |
| `blower_config.threshold`, `blower_config.hysteresis` | Float | Parameter suhu yang tersimpan (°C) |
| `system.wifi` | String | CONNECTED/DISCONNECTED |
| `system.rssi` | Integer | Wi-Fi RSSI |
| `system.rtc_valid` | Integer | 0/1 |
| `system.uptime` | Integer | Seconds since boot |
| `system.last_command_id` | String | ID pesan `config`/`control` terakhir yang diproses |
| `system.last_command_result` | String | `applied`, `ignored`, atau `rejected`; kosong sebelum ada command |

---

# 36. Configuration Data Contract

Konfigurasi one-shot dikirim ke `smarthome-commands`; field dapat dikirim sebagian.

```json
{
  "id": "config-v2",
  "type": "config",
  "lamp": {
    "mode": 1,
    "on": "18:00",
    "off": "06:00"
  },
  "blower": {
    "mode": 0,
    "threshold": 33,
    "hysteresis": 1
  }
}
```

`lamp.mode`: 0=MANUAL, 1=SCHEDULE. `blower.mode`: 0=MANUAL, 1=TEMPERATURE.

---

# 37. Configuration Synchronization

Flow:

```text
Dashboard
   │
   │ Update Config
   ▼
Antares Commands Device
   │
   │ Poll every 5s
   ▼
ESP32
   │
   ├── Validate
   ├── Apply
   └── Save Preferences
```

Firmware hanya menulis ke Preferences jika value benar-benar berubah untuk mengurangi flash write.

Payload config dapat memuat sebagian field. Firmware mengabaikan nilai yang tidak valid, menerapkan field valid yang berubah, lalu langsung mengevaluasi lampu berjadwal atau blower otomatis yang konfigurasinya berubah. Jika tidak ada field valid, hasilnya `rejected`; field valid dengan nilai yang sudah sama tetap `applied` tanpa penulisan ulang flash.

Dashboard membaca kembali nilai konfigurasi aktif dari `smarthome-telemetry`, bukan dari pesan `config` terakhir di `smarthome-commands`.

---

# 38. Control Data Contract

```json
{
  "id": "control-v2",
  "type": "control",
  "lamp": "NONE",
  "blower": "NONE",
  "door_lock": "LOCK"
}
```

Field yang tidak dikirim atau bernilai null dianggap NONE. Lampu/blower: NONE, ON, OFF. Kunci: NONE, LOCK, UNLOCK. Field lain ditolak; tidak tersedia field kontak pada command.

---

# 39. Command ID

`id` harus unik untuk setiap user action, baik `config` maupun `control`. ID adalah string tidak kosong dengan panjang maksimal 63 karakter.

Dashboard dapat menggunakan:

```javascript
crypto.randomUUID()
```

ESP32 menyimpan:

```text
last_command_id
last_cmd_result
```

Command diproses hanya ketika:

```text
incoming_id != last_command_id
```

Setelah pesan dengan ID valid diproses:

```text
last_command_id = incoming_id
```

dan disimpan ke Preferences.

Tujuannya mencegah pesan yang sama dieksekusi setiap polling 5 detik atau setelah restart. Pesan dengan ID valid tetapi `type`/aksi tidak valid disimpan dengan hasil `rejected`; pesan tanpa ID valid atau JSON rusak diabaikan karena tidak dapat diakui lewat telemetry.

Hasil `applied` berarti konfigurasi valid diterima atau minimal satu aksi kontrol diizinkan. Hasil `ignored` berarti aksi kontrol valid tetapi tidak ada yang diizinkan oleh mode saat ini (termasuk semua aksi `NONE`). Hasil `rejected` berarti tipe/payload tidak valid. Hasil dan ID terakhir dikirim pada `system` di telemetry.

Antares `/la` hanya mengembalikan pesan terbaru. Dashboard harus menunggu `system.last_command_id` cocok dengan ID yang dikirim sebelum mengirim pesan berikutnya; beberapa dashboard yang mengirim bersamaan tetap dapat saling menimpa sebelum ESP32 melakukan polling.

---

# 40. Command Processing Rules

Validasi seluruh nama field dilakukan sebelum perubahan apa pun. Command config hanya memiliki `id`, `type`, `lamp`, `blower`; command control menambahkan `door_lock`. Konfigurasi lampu hanya memiliki `mode`, `on`, `off`; blower hanya `mode`, `threshold`, `hysteresis`. Field asing pada root maupun nested config menghasilkan `rejected` tanpa perubahan relay atau konfigurasi.

Lampu menerima ON/OFF hanya dalam MANUAL; blower menerima ON/OFF hanya dalam MANUAL. Servo menerima LOCK/UNLOCK pada semua mode. Semua NONE atau aksi yang dilarang mode menghasilkan `ignored`; setidaknya satu aksi yang diizinkan menghasilkan `applied`.

Config dikenal yang bernilai invalid difilter oleh firmware seperti sebelumnya; field valid dapat diterapkan. Proksi dashboard memvalidasi nilai sebelum mengirim command.

---

# 41. Preferences dan Migrasi v2

Namespace NVS tetap `smart-home`.

| Key | Default |
|---|---|
| `lamp_mode` | `0` |
| `lamp_on` | `18:00` |
| `lamp_off` | `06:00` |
| `blower_mode` | `0` |
| `blower_thr` | `33.0` |
| `blower_hyst` | `1.0` |
| `last_command_id` | string kosong |
| `last_cmd_result` | string kosong |

Saat key baru belum ada, firmware membaca `lamp1_mode`, `lamp1_on`, dan `lamp1_off` untuk migrasi. Mode/jadwal valid disimpan ke key baru; nilai invalid menggunakan default. Key baru selalu diutamakan, dan key yang belum berhasil tersimpan dicoba pada boot berikutnya. Key lampu kedua tidak dibaca; key lama tidak dihapus. Tidak ada reset NVS, sehingga konfigurasi blower dan riwayat command tetap tersedia. UID RFID tetap disimpan di firmware.

---

# 42. Safe Startup State

Saat ESP32 boot:

```text
Lampu   = OFF
Blower  = OFF
Door    = LOCK
Power Sensor Relay = ON
```

Kemudian:

```text
Load Preferences
      ↓
Jika RTC valid, evaluasi jadwal saat boot
      ↓
Mulai koneksi Wi-Fi tanpa menunggu
      ↓
Saat Wi-Fi terhubung, mulai polling/telemetry Antares dan minta NTP
      ↓
Setelah NTP berhasil, perbarui RTC
```

---

# 43. Firmware Initialization Flow

```text
BOOT
 │
 ├── Initialize Serial
 │
 ├── Configure GPIO
 │
 ├── Set safe actuator state
 │
 ├── Enable power sensor relay
 │
 ├── Initialize Servo (LOCK)
 │
 ├── Initialize window dan DHT11
 │
 ├── Initialize I2C
 │     ├── OLED
 │     └── RTC
 │
 ├── Initialize SPI
 │     └── MFRC522
 │
 ├── Initialize HLW8012
 │
 ├── Load persistent configuration
 │
 ├── Evaluate schedule jika RTC valid
 │
 ├── Mulai koneksi Wi-Fi tanpa menunggu
 │
 └── ENTER MAIN LOOP
```

Sinkronisasi NTP dan pembaruan DS3231 dilakukan dari `loop()` setelah Wi-Fi tersambung. RTC tetap dipakai bila NTP belum tersedia.

---

# 44. Firmware Runtime Architecture

Firmware saat ini memakai satu `setup()` dan satu `loop()` dengan timer `millis()`. Tidak ada task FreeRTOS atau antrean pesan. Loop membaca kontak dan RFID, lalu menjalankan pekerjaan yang sudah mencapai intervalnya:

| Pekerjaan | Interval |
|---|---:|
| Debounce kontak jendela | 50 ms stabil |
| Update OLED | 500 ms |
| Baca active power dan periksa jadwal RTC | 1 detik |
| Baca DHT11 | 2 detik |
| Poll `smarthome-commands` | 5 detik |
| Kirim telemetry | 10 detik |
| Coba sambung ulang Wi-Fi | 15 detik |
| Minta sinkronisasi ulang NTP | 6 jam saat Wi-Fi terhubung |

Loop menjalankan maksimal satu request Antares per putaran. Library Antares HTTP v1.6.0 melakukan request HTTPS secara sinkron; bila server lambat, pembacaan RFID dan otomasi lokal dapat tertunda sampai request itu selesai. Saat Wi-Fi terputus, request Antares dilewati dan otomasi lokal tetap berjalan.

---

# 45. Struktur Source Firmware Saat Ini

Firmware MVP memakai satu sketch utama `smart-home-sma.ino`, mengikuti gaya proyek `smart-room-sma.ino`. Kredensial dan nama device berada di `secrets.h` lokal; `secrets.example.h` menyediakan nilai contoh. Sketch berisi mapping GPIO, konstanta hardware, interval, objek library, state, konfigurasi, dan fungsi aktuator, jadwal, sensor, RFID, OLED, Preferences, Wi-Fi/NTP, Antares, lalu `setup()` dan `loop()`.

Pemisahan fungsi tetap dipakai agar siswa dapat mengikuti alur tiap komponen; hanya kredensial yang berada di header terpisah.

---

# 46. Automation Engine

```text
RTC -> Lampu
Temperature -> Blower
RFID -> Servo kunci pintu
```

Otomasi lokal tidak bergantung pada dashboard.

---

# 47. Automation Priority

Lampu MANUAL mengikuti dashboard; lampu SCHEDULE mengikuti RTC. Blower MANUAL mengikuti dashboard; blower TEMPERATURE mengikuti DHT11 dan hysteresis. RFID dan command kunci mengubah servo tanpa mengubah lampu.

---

# 48. Dashboard Architecture

Dashboard menggunakan React + Vite. Browser berkomunikasi dengan proksi Node pada origin yang sama, karena Antares menolak preflight CORS browser.

```text
Browser React -> /api/telemetry dan /api/commands -> Proksi Node -> Antares HTTPS
```

Node menyajikan hasil build dari `dist/`; Vite meneruskan `/api` ke proksi pada port 3001 saat development. Tidak ada database atau login.

---

# 49. Security Limitation

Access key disimpan dalam `.env` dan dibaca hanya oleh proses Node; browser tidak menerima access key. Karena belum ada login, akses LAN dibatasi ke jaringan tepercaya. Produk merupakan prototipe pembelajaran.

---

# 50. Dashboard Main Page

Satu halaman memuat status koneksi, waktu pembaruan, suhu, kelembaban, daya aktif, kontak jendela, serta kontrol Lampu/Blower/Kunci pintu. Grid perangkat menggunakan tiga kolom pada desktop (>950px), dua kolom pada tablet (701–950px) dengan kartu terakhir memenuhi baris, dan satu kolom pada ponsel (≤700px). Kontak jendela memenuhi satu baris.

---

# 51. Dashboard Sensor Cards

Dashboard menampilkan:

### Temperature

```text
29.5 °C
```

### Humidity

```text
67 %
```

### Active Power

```text
125.4 W
```

### Window

```text
OPEN / CLOSED
```

---

# 52. Lamp Dashboard

Satu kartu **Lampu** menampilkan ON/OFF dan mode Manual/Jadwal. Manual menyediakan Nyalakan/Matikan; Jadwal menyediakan waktu nyala, waktu mati, dan Simpan jadwal tanpa override.

Kontrol lampu hanya aktif jika telemetry online, `actuator.lamp` bernilai ON/OFF, `lamp_config.mode` bernilai 0/1, dan jadwal valid. Telemetry kontrak lama tidak dipetakan otomatis ke kontrak baru. GPIO yang ditampilkan mengikuti firmware: GPIO17.

---

# 53. Blower Dashboard

Manual:

```text
Blower

Status:
OFF

Mode:
Manual

[ ON / OFF ]
```

Temperature:

```text
Blower

Status:
ON

Mode:
Temperature

Threshold:
33°C

[ 33 ]
[ Simpan ]
```

Dashboard juga dapat menampilkan:

```text
ON >= 33°C
OFF <= 32°C
```

agar konsep hysteresis terlihat oleh siswa.

---

# 54. Door Lock Dashboard

```text
Door Lock

Status:
LOCKED

[ LOCK ]
[ UNLOCK ]
```

Status dashboard berasal dari telemetry ESP32, bukan hanya dari button terakhir yang ditekan.

---

# 55. Device Connection Status

Dashboard menentukan status berdasarkan waktu telemetry terakhir.

Contoh:

```text
Last telemetry <= 20 seconds
ONLINE

Last telemetry > 20 seconds
STALE / OFFLINE
```

UI tidak boleh menganggap device online hanya karena browser berhasil mengakses Antares.

---

# 56. Dashboard Polling

Setiap:

```text
5 detik
```

dashboard mengambil latest telemetry. Konfigurasi aktif, kondisi aktuator, dan hasil command dibaca dari payload tersebut.

Dashboard mengirim satu command pada satu waktu dan menunggu `system.last_command_id` sama dengan `id` yang dikirim sebelum mengirim command berikutnya. Jika belum cocok, tampilkan status menunggu; jangan menyimpulkan aksi berhasil hanya dari respons POST.

Polling harus menggunakan `setInterval()` atau equivalent non-blocking mechanism.

---

# 57. Dashboard Optimistic UI

Untuk command actuator, UI sebaiknya tidak langsung menganggap perubahan berhasil.

Flow:

```text
User klik ON
     ↓
Command dikirim
     ↓
UI: "Updating..."
     ↓
ESP32 menerima command
     ↓
ESP32 mengubah relay
     ↓
Telemetry berikutnya diterima
     ↓
UI menunjukkan ON
```

Telemetry menjadi source of truth untuk actual device state.

---

# 58. Antares REST API — Dashboard

Berdasarkan library Antares yang digunakan, base URL:

```text
https://platform.antares.id:8443
```

Store data:

```http
POST /~/antares-cse/antares-id/{project}/{device}
```

Latest data:

```http
GET /~/antares-cse/antares-id/{project}/{device}/la
```

Required header:

```http
X-M2M-Origin: {ACCESS_KEY}
Content-Type: application/json;ty=4
Accept: application/json
```

---

# 59. Antares POST Format

Contoh:

```json
{
  "m2m:cin": {
    "con": "{\"id\":\"123\",\"type\":\"config\",\"lamp\":{\"mode\":1,\"on\":\"18:00\",\"off\":\"06:00\"}}"
  }
}
```

Nilai `con` berupa JSON yang disimpan sebagai string sesuai format yang digunakan Antares/library.

---

# 60. Antares GET Response

Dashboard harus mengambil field:

```text
m2m:cin.con
```

kemudian melakukan JSON parse kedua.

Pseudo-code:

```javascript
const response = await fetch(url, options);
const data = await response.json();

const content = data["m2m:cin"]["con"];
const payload = JSON.parse(content);
```

---

# 61. Dashboard Project Structure

```text
smart-home-sma-dashboard/
├── src/App.jsx
├── src/styles.css
├── src/api/antares.js
├── src/api/dashboard.js
├── src/domain/dashboard.js
├── server/app.js
├── server/index.js
├── .env.example
├── package.json
└── vite.config.js
```

Unit/integration tests berada di file `*.test.js` dan `*.test.jsx` di sebelah source terkait.

---

# 62. Dashboard Configuration

Salin `.env.example` menjadi `.env` dan isi ANTARES_ACCESS_KEY, ANTARES_PROJECT, ANTARES_TELEMETRY_DEVICE, ANTARES_COMMANDS_DEVICE. Nama project/device harus sama dengan `secrets.h` firmware. Node membaca `.env`; konfigurasi access key di browser tidak digunakan. Default proksi HOST 127.0.0.1 dan PORT 3001.

---

# 63. Config Validation

Firmware harus melakukan validation terhadap konfigurasi dari cloud.

### Mode

Valid:

```text
0
1
```

Invalid value:

```text
ignore
```

---

### Schedule

Format valid:

```text
HH:MM
```

Range:

```text
HH = 00–23
MM = 00–59
```

Waktu ON dan OFF harus berbeda. Firmware menerima pembaruan sebagian field, tetapi hanya menerapkan pasangan jadwal yang tetap valid.

---

### Temperature Threshold

Recommended valid range:

```text
10°C – 60°C
```

Nilai di luar range harus ditolak.

Hysteresis firmware saat ini harus lebih besar dari `0°C`, maksimal `10°C`, dan lebih kecil dari threshold. Nilai yang gagal validasi diabaikan.

---

# 64. Sensor Error Handling

## DHT11 Failure

Jika pembacaan menghasilkan:

```text
NaN
```

firmware:

- tidak menggunakan nilai tersebut untuk blower automation;
- mempertahankan nilai terakhir yang valid untuk display jika diperlukan;
- blower tidak boleh berubah berdasarkan data invalid.

Telemetry firmware saat ini mengirim nilai terakhir yang valid. Jika belum pernah ada pembacaan valid sejak boot, nilai suhu dan kelembaban pada telemetry berupa `0.0`; OLED menampilkan `--.-` sampai ada pembacaan valid.

---

# 65. Antares Failure

Jika request Antares gagal:

```text
do not reset device
```

Firmware harus:

- melanjutkan local operation;
- mencoba kembali pada interval berikutnya;
- tidak melakukan blocking retry loop;
- mencatat error melalui Serial Monitor.

Implementasi saat ini tidak melakukan retry terus-menerus dalam satu pemanggilan. Poll berikutnya dicoba pada interval berikutnya. Namun, satu pemanggilan `getRaw()` atau `send()` tetap sinkron dan dapat menunda `loop()` ketika koneksi/server lambat.

---

# 66. Wi-Fi Reconnection

Jika Wi-Fi disconnected:

```text
continue local automation
```

Firmware mencoba reconnect secara periodik.

Interval reconnect firmware saat ini adalah 15 detik. `WiFi.begin()` dipanggil tanpa loop tunggu koneksi.

Setelah Wi-Fi kembali:

```text
Reconnect
   ↓
NTP Sync
   ↓
Update RTC
   ↓
Resume Antares Communication
```

---

# 67. Preferences Failure Strategy

Jika namespace NVS gagal dibuka, firmware memakai default tanpa penulisan Preferences: lampu MANUAL, blower MANUAL, jadwal 18:00–06:00, threshold 33°C, hysteresis 1°C. Jika bacaan mode/jadwal tidak valid, firmware memakai nilai default yang sesuai.

---

# 68. Library Dependencies

## DHT11

Adafruit DHT Sensor Library:

```text
https://github.com/adafruit/DHT-sensor-library
```

---

## HLW8012

```text
https://github.com/xoseperez/hlw8012
```

---

## RFID MFRC522

```text
https://github.com/miguelbalboa/rfid
```

Library MFRC522 v1.4.11 yang terpasang pada lingkungan pengembangan memerlukan koreksi dua perbandingan `backLen > 0` menjadi `*backLen > 0` dalam `MFRC522Extended.cpp` agar dapat dikompilasi dengan ESP32 core 3.3.10.

---

## RTC

Adafruit RTClib:

```text
https://github.com/adafruit/RTClib
```

---

## OLED

Adafruit SSD1306:

```text
https://github.com/adafruit/Adafruit_SSD1306
```

Dependency:

```text
Adafruit GFX Library
```

---

## Servo

Firmware menggunakan `ESP32Servo` untuk SG90 pada GPIO25.

---

## Antares

Library:

```text
Antares ESP HTTP v1.6.0
```

Library Antares dipasang melalui lingkungan Arduino; source library tidak disertakan dalam repo firmware.

API utama yang digunakan:

```cpp
AntaresESPHTTP()

add()

send()

getRaw()

getSuccess()
setDebug()
```

Firmware memakai ArduinoJson untuk memvalidasi dan membaca JSON hasil `getRaw()`.

---

## Preferences

ESP32 built-in library:

```cpp
#include <Preferences.h>
```

---

## Wi-Fi

```cpp
#include <WiFi.h>
```

Sinkronisasi waktu menggunakan `configTzTime()` dan status SNTP dari `<esp_sntp.h>` pada core ESP32.

---

# 69. Important Antares Library Implementation Note

`getRaw()` mengembalikan isi `m2m:cin.con` untuk parsing command; telemetry dibentuk melalui `add()` dengan maksimal dua tingkat object, lalu dikirim melalui `send()`. `setDebug(true)` menampilkan respons HTTP di Serial Monitor.

Verifikasi seluruh field telemetry setelah pergantian versi library. Lingkungan pengembangan saat revisi memakai ArduinoJson 7.4.2; dokumentasi v2 tidak mengasumsikan modifikasi kapasitas library eksternal.

---

# 70. Model State Firmware Saat Ini

```cpp
float temperature, humidity, activePower;
bool dhtReadingValid;
bool windowOpen, lampOn, blowerOn, doorLocked;
bool rtcReady, rtcValid, oledReady, preferencesReady;
byte lampMode, blowerMode;
String lampOnTime, lampOffTime;
float blowerThreshold, blowerHysteresis;
String lastCommandId, lastCommandResult;
```

Satu `ContactState` digunakan untuk debounce jendela. Status Wi-Fi dibaca dari WiFi.status().

---

# 71. Main Loop Saat Ini

Pseudo-code:

```cpp
void loop() {
    readWindow();
    checkRfid();
    readDht();
    readPower();
    checkSchedule();
    updateOLED();
    if (!oledReady && millis() - lastOledAttemptAt >= OLED_RETRY_INTERVAL) initOLED();
    checkWiFi();
    checkNtp();

    // Saat Wi-Fi terhubung, lakukan maksimal satu request yang sudah jatuh tempo:
    // pollCommand() atau publishTelemetry().
    delay(1);
}
```

`readDht()`, `readPower()`, `checkSchedule()`, dan `updateOLED()` memeriksa intervalnya sendiri dengan `millis()`. `readWindow()` menangani perubahan status jendela; `checkRfid()` langsung menangani kartu valid/tidak valid. Request Antares adalah bagian sinkron yang dapat menunda putaran berikutnya.

---

# 72. Acceptance Criteria — Sensor

- Perubahan suhu/kelembaban terlihat pada OLED dan dashboard.
- Telemetry diperbarui sekitar 10–15 detik pada koneksi normal.
- Jendela melaporkan OPEN/CLOSED setelah debounce 50 ms.
- HLW8012 melaporkan daya aktif dalam Watt.
- Telemetry kontak hanya memuat jendela; servo kunci tetap memiliki state LOCK/UNLOCK.

---

# 73. Acceptance Criteria — Lampu

- Satu lampu GPIO17 dapat ON/OFF pada mode MANUAL.
- Mode SCHEDULE menyembunyikan kontrol manual dan tidak menyediakan override.
- Command ON/OFF pada SCHEDULE menghasilkan ignored tanpa perubahan lampu.
- Jadwal DS3231 bekerja setiap hari, termasuk lintas tengah malam dan saat offline.
- Boot dengan RTC valid mengevaluasi interval aktif; RTC invalid menunda otomasi jadwal.
- Perubahan jendela, servo, atau RFID tidak menyalakan lampu.

---

# 74. Acceptance Criteria — Blower

### AC-014

Blower dapat dikontrol dari dashboard ketika mode Manual.

### AC-015

Pada Temperature Mode:

```text
temperature >= threshold
```

mengaktifkan blower.

### AC-016

Dengan threshold 33°C:

```text
temperature <= 32°C
```

mematikan blower.

### AC-017

Blower tidak oscillate ON/OFF ketika suhu berada di sekitar threshold.

---

# 75. Acceptance Criteria — RFID

### AC-018

Authorized RFID:

```text
LOCK -> UNLOCK
```

### AC-019

Tap authorized RFID berikutnya:

```text
UNLOCK -> LOCK
```

### AC-020

Unauthorized RFID tidak mengubah servo.

### AC-021

Unauthorized RFID menampilkan:

```text
RFID: DENIED
```

pada OLED.

---

# 76. Acceptance Criteria — Dashboard Lock

### AC-022

Button LOCK menghasilkan:

```text
Servo -> 0°
```

### AC-023

Button UNLOCK menghasilkan:

```text
Servo -> 90°
```

### AC-024

Dashboard menampilkan actual lock state berdasarkan telemetry.

---

# 77. Acceptance Criteria — Offline

### AC-025

Ketika Wi-Fi dimatikan:

- device tidak restart;
- OLED tetap bekerja;
- RFID tetap bekerja;
- RTC schedule tetap bekerja;
- temperature automation tetap bekerja;

### AC-026

Ketika Wi-Fi kembali:

- device reconnect;
- NTP resync dilakukan;
- telemetry kembali terkirim;
- remote command kembali dapat digunakan.

---

# 78. Acceptance Criteria — Persistent Configuration

### AC-027

User mengubah:

```text
threshold = 35°C
```

kemudian device restart.

Setelah restart:

```text
threshold tetap 35°C
```

### AC-028

Schedule lampu tetap tersimpan setelah restart.

### AC-029

Mode lampu/blower tetap tersimpan setelah restart.

---

# 79. Test Scenarios

## TS-01 — Manual Lamp

```text
Given Lampu dalam Manual Mode
When user menekan ON
Then Lampu menyala
And telemetry menunjukkan ON
```

---

## TS-02 — Scheduled Lamp

```text
Given:
ON  = 18:00
OFF = 06:00

When RTC menunjukkan 18:00
Then lampu menyala
```

---

## TS-03 — Kontrol pada Jadwal

Dengan lampu SCHEDULE dan ON, kirim control lamp OFF; hasil ignored dan lampu tetap ON. Ulangi ON saat lampu OFF; hasil ignored.

## TS-04 — Migrasi Preferences

Mulai dengan key lama valid: mode 1, ON 22:00, OFF 05:00. Boot v2 mempertahankan nilai dan membuat key baru. Ubah jadwal v2, lalu restart: nilai baru diutamakan. Key konfigurasi blower dan ID command tetap ada. Ulangi dengan nilai lama invalid untuk memeriksa default.

## TS-05 — Penolakan Kontrak Lama

Command yang memakai lamp1/lamp2, field kontak, atau field nested asing ditolak. Payload campuran lamp valid + field lama menghasilkan rejected tanpa mengubah lampu, blower, servo, atau konfigurasi.

---

## TS-06 — Blower Hysteresis

```text
Threshold = 33°C

Temperature = 33.2°C
-> Blower ON

Temperature = 32.6°C
-> Blower tetap ON

Temperature = 31.9°C
-> Blower OFF
```

---

## TS-07 — RFID Toggle

```text
Initial = LOCK

Tap #1
-> UNLOCK

Tap #2
-> LOCK

Tap #3
-> UNLOCK
```

---

## TS-08 — Invalid RFID

```text
Scan invalid UID

Expected:
Servo tidak bergerak
OLED = DENIED
```

---

## TS-09 — Internet Failure

```text
Disconnect Wi-Fi

Expected:
Local automation tetap berjalan
```

---

## TS-10 — RTC Offline Schedule

```text
Sync RTC
Disconnect Wi-Fi

Wait until scheduled event

Expected:
Schedule tetap dieksekusi
```

---

## TS-11 — Persistence

```text
Kirim {"id":"set-threshold-35","type":"config","blower":{"threshold":35}}

Tunggu telemetry: blower_config.threshold = 35

Restart ESP32

Expected:
threshold = 35
telemetry tetap menunjukkan blower_config.threshold = 35
```

---

## TS-12 — Duplicate Cloud Command

```text
Commands payload = {"id":"ABC123","type":"control","lamp":"ON"}

ESP32 poll #1
-> Process

ESP32 poll #2
-> Ignore

ESP32 poll #3
-> Ignore

Restart ESP32 lalu poll lagi
-> Ignore; last_command_id dan hasil terakhir tetap ada di Preferences
```

Ulangi dengan `type:"config"`. Pesan dengan ID valid tetapi payload tidak valid harus menghasilkan `rejected` satu kali dan tidak diproses ulang.

---

# 80. Serial Logging

Firmware harus menyediakan logging yang mudah dipahami siswa.

`debugLog()` menambahkan waktu sejak boot dan tag pada setiap pesan aplikasi. `SERIAL_DEBUG` mengatur log aplikasi; `ANTARES_HTTP_DEBUG` mengatur log HTTP dari library. Password Wi-Fi dan access key tidak dicetak oleh `debugLog()`.

Contoh:

```text
[       0 ms] [BOOT] Smart Home SMA mulai
[    2050 ms] [DHT] Suhu=29.5 C, hum=67.0 %
[    3300 ms] [WINDOW] OPEN
[    4000 ms] [LAMP] ON
[    4500 ms] [RFID] UID=A1B2C3D4
[    4500 ms] [LOCK] UNLOCK
[   10200 ms] [ANTARES] Request telemetry selesai
```

Pesan akhir telemetry menandakan pemanggilan `send()` selesai, bukan jaminan server menerima payload; kode responsnya terlihat pada log library Antares.

---

# 81. Non-Functional Requirements

## NFR-001 — Responsiveness

Target produk: otomasi lokal tetap responsif walau cloud lambat. Firmware saat ini memprioritaskan kode satu `loop()` yang mudah dipelajari dan melewati Antares saat Wi-Fi terputus. Karena request HTTPS sinkron, target ini belum sepenuhnya terpenuhi ketika server Antares lambat; keterlambatan RFID dan otomasi lokal perlu diukur pada pengujian integrasi.

---

## NFR-002 — Reliability

Kegagalan Wi-Fi atau Antares tidak boleh menyebabkan device restart terus-menerus.

---

## NFR-003 — Maintainability

GPIO dan default konfigurasi ditempatkan pada bagian awal `smart-home-sma.ino`. Kredensial serta nama project/device ditempatkan di `secrets.h` lokal, dengan contoh yang dapat dibagikan di `secrets.example.h`.

---

## NFR-004 — Readability

Firmware ditujukan untuk pembelajaran SMA sehingga:

- nama fungsi harus jelas;
- hindari abstraksi berlebihan;
- hindari macro kompleks;
- gunakan komentar pada bagian penting;
- pisahkan sensor, actuator, communication, dan automation.

Implementasi saat ini memisahkan tanggung jawab tersebut melalui fungsi bernama jelas dalam satu sketch, dengan komentar singkat berbahasa Indonesia.

---

## NFR-005 — Dashboard Responsive

Dashboard minimal dapat digunakan melalui:

- laptop;
- tablet.

Layout ponsel menggunakan satu kolom dengan kontrol tetap dapat digunakan.

---

# 82. Recommended Development Order

## Phase 1 — Hardware Bring-up

Implementasikan:

1. GPIO;
2. relay;
3. DHT11;
4. magnetic sensor;
5. OLED;
6. RTC;
7. RFID;
8. servo;
9. HLW8012.

---

## Phase 2 — Local Smart Home

Implementasikan tanpa Antares:

1. manual relay test;
2. scheduled lamp control;
3. blower temperature automation;
4. schedule automation;
5. RFID lock;
6. OLED;
7. Preferences.

Target:

> Smart Home dapat bekerja sepenuhnya secara lokal.

---

## Phase 3 — Connectivity

Implementasikan:

1. Wi-Fi;
2. NTP;
3. RTC synchronization;
4. Antares telemetry.

---

## Phase 4 — Remote Configuration

Implementasikan:

```text
smarthome-commands, type `config`
```

dan sinkronisasi ke Preferences.

---

## Phase 5 — Remote Control

Implementasikan:

```text
smarthome-commands, type `control`
```

termasuk `id` bersama untuk kedua tipe dan hasil command pada telemetry.

---

## Phase 6 — Dashboard

Implementasikan:

1. telemetry cards;
2. lamp controls;
3. schedule configuration;
4. blower configuration;
5. door lock;
6. device online status.

---

## Phase 7 — Integration Testing

Test:

```text
Hardware
    +
Firmware
    +
Wi-Fi
    +
Antares
    +
Dashboard
```

sebagai satu sistem.

---

# 83. Definition of Done

Project dianggap selesai apabila:

- seluruh sensor dapat dibaca;
- Lampu dapat dikontrol;
- blower dapat dikontrol;
- door lock dapat dikontrol;
- RFID toggle bekerja;
- invalid RFID ditolak;
- blower temperature automation bekerja;
- hysteresis bekerja;
- RTC schedule bekerja;
- NTP sync bekerja;
- offline automation bekerja;
- configuration tersimpan di Preferences;
- telemetry dikirim setiap 10 detik;
- command dipoll setiap 5 detik;
- dashboard refresh setiap 5 detik;
- duplicate command tidak dieksekusi ulang;
- dashboard menunjukkan actual device state;
- satu halaman dashboard dapat digunakan tanpa login;
- seluruh acceptance criteria utama telah lulus.

Daftar ini adalah target produk lengkap, termasuk dashboard. Kompilasi firmware ESP32 telah berhasil, tetapi keberhasilan hardware, komunikasi Antares nyata, dashboard, dan skenario pengujian di atas masih harus diverifikasi pada perangkat dan layanan yang digunakan.

---

# 84. Final System Flow

```text
Dashboard React
      | same-origin /api
Proksi Node (.env)
      | HTTPS
Antares: smarthome-telemetry / smarthome-commands
      | Wi-Fi HTTPS
ESP32
  +-- DHT11 -> sensor telemetry dan blower suhu
  +-- Reed jendela -> contact.window
  +-- HLW8012 -> daya aktif
  +-- DS3231/NTP WIB -> jadwal satu lampu GPIO17
  +-- RFID -> servo kunci GPIO25
  +-- OLED -> suhu, kelembaban, daya, pesan RFID
```

---

# 85. Product Principle

Sistem harus mengikuti prinsip:

> **Cloud digunakan untuk monitoring, configuration, dan remote control.
> ESP32 tetap bertanggung jawab terhadap fungsi Smart Home lokal dan automation.**

Dengan pendekatan ini siswa dapat memahami bahwa perangkat IoT tidak hanya sekadar mengirim data ke internet, tetapi juga memiliki local intelligence yang tetap dapat bekerja ketika koneksi cloud tidak tersedia.

---

**End of PRD — Smart Home IoT SMA v2.0**
