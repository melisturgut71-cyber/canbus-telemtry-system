#include <SPI.h>
#include <mcp_can.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>       
#include <HardwareSerial.h> 

// --- WIFI VE MQTT AYARLARI ---
const char* ssid = "FiberHGW_ZY88A5";        
const char* password = "XLyJVUNm4qE3";  
const char* mqtt_server = "broker.hivemq.com"; 
const char* mqtt_topic = "melis/corsa/data";

WiFiClient espClient;
PubSubClient client(espClient);

const int SPI_CS_PIN = 5;    
MCP_CAN CAN0(SPI_CS_PIN);

// --- GPS AYARLARI ---
TinyGPSPlus gps;
//HardwareSerial gpsSerial(2); // RX=17, TX=16

// --- CAN VERİ YAPISI ---
struct CarData {
  float rpm = 0;
  int motorTemp = 0;
  float intakeTemp = 0;
  float outsideTemp = -99.0;
  String pedalstatusbrake = "BELIRSIZ";
  String pedalstatusgas = "BELIRSIZ";
  String engineState = "KAPALI";
  float voltage = 0;
  long odometer = 0;
} car;

unsigned long lastMsgTime = 0;
unsigned long rxId;
unsigned char len = 0;
unsigned char rxBuf[8];

void setup() {
  // 1. SERİ HABERLEŞMEYİ BAŞLAT
  Serial.begin(115200);
  
  // BURASI ÖNEMLİ: Sen monitörü açana kadar 3 saniye bekler.
  // Böylece "Başlatıldı" yazılarını kaçırmazsın.
  delay(3000); 

  // 2. GPS BAŞLAT
  // ESP32 GPIO 16 (RX) <---bağlanmalı---> GPS TX
  // ESP32 GPIO 17 (TX) <---bağlanmalı---> GPS RX
 // --- GPS KONTROL BLOĞU ---
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // Pinleri kendi devrene göre düzelt!
  Serial.print("GPS Modulu Kontrol Ediliyor...");
  
  unsigned long baslangicZamani = millis();
  bool gpsSinyaliVar = false;

  // 2 saniye boyunca dinle
  while (millis() - baslangicZamani < 2000) {
    if (Serial2.available() > 0) {
      gpsSinyaliVar = true;
      // Tamponu temizle ki loop'ta temiz veri okuyalım
      Serial2.read(); 
      break; 
    }
  }

  if (gpsSinyaliVar) {
    Serial.println(" BAŞARILI!");
    Serial.println("-> GPS modülünden veri paketleri alınıyor.");
  } else {
    Serial.println(" HATA!");
    Serial.println("-> UYARI: GPS modülünden veri gelmiyor!"); 
    Serial.println("-> RX/TX bağlantılarını veya baud rate hızını (9600) kontrol et.");
  }
  Serial.println("--------------------------------");
  // -------------------------
  // 3. CAN BUS BAŞLAT
  Serial.print("[DURUM 2/4] CAN BUS Modulu: ");
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("BAGLANDI (OK)");
    CAN0.setMode(MCP_NORMAL);
  } else {
    Serial.println("HATA! (Kablolari kontrol et)");
  }

  // 4. WIFI BAŞLAT
  setup_wifi();
  
  // 5. MQTT BAŞLAT
  client.setServer(mqtt_server, 1883);
  Serial.println("[DURUM 4/4] Kurulum Tamamlandi. Donguye giriliyor...");

}
void setup_wifi() {
  delay(10);
  Serial.print("[DURUM 3/4] WiFi Baglaniyor: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int denemeSayisi = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    denemeSayisi++;
    if(denemeSayisi > 20) {
        Serial.println("\n[HATA] WiFi Baglanamadi! Sifreyi kontrol et.");
        break; // Sonsuz döngüden çık
    }
  }
  
  if(WiFi.status() == WL_CONNECTED){
      Serial.println("\n            -> WiFi Baglandi!");
      Serial.println(WiFi.localIP());
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("[MQTT] Baglanti koptu, tekrar deneniyor...");
    String clientId = "ESP32Corsa-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println(" BAGLANDI!");
    } else {
      Serial.print(" HATA (rc=");
      Serial.print(client.state());
      Serial.println(") 5 saniye bekle...");
      delay(5000);
    }
  }
}

void parseCanData(long unsigned int id, unsigned char* rxBuf) {
   bool pedalHareketiAlgilandi = false; 

  switch(id) {
  case 0x40E: 
        if(rxBuf[2] == 0x6A) car.engineState = "CALISIYOR";
        else car.engineState = "DURUYOR";
        break;
        
      case 0x208: 
        car.rpm = ((rxBuf[0] * 256) + rxBuf[1]) / 8.0;
        break;
      case 0x52E: car.outsideTemp = (rxBuf[1] / 10.0); break; 

      case 0x56E:
        { 
          // 1. GAZ KONTROLÜ
          if (rxBuf[3] != 0x00) {
              car.pedalstatusgas = "BASILI";
          } else {
              car.pedalstatusgas = "SERBEST";
          }
        if (rxBuf[1] == 0x20) {
              car.pedalstatusbrake = "BASILI";
          } else {
              car.pedalstatusbrake = "SERBEST"; 
          }
        }
        break;
      case 0x608: 
          if (rxBuf[0] == 0x02) car.motorTemp = rxBuf[1];
          break;
      case 0x3CE: car.voltage = rxBuf[1] * 0.125; break;

      case 0x552: { 
        long kmVal = (rxBuf[5] << 8) | rxBuf[6]; 
        if(kmVal > 0) car.odometer = kmVal; 
        break;
      }
  }
}
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // --- 1. GPS VERİSİNİ OKU ---
  while (Serial2.available() > 0) {
    char c = Serial2.read();
    gps.encode(c);
  }

  // --- 2. CAN OKUMA ---
  if(CAN_MSGAVAIL == CAN0.checkReceive()) {
    CAN0.readMsgBuf(&rxId, &len, rxBuf);
    parseCanData(rxId, rxBuf);
  }

  // --- 3. MQTT GÖNDERME (Her 500ms'de bir) ---
  unsigned long now = millis();
  if (now - lastMsgTime > 500) {
    lastMsgTime = now;
    
    JsonDocument doc;
    doc["ip"] = WiFi.localIP().toString(); 
    doc["rpm"] = car.rpm;
    doc["voltage"] = car.voltage;
    doc["motor"] = car.motorTemp;
    doc["outside"] = car.outsideTemp;
    doc["odo"] = car.odometer;
    doc["engine"] = car.engineState;
    doc["gas"] = car.pedalstatusgas;
    doc["brake"] = car.pedalstatusbrake;

    // --- GPS KONTROL VE HATA AYIKLAMA ---
    Serial.print("GPS Durumu: ");
    
    if (gps.location.isValid()) {
      // 1. Durum: Konum Var
      double enlem = gps.location.lat();
      double boylam = gps.location.lng();
      
     doc["lat"] = enlem;
      doc["lng"] = boylam;
      
      Serial.print("GECERLI! -> Enlem: ");
      Serial.print(enlem, 6);
      Serial.print(" Boylam: ");
      Serial.println(boylam, 6);
      
    }
     else {
      // 2. Durum: Konum Yok (Mavi ışık yansa bile ESP okuyamamış olabilir)
      doc["lat"] = 0;
      doc["lng"] = 0;
      
      Serial.print("GECERSIZ!");
      Serial.print(gps.charsProcessed()); 
      Serial.println(")");
    }
    
    char msg[1024]; 
    serializeJson(doc, msg);
    
    client.publish(mqtt_topic, msg);
  }
}