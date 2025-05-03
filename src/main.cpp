#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Khởi tạo LCD với địa chỉ I2C (0x27 hoặc 0x3F tùy module)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Định nghĩa chân nút nhấn, LED, relay và cảm biến ánh sáng
#define BUTTON_LED PF13       // D7
#define BUTTON_MANUAL PE9     // D6
#define BUTTON_AUTO PE11      // D5
#define BUTTON_SETTING PF14   // D4
#define BUTTON_RELAY_ON PE13  // Nút điều khiển relay PF2 (máy bơm)
#define BUTTON_RELAY_OFF PF15 // Nút điều khiển relay PH1 (quạt)
#define LED_PIN PG14          // D1
#define LED_1 PB6             // LED 1 (xanh) cho máy bơm
#define LED_2 PB2             // LED 2 (đỏ) cho quạt
#define RELAY_PIN PF2         // Relay cho máy bơm
#define RELAY_FAN PE6         // Relay cho quạt
#define LIGHT_SENSOR PG9      // Chân DO của cảm biến ánh sáng

// Định nghĩa chân cảm biến khoảng cách HC-SR04
#define TRIG_PIN PA6
#define ECHO_PIN PA7
#define PUMP_ON_DISTANCE 20.0  // cm, khoảng cách để bật máy bơm (mức nước thấp)
#define PUMP_OFF_DISTANCE 10.0 // cm, khoảng cách để tắt máy bơm (mức nước cao)

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
enum Mode
{
  MANUAL,
  AUTO,
  SETTING_TIME
};

// Enum cho trạng thái màn hình
enum ScreenState
{
  HOME,
  MENU,
  LOCK_SYSTEM,
  ENTER_PASSWORD,
  SELECT_MODE,
  SET_TIME_MENU,
  SET_TIME_INPUT,
  SET_CURRENT_TIME,
  DEVICE_STATE 
};

// Biến toàn cục
bool isSystemLocked = false;              // Trạng thái khóa hệ thống
Mode currentMode = MANUAL;                // Chế độ mặc định
ScreenState currentScreen = HOME;         // Màn hình mặc định
unsigned long lastDebounceTime = 0;       // Thời gian debounce
const unsigned long debounceDelay = 100;  // Độ trễ debounce (ms)
int lastManualState = HIGH;               // Trạng thái trước của nút Manual
int lastAutoState = HIGH;                 // Trạng thái trước của nút Auto
int lastSettingState = HIGH;              // Trạng thái trước của nút Setting
int lastLedButtonState = HIGH;            // Trạng thái trước của nút LED
int lastRelayOnState = HIGH;              // Trạng thái trước của nút relay PF2
int lastRelayOffState = HIGH;             // Trạng thái trước của nút relay PH1
unsigned long lastKeypadDebounceTime = 0; // Thời gian debounce bàn phím
String enteredPassword = "";              // Mật khẩu đang nhập
String requiredPassword = "";             // Mật khẩu yêu cầu
unsigned long lastDistanceMeasureTime = 0; // Thời gian đo khoảng cách cuối
const unsigned long distanceMeasureInterval = 1000; // Đo mỗi 1 giây

// Biến cho cài đặt thời gian hiện tại
String currentTimeInput = "";
int currentInputPhase = 0; // 0: hour, 1: minute
unsigned long timeSetMillis = 0;  // Thời điểm cài đặt thời gian (millis)
long currentTimeSeconds = -1;     // Thời gian hiện tại tính bằng giây từ 00:00

// Variables for Setting Time mode
int selectedDevice = 0; // 1: Den, 2: Quat, 3: May bom nuoc
int inputPhase = 0;     // 0: entering on time, 1: entering off time
String timeInput = "";
int onHour = -1, onMinute = -1, offHour = -1, offMinute = -1;

struct DeviceTime {
  int onHour;
  int onMinute;
  int offHour;
  int offMinute;
};

DeviceTime denTime = {-1, -1, -1, -1};
DeviceTime quatTime = {-1, -1, -1, -1};
DeviceTime mayBomTime = {-1, -1, -1, -1};

// Hàm hiển thị trang chủ
void displayHome()
{
  lcd.clear();
  lcd.setCursor(7, 0);
  lcd.print("PTIT");
  lcd.setCursor(2, 1);
  lcd.print("TRAN MINH THUONG");
  lcd.setCursor(5, 2);
  lcd.print("N21DCVT101");
  lcd.setCursor(3, 3);
  lcd.print("- SMART HOME -");
}

// Hàm hiển thị menu
void displayMenu()
{
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
void displayLockSystem()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Vui long chon:");
  lcd.setCursor(0, 1);
  lcd.print("1. Mo he thong");
  lcd.setCursor(0, 2);
  lcd.print("2. Khoa he thong");
}

// Hàm hiển thị giao diện nhập mật khẩu
void displayEnterPassword()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Nhap mat khau:");
  lcd.setCursor(0, 1);
  String displayPassword = "";
  for (size_t i = 0; i < enteredPassword.length(); i++)
  {
    displayPassword += "*";
  }
  lcd.print(displayPassword);
}

// Hàm hiển thị giao diện chọn chế độ
void displaySelectMode()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Nhap ma che do:");
  lcd.setCursor(0, 1);
  String displayPassword = "";
  for (size_t i = 0; i < enteredPassword.length(); i++)
  {
    displayPassword += "*";
  }
  lcd.print(displayPassword);
}

// Hàm hiển thị thông báo
void displayMessage(String message)
{
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print(message);
  delay(2000);
}

// Hàm hiển thị trạng thái thiết bị tạm thời
void displayDeviceStatus(String status)
{
  lcd.setCursor(6, 2);
  lcd.print("                    ");
  lcd.setCursor(6, 2);
  lcd.print(status);
  delay(2000);
  lcd.setCursor(6, 2);
  lcd.print("                    ");
}

// Hàm hiển thị chế độ
void displayMode()
{
  lcd.clear();
  String modeText;
  switch (currentMode)
  {
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
  int padding = (20 - modeText.length()) / 2;
  lcd.setCursor(padding, 0);
  lcd.print(modeText);
}

// Hàm hiển thị menu chọn thiết bị
void displaySetTimeMenu()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Chon thiet bi");
  lcd.setCursor(0, 1);
  lcd.print("1. Den");
  lcd.setCursor(0, 2);
  lcd.print("2. Quat");
  lcd.setCursor(0, 3);
  lcd.print("3. May bom nuoc");
}

// Hàm hiển thị giao diện nhập thời gian thiết bị
void displaySetTimeInput()
{
  lcd.clear();
  String deviceName;
  if (selectedDevice == 1) deviceName = "Den";
  else if (selectedDevice == 2) deviceName = "Quat";
  else if (selectedDevice == 3) deviceName = "May bom nuoc";
  lcd.setCursor(0, 0);
  lcd.print("Thiet bi: " + deviceName);

  if (inputPhase == 0) {
    String onStr = timeInput;
    while (onStr.length() < 4) onStr += " ";
    onStr = onStr.substring(0, 2) + ":" + onStr.substring(2, 4);
    lcd.setCursor(0, 1);
    lcd.print("Bat: " + onStr);
    lcd.setCursor(0, 2);
    lcd.print("Tat: ");
  } else if (inputPhase == 1) {
    String onStr = String(onHour / 10) + String(onHour % 10) + ":" + 
                   String(onMinute / 10) + String(onMinute % 10);
    lcd.setCursor(0, 1);
    lcd.print("Bat: " + onStr);
    String offStr = timeInput;
    while (offStr.length() < 4) offStr += " ";
    offStr = offStr.substring(0, 2) + ":" + offStr.substring(2, 4);
    lcd.setCursor(0, 2);
    lcd.print("Tat: " + offStr);
  }
}

// Hàm hiển thị giao diện cài đặt thời gian hiện tại
void displaySetCurrentTime()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cai dat thoi gian");
  lcd.setCursor(0, 1);
  String timeStr = currentTimeInput;
  if (currentInputPhase == 0) {
    while (timeStr.length() < 2) timeStr += " ";
    lcd.print("Gio: " + timeStr);
    lcd.setCursor(0, 2);
    lcd.print("Phut: ");
  } else if (currentInputPhase == 1) {
    lcd.print("Gio: " + String(currentTimeInput.substring(0, 2)));
    lcd.setCursor(0, 2);
    while (timeStr.length() < 4) timeStr += " ";
    lcd.print("Phut: " + timeStr.substring(2));
  }
}

// Hàm quét bàn phím ma trận
char scanKeypad()
{
  char key = '\0';
  for (int row = 0; row < KEYPAD_NUMBER_OF_ROWS; row++)
  {
    for (int r = 0; r < KEYPAD_NUMBER_OF_ROWS; r++)
    {
      digitalWrite(keypadRowPins[r], HIGH);
    }
    digitalWrite(keypadRowPins[row], LOW);
    for (int col = 0; col < KEYPAD_NUMBER_OF_COLS; col++)
    {
      if (digitalRead(keypadColPins[col]) == LOW)
      {
        key = keypad[row][col];
        while (digitalRead(keypadColPins[col]) == LOW)
        {
          delay(10);
        }
        return key;
      }
    }
  }
  return key;
}

// Hàm đo khoảng cách từ cảm biến HC-SR04
float measureDistance()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
 

 digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = (duration / 2.0) * 0.0343;
  return distance;
}

// Hàm xử lý nút nhấn chung
void handleButton(int buttonPin, int &lastState, int relayPin, int ledOnPin, int ledOffPin, String deviceName)
{
  int buttonState = digitalRead(buttonPin);
  if (buttonState != lastState && millis() - lastDebounceTime > debounceDelay)
  {
    if (!isSystemLocked && currentMode == MANUAL)
    {
      if (buttonState == LOW)
      {
        bool currentRelayState = digitalRead(relayPin);
        digitalWrite(relayPin, !currentRelayState);
        if (digitalRead(relayPin) == LOW)
        {
          digitalWrite(ledOnPin, HIGH);
          digitalWrite(ledOffPin, LOW);
          Serial.println(deviceName + " ON, LED ON, LED OFF");
          displayDeviceStatus("Bat " + deviceName);
        }
        else
        {
          digitalWrite(ledOnPin, LOW);
          digitalWrite(ledOffPin, HIGH);
          Serial.println(deviceName + " OFF, LED OFF, LED ON");
          displayDeviceStatus("Tat " + deviceName);
        }
      }
    }
    else
    {
      Serial.println(deviceName + " Button ignored: System is locked or not in Manual mode");
    }
    lastDebounceTime = millis();
  }
  lastState = buttonState;
}

// Hàm tính thời gian hiện tại (giây kể từ 00:00)
long getCurrentTimeSeconds()
{
  if (currentTimeSeconds == -1) return -1; // Chưa cài đặt thời gian
  unsigned long elapsedMillis = millis() - timeSetMillis;
  long elapsedSeconds = elapsedMillis / 1000;
  long totalSeconds = currentTimeSeconds + elapsedSeconds;
  long currentDaySeconds = totalSeconds % 86400; // Lấy dư để quay lại 0 sau 24 giờ
  // Debug: In thời gian hiện tại
  Serial.print("Current time (seconds): ");
  Serial.print(currentDaySeconds);
  Serial.print(" (");
  Serial.print(currentDaySeconds / 3600);
  Serial.print(":");
  Serial.print((currentDaySeconds % 3600) / 60);
  Serial.println(")");
  return currentDaySeconds;
}

// Hàm kiểm tra và điều khiển thiết bị dựa trên thời gian
void controlDevicesByTime()
{
  if (currentTimeSeconds == -1) {
    Serial.println("Time not set, skipping device control");
    return; // Chưa cài đặt thời gian
  }

  long currentSeconds = getCurrentTimeSeconds();
  if (currentSeconds == -1) return;

  // Kiểm tra đèn
  if (denTime.onHour != -1) {
    long onSeconds = denTime.onHour * 3600 + denTime.onMinute * 60;
    long offSeconds = denTime.offHour * 3600 + denTime.offMinute * 60;
    Serial.print("Den: on=");
    Serial.print(onSeconds);
    Serial.print("s, off=");
    Serial.print(offSeconds);
    Serial.print("s, current=");
    Serial.print(currentSeconds);
    Serial.print("s, state=");
    
    bool shouldBeOn = false;
    if (onSeconds <= offSeconds) {
      shouldBeOn = (currentSeconds >= onSeconds && currentSeconds < offSeconds);
    } else {
      shouldBeOn = (currentSeconds >= onSeconds || currentSeconds < offSeconds);
    }
    
    if (shouldBeOn) {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("ON");
    } else {
      digitalWrite(LED_PIN, LOW);
      Serial.println("OFF");
    }
  } else {
    Serial.println("Den: No schedule set");
  }

  // Kiểm tra quạt
  if (quatTime.onHour != -1) {
    long onSeconds = quatTime.onHour * 3600 + quatTime.onMinute * 60;
    long offSeconds = quatTime.offHour * 3600 + quatTime.offMinute * 60;
    Serial.print("Quat: on=");
    Serial.print(onSeconds);
    Serial.print("s, off=");
    Serial.print(offSeconds);
    Serial.print("s, current=");
    Serial.print(currentSeconds);
    Serial.print("s, state=");
    
    bool shouldBeOn = false;
    if (onSeconds <= offSeconds) {
      shouldBeOn = (currentSeconds >= onSeconds && currentSeconds < offSeconds);
    } else {
      shouldBeOn = (currentSeconds >= onSeconds || currentSeconds < offSeconds);
    }
    
    if (shouldBeOn) {
      digitalWrite(RELAY_FAN, LOW);
      digitalWrite(LED_2, HIGH);
      digitalWrite(LED_1, LOW);
      Serial.println("ON");
      displayDeviceStatus("Bat quat");
    } else {
      digitalWrite(RELAY_FAN, HIGH);
      digitalWrite(LED_2, LOW);
      digitalWrite(LED_1, HIGH);
      Serial.println("OFF");
      displayDeviceStatus("Tat quat");
    }
  } else {
    Serial.println("Quat: No schedule set");
  }

  // Kiểm tra máy bơm
  if (mayBomTime.onHour != -1) {
    long onSeconds = mayBomTime.onHour * 3600 + mayBomTime.onMinute * 60;
    long offSeconds = mayBomTime.offHour * 3600 + mayBomTime.offMinute * 60;
    Serial.print("May bom: on=");
    Serial.print(onSeconds);
    Serial.print("s, off=");
    Serial.print(offSeconds);
    Serial.print("s, current=");
    Serial.print(currentSeconds);
    Serial.print("s, state=");
    
    bool shouldBeOn = false;
    if (onSeconds <= offSeconds) {
      shouldBeOn = (currentSeconds >= onSeconds && currentSeconds < offSeconds);
    } else {
      shouldBeOn = (currentSeconds >= onSeconds || currentSeconds < offSeconds);
    }
    
    if (shouldBeOn) {
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(LED_1, HIGH);
      digitalWrite(LED_2, LOW);
      Serial.println("ON");
      displayDeviceStatus("Bat may bom");
    } else {
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(LED_1, LOW);
      digitalWrite(LED_2, HIGH);
      Serial.println("OFF");
      displayDeviceStatus("Tat may bom");
    }
  } else {
    Serial.println("May bom: No schedule set");
  }
}

// Thêm hàm xác định trạng thái thiết bị
String getDeviceStatus(DeviceTime deviceTime, long currentSeconds) {
  if (deviceTime.onHour == -1 || deviceTime.offHour == -1) {
    return "Not set";
  }
  long onSeconds = deviceTime.onHour * 3600 + deviceTime.onMinute * 60;
  long offSeconds = deviceTime.offHour * 3600 + deviceTime.offMinute * 60;
  bool shouldBeOn = false;
  if (onSeconds <= offSeconds) {
    shouldBeOn = (currentSeconds >= onSeconds && currentSeconds < offSeconds);
    } else {
      shouldBeOn = (currentSeconds >= onSeconds || currentSeconds < offSeconds);
    }
    return shouldBeOn ? "ON" : "OFF";
}

// Hàm hiển thị và cập nhật trạng thái thiết bị trực tiếp
void displayDeviceState() {
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("  Mode: Set time   ");

  long currentSeconds = getCurrentTimeSeconds();
  if (currentSeconds == -1) {
    lcd.setCursor(0, 1);
    lcd.print("Time not set");
    return;
  }

  unsigned long lastUpdateTime = millis();
  const unsigned long updateInterval = 1000; // Cập nhật mỗi giây

  while (true) {
    if (millis() - lastUpdateTime >= updateInterval) {
      lastUpdateTime = millis();
      currentSeconds = getCurrentTimeSeconds();

      // Điều khiển thiết bị dựa trên thời gian hiện tại
      controlDevicesByTime();

      // Hiển thị trạng thái cố định cho từng thiết bị
      lcd.setCursor(0, 1);
      lcd.print("Den: ");
      if (denTime.onHour != -1) {
        String status = getDeviceStatus(denTime, currentSeconds);
        lcd.print(status + "    "); // Thêm khoảng trắng để xóa ký tự cũ
      } else {
        lcd.print("Not set    ");
      }

      lcd.setCursor(0, 2);
      lcd.print("Quat: ");
      if (quatTime.onHour != -1) {
        String status = getDeviceStatus(quatTime, currentSeconds);
        lcd.print(status + "    ");
      } else {
        lcd.print("Not set    ");
      }

      lcd.setCursor(0, 3);
      lcd.print("May bom nuoc:");
      if (mayBomTime.onHour != -1) {
        String status = getDeviceStatus(mayBomTime, currentSeconds);
        lcd.print(status + "    ");
      } else {
        lcd.print("Not set    ");
      }
    }

    // Kiểm tra phím '*'
    char key = scanKeypad();
    if (key == '*') {
      currentScreen = HOME;
      displayHome();
      lastKeypadDebounceTime = millis();
      break;
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  displayHome();

  pinMode(BUTTON_MANUAL, INPUT_PULLUP);
  pinMode(BUTTON_AUTO, INPUT_PULLUP);
  pinMode(BUTTON_SETTING, INPUT_PULLUP);
  pinMode(BUTTON_LED, INPUT_PULLUP);
  pinMode(BUTTON_RELAY_ON, INPUT_PULLUP);
  pinMode(BUTTON_RELAY_OFF, INPUT_PULLUP);

  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(LED_1, LOW);
  digitalWrite(LED_2, LOW);
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(RELAY_FAN, HIGH);

  if (digitalRead(RELAY_FAN) != HIGH)
  {
    digitalWrite(RELAY_FAN, HIGH);
    Serial.println("Initialized Relay PH1 to OFF");
  }

  pinMode(LIGHT_SENSOR, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  for (int i = 0; i < KEYPAD_NUMBER_OF_ROWS; i++)
  {
    pinMode(keypadRowPins[i], OUTPUT);
    digitalWrite(keypadRowPins[i], HIGH);
  }
  for (int i = 0; i < KEYPAD_NUMBER_OF_COLS; i++)
  {
    pinMode(keypadColPins[i], INPUT_PULLUP);
  }
}

void loop()
{
  int manualState = digitalRead(BUTTON_MANUAL);
  int autoState = digitalRead(BUTTON_AUTO);
  int settingState = digitalRead(BUTTON_SETTING);
  int ledButtonState = digitalRead(BUTTON_LED);
  int relayOnState = digitalRead(BUTTON_RELAY_ON);
  int relayOffState = digitalRead(BUTTON_RELAY_OFF);

  if (manualState != lastManualState || autoState != lastAutoState ||
      settingState != lastSettingState || ledButtonState != lastLedButtonState ||
      relayOnState != lastRelayOnState || relayOffState != lastRelayOffState)
  {
    Serial.print("Manual: ");
    Serial.print(manualState);
    Serial.print(" Auto: ");
    Serial.print(autoState);
    Serial.print(" Setting: ");
    Serial.print(settingState);
    Serial.print(" LED Btn: ");
    Serial.print(ledButtonState);
    Serial.print(" Relay On: ");
    Serial.print(relayOnState);
    Serial.print(" Relay Off: ");
    Serial.println(relayOffState);
  }

  if (manualState != lastManualState && millis() - lastDebounceTime > debounceDelay)
  {
    if (!isSystemLocked)
    {
      if (manualState == LOW)
      {
        currentMode = MANUAL;
        currentScreen = HOME;
        digitalWrite(LED_PIN, LOW);
        digitalWrite(LED_1, LOW);
        digitalWrite(LED_2, LOW);
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(RELAY_FAN, HIGH);
        Serial.println("LED OFF, LED_1 OFF, LED_2 OFF, Relays OFF (Switched to Manual)");
        displayMode();
        Serial.println("Switched to Manual");
      }
    }
    else
    {
      Serial.println("Manual Button ignored: System is locked");
    }
    lastDebounceTime = millis();
  }
  lastManualState = manualState;

  if (autoState != lastAutoState && millis() - lastDebounceTime > debounceDelay)
  {
    if (!isSystemLocked)
    {
      if (autoState == LOW)
      {
        currentMode = AUTO;
        currentScreen = HOME;
        digitalWrite(LED_PIN, LOW);
        digitalWrite(LED_1, LOW);
        digitalWrite(LED_2, LOW);
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(RELAY_FAN, HIGH);
        Serial.println("LED OFF, LED_1 OFF, LED_2 OFF, Relays OFF (Switched to Auto)");
        displayMode();
        Serial.println("Switched to Auto");
      }
    }
    else
    {
      Serial.println("Auto Button ignored: System is locked");
    }
    lastDebounceTime = millis();
  }
  lastAutoState = autoState;

  if (settingState != lastSettingState && millis() - lastDebounceTime > debounceDelay)
  {
    if (!isSystemLocked)
    {
      if (settingState == LOW)
      {
        currentMode = SETTING_TIME;
        currentScreen = SET_TIME_MENU;
        digitalWrite(LED_PIN, LOW);
        digitalWrite(LED_1, LOW);
        digitalWrite(LED_2, LOW);
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(RELAY_FAN, HIGH);
        Serial.println("LED OFF, LED_1 OFF, LED_2 OFF, Relays OFF (Switched to Setting Time)");
        displayMode();
        delay(1000);
        displaySetTimeMenu();
        Serial.println("Switched to Set Time");
      }
    }
    else
    {
      Serial.println("Setting Button ignored: System is locked");
    }
    lastDebounceTime = millis();
  }
  lastSettingState = settingState;

  if (ledButtonState != lastLedButtonState && millis() - lastDebounceTime > debounceDelay)
  {
    if (!isSystemLocked)
    {
      if (currentMode == MANUAL)
      {
        if (ledButtonState == LOW)
        {
          digitalWrite(LED_PIN, !digitalRead(LED_PIN));
          Serial.println(digitalRead(LED_PIN) ? "LED ON (Manual)" : "LED OFF (Manual)");
          displayDeviceStatus(digitalRead(LED_PIN) ? "Bat den" : "Tat den");
        }
      }
      else
      {
        Serial.println("LED Button ignored: Not in Manual mode");
      }
    }
    else
    {
      Serial.println("LED Button ignored: System is locked");
    }
    lastDebounceTime = millis();
  }
  lastLedButtonState = ledButtonState;

  handleButton(BUTTON_RELAY_ON, lastRelayOnState, RELAY_PIN, LED_1, LED_2, "may bom");
  handleButton(BUTTON_RELAY_OFF, lastRelayOffState, RELAY_FAN, LED_2, LED_1, "quat");

  if (millis() - lastKeypadDebounceTime > debounceDelay)
  {
    char key = scanKeypad();
    if (key != '\0')
    {
      // Thêm logic xử lý phím A trong chế độ SETTING_TIME
      if (currentMode == SETTING_TIME && key == 'A') {
        currentScreen = DEVICE_STATE;
        displayDeviceState();
        lastKeypadDebounceTime = millis();
      }
      else if (currentScreen == DEVICE_STATE && key == '*') {
        currentScreen = HOME;
        displayHome();
        lastKeypadDebounceTime = millis();
      }
      // Giữ nguyên các xử lý bàn phím khác từ mã gốc
      else
      if (key == '*' && currentScreen == HOME)
      {
        currentScreen = MENU;
        displayMenu();
        Serial.println("Switched to Menu");
        lastKeypadDebounceTime = millis();
      }
      else if (key == '*' && currentScreen == SELECT_MODE)
      {
        currentScreen = MENU;
        enteredPassword = "";
        displayMenu();
        Serial.println("Returned to Menu from Select Mode");
        lastKeypadDebounceTime = millis();
      }
      else if (key == '4' && currentScreen == MENU)
      {
        currentScreen = HOME;
        displayHome();
        Serial.println("Returned to Home");
        lastKeypadDebounceTime = millis();
      }
      else if (key == '2' && currentScreen == MENU)
      {
        currentScreen = LOCK_SYSTEM;
        displayLockSystem();
        Serial.println("Switched to Lock System");
        lastKeypadDebounceTime = millis();
      }
      else if (key == '1' && currentScreen == MENU)
      {
        currentScreen = SELECT_MODE;
        enteredPassword = "";
        displaySelectMode();
        Serial.println("Switched to Select Mode");
        lastKeypadDebounceTime = millis();
      }
      else if (currentScreen == LOCK_SYSTEM)
      {
        if (key == '1')
        {
          currentScreen = ENTER_PASSWORD;
          requiredPassword = "8888";
          enteredPassword = "";
          displayEnterPassword();
          Serial.println("Enter Password for Unlock (8888)");
          lastKeypadDebounceTime = millis();
        }
        else if (key == '2')
        {
          currentScreen = ENTER_PASSWORD;
          requiredPassword = "9999";
          enteredPassword = "";
          displayEnterPassword();
          Serial.println("Enter Password for Lock (9999)");
          lastKeypadDebounceTime = millis();
        }
      }
      else if (currentScreen == ENTER_PASSWORD)
      {
        if ((key >= '0' && key <= '9') && enteredPassword.length() < 4)
        {
          enteredPassword += key;
          displayEnterPassword();
          Serial.print("Password Input: ");
          Serial.println(enteredPassword);
          lastKeypadDebounceTime = millis();
        }
        else if (key == '#')
        {
          if (enteredPassword == requiredPassword)
          {
            if (requiredPassword == "8888")
            {
              isSystemLocked = false;
              displayMessage("System Unlocked");
              Serial.println("System Unlocked");
            }
            else if (requiredPassword == "9999")
            {
              isSystemLocked = true;
              displayMessage("System Locked");
              Serial.println("System Locked");
            }
            currentScreen = HOME;
            displayHome();
          }
          else
          {
            displayMessage("Wrong Password");
            Serial.println("Wrong Password");
            enteredPassword = "";
            displayEnterPassword();
          }
          lastKeypadDebounceTime = millis();
        }
      }
      else if (currentScreen == SELECT_MODE)
      {
        if ((key >= '0' && key <= '9') && enteredPassword.length() < 4)
        {
          enteredPassword += key;
          displaySelectMode();
          Serial.print("Mode Code Input: ");
          Serial.println(enteredPassword);
          lastKeypadDebounceTime = millis();
        }
        else if (key == '#')
        {
          if (enteredPassword == "1111")
          {
            currentMode = MANUAL;
            digitalWrite(LED_PIN, LOW);
            digitalWrite(LED_1, LOW);
            digitalWrite(LED_2, LOW);
            digitalWrite(RELAY_PIN, HIGH);
            digitalWrite(RELAY_FAN, HIGH);
            displayMessage("Mode: Manual");
            displayMode();
            Serial.println("Switched to Manual Mode via Keypad (1111)");
          }
          else if (enteredPassword == "2222")
          {
            currentMode = AUTO;
            digitalWrite(LED_PIN, LOW);
            digitalWrite(LED_1, LOW);
            digitalWrite(LED_2, LOW);
            digitalWrite(RELAY_PIN, HIGH);
            digitalWrite(RELAY_FAN, HIGH);
            displayMessage("Mode: Auto");
            displayMode();
            Serial.println("Switched to Auto Mode via Keypad (2222)");
          }
          else if (enteredPassword == "3333")
          {
            currentMode = SETTING_TIME;
            digitalWrite(LED_PIN, LOW);
            digitalWrite(LED_1, LOW);
            digitalWrite(LED_2, LOW);
            digitalWrite(RELAY_PIN, HIGH);
            digitalWrite(RELAY_FAN, HIGH);
            displayMessage("Mode: Set Time");
            currentScreen = SET_TIME_MENU;
            displaySetTimeMenu();
            Serial.println("Switched to Set Time Mode via Keypad (3333)");
          }
          else
          {
            displayMessage("Wrong Code");
            Serial.println("Wrong Mode Code");
            enteredPassword = "";
            displaySelectMode();
            lastKeypadDebounceTime = millis();
            return;
          }
          enteredPassword = "";
          lastKeypadDebounceTime = millis();
        }
        else if (key == 'D')
        {
          enteredPassword = "";
          displaySelectMode();
          Serial.println("Cleared mode code input (Keypad D)");
          lastKeypadDebounceTime = millis();
        }
      }
      else if (currentScreen == SET_TIME_MENU)
      {
        if (key == '1' || key == '2' || key == '3')
        {
          selectedDevice = key - '0';
          inputPhase = 0;
          timeInput = "";
          onHour = -1;
          onMinute = -1;
          offHour = -1;
          offMinute = -1;
          currentScreen = SET_TIME_INPUT;
          displaySetTimeInput();
        }
        else if (key == '4') // Cài đặt thời gian hiện tại
        {
          currentScreen = SET_CURRENT_TIME;
          currentTimeInput = "";
          currentInputPhase = 0;
          displaySetCurrentTime();
        }
        else if (key == '5') // Thoát
        {
          currentScreen = HOME;
          displayHome();
          Serial.println("Exited Set Time Menu");
        }
        else if (key == '*')
        {
          currentScreen = HOME;
          displayHome();
        }
      }
      else if (currentScreen == SET_TIME_INPUT)
      {
        if (key >= '0' && key <= '9' && timeInput.length() < 4)
        {
          timeInput += key;
          displaySetTimeInput();
        }
        else if (key == 'A' && timeInput.length() > 0)
        {
          timeInput = timeInput.substring(0, timeInput.length() - 1);
          displaySetTimeInput();
        }
        else if (key == 'C')
        {
          timeInput = "";
          displaySetTimeInput();
        }
        else if (key == '#')
        {
          if (timeInput.length() == 4)
          {
            int hh = timeInput.substring(0, 2).toInt();
            int mm = timeInput.substring(2, 4).toInt();
            if (hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59)
            {
              if (inputPhase == 0)
              {
                onHour = hh;
                onMinute = mm;
                inputPhase = 1;
                timeInput = "";
                displaySetTimeInput();
              }
              else if (inputPhase == 1)
              {
                offHour = hh;
                offMinute = mm;
                if (onHour * 60 + onMinute < offHour * 60 + offMinute)
                {
                  if (selectedDevice == 1)
                  {
                    denTime.onHour = onHour;
                    denTime.onMinute = onMinute;
                    denTime.offHour = offHour;
                    denTime.offMinute = offMinute;
                  }
                  else if (selectedDevice == 2)
                  {
                    quatTime.onHour = onHour;
                    quatTime.onMinute = onMinute;
                    quatTime.offHour = offHour;
                    quatTime.offMinute = offMinute;
                  }
                  else if (selectedDevice == 3)
                  {
                    mayBomTime.onHour = onHour;
                    mayBomTime.onMinute = onMinute;
                    mayBomTime.offHour = offHour;
                    mayBomTime.offMinute = offMinute;
                  }
                  lcd.setCursor(0, 3);
                  lcd.print("Cai dat thanh cong");
                  delay(2000);
                  currentScreen = SET_TIME_MENU;
                  displaySetTimeMenu();
                }
                else
                {
                  lcd.setCursor(0, 3);
                  lcd.print("Cai dat sai");
                  delay(2000);
                  inputPhase = 0;
                  timeInput = "";
                  displaySetTimeInput();
                }
              }
            }
            else
            {
              lcd.setCursor(0, 3);
              lcd.print("Thoi gian khong hop le");
              delay(2000);
              lcd.setCursor(0, 3);
              lcd.print("                    ");
            }
          }
          else
          {
            lcd.setCursor(0, 3);
            lcd.print("Nhap du ON va OFF");
            delay(2000);
            lcd.setCursor(0, 3);
            lcd.print("                    ");
          }
        }
        else if (key == '*')
        {
          currentScreen = SET_TIME_MENU;
          displaySetTimeMenu();
        }
      }
      else if (currentScreen == SET_CURRENT_TIME)
      {
        if (key >= '0' && key <= '9')
        {
          if (currentInputPhase == 0 && currentTimeInput.length() < 2)
          {
            currentTimeInput += key;
            displaySetCurrentTime();
          }
          else if (currentInputPhase == 1 && currentTimeInput.length() < 4)
          {
            currentTimeInput += key;
            displaySetCurrentTime();
          }
        }
        else if (key == 'A' && currentTimeInput.length() > 0)
        {
          currentTimeInput = currentTimeInput.substring(0, currentTimeInput.length() - 1);
          displaySetCurrentTime();
        }
        else if (key == 'C')
        {
          currentTimeInput = "";
          displaySetCurrentTime();
        }
        else if (key == '#')
        {
          if (currentInputPhase == 0 && currentTimeInput.length() == 2)
          {
            int hh = currentTimeInput.toInt();
            if (hh >= 0 && hh <= 23)
            {
              currentInputPhase = 1;
              displaySetCurrentTime();
            }
            else
            {
              lcd.setCursor(0, 3);
              lcd.print("Gio khong hop le");
              delay(2000);
              lcd.setCursor(0, 3);
              lcd.print("                    ");
            }
          }
          else if (currentInputPhase == 1 && currentTimeInput.length() == 4)
          {
            int hh = currentTimeInput.substring(0, 2).toInt();
            int mm = currentTimeInput.substring(2, 4).toInt();
            if (mm >= 0 && mm <= 59)
            {
              currentTimeSeconds = hh * 3600 + mm * 60;
              timeSetMillis = millis();
              lcd.setCursor(0, 3);
              lcd.print("Cai dat thanh cong");
              delay(2000);
              currentScreen = SET_TIME_MENU;
              displaySetTimeMenu();
            }
            else
            {
              lcd.setCursor(0, 3);
              lcd.print("Phut khong hop le");
              delay(2000);
              lcd.setCursor(0, 3);
              lcd.print("                    ");
            }
          }
          else
          {
            lcd.setCursor(0, 3);
            lcd.print("Nhap day du gio/phut");
            delay(2000);
            lcd.setCursor(0, 3);
            lcd.print("                    ");
          }
        }
        else if (key == '*')
        {
          currentScreen = SET_TIME_MENU;
          displaySetTimeMenu();
        }
        lastKeypadDebounceTime = millis();
      }
      if (currentMode == MANUAL && !isSystemLocked)
      {
        if (key == 'A')
        {
          digitalWrite(LED_PIN, !digitalRead(LED_PIN));
          Serial.println(digitalRead(LED_PIN) ? "LED_PIN ON (Keypad A)" : "LED_PIN OFF (Keypad A)");
          displayDeviceStatus(digitalRead(LED_PIN) ? "Bat den" : "Tat den");
          lastKeypadDebounceTime = millis();
        }
        else if (key == 'B')
        {
          digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));
          digitalWrite(LED_1, digitalRead(RELAY_PIN));
          digitalWrite(LED_2, !digitalRead(RELAY_PIN));
          Serial.println(digitalRead(RELAY_PIN) ? "Pump ON (Keypad B)" : "Pump OFF (Keypad B)");
          displayDeviceStatus(digitalRead(RELAY_PIN) ? "Bat may bom" : "Tat may bom");
          lastKeypadDebounceTime = millis();
        }
        else if (key == 'C')
        {
          digitalWrite(RELAY_FAN, !digitalRead(RELAY_FAN));
          digitalWrite(LED_2, digitalRead(RELAY_FAN));
          digitalWrite(LED_1, !digitalRead(RELAY_FAN));
          Serial.println(digitalRead(RELAY_FAN) ? "Fan ON (Keypad C)" : "Fan OFF (Keypad C)");
          displayDeviceStatus(digitalRead(RELAY_FAN) ? "Bat quat" : "Tat quat");
          lastKeypadDebounceTime = millis();
        }
      }
    }
  }

  if (currentMode == AUTO && currentScreen != MENU && currentScreen != LOCK_SYSTEM &&
      currentScreen != ENTER_PASSWORD && currentScreen != SELECT_MODE)
  {
    int lightValue = digitalRead(LIGHT_SENSOR);
    Serial.print("Light Sensor: ");
    Serial.println(lightValue == HIGH ? "HIGH (Light)" : "LOW (No light)");

    if (lightValue == LOW)
    {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED OFF");
    }
    else
    {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED ON");
    }
  }

  if (currentMode == AUTO && !isSystemLocked && (millis() - lastDistanceMeasureTime > distanceMeasureInterval))
  {
    float distance = measureDistance();
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    if (distance > PUMP_ON_DISTANCE)
    {
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(LED_1, HIGH);
      digitalWrite(LED_2, LOW);
      digitalWrite(RELAY_FAN, HIGH);
      Serial.println("Pump ON, LED1 ON, LED2 OFF, Fan OFF");
    }
    else if (distance < PUMP_OFF_DISTANCE)
    {
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(LED_1, LOW);
      digitalWrite(LED_2, HIGH);
      digitalWrite(RELAY_FAN, HIGH);
      Serial.println("Pump OFF, LED1 OFF, LED2 ON, Fan OFF");
    }
    lastDistanceMeasureTime = millis();
  }

  // Gọi controlDevicesByTime khi ở chế độ SETTING_TIME và màn hình HOME
  if (currentMode == SETTING_TIME && !isSystemLocked && currentScreen == HOME)
  {
    controlDevicesByTime();
  }
}
