# canbus-telemtry-system
🚗 IoT Tabanlı Araç Telemetri ve Takip Sistemi (ESP32 & CAN Bus)
Bu proje, bir binek aracın (Opel Corsa D/E) CAN Bus (Controller Area Network) hattı üzerinden anlık verilerini okuyan, GPS ile konumunu takip eden ve bu verileri MQTT protokolü üzerinden buluta aktaran bir IoT sistemidir.
Proje; motor sıcaklığı, RPM, pedal durumları ve anlık konum gibi kritik verileri izleyerek uzaktan araç takibi ve diyagnostik analizi sağlar.

📋 Özellikler
Sistem aşağıdaki verileri saniyede 2 kez (500ms periyotla) sunucuya iletir:
Motor Verileri: Motor Devri (RPM), Motor Suyu Sıcaklığı, Dış Hava Sıcaklığı.
Durum Bilgisi: Motor Çalışma Durumu (Çalışıyor/Duruyor), Kontak Voltajı.
Sürüş Analizi: Gaz ve Fren Pedalı Basılma Durumları (Basılı/Serbest).
Konum Takibi: Enlem (Latitude) ve Boylam (Longitude) verisi.
Mesafe: Kilometre Sayacı (Odometer).

🛠️ Donanım Gereksinimleri
Mikrodenetleyici: ESP32 Dev Kit V1
CAN Bus Modülü: MCP2515 (SPI Arayüzü)
GPS Modülü: NEO-6M (veya muadili)
Bağlantı: OBD2 Konnektörü (Araç bağlantısı için)

🔌 Devre Şeması ve Pin Bağlantıları
1. MCP2515 (CAN Bus) Bağlantısı (SPI)
MCP2515 Pin   ESP32 Pin     Açıklama
VCC            5V         Güç
GND           GND        Toprak
CS           GPIO 5      Chip Select
SO (MISO)    GPIO 19     Master In Slave Out
SI (MOSI)    GPIO 23     Master Out Slave In
SCK          GPIO 18     Clock


2. GPS Modülü Bağlantısı (UART2)
   GPS Pin           ESP32 Pin          Notlar
VCC                  3.3V / 5V          Modüle göre değişir
GND                  GND                Toprak
TX                   GPIO 16            ESP'nin RX pinine gider
RX                   GPIO 17            ESP'nin TX pinine gider

Not: Araç tarafında CAN High ve CAN Low kabloları OBD soketinden uygun pinlere bağlanmalıdır.

⚠️ Önemli Notlar
GPS Sinyali: GPS modülü kapalı alanlarda uydu bulamayabilir. İlk kurulumda açık havada GPS Modulu Kontrol Ediliyor... mesajından sonra koordinat gelmesi 1-2 dakika sürebilir.

Baud Rate: Serial Monitor hızı 115200 olarak ayarlanmalıdır.

💻 Yazılım ve Kütüphaneler
Bu proje Arduino IDE kullanılarak geliştirilmiştir. Aşağıdaki kütüphanelerin Library Manager üzerinden yüklenmesi gerekir:
mcp_can (Cory J. Fowler) - CAN Bus haberleşmesi için.
TinyGPSPlus (Mikal Hart) - NMEA GPS verilerini işlemek için.
PubSubClient (Nick O'Leary) - MQTT haberleşmesi için.
ArduinoJson (Benoit Blanchon) - Verileri JSON formatında paketlemek için.

CAN ID Ayarlaması: parseCanData fonksiyonundaki ID'ler (Örn: 0x40E, 0x208) Opel Corsa CAN matrisine göre ayarlanmıştır. Farklı bir araç için bu ID'lerin sniffer ile tespit edilip değiştirilmesi gerekir.
