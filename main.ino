// Libraries and Includes
#include <ezButton.h>
#include <U8g2lib.h>

// Pin Assignments
ezButton button1(12);  // create ezButton object (12);
ezButton button2(6);  // create ezButton object (6);
const int UV_SENSOR_PIN = A7; // UV sensor pin (A7)
const int BUZZER_PIN = 9; // Buzzer/vibration motor pin

// Button Things
const int SHORT_PRESS_TIME  = 1000; // time for a short press
const int LONG_PRESS_TIME  = 1000; // time for a long press
unsigned long pressedTime1  = 0;
unsigned long releasedTime1 = 0;
bool isPressing1 = false;
bool isLongDetected1 = false;
unsigned long pressedTime2  = 0;
unsigned long releasedTime2 = 0;
bool isPressing2 = false;
bool isLongDetected2 = false;

// UV Sensor Vars
int sensorVal;        // variable to store the value read from sensor
float uvVoltage;      // float containing calculated Voltage from sensor
int uvIndex;          // variable to store UV index
const int NUM_PREV_INDEXES = 30; // number of previous UV indexes to store
// array to hold most recent UV indexes (to smoothen changes)
int uvPrevIndexes[NUM_PREV_INDEXES];
int uvAvgIndex;       // average UV index the sensor has detected 
long uvCounter = 0;        // Number of times UV index has been checked
const unsigned long UV_FREQ = 2000;  // frequency to check the UV reading in ms
unsigned long uvCheckMillis = 0;

const int SUN_TIME_CONSTANT = 65; // 65 Mins * Skin Type Mod / UV Index = Max Time allowed
int maxTimeAllowed; // Max time the user can spend in the sun (minutes)
int timeLeft; // Time user has left in the sun
unsigned long timeExposed = 0; // Time the user has spent exposed to damaging levels of UV (ms)
int percentExposure; // Percentage of max time in sun that has been used
const int SPF = 30; // SPF value to calculate time in sun with sunscreen
bool exposureAlarmActive = false;
bool exposureAlarmSilent = false;

const unsigned long MINS_TO_MS = (unsigned long)1000*60; // mins to ms conversion constant

// Skin Menu Vars
enum SkinTypes{I = 1, II, III, IV, V, VI}; // Skin types 1 through 6 (linear scalars for now)
const String skinTypesStrings[] = {"I", "II", "III", "IV", "V", "VI"};
int skinType = I; // Default skintype set to I
//enum Menus{Main, Skin}; // Different Menus
//Menus currentMenu = Main;

// Alarm Vars (for sunscreen reapplication timer)
bool alarmSet = false;
unsigned long alarmSetTime = 0;
const int SUNSCREEN_LENGTH = 1; // Duration of sunscreen protection in minutes (120) //
const unsigned long alarmLength = (unsigned long)SUNSCREEN_LENGTH*MINS_TO_MS; 

// OLED
U8G2_SSD1306_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
const uint8_t *titleFont = u8g2_font_ncenB08_tr;
const uint8_t *standardFont = u8g2_font_ncenR08_tr; //te
 

// End Intialisation
/////////////////////////////////////////////////////////////////////////////////////

// UV Sensor 
void ReadUV() {
  uvCounter++; 
  sensorVal = analogRead(UV_SENSOR_PIN); // Read value on analog pin
  uvVoltage = sensorVal * (5.0 / 1023.0); // Convert to voltage
  uvIndex = round(uvVoltage / 0.1); // Convert voltage to UV Index (based on datasheet)
//  uvIndex = 60; //debugging
//  uvIndex = round(sensorVal / 90); // temporary calc based on photoresistor
//  uvTotalIndex += uvIndex; // Add calculated UV Index to total count
//  uvAvgIndex = round(uvTotalIndex / uvCounter); // Calculate average UV Index 
  uvPrevIndexes[uvCounter%NUM_PREV_INDEXES] = uvIndex; // Add calculated UV Index to total count
  int uvTotalIndex = 0;
  for(int i = 0; i < NUM_PREV_INDEXES; i++) {
    uvTotalIndex += uvPrevIndexes[i];   
  }
  uvAvgIndex = round(uvTotalIndex / NUM_PREV_INDEXES); // Calculate average UV Index  
  
//  if(uvAvgIndex > 0) {
//  maxTimeAllowed = SUN_TIME_CONSTANT * skinType / uvAvgIndex; // Calculate max time allowed(mins)    
//  }
//  else {
//   maxTimeAllowed = SUN_TIME_CONSTANT * skinType; 
//  }
  maxTimeAllowed = SUN_TIME_CONSTANT * skinType;
//  maxTimeAllowed = 1; // for debugging quickly

  if(uvAvgIndex > 0) {
    timeLeft = ceil((maxTimeAllowed - floor(timeExposed/MINS_TO_MS))/uvAvgIndex); // mins left in current sun
  }
  else {
    timeLeft = ceil(maxTimeAllowed - floor(timeExposed/MINS_TO_MS)); // mins left in current sun
  }
  
  percentExposure = 100*timeExposed/(maxTimeAllowed*MINS_TO_MS); // Calculate % of max time used

  if(percentExposure >= 100) { // Check if user has reached max sun intake
    Serial.println("Warning: Max Sun Exposure Reached!");
    exposureAlarmActive = true;
    if(!exposureAlarmSilent)
      ExposureAlarm(); // Run alarm    
  }
}

// Display the current information to the OLED
void UpdateOLED(unsigned long currentMillis) {
  u8g2.firstPage();
  do {  
    u8g2.setFont(titleFont);
    u8g2.drawStr(30,8,"UV Monitor");
    u8g2.setFont(standardFont);
    String tempStr = "Skin: " + skinTypesStrings[skinType-1];
    u8g2.setCursor(0, 20);
    u8g2.print(tempStr);    
    tempStr = "UV Index: " + String(uvIndex);
    u8g2.setCursor(65, 20);
    u8g2.print(tempStr);

    // Sunscreen logic
    if(alarmSet) {
      int sunscreenTimeLeft = ceil(float(alarmLength - (currentMillis - alarmSetTime))/MINS_TO_MS);
      tempStr = "Sunscreen:     " + String(sunscreenTimeLeft) + " mins";
    }
    else if(uvIndex > 2) {
      tempStr = "Apply Sunscreen Now!";
    }
    else {
      tempStr = "Sunscreen not required.";
    }    
    u8g2.setCursor(0, 32);
    u8g2.print(tempStr);

    // Exposure & Time remaining
//    tempStr = String(percentExposure) + "% exposed";
////    tempStr = "100% exposed,";
//    u8g2.setCursor(0, 44);
//    u8g2.print(tempStr);    

    tempStr = "Time outside: " + String(timeLeft) + " mins";
    u8g2.setCursor(0, 44);
    u8g2.print(tempStr);  
    
//    if (timeLeft <= 0) {
//      tempStr = "Seek shade!";
//      u8g2.setCursor(70, 44);
//      u8g2.print(tempStr);
//    }
//    else if (timeLeft >= 10){
//      tempStr = String(timeLeft) + " mins left";
//      u8g2.setCursor(65, 44);
//      u8g2.print(tempStr); 
//    }    
//    else {
//      tempStr = String(timeLeft) + " mins left";
//      u8g2.setCursor(74, 44);
//      u8g2.print(tempStr);
//    }

    // Exposure Graph
    u8g2.drawFrame(0,48,128,16);
    int barSize = percentExposure < 100 ? round(124*((float)percentExposure/100)) : 124;
    u8g2.drawBox(2,50,barSize,12);
    
    u8g2.setFontMode(1);
    u8g2.setDrawColor(2);
//    u8g2.drawStr(32,60,"UV Exposure");
    tempStr = String(percentExposure) + "% Daily UV";
    u8g2.setCursor(32,60);
    u8g2.print(tempStr); 
    
  } while ( u8g2.nextPage() );
}

//// Prints useful things for debugging
//void SerialDebugging(unsigned long currentMillis) { 
//  Serial.println("------------------------------------------");  
//  Serial.println("Skin Type:                " + skinTypesStrings[skinType-1]);
//  
//  Serial.println("Raw Sensor Value:         " + String(sensorVal));
//  Serial.println("Voltage Output:           " +  String(uvVoltage));
//  Serial.println("Current UV Index:         " +  String(uvIndex));
//  Serial.println("Avg UV Index:             " +  String(uvAvgIndex));
//  Serial.println("Max Time Allowed Outside: " +  String(maxTimeAllowed) + " mins");
//  Serial.print("Effective Time Exposed:   " +  String(timeExposed/MINS_TO_MS) + " mins ");
//  Serial.println(String(timeExposed/1000%60) + " secs");   
//  Serial.println("Percent Exposure:         " +  String(percentExposure) + "%");
//  Serial.print("Sunscreen Active:         "); 
//  Serial.println((alarmSet) ? "True" : "False"); 
//  if(alarmSet) {
//    unsigned long timeLeft = alarmLength - (currentMillis - alarmSetTime);
////    Serial.println("Millis: " + String(currentMillis) + ", " + String(alarmSetTime));
//    Serial.print("Alarm Time Remaining:     " +  String(timeLeft/MINS_TO_MS) + " mins "); 
//    Serial.println(String(timeLeft/1000%60) + " secs"); 
//  }
////  Serial.println("currentMenu: " +  String(currentMenu)); 
//  Serial.println("------------------------------------------");
//}

//void SkinMenu() { //////////////////////////////////////////////////////////////////////////
//  if(button2.isPressed()) { // Button 2 increments through Skin Types
//    Serial.println("Button 3 is pressed");
//    if(skinType < VI)
//      skinType++;
//    else
//      skinType = I;
//    Serial.println("Skin Type: " + skinTypesStrings[skinType-1]);
//    UpdateOLED(); // update OLED with new value
//  }
//
//  else if(button1.isPressed()) { // Button 1 saves value and returns to Main
////    Serial.println("Button 2 is pressed");
//    Serial.println("Skin Type set to " + skinTypesStrings[skinType-1] + ".");
//    currentMenu = Main;
//  }
//} 

void SunscreenAlarm() {
  const int HIGH_C = 2093; // in Hz
  const int LOW_C = HIGH_C/2;
  const int alarmDelay = 250; // in ms
  
  tone(BUZZER_PIN, HIGH_C); // turn the buzzer on to high note 
  delay(alarmDelay);
  tone(BUZZER_PIN, LOW_C);    
  delay(alarmDelay/2);
  tone(BUZZER_PIN, HIGH_C); 
  delay(alarmDelay);
  noTone(BUZZER_PIN); // turn the buzzer off
}

void ExposureAlarm() {
  const int HIGH_C = 2093; // in Hz
  const int LOW_C = HIGH_C/2;
  const int alarmDelay = 500; // in ms
  
  tone(BUZZER_PIN, HIGH_C, alarmDelay); // turn the buzzer on to high note 
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT); // Initialise built in LED as OUTPUT
  pinMode(BUZZER_PIN, OUTPUT); // Initialise buzzer as output
  
  Serial.begin(9600); // Begin serial comms
  u8g2.begin(); // Begin OLED
  button1.setDebounceTime(50); // set debounce time to 50 milliseconds
  button2.setDebounceTime(50); 

  // Start OLED display
  u8g2.firstPage();
  do {
    u8g2.setFont(standardFont);
    u8g2.drawStr(0,8,"Initialising...");
  } while ( u8g2.nextPage() );
}

void loop() {
  unsigned long currentMillis = millis(); // get current time
    
  button1.loop(); // MUST call the loop() function first for ezbuttons
  button2.loop(); 
  // Alarm Check
  if(alarmSet && currentMillis - alarmSetTime >= alarmLength) {
    Serial.println("Sunscreen Expired! Reapply now.");
    SunscreenAlarm();
    alarmSet = false;
  }
  
  // UV Sensor Check & Update Calcs
  unsigned long timeSinceUVCheck = currentMillis - uvCheckMillis;
  if(timeSinceUVCheck >= UV_FREQ) {
    if(alarmSet) // check if user has sunscreen on (assume SPF 30)
      timeExposed += (timeSinceUVCheck/SPF)*uvIndex; // Add time since last poll, adjusted for SPF
    else // BOTH CALCULATIONS CHECK IF UV IS AT 0
      timeExposed += timeSinceUVCheck*uvIndex; // Just add time since last poll
    
    ReadUV(); // Get current values
    UpdateOLED(currentMillis); // Update the OLED
//    SerialDebugging(currentMillis); // For debugging through serial monitor
    uvCheckMillis = currentMillis;
  } 

//  switch (currentMenu) {
//    case Main:    
      if(button1.isPressed()) {
        digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
        
        
        pressedTime1 = currentMillis;
        isPressing1 = true;
        isLongDetected1 = false;        
      }

      if(button1.isReleased()) {
        digitalWrite(LED_BUILTIN, LOW);    // turn the LED off
        
        isPressing1 = false;
        releasedTime1 = currentMillis;
    
        long pressDuration = releasedTime1 - pressedTime1;
    
        if(pressDuration < SHORT_PRESS_TIME) // SHORT PRESS OPTION
        {
          // Set alarm active 
          if(!alarmSet) {
            alarmSetTime = currentMillis;
            alarmSet = true;
            UpdateOLED(currentMillis); // update OLED with new value
            tone(BUZZER_PIN, 2093, 100); // turn the buzzer on to high note 
            Serial.println("Alarm set!");
          }
          else {
            Serial.println("Alarm already set!");
          }
        }              
      }

      if(isPressing1 == true && isLongDetected1 == false) {
        long pressDuration = currentMillis - pressedTime1;
    
        if(pressDuration > LONG_PRESS_TIME) { // LONG PRESS OPTION
          isLongDetected1 = true;          
          // reset sunscreen alarm
          if(alarmSet) {
            alarmSet = false;
            UpdateOLED(currentMillis); // update OLED with new value
            tone(BUZZER_PIN, 2093); // turn the buzzer on to high note 
            delay(100);
            noTone(BUZZER_PIN);
            delay(100);
            tone(BUZZER_PIN, 2093, 100);
            Serial.println("Alarm Removed!");
          } 
        }
      }
      
      if(button2.isPressed()) {
        pressedTime2 = currentMillis;
        isPressing2 = true;
        isLongDetected2 = false;
      }   

      if(button2.isReleased()) {
        isPressing2 = false;
        releasedTime2 = currentMillis;
    
        long pressDuration = releasedTime2 - pressedTime2;
    
        if(pressDuration < SHORT_PRESS_TIME) // SHORT PRESS OPTION
        {
          // Skin Type Menu
//          currentMenu = Skin;
//          Serial.println("Skin Type Menu:\nSkin Type: " + skinTypesStrings[skinType-1]);
          if(skinType < VI)
            skinType++;
          else
            skinType = I;
          Serial.println("Skin Type: " + skinTypesStrings[skinType-1]);          
          tone(BUZZER_PIN, 1568, 100); // turn the buzzer on to beep 
          UpdateOLED(currentMillis); // update OLED with new value
        }                   
      }

      if(isPressing2 == true && isLongDetected2 == false) {
        long pressDuration = currentMillis - pressedTime2;
    
        if(pressDuration > LONG_PRESS_TIME) { // LONG PRESS OPTION
          isLongDetected2 = true;          
          // silence active alarm
          if(exposureAlarmActive) {
            Serial.println("Alarm Silenced.");
            exposureAlarmSilent = true;
            tone(BUZZER_PIN, 2093); // turn the buzzer on to high note 
            delay(200);
            noTone(BUZZER_PIN);
            delay(100);
            tone(BUZZER_PIN, 2093, 100);
          } 
        }
      }       
      
//      break;
////      
////    case Skin: 
////      SkinMenu();      
////      break;
//    
//    default: currentMenu = Main; 
//  }
}
