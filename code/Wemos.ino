#include <Wire.h>
#include <FirebaseESP8266.h>
#include <ArduinoJson.h>

const char* ssid = " ";
const char* password = "";

FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

String kualitas, pred_Kualitas;
float ppmNH3 = 0, ppmCH4 = 0, ppmCO2 = 0;
float pred_amonia = 0, pred_metana = 0, pred_co2 = 0;

void setup() {
    Serial.begin(9600); 

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Menghubungkan ke Wi-Fi...");
    }
    Serial.println("Terhubung ke Wi-Fi.");

    firebaseConfig.host = "";
    firebaseConfig.signer.tokens.legacy_token = "";

    Firebase.begin(&firebaseConfig, &firebaseAuth);

    if (Firebase.ready()) {
        Serial.println("Firebase connected!");
    } else {
        Serial.println("Firebase connection failed!");
    }
    delay(2000);
}

void loop() {
    if (Serial.available()) {
        String jsonData = Serial.readStringUntil('\n');
        Serial.println("Data diterima dari ESP32: " + jsonData); 
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, jsonData);
        if (error) {
            Serial.print("Gagal mem-parsing JSON: ");
            Serial.println(error.c_str());
            return;
        }
        String receivedData = Serial.readStringUntil('\n');
        Serial.println("Data diterima: ");     

        ppmNH3 = doc["ppmNH3"];
        ppmCH4 = doc["ppmCH4"];
        ppmCO2 = doc["ppmCO2"];
        pred_amonia = doc["pred_amonia"];
        pred_metana = doc["pred_metana"];
        pred_co2 = doc["pred_co2"];
        kualitas = doc["kualitas"].as<String>();
        pred_Kualitas = doc["pred_Kualitas"].as<String>();

        Serial.printf("NH3 : %f \n", ppmNH3);
        Serial.printf("CH4 : %f \n", ppmCH4);
        Serial.printf("CO2 : %f \n", ppmCO2);
        Serial.printf("pred_amo : %f \n", pred_amonia);
        Serial.printf("pred_met : %f \n", pred_metana);
        Serial.printf("pred_co2 : %f \n", pred_co2);
        Serial.printf("Kualitas : %s \n", kualitas.c_str());
        Serial.printf("pred_kualitas : %s \n", pred_Kualitas.c_str()); 
    }



    sendFirebase();

    delay(1000);
}

void sendFirebase() {
    if (Firebase.ready()) {
        FirebaseJson batchData;
        batchData.set("Amonia", ppmNH3);
        batchData.set("Metana", ppmCH4);
        batchData.set("CO2", ppmCO2);
        batchData.set("Val_RW03", kualitas);
        batchData.set("pred_Amonia", pred_amonia);
        batchData.set("pred_Metana", pred_metana);
        batchData.set("pred_CO2", pred_co2);
        batchData.set("pred_RW03", pred_Kualitas);

        unsigned long startTime = millis();

        if (Firebase.setJSON(firebaseData, "/RW_03", batchData)) {
            // Menghitung waktu delay
            unsigned long endTime = millis();
            unsigned long delayTime = endTime - startTime;

//            Serial.println("Data berhasil diunggah ke Firebase tanpa folder acak");
            Serial.printf("Waktu yang dibutuhkan: %lu ms\n", delayTime);
        } else {
            Serial.print("Gagal mengirim data: ");
            Serial.println(firebaseData.errorReason());
        }
    } else {
        Serial.println("Firebase tidak siap");
    }
}
