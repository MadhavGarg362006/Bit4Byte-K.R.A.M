/*
  K.R.A.M. Synthetic Drone-Only Telemetry Generator
  --------------------------------------------------
  Purpose:
    1. Test the K.R.A.M. dashboard + Render telemetry bridge.
    2. Generate structured synthetic radar-like samples.
    3. Simulate a DRONE-ONLY classifier:
         targetClass = 2  -> DRONE ACCEPTED
         targetClass = 0  -> REJECTED / NO DRONE
    4. Generate labelled synthetic data for later ML-pipeline testing.

  IMPORTANT:
    This is SYNTHETIC data. It is not real radar data and must not be
    reported as experimental detection accuracy.

  Required Arduino libraries:
    - WiFi (ESP32 core)
    - WebSocketsClient by Links2004

  Render server protocol:
    wss://kram-telemetry-server.onrender.com/ws?role=device&token=YOUR_DEVICE_TOKEN
*/

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <math.h>

// ========================= USER SETTINGS =========================

const char* WIFI_SSID     = "_____";
const char* WIFI_PASSWORD = "______";

// Put the DEVICE_TOKEN you created in Render here.
// Do NOT use the VIEWER_TOKEN.
const char* DEVICE_TOKEN  = "__________________";

const char* SERVER_HOST = "kram-telemetry-server.onrender.com";
const uint16_t SERVER_PORT = 443;
const char* SERVER_PATH = "/ws";

// How often to create a synthetic radar sample.
const uint32_t SAMPLE_INTERVAL_MS = 1500;

// Set true to automatically cycle through all test modes.
// Set false to use TEST_MODE below.
const bool AUTO_CYCLE = true;

// Manual mode when AUTO_CYCLE == false.
enum TestMode {
  MODE_BACKGROUND = 0,
  MODE_DRONE      = 1,
  MODE_HUMAN      = 2,
  MODE_BIRD       = 3,
  MODE_VEHICLE    = 4
};

TestMode TEST_MODE = MODE_DRONE;

// Number of samples generated for each automatic-cycle scenario.
const uint32_t SAMPLES_PER_MODE = 10;

// ================================================================

WebSocketsClient webSocket;

static const int NUM_SECTORS = 36;
static const float SECTOR_STEP_DEG = 10.0f;

uint32_t sampleNumber = 0;
uint32_t modeSample = 0;
TestMode currentMode = MODE_BACKGROUND;

unsigned long lastSample = 0;

// Slowly moving target angle gives a radar-like temporal pattern.
float targetAngle = 25.0f;
float targetDirection = 1.0f;

// Deterministic-ish pseudo-random helper.
float noise(float amplitude) {
  return ((float)random(-1000, 1001) / 1000.0f) * amplitude;
}

float clampFloat(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

const char* modeName(TestMode mode) {
  switch (mode) {
    case MODE_DRONE:    return "drone";
    case MODE_HUMAN:    return "human";
    case MODE_BIRD:     return "bird";
    case MODE_VEHICLE:  return "vehicle";
    default:            return "background";
  }
}

/*
  Synthetic radar model.

  Each scenario has:
    - target angle
    - signal intensity
    - Doppler frequency
    - speed
    - SNR
    - distance

  These values are intentionally varied instead of being fixed constants.
*/

void generateScenario(
  TestMode mode,
  int sectors[NUM_SECTORS],
  float &averageFrequency,
  float &speed,
  float &snr,
  float &distance,
  float &confidence
) {
  // Start with a realistic-looking noise floor.
  for (int i = 0; i < NUM_SECTORS; i++) {
    sectors[i] = (int)clampFloat(
      3.0f + noise(4.0f),
      0.0f,
      18.0f
    );
  }

  // Background: only noise.
  if (mode == MODE_BACKGROUND) {
    averageFrequency = 0.0f;
    speed = 0.0f;
    snr = 2.0f + noise(1.5f);
    distance = 0.0f;
    confidence = 0.0f;
    return;
  }

  // Advance target angle.
  targetAngle += targetDirection * 3.0f;

  if (targetAngle > 355.0f) {
    targetAngle = 355.0f;
    targetDirection = -1.0f;
  }

  if (targetAngle < 5.0f) {
    targetAngle = 5.0f;
    targetDirection = 1.0f;
  }

  // Convert target angle to 36-sector index.
  int centerSector = ((int)(targetAngle / SECTOR_STEP_DEG)) % NUM_SECTORS;

  float baseIntensity = 50.0f;
  float targetSpeed = 20.0f;
  float doppler = 150.0f;
  float targetSnr = 15.0f;
  float targetDistance = 15.0f;
  float droneConfidence = 0.1f;

  if (mode == MODE_DRONE) {
    /*
      Drone signature:
        - fairly strong signal
        - relatively stable motion
        - stronger central sector
        - moderate/high Doppler
        - temporal movement
    */
    baseIntensity = 78.0f + noise(10.0f);
    targetSpeed = 28.0f + noise(6.0f);
    doppler = 180.0f + noise(35.0f);
    targetSnr = 22.0f + noise(4.0f);
    targetDistance = 5.0f + (float)random(0, 250) / 10.0f;
    droneConfidence = 0.90f + noise(0.07f);

  } else if (mode == MODE_HUMAN) {
    /*
      Human:
        lower/less radar-like pattern for this synthetic test.
    */
    baseIntensity = 55.0f + noise(10.0f);
    targetSpeed = 3.0f + noise(2.0f);
    doppler = 45.0f + noise(15.0f);
    targetSnr = 12.0f + noise(3.0f);
    targetDistance = 3.0f + (float)random(0, 150) / 10.0f;
    droneConfidence = 0.10f + noise(0.06f);

  } else if (mode == MODE_BIRD) {
    /*
      Bird:
        intermittent/irregular motion.
    */
    baseIntensity = 45.0f + noise(14.0f);
    targetSpeed = 18.0f + noise(10.0f);
    doppler = 110.0f + noise(50.0f);
    targetSnr = 10.0f + noise(4.0f);
    targetDistance = 8.0f + (float)random(0, 200) / 10.0f;
    droneConfidence = 0.20f + noise(0.10f);

  } else { // MODE_VEHICLE
    /*
      Vehicle:
        strong signal but slower and less drone-like temporal behaviour.
    */
    baseIntensity = 82.0f + noise(9.0f);
    targetSpeed = 12.0f + noise(4.0f);
    doppler = 85.0f + noise(20.0f);
    targetSnr = 20.0f + noise(4.0f);
    targetDistance = 10.0f + (float)random(0, 300) / 10.0f;
    droneConfidence = 0.05f + noise(0.05f);
  }

  // Add a Gaussian-ish spatial target around centerSector.
  for (int i = 0; i < NUM_SECTORS; i++) {
    int d = abs(i - centerSector);
    d = min(d, NUM_SECTORS - d);

    float contribution = 0.0f;

    if (d == 0) {
      contribution = baseIntensity;
    } else if (d == 1) {
      contribution = baseIntensity * 0.58f;
    } else if (d == 2) {
      contribution = baseIntensity * 0.25f;
    } else if (d == 3) {
      contribution = baseIntensity * 0.10f;
    }

    // Slight per-sector variation.
    contribution += noise(5.0f);

    sectors[i] = (int)clampFloat(
      sectors[i] + contribution,
      0.0f,
      100.0f
    );
  }

  averageFrequency = clampFloat(doppler, 0.0f, 500.0f);
  speed = clampFloat(targetSpeed, 0.0f, 80.0f);
  snr = clampFloat(targetSnr, 0.0f, 40.0f);
  distance = clampFloat(targetDistance, 0.0f, 100.0f);
  confidence = clampFloat(droneConfidence, 0.0f, 1.0f);
}

/*
  DRONE-ONLY DECISION

  This deliberately does NOT output human/bird/vehicle classes.

  Output:
    2 = drone accepted
    0 = rejected / no drone

  For the synthetic generator, the "classifier" uses the generated
  features. Later, replace this function with your actual ML model.
*/
int droneOnlyClassifier(
  TestMode groundTruth,
  float frequency,
  float speed,
  float snr,
  float confidence
) {
  // Background and non-drone modes are rejected.
  if (groundTruth != MODE_DRONE) {
    return 0;
  }

  /*
    Synthetic drone acceptance condition.

    This is NOT the final ML classifier.
    It simply makes the test behaviour deterministic enough to verify
    that only drone-like samples are accepted.
  */
  bool frequencyOK = frequency > 110.0f && frequency < 260.0f;
  bool speedOK     = speed > 15.0f && speed < 45.0f;
  bool snrOK       = snr > 15.0f;
  bool confidenceOK = confidence > 0.65f;

  if (frequencyOK && speedOK && snrOK && confidenceOK) {
    return 2;
  }

  return 0;
}

void sendSyntheticTelemetry() {
  int sectors[NUM_SECTORS];

  float averageFrequency = 0.0f;
  float speed = 0.0f;
  float snr = 0.0f;
  float distance = 0.0f;
  float confidence = 0.0f;

  generateScenario(
    currentMode,
    sectors,
    averageFrequency,
    speed,
    snr,
    distance,
    confidence
  );

  // ONLY drone vs reject.
  int targetClass = droneOnlyClassifier(
    currentMode,
    averageFrequency,
    speed,
    snr,
    confidence
  );

  /*
    The "groundTruth" field is retained for dataset generation.
    The dashboard-facing "targetClass" remains drone-only:
       2 = drone
       0 = rejected
  */

  String json;
  json.reserve(1600);

  json += "{";
  json += "\"timestamp\":";
  json += String(millis());
  json += ",\"sectors\":[";

  for (int i = 0; i < NUM_SECTORS; i++) {
    if (i > 0) json += ",";
    json += String(sectors[i]);
  }

  json += "],\"averageFrequency\":";
  json += String(averageFrequency, 2);

  json += ",\"targetClass\":";
  json += String(targetClass);

  json += ",\"targetSpeed\":";
  json += String(speed, 2);

  // Additional synthetic/dataset metadata.
  json += ",\"synthetic\":true";

  json += ",\"groundTruth\":\"";
  json += modeName(currentMode);
  json += "\"";

  json += ",\"scenario\":\"";
  json += modeName(currentMode);
  json += "\"";

  json += ",\"distance\":";
  json += String(distance, 2);

  json += ",\"snr\":";
  json += String(snr, 2);

  json += ",\"droneConfidence\":";
  json += String(confidence, 3);

  json += ",\"sampleNumber\":";
  json += String(sampleNumber);

  json += ",\"acceptedAsDrone\":";
  json += (targetClass == 2 ? "true" : "false");

  json += "}";

  webSocket.sendTXT(json);

  Serial.println("------------------------------------------------");
  Serial.printf(
    "Sample: %lu | GT: %-10s | Decision: %s\n",
    (unsigned long)sampleNumber,
    modeName(currentMode),
    targetClass == 2 ? "DRONE" : "REJECT"
  );

  Serial.printf(
    "Freq: %.1f Hz | Speed: %.1f | SNR: %.1f dB | Distance: %.1f m | Confidence: %.2f\n",
    averageFrequency,
    speed,
    snr,
    distance,
    confidence
  );

  sampleNumber++;
  modeSample++;

  if (AUTO_CYCLE && modeSample >= SAMPLES_PER_MODE) {
    modeSample = 0;

    if (currentMode == MODE_VEHICLE) {
      currentMode = MODE_BACKGROUND;
    } else {
      currentMode = (TestMode)((int)currentMode + 1);
    }

    Serial.println();
    Serial.println("===============================================");
    Serial.printf("SWITCHING TO MODE: %s\n", modeName(currentMode));
    Serial.println("===============================================");
  }
}

void webSocketEvent(
  WStype_t type,
  uint8_t* payload,
  size_t length
) {
  switch (type) {

    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected");
      break;

    case WStype_CONNECTED:
      Serial.println("[WS] Connected to K.R.A.M. Render server");
      Serial.println("[WS] Device authenticated");
      break;

    case WStype_TEXT:
      Serial.printf(
        "[WS] Server message: %.*s\n",
        (int)length,
        payload
      );
      break;

    case WStype_ERROR:
      Serial.println("[WS] Error");
      break;

    default:
      break;
  }
}

void connectWiFi() {
  Serial.println();
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("[WiFi] Connected");
  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  delay(5000);

  randomSeed((uint32_t)esp_random());

  Serial.println();
  Serial.println("===============================================");
  Serial.println(" K.R.A.M. SYNTHETIC DRONE-ONLY GENERATOR");
  Serial.println("===============================================");

  connectWiFi();

  // Render uses HTTPS/WSS, so port 443 is used.
  webSocket.beginSSL(
    SERVER_HOST,
    SERVER_PORT,
    String(SERVER_PATH) + "?role=device&token=" + DEVICE_TOKEN
  );

  webSocket.onEvent(webSocketEvent);

  // Keep the WebSocket alive.
  webSocket.setReconnectInterval(5000);

  Serial.println("[WS] Connecting to Render...");
  Serial.printf("[MODE] %s\n", AUTO_CYCLE ? "AUTO CYCLE" : modeName(TEST_MODE));

  currentMode = AUTO_CYCLE ? MODE_BACKGROUND : TEST_MODE;
}

void loop() {
  webSocket.loop();

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();

  if (now - lastSample >= SAMPLE_INTERVAL_MS) {
    lastSample = now;

    if (webSocket.isConnected()) {
      sendSyntheticTelemetry();
    }
  }
}
