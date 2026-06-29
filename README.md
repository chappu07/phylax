# ESP32 Smart Fingerprint Door Lock System

## Overview

This project implements a secure **ESP32-based Smart Door Lock System** using a fingerprint sensor, OLED display, Bluetooth control, and an electronic relay lock.

The system supports:

* Fingerprint authentication
* Master and user fingerprint enrollment
* Fingerprint deletion
* Bluetooth door control
* OLED user interface
* Relay-controlled electronic lock

Only authorized users can unlock the door, while administrative functions require master fingerprint authentication.

---

# Features

* Fingerprint-based door access
* Master fingerprint authentication
* User fingerprint enrollment
* Master fingerprint enrollment
* Fingerprint deletion
* OLED status display
* Bluetooth remote lock/unlock
* Two-button menu navigation
* Relay-controlled door lock
* Protection against deleting the last master fingerprint
* Enrollment timeout and cancel functionality

---

# Hardware Required

| Component                            | Quantity    |
| ------------------------------------ | ----------- |
| ESP32 Development Board              | 1           |
| Adafruit Optical Fingerprint Sensor  | 1           |
| SSD1306 OLED Display (128×64, I2C)   | 1           |
| Relay Module                         | 1           |
| Push Buttons                         | 2           |
| Electronic Door Lock / Solenoid Lock | 1           |
| Jumper Wires                         | As required |
| 5V Power Supply                      | 1           |

---

# Pin Configuration

| ESP32 Pin | Function               |
| --------- | ---------------------- |
| GPIO 16   | Fingerprint RX         |
| GPIO 17   | Fingerprint TX         |
| GPIO 25   | Relay Output           |
| GPIO 4    | Enroll / Select Button |
| GPIO 3    | Cancel / Back Button   |
| GPIO 21   | OLED SDA               |
| GPIO 22   | OLED SCL               |

---

# Required Libraries

Install the following Arduino libraries:

* Adafruit Fingerprint Sensor Library
* Adafruit SSD1306
* Adafruit GFX
* BluetoothSerial (ESP32)
* Wire

---

# System Workflow

## 1. Verification Mode

The system waits for a fingerprint.

If a registered fingerprint is detected:

* Access is granted.
* Relay unlocks the door.
* Door remains unlocked for 4 seconds.
* Door locks automatically.

If fingerprint is not recognized:

* Access is denied.
* OLED displays "Not Found."

---

## 2. Main Menu

Press the **Enroll Button** to enter the menu.

Menu options:

* Enroll Master
* Enroll User
* Delete Fingerprint

Navigation:

* Enroll Button → Select option
* Cancel Button → Next option

---

## 3. Master Enrollment

Requires authentication using an existing master fingerprint.

Master IDs are reserved:

* ID 0
* ID 1
* ID 2

If a master ID already exists, the user can:

* Skip
* Exit

---

## 4. User Enrollment

Requires master authentication.

The system automatically assigns the next available fingerprint ID starting from ID 3.

Enrollment process:

1. Place finger.
2. Lift finger.
3. Place finger again.
4. Fingerprint template is stored.

---

## 5. Fingerprint Deletion

Requires master authentication.

Features:

* Browse fingerprint IDs.
* Long press Enroll button to delete.
* Additional confirmation for deleting master fingerprints.
* Prevents deletion of the last remaining master fingerprint.

---

## 6. Bluetooth Control

ESP32 creates a Bluetooth device named:

```
ESP32_BT_Test
```

Commands:

| Command | Action                    |
| ------- | ------------------------- |
| 1       | Unlock door for 4 seconds |
| 0       | Lock door                 |

---

# OLED Display

The OLED provides real-time feedback such as:

* Waiting Finger
* Access Granted
* Access Denied
* Enroll User
* Enroll Master
* Delete Fingerprint
* Timeout
* Finger Not Found

The display also shows simple success and failure emoji icons.

---

# Fingerprint ID Allocation

| ID Range | Purpose           |
| -------- | ----------------- |
| 0        | Master 1          |
| 1        | Master 2          |
| 2        | Master 3          |
| 3–127    | User Fingerprints |

---

# Security Features

* Master authentication required for administrative operations.
* Prevents unauthorized enrollment.
* Prevents unauthorized fingerprint deletion.
* Prevents deletion of the final master fingerprint.
* Automatic timeout during enrollment and verification.
* Relay automatically locks after access.

---
# Applications

* Smart Home Door Lock
* Office Access Control
* Laboratory Security
* Hostel Room Access
* Industrial Restricted Areas
* Smart Locker System

---
