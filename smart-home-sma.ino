// Smart Home SMA
#include <WiFi.h>
#include <AntaresESPHTTP.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <DHT.h>
#include <HLW8012.h>
#include <MFRC522.h>
#include <RTClib.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_sntp.h>
#include <math.h>
#include <time.h>

// Salin secrets.example.h ke secrets.h lalu isi kredensial sebelum upload.
// File contoh menjaga sketch tetap bisa dikompilasi tanpa kredensial lokal.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

// Nomor GPIO mengikuti PRD.
#define DHT_PIN 4
#define WINDOW_PIN 33
#define DOOR_PIN 35       // Perlu resistor pull-up eksternal
#define LAMP1_PIN 16
#define LAMP2_PIN 17
#define BLOWER_PIN 12     // Periksa level GPIO12 saat ESP32 boot
#define OLED_SDA 21
#define OLED_SCL 22
#define RFID_SS 5         // Periksa level GPIO5 saat ESP32 boot
#define RFID_RST 27
#define SERVO_PIN 25
#define HLW_CF1 13
#define HLW_SEL 26
#define HLW_CF 34         // GPIO34 hanya input
#define POWER_RELAY_PIN 32

// Sesuaikan polaritas dengan rangkaian yang dipakai.
constexpr byte RELAY_ON_LEVEL = LOW;
constexpr byte RELAY_OFF_LEVEL = HIGH;
constexpr byte POWER_RELAY_ON_LEVEL = HIGH;
constexpr byte DOOR_OPEN_LEVEL = HIGH;
constexpr byte WINDOW_OPEN_LEVEL = HIGH;
constexpr byte HLW_CURRENT_LEVEL = HIGH;

// Nilai HLW ini contoh awal. Kalibrasi dengan resistor PCB dan beban acuan.
constexpr double HLW_CURRENT_RESISTOR = 0.001;
constexpr double HLW_VOLTAGE_UPSTREAM = 2350000.0;
constexpr double HLW_VOLTAGE_DOWNSTREAM = 1000.0;
constexpr double HLW_POWER_CORRECTION = 1.0;

// Isi UID kapital tanpa spasi, misalnya "A1B2C3D4". Kosong = semua kartu ditolak.
const char *const AUTHORIZED_UIDS[] = {""};

// Semua pekerjaan berulang memakai millis() agar loop() tetap responsif. Interval dalam milidetik.
constexpr unsigned long DHT_INTERVAL = 2000;
constexpr unsigned long POWER_INTERVAL = 1000;
constexpr unsigned long SCHEDULE_INTERVAL = 1000;
constexpr unsigned long OLED_INTERVAL = 500;
constexpr unsigned long OLED_RETRY_INTERVAL = 30000;
constexpr unsigned long CLOUD_INTERVAL = 5000;
constexpr unsigned long TELEMETRY_INTERVAL = 10000;
constexpr unsigned long WIFI_RETRY_INTERVAL = 15000;
constexpr unsigned long NTP_RETRY_INTERVAL = 6UL * 60UL * 60UL * 1000UL;
constexpr unsigned long CONTACT_DEBOUNCE = 50;
constexpr unsigned long RFID_MESSAGE_TIME = 2500;
constexpr bool SERIAL_DEBUG = true;
constexpr bool ANTARES_HTTP_DEBUG = true;

// Satu objek untuk tiap komponen utama.
AntaresESPHTTP antares(ANTARES_ACCESS_KEY);
Preferences preferences;
DHT dht(DHT_PIN, DHT11);
HLW8012 powerMeter;
MFRC522 rfid(RFID_SS, RFID_RST);
RTC_DS3231 rtc;
Servo lockServo;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// State perangkat. Suhu/kelembaban NAN berarti belum ada bacaan valid.
float temperature = NAN;
float humidity = NAN;
float activePower = 0;
bool dhtReadingValid = false;
bool doorOpen = false;
bool windowOpen = false;
bool lamp1On = false;
bool lamp2On = false;
bool blowerOn = false;
bool doorLocked = true;
bool rtcReady = false;
bool rtcValid = false;
bool oledReady = false;
bool preferencesReady = false;
bool wifiWasConnected = false;
bool ntpTimePending = false;

// Konfigurasi awal. Nilai dari Preferences atau Antares dapat menggantinya.
byte lamp1Mode = 0;       // 0=manual, 1=jadwal
byte lamp2Mode = 0;
byte blowerMode = 0;      // 0=manual, 1=berdasarkan suhu
String lamp1OnTime = "18:00";
String lamp1OffTime = "06:00";
String lamp2OnTime = "18:00";
String lamp2OffTime = "06:00";
float blowerThreshold = 33.0;
float blowerHysteresis = 1.0;
String lastCommandId;
String lastCommandResult;

unsigned long lastDhtAt = 0;
unsigned long lastPowerAt = 0;
unsigned long lastScheduleAt = 0;
unsigned long lastOledAt = 0;
unsigned long lastOledAttemptAt = 0;
unsigned long lastWifiAttemptAt = 0;
unsigned long lastNtpAt = 0;
unsigned long lastCommandAt = 0;
unsigned long lastTelemetryAt = 0;
unsigned long rfidMessageUntil = 0;
uint32_t lastCheckedMinute = UINT32_MAX;
const char *rfidMessage = "READY";

// Raw = bacaan sekarang; stable = bacaan yang sudah stabil 50 ms.
struct ContactState {
  byte pin;
  byte openLevel;
  bool raw = false;
  bool stable = false;
  unsigned long changedAt = 0;
};
ContactState doorContact{DOOR_PIN, DOOR_OPEN_LEVEL};
ContactState windowContact{WINDOW_PIN, WINDOW_OPEN_LEVEL};

// Log diberi waktu sejak boot agar siswa mudah mengikuti urutan kejadian.
void debugLog(const char *tag, const String &message) {
  if (!SERIAL_DEBUG) return;
  Serial.printf("[%8lu ms] [%s] %s\n", millis(), tag, message.c_str());
}

// Aktuator hanya diubah jika berbeda dari state sekarang. Log debug menampilkan perubahan.
void setLamp1(bool on) {
  if (lamp1On == on) return;
  lamp1On = on;
  digitalWrite(LAMP1_PIN, on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
  debugLog("LAMP1", on ? "ON" : "OFF");
}

void setLamp2(bool on) {
  if (lamp2On == on) return;
  lamp2On = on;
  digitalWrite(LAMP2_PIN, on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
  debugLog("LAMP2", on ? "ON" : "OFF");
}

void setBlower(bool on) {
  if (blowerOn == on) return;
  blowerOn = on;
  digitalWrite(BLOWER_PIN, on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
  debugLog("BLOWER", on ? "ON" : "OFF");
}

// Servo 0 derajat = kunci, 90 derajat = buka. Log debug menampilkan perubahan.
void lockDoor() {
  if (doorLocked) return;
  lockServo.write(0);
  doorLocked = true;
  debugLog("LOCK", "LOCK");
}

void unlockDoor() {
  if (!doorLocked) return;
  lockServo.write(90);
  doorLocked = false;
  debugLog("LOCK", "UNLOCK");
}

// Toggle kunci pintu. Jika terkunci, buka; jika terbuka, kunci.
void toggleDoorLock() {
  if (doorLocked) unlockDoor();
  else lockDoor();
}

// HH:MM harus tepat lima karakter dan berada dalam rentang 00:00-23:59.
bool validTime(const String &value) {
  if (value.length() != 5 || value[2] != ':') return false;
  for (int i : {0, 1, 3, 4}) {
    if (value[i] < '0' || value[i] > '9') return false;
  }
  int hour = (value[0] - '0') * 10 + value[1] - '0';
  int minute = (value[3] - '0') * 10 + value[4] - '0';
  return hour < 24 && minute < 60;
}

int minutesOfDay(const String &value) {
  return value.substring(0, 2).toInt() * 60 + value.substring(3, 5).toInt();
}

bool validRtcTime(const DateTime &value) {
  return value.year() >= 2024 && value.year() <= 2099 &&
         value.month() >= 1 && value.month() <= 12 &&
         value.day() >= 1 && value.day() <= 31;
}

// Interval ON=18:00, OFF=06:00 tetap aktif melewati tengah malam.
bool scheduledOn(const String &on, const String &off, int nowMinutes) {
  int start = minutesOfDay(on);
  int finish = minutesOfDay(off);
  if (start < finish) return nowMinutes >= start && nowMinutes < finish;
  return nowMinutes >= start || nowMinutes < finish;
}

// Evaluasi saat boot, mode/jadwal berubah, atau RTC baru memperoleh waktu.
void applyScheduledLamps(bool checkLamp1 = true, bool checkLamp2 = true) {
  if (!rtcValid) return;
  DateTime now = rtc.now();
  int minute = now.hour() * 60 + now.minute();
  if (checkLamp1 && lamp1Mode == 1)
    setLamp1(scheduledOn(lamp1OnTime, lamp1OffTime, minute));
  if (checkLamp2 && lamp2Mode == 1)
    setLamp2(scheduledOn(lamp2OnTime, lamp2OffTime, minute));
}

// Jadwal hanya mengubah relay pada menit event. Door/Force OFF tidak ditimpa
// berulang-ulang selama menunggu event berikutnya.
void checkSchedule() {
  unsigned long nowMs = millis();
  if (nowMs - lastScheduleAt < SCHEDULE_INTERVAL) return;
  lastScheduleAt = nowMs;
  if (!rtcReady) return;

  DateTime now = rtc.now();
  bool valid = !rtc.lostPower() && validRtcTime(now);
  if (valid != rtcValid) {
    rtcValid = valid;
    lastCheckedMinute = UINT32_MAX;
    debugLog("RTC", valid ? "Waktu valid" : "Waktu belum valid");
    if (valid) {
      applyScheduledLamps();
      lastCheckedMinute = now.unixtime() / 60;
    }
  }
  if (!rtcValid) return;

  uint32_t minuteKey = now.unixtime() / 60;
  if (minuteKey == lastCheckedMinute) return;
  lastCheckedMinute = minuteKey;
  int minute = now.hour() * 60 + now.minute();
  if (lamp1Mode == 1) {
    if (minute == minutesOfDay(lamp1OnTime)) setLamp1(true);
    else if (minute == minutesOfDay(lamp1OffTime)) setLamp1(false);
  }
  if (lamp2Mode == 1) {
    if (minute == minutesOfDay(lamp2OnTime)) setLamp2(true);
    else if (minute == minutesOfDay(lamp2OffTime)) setLamp2(false);
  }
}

// Pin input diatur sesuai tipe sensor. Bacaan awal dianggap stabil.
void initContact(ContactState &contact) {
  pinMode(contact.pin, contact.pin == DOOR_PIN ? INPUT : INPUT_PULLUP);
  contact.raw = digitalRead(contact.pin) == contact.openLevel;
  contact.stable = contact.raw;
  contact.changedAt = millis();
}

// Nilai yang berubah harus bertahan 50 ms sebelum dianggap event.
bool readContact(ContactState &contact) {
  bool raw = digitalRead(contact.pin) == contact.openLevel;
  unsigned long now = millis();
  if (raw != contact.raw) {
    contact.raw = raw;
    contact.changedAt = now;
  }
  if (raw != contact.stable && now - contact.changedAt >= CONTACT_DEBOUNCE) {
    contact.stable = raw;
    return true;
  }
  return false;
}

// Bacaan door/window hanya dipakai untuk log dan telemetry. Lampu 1 bisa menyala otomatis saat pintu dibuka.
void readDoorAndWindow() {
  if (readContact(doorContact)) {
    bool wasOpen = doorOpen;
    doorOpen = doorContact.stable;
    debugLog("DOOR", doorOpen ? "OPEN" : "CLOSED");
    
    if (!wasOpen && doorOpen) setLamp1(true);
  }

  if (readContact(windowContact)) {
    windowOpen = windowContact.stable;
    debugLog("WINDOW", windowOpen ? "OPEN" : "CLOSED");
  }
}

// Hysteresis mencegah blower berkedip saat suhu dekat threshold.
void applyBlowerAutomation() {
  if (blowerMode != 1 || !dhtReadingValid) return;
  if (temperature >= blowerThreshold) setBlower(true);
  else if (temperature <= blowerThreshold - blowerHysteresis) setBlower(false);
}

// Bacaan DHT hanya dipakai untuk log, telemetry, dan kontrol blower. Bacaan gagal tidak mengubah state blower.
void readDht() {
  unsigned long now = millis();
  if (now - lastDhtAt < DHT_INTERVAL) return;
  lastDhtAt = now;
  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();
  if (isnan(newHumidity) || isnan(newTemperature)) {
    dhtReadingValid = false;
    debugLog("DHT", "Pembacaan gagal; blower tidak diubah");
    return;
  }
  humidity = newHumidity;
  temperature = newTemperature;
  dhtReadingValid = true;
  debugLog("DHT", "Suhu=" + String(temperature, 1) + " C, hum=" + String(humidity, 1) + " %");
  applyBlowerAutomation();
}

void IRAM_ATTR onHlwCf() { powerMeter.cf_interrupt(); }
void IRAM_ATTR onHlwCf1() { powerMeter.cf1_interrupt(); }

// Bacaan daya aktif hanya dipakai untuk log dan telemetry. Bacaan gagal tidak mengubah state.
void readPower() {
  unsigned long now = millis();
  if (now - lastPowerAt < POWER_INTERVAL) return;
  lastPowerAt = now;
  activePower = powerMeter.getActivePower();
}

// UID kartu dibaca dari MFRC522. Hanya kartu yang ada di AUTHORIZED_UIDS yang diterima.
bool authorizedUid(const char *uid) {
  for (const char *allowed : AUTHORIZED_UIDS) {
    if (allowed[0] && strcmp(uid, allowed) == 0) return true;
  }
  return false;
}

// RFID hanya dipakai untuk log, telemetry, dan kontrol kunci pintu. Bacaan gagal tidak mengubah state.
void checkRfid() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;
  char uid[21] = {};
  for (byte i = 0; i < rfid.uid.size && i < 10; ++i)
    snprintf(uid + i * 2, sizeof(uid) - i * 2, "%02X", rfid.uid.uidByte[i]);

  debugLog("RFID", "UID=" + String(uid));
  
  if (authorizedUid(uid)) {
    rfidMessage = "AUTHORIZED";
    toggleDoorLock();
  } else {
    rfidMessage = "DENIED";
  }

  rfidMessageUntil = millis() + RFID_MESSAGE_TIME;
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// OLED satu halaman: nilai terakhir yang valid dan pesan RFID sementara.
void updateOLED() {
  if (!oledReady) return;
  unsigned long now = millis();
  if (now - lastOledAt < OLED_INTERVAL) return;
  lastOledAt = now;
  if ((int32_t)(now - rfidMessageUntil) >= 0) rfidMessage = "READY";

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("TEMP : ");
  if (isnan(temperature)) display.print("--.-");
  else display.print(temperature, 1);
  display.println(" C");
  display.print("HUM  : ");
  if (isnan(humidity)) display.print("--.-");
  else display.print(humidity, 1);
  display.println(" %");
  display.print("POWER: ");
  display.print(activePower, 1);
  display.println(" W");
  display.print("RFID : ");
  display.println(rfidMessage);
  display.display();
}

// OLED bisa gagal saat boot karena tegangan I2C rendah. Coba lagi setiap 30 detik.
void initOLED() {
  lastOledAttemptAt = millis();
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, false);
  debugLog("OLED", oledReady ? "SSD1306 siap" : "Tidak ditemukan; coba lagi nanti");
  if (oledReady) lastOledAt = millis() - OLED_INTERVAL;
}

// Preferences menyimpan konfigurasi; key NVS maksimal 15 karakter.
void loadConfig() {
  preferencesReady = preferences.begin("smart-home", false);
  if (!preferencesReady) {
    debugLog("CONFIG", "NVS gagal dibuka; memakai nilai awal");
    return;
  }
  byte mode = preferences.getUChar("lamp1_mode", 0);
  if (mode <= 1) lamp1Mode = mode;
  mode = preferences.getUChar("lamp2_mode", 0);
  if (mode <= 1) lamp2Mode = mode;
  mode = preferences.getUChar("blower_mode", 0);
  if (mode <= 1) blowerMode = mode;

  String on = preferences.getString("lamp1_on", "18:00");
  String off = preferences.getString("lamp1_off", "06:00");
  if (validTime(on) && validTime(off) && on != off) {
    lamp1OnTime = on;
    lamp1OffTime = off;
  }
  on = preferences.getString("lamp2_on", "18:00");
  off = preferences.getString("lamp2_off", "06:00");
  if (validTime(on) && validTime(off) && on != off) {
    lamp2OnTime = on;
    lamp2OffTime = off;
  }

  float threshold = preferences.getFloat("blower_thr", 33.0);
  float hysteresis = preferences.getFloat("blower_hyst", 1.0);
  if (isfinite(threshold) && isfinite(hysteresis) && threshold >= 10 &&
      threshold <= 60 && hysteresis > 0 && hysteresis <= 10 && hysteresis < threshold) {
    blowerThreshold = threshold;
    blowerHysteresis = hysteresis;
  }
  lastCommandId = preferences.getString("last_command_id", "");
  lastCommandResult = preferences.getString("last_cmd_result", "");
  debugLog("CONFIG", "Konfigurasi dari NVS dimuat");
}

// Hanya field valid yang berubah ditulis ke NVS untuk mengurangi flash write.
bool applyLampConfig(JsonObjectConst data, byte &mode, String &on, String &off,
                     const char *modeKey, const char *onKey, const char *offKey,
                     bool &changed) {
  if (data.isNull()) return false;
  bool accepted = false;
  if (data["mode"].is<int>()) {
    int newMode = data["mode"].as<int>();
    if (newMode == 0 || newMode == 1) {
      accepted = true;
      if (mode != newMode) {
        mode = newMode;
        changed = true;
        if (preferencesReady) preferences.putUChar(modeKey, mode);
      }
    }
  }
  String newOn = on;
  String newOff = off;
  bool hasOn = data["on"].is<const char *>();
  bool hasOff = data["off"].is<const char *>();
  if (hasOn) newOn = data["on"].as<String>();
  if (hasOff) newOff = data["off"].as<String>();
  if ((hasOn || hasOff) && validTime(newOn) && validTime(newOff) && newOn != newOff) {
    accepted = true;
    if (newOn != on) {
      on = newOn;
      changed = true;
      if (preferencesReady) preferences.putString(onKey, on);
    }
    if (newOff != off) {
      off = newOff;
      changed = true;
      if (preferencesReady) preferences.putString(offKey, off);
    }
  }
  return accepted;
}

bool applyConfig(JsonObjectConst doc) {
  bool lamp1Changed = false;
  bool lamp2Changed = false;
  bool blowerChanged = false;
  bool accepted = applyLampConfig(doc["lamp1"].as<JsonObjectConst>(), lamp1Mode,
                                  lamp1OnTime, lamp1OffTime, "lamp1_mode", "lamp1_on",
                                  "lamp1_off", lamp1Changed);
  accepted |= applyLampConfig(doc["lamp2"].as<JsonObjectConst>(), lamp2Mode,
                              lamp2OnTime, lamp2OffTime, "lamp2_mode", "lamp2_on",
                              "lamp2_off", lamp2Changed);

  JsonObjectConst blower = doc["blower"].as<JsonObjectConst>();
  if (!blower.isNull()) {
    if (blower["mode"].is<int>()) {
      int mode = blower["mode"].as<int>();
      if (mode == 0 || mode == 1) {
        accepted = true;
        if (blowerMode != mode) {
          blowerMode = mode;
          blowerChanged = true;
          if (preferencesReady) preferences.putUChar("blower_mode", blowerMode);
        }
      }
    }
    float threshold = blowerThreshold;
    float hysteresis = blowerHysteresis;
    if (blower["threshold"].is<float>() || blower["threshold"].is<int>())
      threshold = blower["threshold"].as<float>();
    if (blower["hysteresis"].is<float>() || blower["hysteresis"].is<int>())
      hysteresis = blower["hysteresis"].as<float>();
    bool hasThreshold = blower["threshold"].is<float>() || blower["threshold"].is<int>();
    bool hasHysteresis = blower["hysteresis"].is<float>() || blower["hysteresis"].is<int>();
    if ((hasThreshold || hasHysteresis) && isfinite(threshold) && isfinite(hysteresis) && threshold >= 10 &&
        threshold <= 60 && hysteresis > 0 && hysteresis <= 10 && hysteresis < threshold) {
      accepted = true;
      if (threshold != blowerThreshold) {
        blowerThreshold = threshold;
        blowerChanged = true;
        if (preferencesReady) preferences.putFloat("blower_thr", threshold);
      }
      if (hysteresis != blowerHysteresis) {
        blowerHysteresis = hysteresis;
        blowerChanged = true;
        if (preferencesReady) preferences.putFloat("blower_hyst", hysteresis);
      }
    }
  }
  if (lamp1Changed || lamp2Changed) applyScheduledLamps(lamp1Changed, lamp2Changed);
  if (blowerChanged) applyBlowerAutomation();
  if (lamp1Changed || lamp2Changed || blowerChanged)
    debugLog("CONFIG", "Perubahan disimpan");
  return accepted;
}

// Nilai NONE berarti aktuator tersebut tidak menerima perintah.
bool validAction(JsonVariantConst value, const char *first, const char *second) {
  if (value.isNull()) return true;
  if (!value.is<const char *>()) return false;
  const char *text = value.as<const char *>();
  return strcmp(text, "NONE") == 0 || strcmp(text, first) == 0 || strcmp(text, second) == 0;
}

const char *action(JsonVariantConst value) {
  return value.isNull() ? "NONE" : value.as<const char *>();
}

// Satu ID berlaku untuk konfigurasi dan kontrol; hasilnya dikirim lewat telemetry.
void saveCommandResult(const String &id, const char *result) {
  lastCommandId = id;
  if (lastCommandResult != result) {
    lastCommandResult = result;
    if (preferencesReady) preferences.putString("last_cmd_result", result);
  }
  if (preferencesReady) preferences.putString("last_command_id", id);
  debugLog("COMMAND", "ID " + id + ": " + result);
}

const char *applyControl(JsonObjectConst doc) {
  if (!validAction(doc["lamp1"], "ON", "OFF") ||
      !validAction(doc["lamp2"], "ON", "OFF") ||
      !validAction(doc["blower"], "ON", "OFF") ||
      !validAction(doc["door_lock"], "LOCK", "UNLOCK")) {
    debugLog("COMMAND", "Aksi tidak valid");
    return "rejected";
  }

  const char *lamp1 = action(doc["lamp1"]);
  const char *lamp2 = action(doc["lamp2"]);
  const char *blower = action(doc["blower"]);
  const char *door = action(doc["door_lock"]);
  bool applied = false;
  if (strcmp(lamp1, "OFF") == 0) {
    setLamp1(false); // Matikan Sekarang juga berlaku dalam mode jadwal
    applied = true;
  } else if (strcmp(lamp1, "ON") == 0 && lamp1Mode == 0) {
    setLamp1(true);
    applied = true;
  }
  if (lamp2Mode == 0) {
    if (strcmp(lamp2, "ON") == 0) { setLamp2(true); applied = true; }
    else if (strcmp(lamp2, "OFF") == 0) { setLamp2(false); applied = true; }
  }
  if (blowerMode == 0) {
    if (strcmp(blower, "ON") == 0) { setBlower(true); applied = true; }
    else if (strcmp(blower, "OFF") == 0) { setBlower(false); applied = true; }
  }
  if (strcmp(door, "LOCK") == 0) { lockDoor(); applied = true; }
  else if (strcmp(door, "UNLOCK") == 0) { unlockDoor(); applied = true; }
  return applied ? "applied" : "ignored";
}

void applyCommand(const String &raw) {
  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, raw) || !doc.is<JsonObject>()) {
    debugLog("COMMAND", "JSON tidak valid");
    return;
  }
  if (!doc["id"].is<const char *>()) return;
  String id = doc["id"].as<String>();
  if (id.isEmpty() || id.length() > 63 || id == lastCommandId) return;

  const char *result = "rejected";
  if (doc["type"].is<const char *>()) {
    String type = doc["type"].as<String>();
    if (type == "config") result = applyConfig(doc.as<JsonObjectConst>()) ? "applied" : "rejected";
    else if (type == "control") result = applyControl(doc.as<JsonObjectConst>());
  }
  saveCommandResult(id, result);
}

// WiFi.begin tidak menunggu koneksi, jadi otomasi lokal terus berjalan.
void checkWiFi() {
  unsigned long now = millis();
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != wifiWasConnected) {
    wifiWasConnected = connected;
    debugLog("WIFI", connected ? "Terhubung" : "Terputus");
    if (connected) {
      sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
      configTzTime("WIB-7", "pool.ntp.org", "time.nist.gov");
      lastNtpAt = now;
      ntpTimePending = false;
      lastCommandAt = now - CLOUD_INTERVAL;
      lastTelemetryAt = now - TELEMETRY_INTERVAL;
    }
  }
  if (!connected && WIFI_SSID[0] && now - lastWifiAttemptAt >= WIFI_RETRY_INTERVAL) {
    lastWifiAttemptAt = now;
    debugLog("WIFI", "Mencoba sambung ulang");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  if (connected && now - lastNtpAt >= NTP_RETRY_INTERVAL) {
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    configTzTime("WIB-7", "pool.ntp.org", "time.nist.gov");
    lastNtpAt = now;
    ntpTimePending = false;
  }
}

// DS3231 menyimpan WIB. Saat offline, jadwal tetap membaca DS3231.
void checkNtp() {
  if (!rtcReady || WiFi.status() != WL_CONNECTED) return;
  if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) ntpTimePending = true;
  if (!ntpTimePending) return;
  struct tm localTime;
  if (!getLocalTime(&localTime, 0) || localTime.tm_year + 1900 < 2024) return;
  DateTime value(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
                 localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
  bool wasValid = rtcValid;
  rtc.adjust(value);
  rtcValid = true;
  if (!wasValid) {
    applyScheduledLamps();
    lastCheckedMinute = value.unixtime() / 60;
  }
  ntpTimePending = false;
  debugLog("NTP", "RTC disinkronkan ke WIB");
}

// Periksa perintah baru di Antares. Hanya satu perintah yang diproses tiap 5 detik.
void pollCommand() {
  String raw = antares.getRaw(ANTARES_PROJECT, DEVICE_COMMANDS);
  if (antares.getSuccess() && !raw.isEmpty()) applyCommand(raw);
  else debugLog("ANTARES", "Command gagal dibaca");
}

// Payload dua tingkat mengikuti kontrak telemetry pada PRD.
void publishTelemetry() {
  char timestamp[32] = "";
  if (rtcValid) {
    DateTime now = rtc.now();
    snprintf(timestamp, sizeof(timestamp), "%04u-%02u-%02uT%02u:%02u:%02u+07:00",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  }
  antares.add("ts", String(timestamp));
  antares.add("sensor", "temperature", isnan(temperature) ? 0.0f : temperature);
  antares.add("sensor", "humidity", isnan(humidity) ? 0.0f : humidity);
  antares.add("sensor", "active_power", activePower);
  antares.add("contact", "door", String(doorOpen ? "OPEN" : "CLOSED"));
  antares.add("contact", "window", String(windowOpen ? "OPEN" : "CLOSED"));
  antares.add("actuator", "lamp1", String(lamp1On ? "ON" : "OFF"));
  antares.add("actuator", "lamp2", String(lamp2On ? "ON" : "OFF"));
  antares.add("actuator", "blower", String(blowerOn ? "ON" : "OFF"));
  antares.add("actuator", "door_lock", String(doorLocked ? "LOCK" : "UNLOCK"));
  antares.add("lamp1_config", "mode", (int)lamp1Mode);
  antares.add("lamp1_config", "on", lamp1OnTime);
  antares.add("lamp1_config", "off", lamp1OffTime);
  antares.add("lamp2_config", "mode", (int)lamp2Mode);
  antares.add("lamp2_config", "on", lamp2OnTime);
  antares.add("lamp2_config", "off", lamp2OffTime);
  antares.add("blower_config", "mode", (int)blowerMode);
  antares.add("blower_config", "threshold", blowerThreshold);
  antares.add("blower_config", "hysteresis", blowerHysteresis);
  antares.add("system", "wifi", String("CONNECTED"));
  antares.add("system", "rssi", (int)WiFi.RSSI());
  antares.add("system", "rtc_valid", rtcValid ? 1 : 0);
  antares.add("system", "uptime", (int)(millis() / 1000));
  antares.add("system", "last_command_id", lastCommandId);
  antares.add("system", "last_command_result", lastCommandResult);
  antares.send(ANTARES_PROJECT, DEVICE_TELEMETRY);
  // send() tidak mengembalikan status; ANTARES_HTTP_DEBUG menampilkan respons HTTP.
  debugLog("ANTARES", "Request telemetry selesai");
}

void setup() {
  Serial.begin(115200);
  debugLog("BOOT", "Smart Home SMA mulai");
  antares.setDebug(ANTARES_HTTP_DEBUG);

  // Relay utama dimatikan sebelum pin menjadi output. Relay sensor daya ON.
  digitalWrite(LAMP1_PIN, RELAY_OFF_LEVEL); pinMode(LAMP1_PIN, OUTPUT);
  digitalWrite(LAMP2_PIN, RELAY_OFF_LEVEL); pinMode(LAMP2_PIN, OUTPUT);
  digitalWrite(BLOWER_PIN, RELAY_OFF_LEVEL); pinMode(BLOWER_PIN, OUTPUT);
  digitalWrite(POWER_RELAY_PIN, POWER_RELAY_ON_LEVEL); pinMode(POWER_RELAY_PIN, OUTPUT);
  lockServo.setPeriodHertz(50);
  lockServo.attach(SERVO_PIN, 500, 2400);
  lockServo.write(0); // Kunci mulai dalam state LOCK.

  initContact(doorContact);
  initContact(windowContact);
  doorOpen = doorContact.stable;
  windowOpen = windowContact.stable;
  dht.begin();

  Wire.begin(OLED_SDA, OLED_SCL); // OLED dan DS3231 berbagi bus I2C.
  Wire.setTimeOut(50);
  initOLED();
  rtcReady = rtc.begin();
  if (rtcReady) rtcValid = !rtc.lostPower() && validRtcTime(rtc.now());
  else debugLog("RTC", "DS3231 tidak ditemukan; jadwal menunggu RTC");

  SPI.begin(18, 19, 23, RFID_SS);
  rfid.PCD_Init();
  powerMeter.begin(HLW_CF, HLW_CF1, HLW_SEL, HLW_CURRENT_LEVEL, true);
  powerMeter.setResistors(HLW_CURRENT_RESISTOR, HLW_VOLTAGE_UPSTREAM, HLW_VOLTAGE_DOWNSTREAM);
  powerMeter.setPowerMultiplier(powerMeter.getPowerMultiplier() * HLW_POWER_CORRECTION);
  attachInterrupt(digitalPinToInterrupt(HLW_CF), onHlwCf, CHANGE);
  attachInterrupt(digitalPinToInterrupt(HLW_CF1), onHlwCf1, CHANGE);

  loadConfig();
  applyScheduledLamps(); // Evaluasi jadwal saat boot jika RTC sudah valid.
  if (rtcValid) lastCheckedMinute = rtc.now().unixtime() / 60;
  updateOLED();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  if (WIFI_SSID[0]) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastWifiAttemptAt = millis();
    debugLog("WIFI", "Menghubungkan");
  } else {
    debugLog("WIFI", "Isi WIFI_SSID untuk mengaktifkan koneksi");
  }
  debugLog("BOOT", "Inisialisasi selesai");
}

// Urutan lokal ditempatkan lebih dulu. Tiap putaran hanya membuat satu
// request Antares agar sensor dan RFID sempat diproses di antara request.
void loop() {
  readDoorAndWindow();
  checkRfid();
  readDht();
  readPower();
  checkSchedule();
  updateOLED();
  if (!oledReady && millis() - lastOledAttemptAt >= OLED_RETRY_INTERVAL) initOLED();
  checkWiFi();
  checkNtp();

  if (WiFi.status() == WL_CONNECTED && ANTARES_ACCESS_KEY[0]) {
    unsigned long now = millis();
    if (now - lastCommandAt >= CLOUD_INTERVAL) {
      lastCommandAt = now;
      pollCommand();
    } else if (now - lastTelemetryAt >= TELEMETRY_INTERVAL) {
      lastTelemetryAt = now;
      publishTelemetry();
    }
  }
  delay(1);
}
