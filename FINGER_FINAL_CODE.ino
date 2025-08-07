#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define ENROLL_BUTTON 4   // Used for Select/Next/Confirm
#define CANCEL_BUTTON 3   // Used for Back/Exit/Cancel
#define RELAY_PIN 25

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

HardwareSerial mySerial(1); // For Fingerprint (RX=16, TX=17)
Adafruit_Fingerprint finger(&mySerial);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Global State Variables ---
bool enrollmentMode = false;
bool masterEnrollMode = false;
bool deleteMode = false; // Track delete mode
int nextID = 3; // IDs 0,1,2 are reserved for master
const int masterIDs[] = {0, 1, 2};
const int NUM_MASTERS = sizeof(masterIDs) / sizeof(masterIDs[0]);
bool lockState = true; // true = locked (HIGH), false = unlocked (LOW)

// --- Debounce Variables ---
unsigned long lastEnrollButtonPressTime = 0;
unsigned long lastCancelButtonPressTime = 0;
const unsigned long debounceDelay = 150; // milliseconds

// --- Menu States ---
enum MenuState {
  STATE_VERIFY,
  STATE_MAIN_MENU,
  STATE_ENROLL_MASTER_AUTH,
  STATE_ENROLL_MASTER_MODE,
  STATE_ENROLL_USER_AUTH,
  STATE_ENROLL_USER_MODE,
  STATE_DELETE_AUTH,
  STATE_DELETE_MODE
};

MenuState currentMenuState = STATE_VERIFY; // Initial state

// --- Function Prototypes ---
void showMessage(String msg);
void showMessageWithEmoji(String msg, bool success);
bool verifyMaster();
bool isIDOccupied(int id);
int enrollFingerprint(int id);

// Menu Handlers
void handleVerifyState();
void handleMainMenuState();
void handleEnrollMasterAuth();
void handleEnrollMasterMode();
void handleEnrollUserAuth();
void handleEnrollUserMode();
void handleDeleteAuth();
void handleDeleteMode();

// Helper for button presses in menus
int getButtonPress(unsigned long timeout_ms); // Returns button pin, or 0 on timeout


// --- Emoji Bitmaps (16x16 pixel examples) ---
const unsigned char PROGMEM smile_emoji_bmp[] =
{ B00000000, B00000000,
  B00000110, B00000000,
  B00001001, B00000000,
  B00010000, B10000000,
  B00100000, B01000000,
  B01000000, B00100000,
  B01000000, B00100000,
  B01000000, B00100000,
  B00100000, B01000000,
  B00010000, B10000000,
  B00001001, B00000000,
  B00000110, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000 };

const unsigned char PROGMEM sad_emoji_bmp[] =
{ B00000000, B00000000,
  B00000110, B00000000,
  B00001001, B00000000,
  B00010000, B10000000,
  B00100000, B01000000,
  B01000000, B00100000,
  B01000000, B00100000,
  B00101000, B01000000,
  B00010100, B00000000,
  B00001010, B00000000,
  B00000101, B00000000,
  B00000010, B10000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000,
  B00000000, B00000000 };


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- Starting Setup ---");

  pinMode(ENROLL_BUTTON, INPUT_PULLUP);
  pinMode(CANCEL_BUTTON, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  // For active-low relay: HIGH = OFF (locked), LOW = ON (unlocked)
  digitalWrite(RELAY_PIN, HIGH); // Ensure relay is OFF (locked) initially
  lockState = true;

  mySerial.begin(57600, SERIAL_8N1, 16, 17); // RX=16, TX=17
  Serial.println("HardwareSerial for fingerprint sensor initialized.");

  finger.begin(57600);
  Serial.println("Attempting to connect to fingerprint sensor...");
  if (!finger.verifyPassword()) {
    Serial.println("ERROR: Fingerprint sensor not found or password incorrect!");
    while (1); // Halt if sensor not found
  }
  Serial.println("Fingerprint sensor connected and verified.");

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("ERROR: OLED display initialization failed!");
    while (1); // Halt if OLED not found
  }
  display.display();
  delay(1000);
  Serial.println("OLED display initialized.");

  showMessage("Initializing...");
  delay(1000);
  showMessage("Ready\nWaiting...");
  Serial.println("Setup complete. Waiting for finger...");
}

void loop() {
  // --- Check for button presses for menu navigation (outside of current state handler) ---
  if (currentMenuState == STATE_VERIFY) {
    if (digitalRead(ENROLL_BUTTON) == LOW) { // Short press to enter main menu
      if (millis() - lastEnrollButtonPressTime > debounceDelay) {
        lastEnrollButtonPressTime = millis();
        currentMenuState = STATE_MAIN_MENU;
        Serial.println("Entered Main Menu");
        delay(300); // Add a small delay to prevent rapid re-triggering
      }
    }
  } else { // In any other mode, CANCEL button can typically take you back or out
    if (digitalRead(CANCEL_BUTTON) == LOW) {
      if (millis() - lastCancelButtonPressTime > debounceDelay) {
        lastCancelButtonPressTime = millis();
        // Specific handling for modes that need to revert flags
        if (enrollmentMode || masterEnrollMode || deleteMode) {
            enrollmentMode = false;
            masterEnrollMode = false;
            deleteMode = false;
            showMessage("Verify Mode"); // Show message before going back to STATE_VERIFY
            delay(500);
            currentMenuState = STATE_VERIFY;
        } else if (currentMenuState != STATE_VERIFY) { // If not verify, go back to verify
            currentMenuState = STATE_VERIFY;
            showMessage("Verify Mode");
            delay(500);
        }
      }
    }
  }

  // --- State Machine for Menu Navigation ---
  switch (currentMenuState) {
    case STATE_VERIFY:
      handleVerifyState();
      break;
    case STATE_MAIN_MENU:
      handleMainMenuState();
      break;
    case STATE_ENROLL_MASTER_AUTH:
      handleEnrollMasterAuth();
      break;
    case STATE_ENROLL_MASTER_MODE:
      handleEnrollMasterMode();
      break;
    case STATE_ENROLL_USER_AUTH:
      handleEnrollUserAuth();
      break;
    case STATE_ENROLL_USER_MODE:
      handleEnrollUserMode();
      break;
    case STATE_DELETE_AUTH:
      handleDeleteAuth();
      break;
    case STATE_DELETE_MODE:
      handleDeleteMode();
      break;
  }

  delay(10); // A small delay in the main loop
}

bool verifyMaster() {
  showMessage("Scan Master\n(5s Timeout)"); // Added message for master scan
  unsigned long start = millis();
  while (millis() - start < 5000) { // 5 second timeout for master scan
    if (digitalRead(CANCEL_BUTTON) == LOW) { // Allow cancelling master verification
      delay(50); while(digitalRead(CANCEL_BUTTON) == LOW); // Wait for release
      showMessage("Master Auth\nCancelled"); // Indicate cancellation
      delay(1000); // Give user time to see message
      return false;
    }
    if (finger.getImage() == FINGERPRINT_OK) {
      if (finger.image2Tz() == FINGERPRINT_OK && finger.fingerSearch() == FINGERPRINT_OK) {
        int id = finger.fingerID;
        for (int i = 0; i < NUM_MASTERS; i++) {
          if (id == masterIDs[i]) {
                showMessageWithEmoji("Access Granted", true); // Confirm success
                delay(1000); // Keep message on screen for 1 second
            return true; // Master found
          }
        }
        showMessageWithEmoji("Not a Master", false); // Finger found, but not a master
        delay(1000);
        return false;
      } else {
        showMessageWithEmoji("No Match", false); // Finger found, but no match
        delay(1000);
      }
    }
    delay(10); // Small delay to prevent busy-waiting
  }
  showMessage("Master Auth\nTimeout");
  delay(1000);
  return false; // Timeout
}

bool isIDOccupied(int id) {
  uint8_t p = finger.loadModel(id);
  // FINGERPRINT_OK means a template exists at that ID
  return (p == FINGERPRINT_OK);
}


void handleVerifyState() {
  showMessage("Waiting Finger...");
  unsigned long start = millis();
  while (millis() - start < 5000) { // Wait for 5 seconds for a finger
    // Check for fingerprint
    if (finger.getImage() == FINGERPRINT_OK) {
      if (finger.image2Tz() == FINGERPRINT_OK && finger.fingerSearch() == FINGERPRINT_OK) {
        int id = finger.fingerID;
        showMessageWithEmoji("Access Granted\nID: " + String(id), true);
        digitalWrite(RELAY_PIN, LOW); // Unlock (assuming LOW activates the relay)
        lockState = false;
        delay(4000); // Keep open for 4 seconds
        digitalWrite(RELAY_PIN, HIGH); // Relock
        lockState = true;
      } else {
        showMessageWithEmoji("Not Found", false);
      }
      delay(1000);
      return; // Return to loop to allow button check for menu
    }

    // Check for ENROLL_BUTTON to enter menu (handled in main loop for STATE_VERIFY)
    if (digitalRead(ENROLL_BUTTON) == LOW && millis() - lastEnrollButtonPressTime > debounceDelay) {
        // This will be caught by the general button check in loop() and change state
        return;
    }
    delay(10); // Small delay to prevent busy-waiting
  }
  // If 5 seconds pass without a finger
  showMessage("No Finger\nDetected");
  delay(1000);
}


void handleMainMenuState() {
  static int selectedOption = 0;
  const char* menuOptions[] = {"Enroll Master", "Enroll User", "Delete Finger"};
  const int numOptions = sizeof(menuOptions) / sizeof(menuOptions[0]);

  showMessage(String(menuOptions[selectedOption]) + "\nENR=Select CAN=Next");

  int buttonPressed = getButtonPress(10000); // Wait for 10 seconds for user input

  if (buttonPressed == ENROLL_BUTTON) { // Select
    switch (selectedOption) {
      case 0: // Enroll Master
        currentMenuState = STATE_ENROLL_MASTER_AUTH;
        break;
      case 1: // Enroll User
        currentMenuState = STATE_ENROLL_USER_AUTH;
        break;
      case 2: // Delete Finger
        currentMenuState = STATE_DELETE_AUTH;
        break;
    }
    Serial.println("Selected: " + String(menuOptions[selectedOption]));
  } else if (buttonPressed == CANCEL_BUTTON) { // Next Option
    selectedOption = (selectedOption + 1) % numOptions;
    Serial.println("Next option: " + String(menuOptions[selectedOption]));
  } else if (buttonPressed == 0) { // Timeout
    showMessage("Timeout\nVerify Mode");
    delay(500);
    currentMenuState = STATE_VERIFY;
  }
}

void handleEnrollMasterAuth() {
  showMessage("Scan Master\nto Enroll Master");
  if (verifyMaster()) {
    // The verifyMaster() function now includes a delay for "Access Granted"
    masterEnrollMode = true; // Set flag for enrollment function
    currentMenuState = STATE_ENROLL_MASTER_MODE;
  } else {
    showMessage("Access Denied"); // This message will appear if verifyMaster returns false
    delay(1000); // Add delay after "Access Denied"
    currentMenuState = STATE_MAIN_MENU; // Go back to main menu
  }
}

void handleEnrollMasterMode() {
  for (int i = 0; i < NUM_MASTERS; i++) {
    if (!masterEnrollMode) return; // Exit if mode cancelled externally (e.g., by CANCEL button)

    int id = masterIDs[i];
    if (isIDOccupied(id)) {
      showMessage("Master ID " + String(id) + "\nAlready Exists\nENR=Skip CAN=Exit");
      int button = getButtonPress(10000);
      if (button == ENROLL_BUTTON) {
        continue; // Skip to next master ID
      } else if (button == CANCEL_BUTTON || button == 0) { // Cancel or Timeout
        masterEnrollMode = false;
        showMessage("Enroll Master\nCancelled");
        delay(1000);
        currentMenuState = STATE_MAIN_MENU; // Go back to main menu
        return;
      }
    }

    showMessage("Enroll Master\nID: " + String(id));
    int result = enrollFingerprint(id);

    if (result == FINGERPRINT_OK) {
      showMessage("Master ID " + String(id) + "\nEnrolled OK");
    } else if (result == -1) { // Cancelled by user during enrollment steps
      showMessage("Enroll Master\nCancelled");
      masterEnrollMode = false;
      delay(1000);
      currentMenuState = STATE_MAIN_MENU;
      return;
    } else {
      showMessage("Master ID " + String(id) + "\nEnroll Failed (" + String(result) + ")");
    }

    delay(1000);
    if (i < NUM_MASTERS - 1) { // If not the last master, ask to continue
      showMessage("ENR=Next Master\nCAN=Exit");
      int button = getButtonPress(10000);
      if (button == CANCEL_BUTTON || button == 0) { // Cancel or Timeout
        masterEnrollMode = false;
        currentMenuState = STATE_MAIN_MENU;
        return;
      }
    }
  }
  masterEnrollMode = false; // All master IDs processed or exited
  showMessage("Master Setup\nComplete");
  delay(1000);
  currentMenuState = STATE_MAIN_MENU;
}


void handleEnrollUserAuth() {
  showMessage("Scan Master\nto Enroll User");
  if (verifyMaster()) {
    // The verifyMaster() function now includes a delay for "Access Granted"
    enrollmentMode = true; // Set flag for enrollment function
    currentMenuState = STATE_ENROLL_USER_MODE;
  } else {
    showMessage("Access Denied"); // This message will appear if verifyMaster returns false
    delay(1000); // Add delay after "Access Denied"
    currentMenuState = STATE_MAIN_MENU; // Go back to main menu
  }
}

void handleEnrollUserMode() {
  while (enrollmentMode) { // Loop as long as enrollmentMode is true
    // Find the next available ID starting from nextID
    bool foundEmptyID = false;
    int tempID = nextID;
    for (int i = tempID; i < 128; i++) { // Max 128 IDs for this sensor typically
      // Master IDs are 0, 1, 2. Ensure we don't try to enroll normal users into these.
      bool isMasterID = false;
      for (int j = 0; j < NUM_MASTERS; j++) {
          if (i == masterIDs[j]) {
              isMasterID = true;
              break;
          }
      }
      if (!isMasterID && !isIDOccupied(i)) {
        nextID = i;
        foundEmptyID = true;
        break;
      }
    }
    if (!foundEmptyID) {
      showMessage("No Empty IDs\nAvailable!");
      delay(2000);
      enrollmentMode = false;
      currentMenuState = STATE_MAIN_MENU;
      return;
    }

    showMessage("Enroll User\nID: " + String(nextID) + "\nCAN=Exit");
    int result = enrollFingerprint(nextID);

    if (result == FINGERPRINT_OK) {
      showMessage("ID " + String(nextID) + "\nEnrolled OK");
      nextID++; // Increment for the next enrollment
    } else if (result == -1) { // Cancelled by user
      showMessage("Enroll Cancelled");
      enrollmentMode = false;
      delay(1000);
      currentMenuState = STATE_MAIN_MENU;
      return;
    } else {
      showMessage("ID " + String(nextID) + "\nEnroll Failed (" + String(result) + ")");
    }

    delay(1000);
    showMessage("ENR=Next User\nCAN=Exit");

    int button = getButtonPress(10000);
    if (button == CANCEL_BUTTON || button == 0) { // Cancel or Timeout
      enrollmentMode = false;
      currentMenuState = STATE_MAIN_MENU;
      return;
    }
  }
  currentMenuState = STATE_MAIN_MENU; // Should be handled by the loop above, but as a fallback
}


void handleDeleteAuth() {
  showMessage("Scan Master\nto Delete Finger");
  if (verifyMaster()) {
    // The verifyMaster() function now includes a delay for "Access Granted"
    deleteMode = true; // Set flag for deletion function
    currentMenuState = STATE_DELETE_MODE;
  } else {
    showMessage("Access Denied"); // This message will appear if verifyMaster returns false
    delay(1000); // Add delay after "Access Denied"
    currentMenuState = STATE_MAIN_MENU; // Go back to main menu
  }
}

void handleDeleteMode() {
  int idToDelete = 0; // Start from ID 0 for deletion
  // Ensure we don't start at an ID that is always a master if we want to prompt for next non-master by default
  // For simplicity, we'll iterate through all IDs 0-127.
  // The logic for 'Cannot Delete Last Master' still applies.

  while (deleteMode) { // Loop as long as deleteMode is true
    showMessage("Del ID: " + String(idToDelete) + "\nENR=Next CAN=Exit\nENR(Hold)=Delete");

    // This section now waits for a button press with explicit handling for short vs. long press of ENROLL_BUTTON
    unsigned long waitStartTime = millis();
    int detectedButton = 0;
    bool longPressDetected = false;

    while (millis() - waitStartTime < 10000) { // Timeout for overall prompt
        if (digitalRead(ENROLL_BUTTON) == LOW) {
            unsigned long enrollPressStartTime = millis();
            // Wait for button release or long press threshold
            while(digitalRead(ENROLL_BUTTON) == LOW) {
                if (millis() - enrollPressStartTime > 1000) { // 1 second for long press
                    longPressDetected = true;
                    break;
                }
                delay(10); // Small delay to prevent busy-waiting
            }
            if (longPressDetected) {
                detectedButton = ENROLL_BUTTON; // This signifies a long press for delete
                while(digitalRead(ENROLL_BUTTON) == LOW); // Wait for actual release
                delay(debounceDelay); // Debounce after release
                break;
            } else { // Short press
                if (millis() - lastEnrollButtonPressTime > debounceDelay) {
                    lastEnrollButtonPressTime = millis();
                    detectedButton = ENROLL_BUTTON;
                    while(digitalRead(ENROLL_BUTTON) == LOW); // Wait for actual release
                    delay(debounceDelay); // Debounce after release
                    break;
                }
            }
        }
        if (digitalRead(CANCEL_BUTTON) == LOW) {
            if (millis() - lastCancelButtonPressTime > debounceDelay) {
                lastCancelButtonPressTime = millis();
                detectedButton = CANCEL_BUTTON;
                while(digitalRead(CANCEL_BUTTON) == LOW); // Wait for actual release
                delay(debounceDelay); // Debounce after release
                break;
            }
        }
        delay(10); // Small delay for the while loop
    }

    if (detectedButton == ENROLL_BUTTON && !longPressDetected) { // Short press of ENROLL_BUTTON
        idToDelete++;
        if (idToDelete >= 128) idToDelete = 0; // Wrap around
    } else if (detectedButton == CANCEL_BUTTON) { // CANCEL button exits delete mode
        deleteMode = false;
        showMessage("Exit Delete\nMode");
        delay(1000);
        currentMenuState = STATE_MAIN_MENU;
        return;
    } else if (detectedButton == 0) { // Timeout
        showMessage("Timeout\nExit Delete");
        delay(1000);
        deleteMode = false;
        currentMenuState = STATE_MAIN_MENU;
        return;
    } else if (detectedButton == ENROLL_BUTTON && longPressDetected) { // Long press of ENROLL_BUTTON to confirm deletion
        // Check if it's a master ID
        bool isMaster = false;
        for (int i = 0; i < NUM_MASTERS; i++) {
          if (idToDelete == masterIDs[i]) {
            isMaster = true;
            break;
          }
        }

        if (isMaster) {
          int enrolledMasters = 0;
          for (int i = 0; i < NUM_MASTERS; i++) {
            if (finger.loadModel(masterIDs[i]) == FINGERPRINT_OK) {
              enrolledMasters++;
            }
          }
          if (enrolledMasters <= 1) {
            showMessage("Cannot Delete\nLast Master!");
            delay(1500);
            continue; // Skip deletion and continue loop to show current ID
          }
          showMessage("Confirm Delete\nMaster ID " + String(idToDelete) + "?\nHOLD ENR again");
          delay(1000); // Give time to read message
          // Second confirmation for master deletion (must long hold ENROLL again)
          unsigned long confirmStart = millis();
          bool confirmLongPress = false;
          while (digitalRead(ENROLL_BUTTON) == HIGH && millis() - confirmStart < 5000); // Wait for press for 5 sec

          if (digitalRead(ENROLL_BUTTON) == LOW) {
            unsigned long secondPressStart = millis();
            while (digitalRead(ENROLL_BUTTON) == LOW) {
              if (millis() - secondPressStart > 1000) { // Second long press confirmation
                confirmLongPress = true;
                break;
              }
            }
            if (digitalRead(ENROLL_BUTTON) == LOW) { // If still pressed (was a long press)
                while(digitalRead(ENROLL_BUTTON) == LOW); // Wait for actual release
            }
          }

          if (!confirmLongPress) {
            showMessage("Cancelled");
            delay(1000);
            continue; // Go back to showing the current ID
          }
        }

        // Perform deletion
        if (finger.deleteModel(idToDelete) == FINGERPRINT_OK) {
          showMessage("Deleted ID: " + String(idToDelete));
        } else {
          showMessage("Del Failed ID: " + String(idToDelete) + "\n(Maybe empty)");
        }
        delay(1500); // Show result
    }
  }
  currentMenuState = STATE_MAIN_MENU; // Exit loop, go back to main menu
}


int enrollFingerprint(int id) {
  showMessage("Place Finger\nID: " + String(id));
  unsigned long start = millis();
  while (millis() - start < 10000) { // 10 second timeout
    if (digitalRead(CANCEL_BUTTON) == LOW) {
      delay(50); while(digitalRead(CANCEL_BUTTON) == LOW);
      return -1; // Cancelled
    }
    if (finger.getImage() == FINGERPRINT_OK) break;
    delay(10); // Small delay to prevent busy-waiting
  }
  if (millis() - start >= 10000) return -6; // Timeout error

  if (finger.image2Tz(1) != FINGERPRINT_OK) return -2;

  showMessage("Lift Finger");
  delay(1500);
  start = millis();
  while (finger.getImage() != FINGERPRINT_NOFINGER && millis() - start < 5000);
  if (millis() - start >= 5000) return -7;

  showMessage("Place Again\nID: " + String(id));
  start = millis();
  while (millis() - start < 10000) {
    if (digitalRead(CANCEL_BUTTON) == LOW) {
      delay(50); while(digitalRead(CANCEL_BUTTON) == LOW);
      return -1; // Cancelled
    }
    if (finger.getImage() == FINGERPRINT_OK) break;
    delay(10); // Small delay to prevent busy-waiting
  }
  if (millis() - start >= 10000) return -8;

  if (finger.image2Tz(2) != FINGERPRINT_OK) return -3;
  if (finger.createModel() != FINGERPRINT_OK) return -4;
  if (finger.storeModel(id) != FINGERPRINT_OK) return -5;

  return FINGERPRINT_OK; // Success
}

// Helper function to show messages on OLED and Serial
void showMessage(String msg) {
  Serial.println(msg);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  int line = 0;
  int start = 0;
  int len = msg.length();
  for (int i = 0; i <= len; i++) {
    if (i == len || msg[i] == '\n') {
      String part = msg.substring(start, i);
      display.setCursor(0, line * 8);
      display.print(part);
      start = i + 1;
      line++;
    }
  }
  display.display();
}

// Helper function to show messages with emoji
void showMessageWithEmoji(String msg, bool success) {
  Serial.println(msg);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  int textX = 20;
  int textY = 0;

  if (success) {
    display.drawBitmap(0, 0, smile_emoji_bmp, 16, 16, SSD1306_WHITE);
  } else {
    display.drawBitmap(0, 0, sad_emoji_bmp, 16, 16, SSD1306_WHITE);
  }

  int line = 0;
  int start = 0;
  int len = msg.length();
  for (int i = 0; i <= len; i++) {
    if (i == len || msg[i] == '\n') {
      String part = msg.substring(start, i);
      display.setCursor(textX, textY + line * 8);
      display.print(part);
      start = i + 1;
      line++;
    }
  }
  display.display();
}

// Helper function to wait for a button press with timeout
int getButtonPress(unsigned long timeout_ms) {
  unsigned long startWait = millis();
  while (millis() - startWait < timeout_ms) {
    if (digitalRead(ENROLL_BUTTON) == LOW) {
      if (millis() - lastEnrollButtonPressTime > debounceDelay) {
        lastEnrollButtonPressTime = millis();
        // Wait for release before returning
        while(digitalRead(ENROLL_BUTTON) == LOW);
        delay(debounceDelay); // Debounce after release
        return ENROLL_BUTTON;
      }
    }
    if (digitalRead(CANCEL_BUTTON) == LOW) {
      if (millis() - lastCancelButtonPressTime > debounceDelay) {
        lastCancelButtonPressTime = millis();
        // Wait for release before returning
        while(digitalRead(CANCEL_BUTTON) == LOW);
        delay(debounceDelay); // Debounce after release
        return CANCEL_BUTTON;
      }
    }
    delay(10); // Small delay to prevent busy-waiting
  }
  return 0; // Timeout
}