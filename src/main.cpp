#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Khởi tạo LCD với địa chỉ I2C (0x27 hoặc 0x3F tùy module)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Định nghĩa chân nút nhấn, LED, relay và cảm biến ánh sáng
#define BUTTON_LED     PF13  // D7
#define BUTTON_MANUAL  PE9   // D6
#define BUTTON_AUTO    PE11  // D5
#define BUTTON_SETTING PF14  // D4
#define BUTTON_RELAY_ON  PE13  // Nút bật relay
#define BUTTON_RELAY_OFF PF15  // Nút tắt relay
#define LED_PIN        PG14  // D1
#define LED_1          PB6   // LED 1 (xanh)
#define LED_2          PB2   // LED 2 (đỏ)
#define RELAY_PIN      PF2   // Relay
#define LIGHT_SENSOR   PG9   // Chân DO của cảm biến ánh sáng

// Định nghĩa chân bàn phím ma trận 4x4
#define KEYPAD_NUMBER_OF_ROWS 4
#define KEYPAD_NUMBER_OF_COLS 4
const int keypadRowPins[KEYPAD_NUMBER_OF_ROWS] = {PB3, PB5, PC7, PA15};
const int keypadColPins[KEYPAD_NUMBER_OF_COLS] = {PB12, PB13, PB15, PC6};

// Bàn phím ma trận ánh xạ ký tự
const char keypad[KEYPAD_NUMBER_OF_ROWS][KEYPAD_NUMBER_OF_COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Enum cho các chế độ
enum Mode {
  MANUAL,
  AUTO,
  SETTING_TIME
};

// Enum cho trạng thái màn hình
enum ScreenState {
  HOME,
  MENU,
  LOCK_SYSTEM,
  ENTER_PASSWORD,
  SELECT_MODE
};

// Biến toàn cục
bool isSystemLocked = false; // Trạng thái khóa hệ thống, mặc định là mở
Mode currentMode = MANUAL; // Chế độ mặc định
ScreenState currentScreen = HOME; // Màn hình mặc định là trang chủ
unsigned long lastDebounceTime = 0; // Thời gian debounce
const unsigned long debounceDelay = 100; // Độ trễ debounce (ms)
int lastManualState = HIGH; // Trạng thái trước của nút Manual
int lastAutoState = HIGH;   // Trạng thái trước của nút Auto
int lastSettingState = HIGH; // Trạng thái trước của nút Setting
int lastLedButtonState = HIGH; // Trạng thái trước của nút LED
int lastRelayOnState = HIGH;   // Trạng thái trước của nút bật relay
int lastRelayOffState = HIGH;  // Trạng thái trước của nút tắt relay
unsigned long lastKeypadDebounceTime = 0; // Thời gian debounce bàn phím
String enteredPassword = ""; // Mật khẩu đang nhập
String requiredPassword = ""; // Mật khẩu yêu cầu

// Hàm hiển thị trang chủ
void displayHome() {
  lcd.clear();
  lcd.setCursor(7, 0); // Căn giữa "PTIT" (4 ký tự)
  lcd.print("PTIT");
  lcd.setCursor(2, 1); // Căn giữa "TRANMINHTHUONG" (14 ký tự)
  lcd.print("TRAN MINH THUONG");
  lcd.setCursor(5, 2); // Căn giữa "N21DCVT101" (10 ký tự)
  lcd.print("N21DCVT101");
  lcd.setCursor(3, 3);
  lcd.print("- SMART HOME -");
}

// Hàm hiển thị menu
void displayMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("1. Chon che do");
  lcd.setCursor(0, 1);
  lcd.print("2. Khoa he thong");
  lcd.setCursor(0, 2);
  lcd.print("3. Cai dat muc nuoc");
  lcd.setCursor(0, 3);
  lcd.print("4. Thoat");
}

// Hàm hiển thị giao diện chọn mở/khóa hệ thống
void displayLockSystem() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Vui long chon:");
  lcd.setCursor(0, 1);
  lcd.print("1. Mo he thong");
  lcd.setCursor(0, 2);
  lcd.print("2. Khoa he thong");
}

// Hàm hiển thị giao diện nhập mật khẩu
void displayEnterPassword() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Nhap mat khau:");
  lcd.setCursor(0, 1);
  String displayPassword = "";
  for (size_t i = 0; i < enteredPassword.length(); i++) {
    displayPassword += "*";
  }
  lcd.print(displayPassword);
}

// Hàm hiển thị giao diện chọn chế độ
void displaySelectMode() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Nhap ma che do:");
  lcd.setCursor(0, 1);
  String displayPassword = "";
  for (size_t i = 0; i < enteredPassword.length(); i++) {
    displayPassword += "*";
  }
  lcd.print(displayPassword);
}

// Hàm hiển thị thông báo thành công hoặc lỗi
void displayMessage(String message) {
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print(message);
  delay(2000); // Hiển thị thông báo trong 2 giây
}

// Hàm hiển thị trạng thái thiết bị tạm thời
void displayDeviceStatus(String status) {
  lcd.setCursor(6, 2); // Hiển thị ở dòng 1 để giữ dòng 0 là chế độ
  lcd.print("                    "); // Xóa dòng 1
  lcd.setCursor(6, 2);
  lcd.print(status);
  delay(2000); // Hiển thị trạng thái trong 2 giây
  lcd.setCursor(6, 2);
  lcd.print("                    "); // Xóa dòng 1 sau khi hiển thị
}

// Hàm hiển thị chế độ căn giữa trên dòng 0
void displayMode() {
  lcd.clear();
  String modeText;
  switch (currentMode) {
    case MANUAL:
      modeText = "Mode: Manual";
      break;
    case AUTO:
      modeText = "Mode: Auto";
      break;
    case SETTING_TIME:
      modeText = "Mode: Set Time";
      break;
  }
  // Căn giữa: LCD 20 ký tự, thêm khoảng trắng ở đầu
  int padding = (20 - modeText.length()) / 2;
  lcd.setCursor(padding, 0);
  lcd.print(modeText);
}

// Hàm quét bàn phím ma trận
char scanKeypad() {
  char key = '\0';
  for (int row = 0; row < KEYPAD_NUMBER_OF_ROWS; row++) {
    // Đặt tất cả hàng thành HIGH
    for (int r = 0; r < KEYPAD_NUMBER_OF_ROWS; r++) {
      digitalWrite(keypadRowPins[r], HIGH);
    }
    // Đặt hàng hiện tại thành LOW
    digitalWrite(keypadRowPins[row], LOW);
    
    // Đọc các cột
    for (int col = 0; col < KEYPAD_NUMBER_OF_COLS; col++) {
      if (digitalRead(keypadColPins[col]) == LOW) {
        // Phát hiện phím được nhấn
        key = keypad[row][col];
        // Chờ nhả phím để tránh lặp
        while (digitalRead(keypadColPins[col]) == LOW) {
          delay(10);
        }
        return key;
      }
    }
  }
  return key;
}

void setup() {
  // Khởi tạo Serial để debug
  Serial.begin(115200);

  // Khởi tạo I2C
  Wire.begin(); // I2C1: PB8 (SCL), PB9 (SDA)

  // Khởi tạo LCD
  lcd.init();
  lcd.backlight();

  // Hiển thị trang chủ
  displayHome();

  // Cấu hình chân nút nhấn
  pinMode(BUTTON_MANUAL, INPUT_PULLUP);
  pinMode(BUTTON_AUTO, INPUT_PULLUP);
  pinMode(BUTTON_SETTING, INPUT_PULLUP);
  pinMode(BUTTON_LED, INPUT_PULLUP);
  pinMode(BUTTON_RELAY_ON, INPUT_PULLUP);
  pinMode(BUTTON_RELAY_OFF, INPUT_PULLUP);

  // Cấu hình chân LED và relay
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Tắt LED ban đầu
  digitalWrite(LED_1, LOW);   // Tắt LED 1 ban đầu
  digitalWrite(LED_2, LOW);   // Tắt LED 2 ban đầu
  digitalWrite(RELAY_PIN, LOW); // Tắt relay ban đầu

  // Cấu hình chân cảm biến ánh sáng
  pinMode(LIGHT_SENSOR, INPUT);

  // Cấu hình chân bàn phím
  for (int i = 0; i < KEYPAD_NUMBER_OF_ROWS; i++) {
    pinMode(keypadRowPins[i], OUTPUT);
    digitalWrite(keypadRowPins[i], HIGH); // Mặc định HIGH
  }
  for (int i = 0; i < KEYPAD_NUMBER_OF_COLS; i++) {
    pinMode(keypadColPins[i], INPUT_PULLUP);
  }
}

void loop() {
  // Đọc trạng thái nút nhấn
  int manualState = digitalRead(BUTTON_MANUAL);
  int autoState = digitalRead(BUTTON_AUTO);
  int settingState = digitalRead(BUTTON_SETTING);
  int ledButtonState = digitalRead(BUTTON_LED);
  int relayOnState = digitalRead(BUTTON_RELAY_ON);
  int relayOffState = digitalRead(BUTTON_RELAY_OFF);

  // Debug: In trạng thái nút
  if (manualState != lastManualState || autoState != lastAutoState || 
      settingState != lastSettingState || ledButtonState != lastLedButtonState ||
      relayOnState != lastRelayOnState || relayOffState != lastRelayOffState) {
    Serial.print("Manual: "); Serial.print(manualState);
    Serial.print(" Auto: "); Serial.print(autoState);
    Serial.print(" Setting: "); Serial.print(settingState);
    Serial.print(" LED Btn: "); Serial.print(ledButtonState);
    Serial.print(" Relay On: "); Serial.print(relayOnState);
    Serial.print(" Relay Off: "); Serial.println(relayOffState);
  }

  // Xử lý nút Manual (D6)
  if (manualState != lastManualState && millis() - lastDebounceTime > debounceDelay) {
    if (!isSystemLocked) { // Chỉ xử lý khi hệ thống không bị khóa
      if (manualState == LOW) { // Phát hiện cạnh xuống
        currentMode = MANUAL;
        currentScreen = HOME; // Quay lại trang chủ khi chuyển chế độ
        digitalWrite(LED_PIN, LOW); // Tắt LED khi chuyển sang MANUAL
        digitalWrite(LED_1, LOW);   // Tắt LED_1 khi chuyển sang MANUAL
        digitalWrite(LED_2, LOW);   // Tắt LED_2 khi chuyển sang MANUAL
        Serial.println("LED OFF, LED_1 OFF, LED_2 OFF (Switched to Manual)");
        displayMode();
        Serial.println("Switched to Manual");
      }
    } else {
      Serial.println("Manual Button ignored: System is locked");
    }
    lastDebounceTime = millis();
  }
  lastManualState = manualState;

  // Xử lý nút Auto (D5)
  if (autoState != lastAutoState && millis() - lastDebounceTime > debounceDelay) {
    if (!isSystemLocked) { // Chỉ xử lý khi hệ thống không bị khóa
      if (autoState == LOW) { // Phát hiện cạnh xuống
        currentMode = AUTO;
        currentScreen = HOME; // Quay lại trang chủ khi chuyển chế độ
        digitalWrite(LED_PIN, LOW); // Tắt LED khi chuyển sang AUTO
        digitalWrite(LED_1, LOW);   // Tắt LED_1 khi chuyển sang AUTO
        digitalWrite(LED_2, LOW);   // Tắt LED_2 khi chuyển sang AUTO
        digitalWrite(RELAY_PIN, LOW); // Tắt relay khi chuyển sang AUTO
        Serial.println("LED OFF, LED_1 OFF, LED_2 OFF, Relay OFF (Switched to Auto)");
        displayMode();
        Serial.println("Switched to Auto");
      }
    } else {
      Serial.println("Auto Button ignored: System is locked");
    }
    lastDebounceTime = millis();
  }
  lastAutoState = autoState;

  // Xử lý nút Setting Time (D4)
  if (settingState != lastSettingState && millis() - lastDebounceTime > debounceDelay) {
    if (!isSystemLocked) { // Chỉ xử lý khi hệ thống không bị khóa
      if (settingState == LOW) { // Phát hiện cạnh xuống
        currentMode = SETTING_TIME;
        currentScreen = HOME; // Quay lại trang chủ khi chuyển chế độ
        digitalWrite(LED_PIN, LOW); // Tắt LED khi chuyển sang SETTING_TIME
        digitalWrite(LED_1, LOW);   // Tắt LED_1 khi chuyển sang SETTING_TIME
        digitalWrite(LED_2, LOW);   // Tắt LED_2 khi chuyển sang SETTING_TIME
        digitalWrite(RELAY_PIN, LOW); // Tắt relay khi chuyển sang SETTING_TIME
        Serial.println("LED OFF, LED_1 OFF, LED_2 OFF, Relay OFF (Switched to Setting Time)");
        displayMode();
        Serial.println("Switched to Set Time");
      }
    } else {
      Serial.println("Setting Button ignored: System is locked");
    }
    lastDebounceTime = millis();
  }
  lastSettingState = settingState;

  // Xử lý nút LED (D7) - Chỉ hoạt động ở chế độ MANUAL
  if (ledButtonState != lastLedButtonState && millis() - lastDebounceTime > debounceDelay) {
    if (!isSystemLocked) { // Chỉ xử lý khi hệ thống không bị khóa
      if (currentMode == MANUAL) { // Chỉ xử lý khi ở chế độ MANUAL
        if (ledButtonState == LOW) { // Phát hiện cạnh xuống
          digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Toggle LED
          Serial.println(digitalRead(LED_PIN) ? "LED ON (Manual)" : "LED OFF (Manual)");
        }
      } else {
        Serial.println("LED Button ignored: Not in Manual mode");
      }
    } else {
      Serial.println("LED Button ignored: System is locked");
    }
    lastDebounceTime = millis();
  }
  lastLedButtonState = ledButtonState;

  // Xử lý nút bật relay (PE13) - Chỉ hoạt động ở chế độ MANUAL
  if (relayOnState != lastRelayOnState && millis() - lastDebounceTime > debounceDelay) {
    if (!isSystemLocked) { // Chỉ xử lý khi hệ thống không bị khóa
      if (currentMode == MANUAL) { // Chỉ xử lý khi ở chế độ MANUAL
        if (relayOnState == LOW) { // Phát hiện cạnh xuống
          digitalWrite(RELAY_PIN, HIGH); // Bật relay
          digitalWrite(LED_1, HIGH);    // Bật LED PB6
          digitalWrite(LED_2, LOW);     // Tắt LED PB2
          Serial.println("Relay ON, LED PB6 ON, LED PB2 OFF");
        }
      } else {
        Serial.println("Relay On Button ignored: Not in Manual mode");
      }
    } else {
      Serial.println("Relay On Button ignored: System is locked");
    }
    lastDebounceTime = millis();
  }
  lastRelayOnState = relayOnState;

  // Xử lý nút tắt relay (PF15) - Chỉ hoạt động ở chế độ MANUAL
  if (relayOffState != lastRelayOffState && millis() - lastDebounceTime > debounceDelay) {
    if (!isSystemLocked) { // Chỉ xử lý khi hệ thống không bị khóa
      if (currentMode == MANUAL) { // Chỉ xử lý khi ở chế độ MANUAL
        if (relayOffState == LOW) { // Phát hiện cạnh xuống
          digitalWrite(RELAY_PIN, LOW);  // Tắt relay
          digitalWrite(LED_1, LOW);     // Tắt LED PB6
          digitalWrite(LED_2, HIGH);    // Bật LED PB2
          Serial.println("Relay OFF, LED PB6 OFF, LED PB2 ON");
        }
      } else {
        Serial.println("Relay Off Button ignored: Not in Manual mode");
      }
    } else {
      Serial.println("Relay Off Button ignored: System is locked");
    }
    lastDebounceTime = millis();
  }
  lastRelayOffState = relayOffState;

// Xử lý bàn phím ma trận
if (millis() - lastKeypadDebounceTime > debounceDelay) {
  char key = scanKeypad();
  if (key != '\0') {
    // Xử lý phím '*' ở trạng thái HOME
    if (key == '*' && currentScreen == HOME) {
      currentScreen = MENU;
      displayMenu();
      Serial.println("Switched to Menu");
      lastKeypadDebounceTime = millis();
    }
    // Xử lý phím '4' ở trạng thái MENU
    else if (key == '4' && currentScreen == MENU) {
      currentScreen = HOME;
      displayHome();
      Serial.println("Returned to Home");
      lastKeypadDebounceTime = millis();
    }
    // Xử lý phím '2' ở trạng thái MENU
    else if (key == '2' && currentScreen == MENU) {
      currentScreen = LOCK_SYSTEM;
      displayLockSystem();
      Serial.println("Switched to Lock System");
      lastKeypadDebounceTime = millis();
    }
    // Xử lý phím '1' ở trạng thái MENU (Chọn chế độ)
    else if (key == '1' && currentScreen == MENU) {
      currentScreen = SELECT_MODE;
      enteredPassword = "";
      displaySelectMode();
      Serial.println("Switched to Select Mode");
      lastKeypadDebounceTime = millis();
    }
    // Xử lý phím '1' hoặc '2' ở trạng thái LOCK_SYSTEM
    else if (currentScreen == LOCK_SYSTEM) {
      if (key == '1') {
        currentScreen = ENTER_PASSWORD;
        requiredPassword = "8888";
        enteredPassword = "";
        displayEnterPassword();
        Serial.println("Enter Password for Unlock (8888)");
        lastKeypadDebounceTime = millis();
      } else if (key == '2') {
        currentScreen = ENTER_PASSWORD;
        requiredPassword = "9999";
        enteredPassword = "";
        displayEnterPassword();
        Serial.println("Enter Password for Lock (9999)");
        lastKeypadDebounceTime = millis();
      }
    }
    // Xử lý nhập mật khẩu ở trạng thái ENTER_PASSWORD
    else if (currentScreen == ENTER_PASSWORD) {
      if ((key >= '0' && key <= '9') && enteredPassword.length() < 4) {
        enteredPassword += key;
        displayEnterPassword();
        Serial.print("Password Input: ");
        Serial.println(enteredPassword);
        lastKeypadDebounceTime = millis();
      } else if (key == '#') {
        if (enteredPassword == requiredPassword) {
          if (requiredPassword == "8888") {
            isSystemLocked = false; // Mở khóa hệ thống
            displayMessage("System Unlocked");
            Serial.println("System Unlocked");
          } else if (requiredPassword == "9999") {
            isSystemLocked = true; // Khóa hệ thống
            displayMessage("System Locked");
            Serial.println("System Locked");
          }
          currentScreen = HOME;
          displayHome();
        } else {
          displayMessage("Wrong Password");
          Serial.println("Wrong Password");
          enteredPassword = "";
          displayEnterPassword();
        }
        lastKeypadDebounceTime = millis();
      }
    }
    // Xử lý nhập mã chế độ ở trạng thái SELECT_MODE
    else if (currentScreen == SELECT_MODE) {
      if ((key >= '0' && key <= '9') && enteredPassword.length() < 4) {
        enteredPassword += key;
        displaySelectMode();
        Serial.print("Mode Code Input: ");
        Serial.println(enteredPassword);
        lastKeypadDebounceTime = millis();
      } else if (key == '#') {
        if (enteredPassword == "1111") {
          currentMode = MANUAL;
          digitalWrite(LED_PIN, LOW); // Tắt LED khi chuyển sang MANUAL
          digitalWrite(LED_1, LOW);   // Tắt LED_1 khi chuyển sang MANUAL
          digitalWrite(LED_2, LOW);   // Tắt LED_2 khi chuyển sang MANUAL
          digitalWrite(RELAY_PIN, LOW); // Tắt relay
          displayMessage("Mode: Manual");
          displayMode(); // Hiển thị chế độ thay vì về trang chủ
          Serial.println("Switched to Manual Mode via Keypad (1111)");
        } else if (enteredPassword == "2222") {
          currentMode = AUTO;
          digitalWrite(LED_PIN, LOW); // Tắt LED khi chuyển sang AUTO
          digitalWrite(LED_1, LOW);   // Tắt LED_1 khi chuyển sang AUTO
          digitalWrite(LED_2, LOW);   // Tắt LED_2 khi chuyển sang AUTO
          digitalWrite(RELAY_PIN, LOW); // Tắt relay
          displayMessage("Mode: Auto");
          displayMode(); // Hiển thị chế độ thay vì về trang chủ
          Serial.println("Switched to Auto Mode via Keypad (2222)");
        } else if (enteredPassword == "3333") {
          currentMode = SETTING_TIME;
          digitalWrite(LED_PIN, LOW); // Tắt LED khi chuyển sang SETTING_TIME
          digitalWrite(LED_1, LOW);   // Tắt LED_1 khi chuyển sang SETTING_TIME
          digitalWrite(LED_2, LOW);   // Tắt LED_2 khi chuyển sang SETTING_TIME
          digitalWrite(RELAY_PIN, LOW); // Tắt relay
          displayMessage("Mode: Set Time");
          displayMode(); // Hiển thị chế độ thay vì về trang chủ
          Serial.println("Switched to Set Time Mode via Keypad (3333)");
        } else {
          displayMessage("Wrong Code");
          Serial.println("Wrong Mode Code");
          enteredPassword = "";
          displaySelectMode();
          lastKeypadDebounceTime = millis();
          return;
        }
        enteredPassword = ""; // Xóa mã đã nhập
        lastKeypadDebounceTime = millis();
      }
    }
    // Xử lý phím A, B, C ở chế độ MANUAL (bất kỳ màn hình nào)
    if (currentMode == MANUAL && !isSystemLocked) {
      if (key == 'A') {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Toggle LED_PIN
        Serial.println(digitalRead(LED_PIN) ? "LED_PIN ON (Keypad A)" : "LED_PIN OFF (Keypad A)");
        displayDeviceStatus(digitalRead(LED_PIN) ? "Bat den" : "Tat den");
        lastKeypadDebounceTime = millis();
      } else if (key == 'B') {
        digitalWrite(LED_1, !digitalRead(LED_1)); // Toggle LED_1 (xanh)
        Serial.println(digitalRead(LED_1) ? "LED_1 ON (Keypad B)" : "LED_1 OFF (Keypad B)");
        displayDeviceStatus(digitalRead(LED_1) ? "Bat may bom" : "Tat may bom");
        lastKeypadDebounceTime = millis();
      } else if (key == 'C') {
        digitalWrite(LED_2, !digitalRead(LED_2)); // Toggle LED_2 (đỏ)
        Serial.println(digitalRead(LED_2) ? "LED_2 ON (Keypad C)" : "LED_2 OFF (Keypad C)");
        displayDeviceStatus(digitalRead(LED_2) ? "Bat quat" : "Tat quat");
        lastKeypadDebounceTime = millis();
      }
    }
  }
}

  // Xử lý cảm biến ánh sáng - Chỉ hoạt động ở chế độ AUTO
  if (currentMode == AUTO && currentScreen != MENU && currentScreen != LOCK_SYSTEM && 
      currentScreen != ENTER_PASSWORD && currentScreen != SELECT_MODE) {
    int lightValue = digitalRead(LIGHT_SENSOR); // Đọc giá trị cảm biến ánh sáng (digital)
    Serial.print("Light Sensor: "); Serial.println(lightValue == HIGH ? "HIGH (Light)" : "LOW (No light)");

    // Điều khiển LED dựa trên giá trị cảm biến ánh sáng
    if (lightValue == LOW) {
      digitalWrite(LED_PIN, LOW); // Tắt LED khi không có ánh sáng (LOW)
      Serial.println("LED OFF");
    } else {
      digitalWrite(LED_PIN, HIGH);  // Bật LED khi có ánh sáng (HIGH)
      Serial.println("LED ON");
    }
  }
}
