#include <WiFi.h>
#include <SPIFFS.h>
#include <time.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>

const char* ssid = " ";
const char* password = "";
LiquidCrystal_I2C lcd(0x27, 20, 4);

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

const char* model_file = "/model_details.json";

#define MQ_9 35
#define MQ_135 32
#define MQ_137 33
#define RX2 16
#define TX2 17

float R0_9 = 0.39, R0_135 = 1.71, R0_137 = 0.15;
const float A_9 = 4265.95, B_9 = -2.39268;
const float A_135 = 109.691, B_135 = -2.37977;
const float A_137 = 38.11, B_137 = -3.30048;

float ppmNH3, ppmCH4, ppmCO2, pred_amonia, pred_metana, pred_co2;
float aqiNH3, aqiCH4, aqiCO2, aqi_predAmo, aqi_predMet,aqi_predCO2;
String kualitas, pred_Kualitas;
bool predictionDoneToday;

DynamicJsonDocument model(4096);
const size_t featureCount = 6;
float amonia_lag[featureCount] = {0};
float metana_lag[featureCount] = {0};
float co2_lag[featureCount] = {0};
String lastPredictionDate = "";
int tempDisplay;

bool loadModelFile();
float predictGasUsingRFModel(float lag_data[], JsonArray feature_importances);
void dailyPrediction(float ppmNH3, float ppmCH4, float ppmCO2, float aqiNH3, float aqiCH4, float aqiCO2);
void syncTime();
void savePredictionWithTimestamp(float pred_amonia, float pred_metana, float pred_co2, String target_time);
String determineTomorrowTargetTime();
void updateLagArray(float newData, float lagArray[], size_t lagSize);
float hitungPPM(float Raw, float R0, float a, float b);
float calculateAQI(float nilaiKonsentrasi, float batas1, float batas2, float batas3);
void displayLCD(float amonia, float metana, float co2, String Kualitas);
void sendDataToWemos();

void setup() {
    Serial.begin(115200);       // Untuk debugging
    Serial2.begin(9600, SERIAL_8N1, RX2, TX2);        // Untuk komunikasi Serial ke Wemos D1 Mini
    lcd.begin();
    
    if (!SPIFFS.begin(true)) {
        Serial.println("Gagal menginisialisasi SPIFFS.");
        return;
    }

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Menghubungkan ke Wi-Fi...");
    }
    Serial.println("Terhubung ke Wi-Fi.");
    lcd.print("Terhubung ke WiFi");
    lcd.clear();

    syncTime();

    if (!loadModelFile()) {
        Serial.println("Error: Model file tidak dapat digunakan.");
        return;
    }
}

void loop() {
    int rawNH3 = analogRead(MQ_137);
    int rawCH4 = analogRead(MQ_9);
    int rawCO2 = analogRead(MQ_135);

    if (rawNH3 < 10 || rawCH4 < 10 || rawCO2 < 10) {
        Serial.println("Error: Invalid sensor readings.");
    }

    ppmNH3 = hitungPPM(rawNH3, R0_137, A_137, B_137);
    ppmCH4 = hitungPPM(rawCH4, R0_9, A_9, B_9);
    ppmCO2 = hitungPPM(rawCO2, R0_135, A_135, B_135);
    Serial.printf("NH3 : %f", ppmNH3);
    Serial.printf("CH4 : %f", ppmCH4);
    Serial.printf("CO2 : %f", ppmCO2);
    
    float aqiNH3 = calculateAQI(ppmNH3, 10, 25, 50); 
    float aqiCH4 = calculateAQI(ppmCH4, 10000, 25000, 50000); 
    float aqiCO2 = calculateAQI(ppmCO2, 1000, 2500, 5000);   

    float maxAqi = max(aqiNH3, max(aqiCH4, aqiCO2));
    kualitas = (maxAqi <= 50) ? "Baik" : (maxAqi <= 100) ? "Sedang" : "Bahaya";

    dailyPrediction(ppmNH3, ppmCH4, ppmCO2, aqiNH3, aqiCH4, aqiCO2);

    sendDataToWemos();

    if(millis() - tempDisplay >= 1000){
      unsigned long Start = millis();
      displayLCD(ppmNH3, ppmCH4, ppmCO2, kualitas);
      unsigned long End = millis();
      tempDisplay = millis();
      Serial.printf("Delay waktu LCD : %d ms \n", End - Start);
    }
    
    delay(500);
}

void syncTime() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Gagal mendapatkan waktu.");
        return;
    }
    Serial.println("Waktu berhasil disinkronkan:");
    Serial.println(&timeinfo, "%A, %d %B %Y %H:%M:%S");
}

// Fungsi untuk membaca PPM dari sensor
float hitungPPM(float Raw, float R0, float a, float b) {
    float voltage = 5; // Tegangan referensi ADC
    float rs = (voltage - (Raw * (voltage / 4095.0))) / (Raw * (voltage / 4095.0));
    return a * pow((rs / R0), b);
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

void updateLagArray(float newData, float lagArray[], size_t lagSize) {
    for (size_t i = 1; i < lagSize; i++) {
        lagArray[i - 1] = lagArray[i];
    }
    lagArray[lagSize - 1] = newData;
}

void dailyPrediction(float ppmNH3, float ppmCH4, float ppmCO2, float aqiNH3, float aqiCH4, float aqiCO2) {
    time_t now = time(NULL);
    struct tm current_time;
    localtime_r(&now, &current_time);

    char currentDate[11]; 
    strftime(currentDate, sizeof(currentDate), "%Y-%m-%d", &current_time);

    if (lastPredictionDate != String(currentDate)) {
        lastPredictionDate = String(currentDate);
        predictionDoneToday = false;
    }

    if (!predictionDoneToday && current_time.tm_hour == 16 && current_time.tm_min == 0) {
        predictionDoneToday = true;

        updateLagArray(ppmNH3, amonia_lag, featureCount);
        updateLagArray(ppmCH4, metana_lag, featureCount);
        updateLagArray(ppmCO2, co2_lag, featureCount);

        pred_amonia = predictGasUsingRFModel(amonia_lag, model["amonia"]["feature_importances"].as<JsonArray>());
        pred_metana = predictGasUsingRFModel(metana_lag, model["metana"]["feature_importances"].as<JsonArray>());
        pred_co2 = predictGasUsingRFModel(co2_lag, model["karbon_dioksida"]["feature_importances"].as<JsonArray>());

        float pred_aqiAmo = calculateAQI(pred_amonia, 10, 25, 50); 
        float pred_aqiMet = calculateAQI(pred_metana, 10000, 25000, 50000); 
        float pred_aqiCO2 = calculateAQI(pred_co2, 1000, 2500, 5000);   

        Serial.printf("pred amo : %f \n", pred_amonia);
        Serial.printf("pred met : %f \n", pred_metana);
        Serial.printf("pred co2 : %f \n", pred_co2);
        float maxAqi = max(pred_aqiAmo, max(pred_aqiMet, pred_aqiCO2));
        pred_Kualitas = (maxAqi <= 50) ? "Baik" : (maxAqi <= 100) ? "Sedang" : "Bahaya";
    }
}

float predictGasUsingRFModel(float lag_data[], JsonArray feature_importances) {
    float prediction = 0.0;
    for (size_t i = 0; i < feature_importances.size(); i++) {
        prediction += lag_data[i] * feature_importances[i].as<float>();
    }
    return prediction;
}

bool loadModelFile() {
    Serial.println("=== Membaca File Model RF ===");
    File file = SPIFFS.open(model_file, "r");
    if (!file) {
        Serial.printf("Gagal membuka file: %s\n", model_file);
        return false;
    }

    Serial.printf("Berhasil membuka file: %s\n", model_file);
    DeserializationError error = deserializeJson(model, file);
    file.close();

    if (error) {
        Serial.print("Gagal mem-parsing JSON: ");
        Serial.println(error.c_str());
        return false;
    }

    Serial.println("Parsing JSON berhasil.");
    return true;
}

void displayLCD(float amonia, float metana, float co2, String Kualitas){
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Amonia = ");lcd.setCursor(9, 0); lcd.print(amonia);
    lcd.setCursor(0, 1); lcd.print("Metana = ");lcd.setCursor(9, 1); lcd.print(metana);
    lcd.setCursor(0, 2); lcd.print("CO2 = ");lcd.setCursor(6, 2); lcd.print(co2);
    lcd.setCursor(0, 3); lcd.print("Kualitas = ");lcd.setCursor(11, 3); lcd.print(Kualitas);
}

void sendDataToWemos() {
    DynamicJsonDocument doc(512);
    doc["ppmNH3"] = ppmNH3;
    doc["ppmCH4"] = ppmCH4;
    doc["ppmCO2"] = ppmCO2;
    doc["pred_amonia"] = pred_amonia;
    doc["pred_metana"] = pred_metana;
    doc["pred_co2"] = pred_co2;
    doc["kualitas"] = kualitas;
    doc["pred_Kualitas"] = pred_Kualitas;

    String jsonString;
    serializeJson(doc, jsonString);

    Serial.println("Mengirim ke Wemos: " + jsonString); // Debugging
    Serial2.println(jsonString); // Kirim ke Wemos
    Serial2.println("Hello Wemos");
}
