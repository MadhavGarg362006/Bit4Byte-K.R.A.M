# 360° Doppler Radar — ESP32-S3

> A real-time embedded Doppler radar system built around the HB100 microwave radar sensor, combining analog signal conditioning, comparator-based digitization, embedded FFT/frequency processing, cloud data transmission, and a real-time web dashboard.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

---

## 📡 Project Overview

This project builds a **360° Doppler radar system** using the **HB100 microwave Doppler radar sensor** (10.525 GHz) and an **ESP32-S3**.

The HB100's raw Doppler output is far too weak and noisy to use directly, so the project implements a full analog front end — amplification, 4th-order active band-pass filtering, and further amplification — before converting the conditioned signal to a digital pulse train using a comparator. The ESP32-S3 handles digital filtering, FFT, and frequency extraction on that signal, drives a local OLED display, and sends processed data over Wi-Fi to **ThingSpeak**, which acts as the cloud/API layer. A custom web dashboard pulls this data and renders it as a real-time 360° radar display.

The longer-term goal is to extend the system toward **ML-based object classification** and richer spatial visualization.

This project is **open source** — see [License](#-license) below.

---

## 🎯 Objectives

* Build a working Doppler radar around the HB100
* Design an analog front end suited to the HB100's weak output
* Amplify and band-pass filter the signal (target band: 70 Hz – 2000 Hz)
* Digitize the conditioned signal and process it on the ESP32-S3
* Detect targets based on signal characteristics
* Represent detections across a 360° field using multiple sectors
* Send radar data to the cloud via ThingSpeak
* Build a real-time web dashboard
* Eventually classify detected objects using machine learning
* Eventually add a heat-map visualization

---

## 🧠 System Architecture

```text
                         HB100 RADAR SENSOR
                                │
                                ▼
                       Coupling Capacitor
                                │
                                ▼
                         Analog Gain ×10
                                │
                                ▼
                   4th-Order Active Band-Pass
                             Filter
                                │
                                ▼
                         High-Gain ×10
                                │
                                ▼
                           Comparator
                                │
                                ▼
                           ESP32-S3
                                │
                         ┌──────┴──────┐
                         │             │
                         ▼             ▼
                       OLED       Signal Processing
                       Display     • Digital Filtering
                         │          • FFT
                         │          • Frequency
                         │            Extraction
                         │          • Target Detection
                         │          • ML Classification
                         │
                         └──────┬──────┘
                                │
                           ESP32 Wi-Fi
                                │
                                ▼
                           ThingSpeak
                                │
                                ▼
                         ThingSpeak API
                                │
                                ▼
                         Web Dashboard
                                │
                ┌───────────────┼───────────────┐
                ▼               ▼               ▼
             360° Radar      FFT Plot        Heat Map
                                │
                                ▼
                       ML Classification
```

**Signal-Processing Architecture**

The overall analog signal path:
```text
HB100 → Coupling → Gain ×10 → 4th-Order Active BPF → Gain ×10 → Comparator → ESP32-S3
```

The ESP32-S3 then handles digital processing and system-level functions:
```text
ESP32-S3 → Digital Filtering → FFT → Frequency Extraction → Detection / ML → OLED + Wi-Fi
```

The OLED provides local real-time system information, while the Wi-Fi connection sends processed data to ThingSpeak for the web dashboard.

---

## 🔧 Hardware Signal Chain

**1. HB100 Doppler Radar Sensor** — 10.525 GHz microwave source/receiver. Motion of a target relative to the sensor produces a Doppler-shifted component in the reflected signal.

**2. AC Coupling** — removes the DC offset so only the Doppler component is processed downstream.

**3. First Gain Stage — ×10** — brings the weak radar signal up to a level usable by the filter stage.

**4. 4th-Order Active Band-Pass Filter** — passes 70 Hz – 2000 Hz, the target range for the Doppler frequencies of interest, and rejects out-of-band noise before the second gain stage amplifies further.

**5. Second Gain Stage — ×100** — brings the filtered signal up to a level suitable for clean digitization.

**6. Comparator (LM311)** — converts the conditioned analog waveform into a clean digital pulse train, referenced against a threshold, for the ESP32 to process.

### Op-amps / ICs used
* TL072CP (dual op-amp) × 2 → 4 op-amp stages
* LM311 (comparator) × 1 → digitization stage

### Passive component pool
Resistors and capacitors used across the gain/filter stages, mixed in different combinations per stage:

* Capacitors: 1 µF, 10 µF, 3.3 nF, 10 nF, 4.7 nF, 0.1 µF
* Resistors: 100 Ω, 330 Ω, 220 Ω, 1 kΩ, 4.7 kΩ, 10k ,68k

*(Full per-stage BOM with exact values per filter section — TBD,)*

---

## 🧮 Embedded Digital Processing

```text
Comparator Pulse Train
         │
         ▼
   ESP32-S3 GPIO
         │
         ▼
  Digital Filtering
         │
         ▼
        FFT
         │
         ▼
Frequency Extraction
         │
         ▼
Target Detection / ML
         │
    ┌────┴────┐
    ▼         ▼
  OLED     Wi-Fi → ThingSpeak
```

---

## 🛰️ 360° Radar Representation

The dashboard divides the radar view into **36 sectors** (360° / 36 = 10° per sector), from Sector 0 (0°) to Sector 35 (350°). Each sector holds an intensity value, which the dashboard uses to determine:

* Detection intensity
* Strongest sector / angle
* Number of active sectors
* Target positions

---

## ☁️ ThingSpeak Integration

ThingSpeak is the cloud/data layer between the ESP32 and the dashboard, accessed via the standard ThingSpeak REST API.

**Current fields:**
```text
Field 1 → 36-sector intensity data (CSV)
Field 2 → Average Doppler frequency
```

**Update interval:** 5 seconds

**Planned future fields:**
```text
Field 3 → Velocity
Field 4 → FFT peak
Field 5 → Object classification
Field 6 → ML confidence
Field 7 → Position / angle
Field 8 → Detection status
```

---

## 🖥️ Web Dashboard

A custom HTML/CSS/JavaScript dashboard (rendered on `<canvas>`) provides:

* 360° radar display with animated sweep
* 36 detection sectors
* Signal-intensity visualization
* Maximum signal intensity
* Average Doppler frequency
* Strongest detection angle
* Active-sector count
* Detected-sector list
* ThingSpeak connection status
* Real-time updates (5s interval)

The dashboard is designed to be extended rather than replaced as new fields (velocity, classification, etc.) come online.

---

## 🔌 ESP32-S3 Pin Reference

```cpp
#define SIGNAL_PIN 4   // Comparator output → ESP32 GPIO
#define OLED_SDA   8
#define OLED_SCL   9
// Servo / additional pins: TBD — placeholders, to be finalized
```

---

## 🤖 Machine Learning — Planned

```text
Digitized Signal → Feature Extraction (incl. FFT) → ML Model → Object Classification
```

Classes will be defined from measured radar signatures rather than assumed in advance.

---

## 🔥 Heat Map — Planned

Will combine sector, signal intensity, Doppler frequency, and detection history over time to show how the radar field changes, not just instantaneous detection.

---

## 🧪 Development Approach — Phased

| Phase | Focus |
|---|---|
| Software |
|---|---|
| 1 | Simulating Circuit as planned |
| 2 | Acquiring component libraries in software Proteus/KiCAD |
| 3 | Testing theoretical values on DSO |
| Hardware |
|---|---|
| 1 | Radar signal acquisition + amplification |
| 2 | Analog conditioning (×10 → 4th-order BPF → ×10) |
| 3 | Comparator digitization + embedded digital filtering / FFT / frequency extraction |
| 4 | OLED local display + ThingSpeak + 36-sector web dashboard |
| 5 | ML feature extraction + classification |
| 6 | Heat map + advanced target tracking |

---

## 📁 Repository Structure

```text
AI-Based-Doppler-Radar-Surveillance-System/
│
├── ESP32/
│   └── doppler_radar.ino
│
├── Dashboard/
│   ├── index.html
│   ├── style.css
│   └── script.js
│
├── ML/
│   ├── dataset/
│   ├── training/
│   └── model/
│
├── Hardware/
│   ├── schematic/
│   ├── circuit-diagram/
│   └── PCB/
│
├── Documentation/
│
├── Images/
│
├── LICENSE
│
└── README.md
```

---

## 🛠️ Technologies & Components

**Hardware:** HB100 (10.525 GHz), ESP32-S3, TL072CP ×2, LM311 comparator, 4th-order active band-pass filter, analog gain stages, OLED display, supporting passives

**Embedded:** C/C++, ESP32-S3, digital filtering, FFT, frequency extraction, Wi-Fi

**Signal Processing:** Analog amplification, 4th-order active band-pass filtering, comparator-based digitization, embedded FFT, frequency-domain peak detection

**Cloud & Dashboard:** ThingSpeak, ThingSpeak REST API, HTML, CSS, JavaScript, HTML Canvas

**Machine Learning (planned):** Dataset generation, feature extraction, model training, object classification

---

## 🚧 Current Status

| Component | Status |
|---|---|
| Circuit Simulation |  ✅ Done | 🟡 refining |
| HB100 radar sensing | 🔵 Planned |
| Analog amplification (×10 / ×100) | 🔵 Planned|
| 4th-order active band-pass filter | 🔵 Planned |
| Comparator digitization | 🔵 Planned |
| Digital filtering + FFT + frequency extraction | 🔵 Planned|
| OLED display | 🔵 Planned |
| ThingSpeak communication | 🟡 In Progress  |
| 36-sector radar dashboard | 🟡 In Progress  |
| Real-time dashboard | 🟡 In Progress |
| Velocity estimation | 🔵 Planned |
| Servo / pin finalization | 🟡 In Progress |
| ML dataset | 🟡 In Progress |
| ML classification | 🟡 In Progress |
| Dedicated heat map | 🔵 Planned |
| Advanced target tracking | 🔵 Planned |

---

<h2>📸 Project Images</h2>



## 📌 Why This Project?

The goal isn't just to detect motion with an HB100 — it's to work through the full pipeline from a raw microwave Doppler signal to meaningful information:

**Sensor → Analog Electronics → Comparator Digitization → Embedded FFT/Frequency Extraction → Cloud Communication → Visualization → (Planned: ML Classification)**

This combines embedded systems, analog electronics, signal processing, IoT, and data visualization — with machine learning as the next stage.

---

## 📄 License

This project is licensed under the **MIT License** — see [LICENSE](LICENSE) for details. You're free to use, modify, and distribute this project, including commercially, as long as the original copyright and license notice are included.

---

## ⚠️ Note
 This repo will be updated as those stages are built.
