#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <WiFi.h>

#define MQ_9 4
#define MQ_135 15
#define MQ_137 18

//const char* ssid = "BANK SAMPAH";
//const char* password = "AZARI1969";
char ssid[] = "zhar";
char password[] = "zuzuzuzu";
const char* serverName = "https://script.google.com/macros/s/AKfycbyPOpfVUoe_Gr_5OL6SfCH0LYjZtPS0P7zmv30ZhSLKSTf6QCmD8bcqPchCy2gMviLv/exec";

LiquidCrystal_I2C lcd(0x27, 20, 4);
HTTPClient http;

const float A_9 = 4265.95, B_9 = -2.39268;
const float A_135 = 109.691, B_135 = -2.37977;
const float A_137 = 38.11, B_137 = -3.30048;
float R0_9 = 0.39, R0_135 = 1.71, R0_137 = 0.15;

const int waktuUpload = 300000, waktuDisplay = 2000;
int tempUpload = 0, tempDisplay = 0;

float analog9, analog135, analog137;
String kualitas;

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  lcd.begin();
  
  // WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    lcd.setCursor(0, 0);
    lcd.print("Connecting to WiFi...");
    Serial.print("Connecting to WiFi...");
  }
  lcd.clear();
  lcd.print("Connected to WiFi");
  Serial.println("Connected");
}

void loop() {
  analog9 = hitungPPM(analogRead(MQ_9), R0_9, A_9, B_9);
  analog135 = hitungPPM(analogRead(MQ_135), R0_135, A_135, B_135);
  analog137 = hitungPPM(analogRead(MQ_137), R0_137, A_137, B_137);

    float aqiNH3 = calculateAQI(ppmNH3, 10, 25, 50); // Batas NH3
    float aqiCH4 = calculateAQI(ppmCH4, 10000, 25000, 50000); // Batas CH4
    float aqiCO2 = calculateAQI(ppmCO2, 1000, 2500, 5000);   // Batas CO2

  float maxAqi = max(aqiNH3, max(aqiCH4, aqiCO2));
  kualitas = (maxAqi <= 50) ? "Baik" : (maxAqi <= 100) ? "Sedang" : "Bahaya";

  if (millis() - tempDisplay >= waktuDisplay) {
    tempDisplay = millis();
    displayLCD(analog137, analog135, analog9, kualitas);
    Serial.print("NH3 = "); Serial.println(aqiNH3);
    Serial.print("CH4 = "); Serial.println(aqiCH4);
    Serial.print("CO2 = "); Serial.println(aqiCO2);
  }

  if (millis() - tempUpload >= waktuUpload) {
    tempUpload = millis();
    String postData = "amonia=" + String(analog137) + "&metana=" + String(analog9) + "&karbon_dioksida=" + String(analog135) + "&kualitas=" + kualitas;
    
    http.begin(serverName);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    int httpResponseCode = http.POST(postData);
    Serial.println("HTTP Response code: " + String(httpResponseCode));
    http.end();
  }
  delay(1000);
}

float calculateAQI(float nilaiKonsentrasi, float batas1, float batas2, float batas3) {
    if (nilaiKonsentrasi <= batas1) {
        return (50.0 / batas1) * nilaiKonsentrasi;
    } else if (nilaiKonsentrasi <= batas2) {
        return 50.0 + ((100.0 - 50.0) / (batas2 - batas1)) * (nilaiKonsentrasi - batas1);
    } else if (nilaiKonsentrasi <= batas3) {
        return 100.0 + ((200.0 - 100.0) / (batas3 - batas2)) * (nilaiKonsentrasi - batas2);
    } else {
        return 200.0; // AQI di luar batas normal
    }
}

float hitungPPM(float Raw, float R0, float a, float b) {
  float rs = (5.0 - (Raw * (5.0 / 4095.0))) / (Raw * (5.0 / 4095.0));
  return (a * pow((rs / R0), b));
}

void displayLCD(float analog137, float analog135, float analog9, String kualitas) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Amonia = ");lcd.setCursor(9, 0); lcd.print(analog137);
  lcd.setCursor(0, 1); lcd.print("Metana = ");lcd.setCursor(9, 0); lcd.print(analog9);
  lcd.setCursor(0, 2); lcd.print("CO2 = ");lcd.setCursor(6, 0); lcd.print(analog135);
  lcd.setCursor(0, 3); lcd.print("Kualitas = ");lcd.setCursor(11, 0); lcd.print(kualitas);
}
