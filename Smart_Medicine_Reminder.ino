#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <Preferences.h>

// =====================================================
// OLED
// =====================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// =====================================================
// RTC
// =====================================================
RTC_DS3231 rtc;

// =====================================================
// ESP32 MEMORY
// =====================================================
Preferences preferences;

// =====================================================
// 4 SERVO MOTORS
// =====================================================
Servo box1;
Servo box2;
Servo box3;
Servo box4;

#define SERVO1_PIN 33
#define SERVO2_PIN 26
#define SERVO3_PIN 25
#define SERVO4_PIN 14

// Servo positions
#define SERVO_CLOSED 0
#define SERVO_OPEN   90

// =====================================================
// 4 IR SENSORS
// =====================================================
#define IR_SENSOR_1 34
#define IR_SENSOR_2 35
#define IR_SENSOR_3 27
#define IR_SENSOR_4 32

// IR sensor active state
#define IR_ACTIVE_STATE LOW

// =====================================================
// LED + BUZZER
// =====================================================
#define RED_LED   19
#define GREEN_LED 23
#define BUZZER    18

// =====================================================
// 4 PUSH BUTTONS
// =====================================================
//
// B1 = NEXT
// B2 = PLUS
// B3 = MINUS
// B4 = SAVE ONLY
//
// =====================================================
#define BUTTON_NEXT  13
#define BUTTON_PLUS   4
#define BUTTON_MINUS 16
#define BUTTON_SAVE  17

// =====================================================
// BUTTON TIMING
// =====================================================
#define DEBOUNCE_TIME 50

// B4 must be held for 5 seconds
#define SAVE_HOLD_TIME 5000

// =====================================================
// REMINDER TIMING
// =====================================================
#define GREEN_TIME 2000

// =====================================================
// MEDICINE NAMES
// =====================================================
const char* medicineName[4] = {
  "BP Medicine",
  "Sugar Medicine",
  "Cholesterol",
  "Acidity Medicine"
};

// =====================================================
// MEDICINE REMINDER TIMES
// =====================================================
//
// Default values
// These will be replaced after manual SAVE.
// =====================================================
int medicineHour[4] = {
  7,
  14,
  18,
  21
};

int medicineMinute[4] = {
  0,
  0,
  0,
  30
};

// =====================================================
// SETTING VARIABLES
// =====================================================
bool settingMode = false;

// 0 to 12 = 13 settings
int settingField = 0;

// =====================================================
// RTC SETTINGS
// =====================================================
int setHour;
int setMinute;
int setDay;
int setMonth;
int setYear;

// =====================================================
// MEDICINE SETTINGS
// =====================================================
int settingBoxHour[4];
int settingBoxMinute[4];

// =====================================================
// REMINDER VARIABLES
// =====================================================
bool reminderActive = false;

int currentReminderBox = -1;

bool reminderInitialIRState = HIGH;
unsigned long irChangeTime = 0;

unsigned long reminderStartTime = 0;

// Prevent same reminder from repeatedly triggering
int lastReminderDay = -1;
int lastReminderMonth = -1;
int lastReminderYear = -1;
int lastReminderBox = -1;


// =====================================================
// FORMAT TIME
// =====================================================
String formatTime12(int hour, int minute) {

  int h = hour;

  String ampm = "AM";

  if (h >= 12) {
    ampm = "PM";
  }

  if (h == 0) {
    h = 12;
  }
  else if (h > 12) {
    h = h - 12;
  }

  String result = "";

  if (h < 10) {
    result += "0";
  }

  result += String(h);
  result += ":";

  if (minute < 10) {
    result += "0";
  }

  result += String(minute);
  result += " ";
  result += ampm;

  return result;
}


// =====================================================
// LIVE RTC TIME
// =====================================================
String getLiveTime() {

  DateTime now = rtc.now();

  return formatTime12(
    now.hour(),
    now.minute()
  );
}


// =====================================================
// LIVE RTC DATE
// =====================================================
String getLiveDate() {

  DateTime now = rtc.now();

  String date = "";

  if (now.day() < 10) {
    date += "0";
  }

  date += String(now.day());

  date += "/";

  if (now.month() < 10) {
    date += "0";
  }

  date += String(now.month());

  date += "/";

  date += String(now.year());

  return date;
}


// =====================================================
// DAYS IN MONTH
// =====================================================
int daysInMonth(int month, int year) {

  if (month == 2) {

    bool leapYear =
      ((year % 4 == 0 && year % 100 != 0) ||
       (year % 400 == 0));

    if (leapYear) {
      return 29;
    }

    return 28;
  }

  if (month == 4 ||
      month == 6 ||
      month == 9 ||
      month == 11) {

    return 30;
  }

  return 31;
}


// =====================================================
// FIX INVALID DAY
// =====================================================
void fixDayForMonth() {

  int maxDay =
    daysInMonth(
      setMonth,
      setYear
    );

  if (setDay > maxDay) {
    setDay = maxDay;
  }

  if (setDay < 1) {
    setDay = 1;
  }
}


// =====================================================
// SHORT BUTTON PRESS
// =====================================================
bool buttonPressed(uint8_t pin) {

  if (digitalRead(pin) != LOW) {
    return false;
  }

  delay(DEBOUNCE_TIME);

  if (digitalRead(pin) != LOW) {
    return false;
  }

  while (digitalRead(pin) == LOW) {
    delay(5);
  }

  delay(DEBOUNCE_TIME);

  return true;
}


// =====================================================
// B4 = 5 SECOND SAVE
// =====================================================
//
// B4 short press       = NOTHING
// B4 < 5 seconds       = NOTHING
// B4 >= 5 seconds      = SAVE
// B4 never = BACK
//
// =====================================================
bool saveButtonLongPressed() {

  // Button not pressed
  if (digitalRead(BUTTON_SAVE) != LOW) {
    return false;
  }

  delay(DEBOUNCE_TIME);

  // Check again
  if (digitalRead(BUTTON_SAVE) != LOW) {
    return false;
  }

  unsigned long startTime = millis();

  // Keep checking while button is held
  while (digitalRead(BUTTON_SAVE) == LOW) {

    unsigned long elapsed =
      millis() - startTime;

    // 5 seconds completed
    if (elapsed >= SAVE_HOLD_TIME) {

      Serial.println(
        "B4 5 SECOND HOLD DETECTED"
      );

      // Wait until button is released
      while (digitalRead(BUTTON_SAVE) == LOW) {
        delay(10);
      }

      delay(100);

      return true;
    }

    delay(10);
  }

  // Released before 5 seconds
  // NO SAVE
  return false;
}


// =====================================================
// NORMAL OLED SCREEN
// =====================================================
void showNormalScreen() {

  DateTime now = rtc.now();

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("SMART MEDICINE");

  display.setCursor(0, 12);
  display.print("TIME: ");
  display.println(getLiveTime());

  display.setCursor(0, 24);
  display.print("DATE: ");
  display.println(getLiveDate());

  display.setCursor(0, 36);
  display.println("NEXT MEDICINE:");

  display.setCursor(0, 48);

  int nextBox = -1;

  int currentMinutes =
    now.hour() * 60 + now.minute();

  int smallestDifference = 9999;

  for (int i = 0; i < 4; i++) {

    int medicineMinutes =
      medicineHour[i] * 60 +
      medicineMinute[i];

    int difference =
      medicineMinutes -
      currentMinutes;

    if (difference < 0) {
      difference += 1440;
    }

    if (difference < smallestDifference) {

      smallestDifference =
        difference;

      nextBox = i;
    }
  }

  if (nextBox >= 0) {

    display.print("B");
    display.print(nextBox + 1);
    display.print(" ");

    display.println(
      formatTime12(
        medicineHour[nextBox],
        medicineMinute[nextBox]
      )
    );
  }

  display.display();
}


// =====================================================
// OPEN BOX
// =====================================================
void openBox(int boxNumber) {

  if (boxNumber == 0) {
    box1.write(SERVO_OPEN);
  }

  else if (boxNumber == 1) {
    box2.write(SERVO_OPEN);
  }

  else if (boxNumber == 2) {
    box3.write(SERVO_OPEN);
  }

  else if (boxNumber == 3) {
    box4.write(SERVO_OPEN);
  }
}


// =====================================================
// CLOSE BOX
// =====================================================
void closeBox(int boxNumber) {

  if (boxNumber == 0) {
    box1.write(SERVO_CLOSED);
  }

  else if (boxNumber == 1) {
    box2.write(SERVO_CLOSED);
  }

  else if (boxNumber == 2) {
    box3.write(SERVO_CLOSED);
  }

  else if (boxNumber == 3) {
    box4.write(SERVO_CLOSED);
  }
}


// =====================================================
// READ IR SENSOR
// =====================================================
int getIR(int boxNumber) {

  if (boxNumber == 0) {
    return digitalRead(IR_SENSOR_1);
  }

  else if (boxNumber == 1) {
    return digitalRead(IR_SENSOR_2);
  }

  else if (boxNumber == 2) {
    return digitalRead(IR_SENSOR_3);
  }

  else {
    return digitalRead(IR_SENSOR_4);
  }
}


// =====================================================
// START REMINDER
// =====================================================
void startReminder(int boxNumber) {

  reminderActive = true;

  currentReminderBox =
    boxNumber;

  reminderStartTime =
    millis();

  reminderInitialIRState = getIR(boxNumber);
  irChangeTime = 0;

  Serial.println();
  Serial.println("==============================");
  Serial.println("MEDICINE REMINDER STARTED");
  Serial.println("==============================");

  Serial.print("BOX: ");
  Serial.println(boxNumber + 1);

  Serial.print("MEDICINE: ");
  Serial.println(
    medicineName[boxNumber]
  );

  Serial.print("TIME: ");
  Serial.println(getLiveTime());

  // Open medicine box
  openBox(boxNumber);

  // Red LED ON
  digitalWrite(
    RED_LED,
    HIGH
  );

  // Buzzer ON
  digitalWrite(
    BUZZER,
    HIGH
  );
}


// =====================================================
// MEDICINE TAKEN
// =====================================================
void medicineTaken() {

  int boxNumber =
    currentReminderBox;

  if (boxNumber < 0 ||
      boxNumber > 3) {

    return;
  }

  Serial.println();
  Serial.println("==============================");
  Serial.println("MEDICINE TAKEN");
  Serial.println("==============================");

  // Buzzer OFF
  digitalWrite(
    BUZZER,
    LOW
  );

  // Red LED OFF
  digitalWrite(
    RED_LED,
    LOW
  );

  // Close box
  closeBox(boxNumber);

  // Green LED ON
  digitalWrite(
    GREEN_LED,
    HIGH
  );

  // OLED
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(
    SSD1306_WHITE
  );

  display.setCursor(0, 0);

  display.println(
    "GOOD FOR TAKING"
  );

  display.setCursor(0, 14);

  display.println(
    medicineName[boxNumber]
  );

  display.setCursor(0, 28);

  display.println(
    "MEDICINE TAKEN"
  );

  display.setCursor(0, 42);

  display.print("TIME: ");

  display.println(
    getLiveTime()
  );

  display.display();

  delay(GREEN_TIME);

  // Green LED OFF
  digitalWrite(
    GREEN_LED,
    LOW
  );

  // Record reminder
  DateTime now = rtc.now();

  lastReminderDay =
    now.day();

  lastReminderMonth =
    now.month();

  lastReminderYear =
    now.year();

  lastReminderBox =
    boxNumber;

  reminderActive = false;

  currentReminderBox = -1;
}


// =====================================================
// CHECK MEDICINE SCHEDULE
// =====================================================
void checkMedicineSchedule() {

  if (reminderActive) {
    return;
  }

  DateTime now = rtc.now();

  for (int i = 0; i < 4; i++) {

    if (
      now.hour() ==
      medicineHour[i] &&

      now.minute() ==
      medicineMinute[i]
    ) {

      bool alreadyTriggered =

        lastReminderDay ==
        now.day()

        &&

        lastReminderMonth ==
        now.month()

        &&

        lastReminderYear ==
        now.year()

        &&

        lastReminderBox ==
        i;

      if (!alreadyTriggered) {

        startReminder(i);

        return;
      }
    }
  }
}


// =====================================================
// SETTING SCREEN
// =====================================================
void showSettingScreen(
  int fieldNumber,
  String title,
  String value
) {

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(0, 0);

  display.print("SETTING ");

  display.print(fieldNumber);

  display.println("/13");

  display.setCursor(0, 13);

  display.println(title);

  display.setTextSize(2);

  display.setCursor(0, 28);

  display.println(value);

  display.setTextSize(1);

  display.setCursor(0, 52);

  display.println(
    "B1:NEXT B2:+ B3:-"
  );

  display.display();
}


// =====================================================
// SHOW CURRENT SETTING
// =====================================================
void showCurrentSetting() {

  switch (settingField) {

    // ================================================
    // 1. RTC HOUR
    // ================================================
    case 0:

      showSettingScreen(
        1,
        "RTC HOUR",
        String(setHour)
      );

      break;


    // ================================================
    // 2. RTC MINUTE
    // ================================================
    case 1:

      showSettingScreen(
        2,
        "RTC MINUTE",
        String(setMinute)
      );

      break;


    // ================================================
    // 3. DAY
    // ================================================
    case 2:

      showSettingScreen(
        3,
        "DAY",
        String(setDay)
      );

      break;


    // ================================================
    // 4. MONTH
    // ================================================
    case 3:

      showSettingScreen(
        4,
        "MONTH",
        String(setMonth)
      );

      break;


    // ================================================
    // 5. YEAR
    // ================================================
    case 4:

      showSettingScreen(
        5,
        "YEAR",
        String(setYear)
      );

      break;


    // ================================================
    // 6. BOX 1 HOUR
    // ================================================
    case 5:

      showSettingScreen(
        6,
        "BOX 1 HOUR",
        String(settingBoxHour[0])
      );

      break;


    // ================================================
    // 7. BOX 1 MINUTE
    // ================================================
    case 6:

      showSettingScreen(
        7,
        "BOX 1 MINUTE",
        String(settingBoxMinute[0])
      );

      break;


    // ================================================
    // 8. BOX 2 HOUR
    // ================================================
    case 7:

      showSettingScreen(
        8,
        "BOX 2 HOUR",
        String(settingBoxHour[1])
      );

      break;


    // ================================================
    // 9. BOX 2 MINUTE
    // ================================================
    case 8:

      showSettingScreen(
        9,
        "BOX 2 MINUTE",
        String(settingBoxMinute[1])
      );

      break;


    // ================================================
    // 10. BOX 3 HOUR
    // ================================================
    case 9:

      showSettingScreen(
        10,
        "BOX 3 HOUR",
        String(settingBoxHour[2])
      );

      break;


    // ================================================
    // 11. BOX 3 MINUTE
    // ================================================
    case 10:

      showSettingScreen(
        11,
        "BOX 3 MINUTE",
        String(settingBoxMinute[2])
      );

      break;


    // ================================================
    // 12. BOX 4 HOUR
    // ================================================
    case 11:

      showSettingScreen(
        12,
        "BOX 4 HOUR",
        String(settingBoxHour[3])
      );

      break;


    // ================================================
    // 13. BOX 4 MINUTE
    // ================================================
    case 12:

      display.clearDisplay();

      display.setTextColor(
        SSD1306_WHITE
      );

      display.setTextSize(1);

      display.setCursor(0, 0);

      display.println(
        "SETTING 13/13"
      );

      display.setCursor(0, 13);

      display.println(
        "BOX 4 MINUTE"
      );

      display.setTextSize(2);

      display.setCursor(0, 28);

      display.println(
        String(settingBoxMinute[3])
      );

      display.setTextSize(1);

      display.setCursor(0, 50);

      display.println(
        "B2:+ B3:- B4=SAVE"
      );

      display.display();

      break;
  }
}


// =====================================================
// INCREASE SETTING
// =====================================================
void increaseSetting() {

  switch (settingField) {

    // 1 RTC HOUR
    case 0:

      setHour++;

      if (setHour > 23) {
        setHour = 0;
      }

      break;


    // 2 RTC MINUTE
    case 1:

      setMinute++;

      if (setMinute > 59) {
        setMinute = 0;
      }

      break;


    // 3 DAY
    case 2:

      setDay++;

      if (
        setDay >
        daysInMonth(
          setMonth,
          setYear
        )
      ) {

        setDay = 1;
      }

      break;


    // 4 MONTH
    case 3:

      setMonth++;

      if (setMonth > 12) {
        setMonth = 1;
      }

      fixDayForMonth();

      break;


    // 5 YEAR
    case 4:

      setYear++;

      if (setYear > 2099) {
        setYear = 2025;
      }

      fixDayForMonth();

      break;


    // 6 BOX 1 HOUR
    case 5:

      settingBoxHour[0]++;

      if (settingBoxHour[0] > 23) {
        settingBoxHour[0] = 0;
      }

      break;


    // 7 BOX 1 MINUTE
    case 6:

      settingBoxMinute[0]++;

      if (settingBoxMinute[0] > 59) {
        settingBoxMinute[0] = 0;
      }

      break;


    // 8 BOX 2 HOUR
    case 7:

      settingBoxHour[1]++;

      if (settingBoxHour[1] > 23) {
        settingBoxHour[1] = 0;
      }

      break;


    // 9 BOX 2 MINUTE
    case 8:

      settingBoxMinute[1]++;

      if (settingBoxMinute[1] > 59) {
        settingBoxMinute[1] = 0;
      }

      break;


    // 10 BOX 3 HOUR
    case 9:

      settingBoxHour[2]++;

      if (settingBoxHour[2] > 23) {
        settingBoxHour[2] = 0;
      }

      break;


    // 11 BOX 3 MINUTE
    case 10:

      settingBoxMinute[2]++;

      if (settingBoxMinute[2] > 59) {
        settingBoxMinute[2] = 0;
      }

      break;


    // 12 BOX 4 HOUR
    case 11:

      settingBoxHour[3]++;

      if (settingBoxHour[3] > 23) {
        settingBoxHour[3] = 0;
      }

      break;


    // 13 BOX 4 MINUTE
    case 12:

      settingBoxMinute[3]++;

      if (settingBoxMinute[3] > 59) {
        settingBoxMinute[3] = 0;
      }

      break;
  }
}


// =====================================================
// DECREASE SETTING
// =====================================================
void decreaseSetting() {

  switch (settingField) {

    // 1 RTC HOUR
    case 0:

      setHour--;

      if (setHour < 0) {
        setHour = 23;
      }

      break;


    // 2 RTC MINUTE
    case 1:

      setMinute--;

      if (setMinute < 0) {
        setMinute = 59;
      }

      break;


    // 3 DAY
    case 2:

      setDay--;

      if (setDay < 1) {

        setDay =
          daysInMonth(
            setMonth,
            setYear
          );
      }

      break;


    // 4 MONTH
    case 3:

      setMonth--;

      if (setMonth < 1) {
        setMonth = 12;
      }

      fixDayForMonth();

      break;


    // 5 YEAR
    case 4:

      setYear--;

      if (setYear < 2025) {
        setYear = 2099;
      }

      fixDayForMonth();

      break;


    // 6 BOX 1 HOUR
    case 5:

      settingBoxHour[0]--;

      if (settingBoxHour[0] < 0) {
        settingBoxHour[0] = 23;
      }

      break;


    // 7 BOX 1 MINUTE
    case 6:

      settingBoxMinute[0]--;

      if (settingBoxMinute[0] < 0) {
        settingBoxMinute[0] = 59;
      }

      break;


    // 8 BOX 2 HOUR
    case 7:

      settingBoxHour[1]--;

      if (settingBoxHour[1] < 0) {
        settingBoxHour[1] = 23;
      }

      break;


    // 9 BOX 2 MINUTE
    case 8:

      settingBoxMinute[1]--;

      if (settingBoxMinute[1] < 0) {
        settingBoxMinute[1] = 59;
      }

      break;


    // 10 BOX 3 HOUR
    case 9:

      settingBoxHour[2]--;

      if (settingBoxHour[2] < 0) {
        settingBoxHour[2] = 23;
      }

      break;


    // 11 BOX 3 MINUTE
    case 10:

      settingBoxMinute[2]--;

      if (settingBoxMinute[2] < 0) {
        settingBoxMinute[2] = 59;
      }

      break;


    // 12 BOX 4 HOUR
    case 11:

      settingBoxHour[3]--;

      if (settingBoxHour[3] < 0) {
        settingBoxHour[3] = 23;
      }

      break;


    // 13 BOX 4 MINUTE
    case 12:

      settingBoxMinute[3]--;

      if (settingBoxMinute[3] < 0) {
        settingBoxMinute[3] = 59;
      }

      break;
  }
}


// =====================================================
// SAVE ALL 13 SETTINGS
// =====================================================
void saveAllSettings() {

  // Make date valid
  fixDayForMonth();

  // ===================================================
  // SAVE RTC
  // ===================================================
  rtc.adjust(
    DateTime(
      setYear,
      setMonth,
      setDay,
      setHour,
      setMinute,
      0
    )
  );


  // ===================================================
  // SAVE 4 MEDICINE TIMES
  // ===================================================
  for (int i = 0; i < 4; i++) {

    medicineHour[i] =
      settingBoxHour[i];

    medicineMinute[i] =
      settingBoxMinute[i];
  }


  // ===================================================
  // SAVE TO ESP32 PERMANENT MEMORY
  // ===================================================
  preferences.begin(
    "medicine",
    false
  );

  for (int i = 0; i < 4; i++) {

    String hourKey =
      "h" + String(i);

    String minuteKey =
      "m" + String(i);

    preferences.putInt(
      hourKey.c_str(),
      medicineHour[i]
    );

    preferences.putInt(
      minuteKey.c_str(),
      medicineMinute[i]
    );
  }

  preferences.end();


  // ===================================================
  // RESET REMINDER TRACKING
  // ===================================================
  lastReminderDay = -1;
  lastReminderMonth = -1;
  lastReminderYear = -1;
  lastReminderBox = -1;


  // ===================================================
  // SERIAL MONITOR
  // ===================================================
  Serial.println();
  Serial.println("==============================");
  Serial.println("ALL 13 SETTINGS SAVED");
  Serial.println("==============================");

  Serial.print("DATE: ");
  Serial.println(getLiveDate());

  Serial.print("TIME: ");
  Serial.println(getLiveTime());

  for (int i = 0; i < 4; i++) {

    Serial.print("BOX ");
    Serial.print(i + 1);
    Serial.print(": ");

    Serial.println(
      formatTime12(
        medicineHour[i],
        medicineMinute[i]
      )
    );
  }


  // ===================================================
  // OLED SAVED SCREEN
  // ===================================================
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(
    SSD1306_WHITE
  );

  display.setCursor(0, 0);

  display.println(
    "ALL SETTINGS SAVED"
  );

  display.setCursor(0, 15);

  display.print("TIME: ");

  display.println(
    getLiveTime()
  );

  display.setCursor(0, 30);

  display.println(
    "4 MEDICINES SAVED"
  );

  display.setCursor(0, 45);

  display.println(
    "SYSTEM READY"
  );

  display.display();

  delay(2000);
}


// =====================================================
// ENTER SETTINGS
// =====================================================
void enterSettingMode() {

  settingMode = true;

  settingField = 0;


  // ===================================================
  // GET CURRENT RTC VALUES
  // ===================================================
  DateTime now = rtc.now();

  setHour =
    now.hour();

  setMinute =
    now.minute();

  setDay =
    now.day();

  setMonth =
    now.month();

  setYear =
    now.year();


  // ===================================================
  // COPY CURRENT MEDICINE VALUES
  // ===================================================
  for (int i = 0; i < 4; i++) {

    settingBoxHour[i] =
      medicineHour[i];

    settingBoxMinute[i] =
      medicineMinute[i];
  }

  fixDayForMonth();


  // ===================================================
  // SHOW SETTING 1
  // ===================================================
  showCurrentSetting();


  // ===================================================
  // SETTINGS LOOP
  // ===================================================
  while (settingMode) {

    // =================================================
    // B2 = PLUS
    // =================================================
    if (
      buttonPressed(
        BUTTON_PLUS
      )
    ) {

      increaseSetting();

      showCurrentSetting();
    }


    // =================================================
    // B3 = MINUS
    // =================================================
    if (
      buttonPressed(
        BUTTON_MINUS
      )
    ) {

      decreaseSetting();

      showCurrentSetting();
    }


    // =================================================
    // B1 = NEXT
    // =================================================
    if (
      buttonPressed(
        BUTTON_NEXT
      )
    ) {

      // -----------------------------------------------
      // Move from setting 1 to setting 13
      // -----------------------------------------------
      if (settingField < 12) {

        settingField++;

        showCurrentSetting();
      }

      // -----------------------------------------------
      // At setting 13:
      // B1 does NOT go anywhere.
      // -----------------------------------------------
      else {

        showCurrentSetting();
      }
    }


    // =================================================
    // B4 = SAVE ONLY
    // =================================================
    //
    // B4 works ONLY when settingField == 12
    // which means SETTING 13/13.
    //
    // B4 must be held for 5 seconds.
    //
    // =================================================
    if (settingField == 12) {

      if (
        saveButtonLongPressed()
      ) {

        // ---------------------------------------------
        // SHOW SAVING
        // ---------------------------------------------
        display.clearDisplay();

        display.setTextSize(1);

        display.setTextColor(
          SSD1306_WHITE
        );

        display.setCursor(0, 20);

        display.println(
          "SAVING ALL SETTINGS..."
        );

        display.display();

        delay(700);


        // ---------------------------------------------
        // SAVE EVERYTHING
        // ---------------------------------------------
        saveAllSettings();


        // ---------------------------------------------
        // EXIT SETTINGS
        // ---------------------------------------------
        settingMode = false;

        break;
      }
    }

    // =================================================
    // IMPORTANT:
    //
    // B4 BEFORE SETTING 13:
    // NOTHING HAPPENS
    //
    // B4 SHORT PRESS:
    // NOTHING HAPPENS
    //
    // B4 BACK:
    // NOT AVAILABLE
    // =================================================
  }
}


// =====================================================
// LOAD SAVED MEDICINE TIMES
// =====================================================
void loadMedicineTimes() {

  preferences.begin(
    "medicine",
    true
  );


  for (int i = 0; i < 4; i++) {

    String hourKey =
      "h" + String(i);

    String minuteKey =
      "m" + String(i);


    medicineHour[i] =
      preferences.getInt(
        hourKey.c_str(),
        medicineHour[i]
      );


    medicineMinute[i] =
      preferences.getInt(
        minuteKey.c_str(),
        medicineMinute[i]
      );


    // Safety
    if (
      medicineHour[i] < 0 ||
      medicineHour[i] > 23
    ) {

      medicineHour[i] = 0;
    }


    if (
      medicineMinute[i] < 0 ||
      medicineMinute[i] > 59
    ) {

      medicineMinute[i] = 0;
    }
  }


  preferences.end();
}


// =====================================================
// SETUP
// =====================================================
void setup() {

  // ===================================================
  // SERIAL
  // ===================================================
  Serial.begin(115200);

  delay(500);


  // ===================================================
  // I2C
  // ===================================================
  Wire.begin(
    21,
    22
  );


  // ===================================================
  // OLED
  // ===================================================
  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      0x3C
    )
  ) {

    Serial.println(
      "OLED ERROR"
    );

    while (1) {
      delay(100);
    }
  }


  // ===================================================
  // RTC
  // ===================================================
  if (!rtc.begin()) {

    Serial.println(
      "RTC ERROR - CHECK WIRING"
    );

    display.clearDisplay();

    display.setTextSize(1);

    display.setTextColor(
      SSD1306_WHITE
    );

    display.setCursor(0, 15);

    display.println(
      "RTC ERROR"
    );

    display.setCursor(0, 30);

    display.println(
      "CHECK WIRING"
    );

    display.display();

    while (1) {
      delay(100);
    }
  }


  // ===================================================
  // IR SENSORS
  // ===================================================
  pinMode(
    IR_SENSOR_1,
    INPUT
  );

  pinMode(
    IR_SENSOR_2,
    INPUT
  );

  pinMode(
    IR_SENSOR_3,
    INPUT
  );

  pinMode(
    IR_SENSOR_4,
    INPUT
  );


  // ===================================================
  // LED
  // ===================================================
  pinMode(
    RED_LED,
    OUTPUT
  );

  pinMode(
    GREEN_LED,
    OUTPUT
  );

  digitalWrite(
    RED_LED,
    LOW
  );

  digitalWrite(
    GREEN_LED,
    LOW
  );


  // ===================================================
  // BUZZER
  // ===================================================
  pinMode(
    BUZZER,
    OUTPUT
  );

  digitalWrite(
    BUZZER,
    LOW
  );


  // ===================================================
  // BUTTONS
  // ===================================================
  //
  // Button other terminal -> GND
  //
  pinMode(
    BUTTON_NEXT,
    INPUT_PULLUP
  );

  pinMode(
    BUTTON_PLUS,
    INPUT_PULLUP
  );

  pinMode(
    BUTTON_MINUS,
    INPUT_PULLUP
  );

  pinMode(
    BUTTON_SAVE,
    INPUT_PULLUP
  );


  // ===================================================
  // SERVO 1
  // ===================================================
  box1.setPeriodHertz(50);

  box1.attach(
    SERVO1_PIN,
    500,
    2400
  );


  // ===================================================
  // SERVO 2
  // ===================================================
  box2.setPeriodHertz(50);

  box2.attach(
    SERVO2_PIN,
    500,
    2400
  );


  // ===================================================
  // SERVO 3
  // ===================================================
  box3.setPeriodHertz(50);

  box3.attach(
    SERVO3_PIN,
    500,
    2400
  );


  // ===================================================
  // SERVO 4
  // ===================================================
  box4.setPeriodHertz(50);

  box4.attach(
    SERVO4_PIN,
    500,
    2400
  );


  // ===================================================
  // CLOSE ALL BOXES
  // ===================================================
  box1.write(SERVO_CLOSED);
  box2.write(SERVO_CLOSED);
  box3.write(SERVO_CLOSED);
  box4.write(SERVO_CLOSED);


  // ===================================================
  // LOAD SAVED MEDICINE TIMES
  // ===================================================
  loadMedicineTimes();


  // ===================================================
  // START SCREEN
  // ===================================================
  display.clearDisplay();

  display.setTextSize(1);

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setCursor(0, 0);

  display.println(
    "SMART MEDICINE"
  );

  display.setCursor(0, 14);

  display.println(
    "SYSTEM READY"
  );

  display.setCursor(0, 28);

  display.print(
    "TIME: "
  );

  display.println(
    getLiveTime()
  );

  display.setCursor(0, 42);

  display.println(
    "B1 = SETTINGS"
  );

  display.display();

  delay(2000);


  // ===================================================
  // SERIAL READY
  // ===================================================
  Serial.println();
  Serial.println("==============================");
  Serial.println("SMART MEDICINE SYSTEM READY");
  Serial.println("==============================");

  Serial.print("RTC DATE: ");
  Serial.println(getLiveDate());

  Serial.print("RTC TIME: ");
  Serial.println(getLiveTime());

  for (int i = 0; i < 4; i++) {

    Serial.print("BOX ");
    Serial.print(i + 1);
    Serial.print(" REMINDER: ");

    Serial.println(
      formatTime12(
        medicineHour[i],
        medicineMinute[i]
      )
    );
  }
}


// =====================================================
// LOOP
// =====================================================
void loop() {

  // ===================================================
  // NORMAL MODE
  // ===================================================
  if (!reminderActive) {

    // B1 = ENTER SETTINGS
    if (
      buttonPressed(
        BUTTON_NEXT
      )
    ) {

      enterSettingMode();

      return;
    }
  }


  // ===================================================
  // ACTIVE MEDICINE REMINDER
  // ===================================================
  if (reminderActive) {

    bool currentIRState = getIR(currentReminderBox);

    // Detect a NEW, stable IR state change.
    // This prevents an already-active sensor from instantly
    // marking the medicine as taken.
    if (currentIRState != reminderInitialIRState) {
      if (irChangeTime == 0) {
        irChangeTime = millis();
      }

      if (millis() - irChangeTime >= 300) {
        Serial.println("IR CHANGE DETECTED - MEDICINE TAKEN");
        medicineTaken();
        return;
      }
    } else {
      irChangeTime = 0;
    }

    // NO TIMEOUT: reminder continues until IR changes.
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println("TAKE THE MEDICINE");

    display.setCursor(0, 14);
    display.println(medicineName[currentReminderBox]);

    display.setCursor(0, 28);
    display.println("BOX OPEN");

    display.setCursor(0, 42);
    display.println("WAITING FOR IR...");

    display.display();
    delay(100);
    return;
  }

  // ===================================================
  // CHECK MEDICINE SCHEDULE
  // ===================================================
  checkMedicineSchedule();


  // ===================================================
  // NORMAL DISPLAY
  // ===================================================
  showNormalScreen();

  delay(300);
}
