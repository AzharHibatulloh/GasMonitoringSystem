#define MQ_9 35
#define MQ_135 33
#define MQ_137 34

#define CALIBRATION_SAMPLE_TIMES 50
#define CALIBRATION_SAMPLE_INTERVAL 500
#define Vc 5.0 
#define R_L 10000 

const float A_9 = 4265.95;
const float A_135 = 109.691;
const float A_137 = 38.11;

const float B_9 = -2.39268;
const float B_135 = -2.37977;
const float B_137 = -3.30048;

float calibrateSensor(int pin, float rat);

float R0_9, R0_135, R0_137;
int waktuCal = 120000;
int tempCal = 0, count = 0;
float sensorVolt, Rs_air, R0, ppm, sensorValue = 0;

void setup() {
  // put your setup code here, to run once:
  pinMode(MQ_9, INPUT);
  pinMode(MQ_135, INPUT);
  pinMode(MQ_137, INPUT);
  Serial.begin(9600);
  Serial.println("Starting Calibration");

  delay(20000);  
}

void loop() {
      // Serial.println(analogRead(MQ_137));
  while (count != 5) {
    if (millis() - tempCal >= 120000) {
      tempCal = millis();
      // Serial.println(tempCal);
      
      R0_9 = calibrateSensor(MQ_9, 9.75674181693632);
      R0_135 = calibrateSensor(MQ_135, 3.5938136638046285);
      R0_137 = calibrateSensor(MQ_137, 3.568711295956227);
      
      Serial.print("R0  9 = ");
      Serial.println(R0_9);
      Serial.print("R0  135 = ");
      Serial.println(R0_135);
      Serial.print("R0  137 = ");
      Serial.println(R0_137);
      
      count++;
      Serial.println("============================");
    }
  }
}

float calibrateSensor(int pin, float rat) { 
  float sensorValue = 0;
  
  for (int i = 0; i < CALIBRATION_SAMPLE_TIMES; i++) {
    sensorValue += analogRead(pin);
    delay(CALIBRATION_SAMPLE_INTERVAL);
  }
  
  sensorValue = sensorValue / CALIBRATION_SAMPLE_TIMES;  // calculate the average
  
  // Konversi nilai ADC ke tegangan sensor
  sensorVolt = sensorValue * (Vc / 4095.0);  3V
  
  // Cek apakah sensorVolt bernilai 0 untuk mencegah pembagian dengan nol
  if (sensorVolt == 0) {
    Serial.println("Error: sensorVolt = 0, check sensor connections.");
    return INFINITY;  // Kembalikan inf jika sensorVolt 0 (mengindikasikan kesalahan)
  }

  // Hitung Rs_air menggunakan rumus yang benar
  Rs_air = ((Vc - sensorVolt) / sensorVolt) * R_L;
  
  // Cek apakah Rs_air valid
  if (Rs_air < 0 || isnan(Rs_air) || isinf(Rs_air)) {
    Serial.println("Error: Invalid Rs_air value.");
    return INFINITY;  // Kembalikan inf jika Rs_air tidak valid
  }

  // Hitung nilai R0 (referensi dalam udara bersih)
  R0 = Rs_air / rat;

  return R0;
}
