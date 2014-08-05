#include "JsonParser.h"
#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
#define DAYS_IN_WEEK 7

typedef struct 
{
	bool enabled;
	int onHour, onMinute, offHour, offMinute;
} dayConfig;

// GLOBALS
// pins
const int relayPin = D0;  
const int buttonPin = D1;

// button stuff
long lastDebounceTime = 0;  // the last time the output pin was toggled
const long debounceDelay = 40;    // the debounce time; increase if the output flickers
int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin

// days and configuration 
char *dayNames[DAYS_IN_WEEK]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
dayConfig days[DAYS_IN_WEEK];
dayConfig currentDayConfig;
int active = 0;

// time related variables
long lastSync, lastBlink;
int timezone = 3;

#define CONFIG_STR_MAX_SIZE 600
char configStr[CONFIG_STR_MAX_SIZE]; // JSON string of configuration
char debugStr[60]; // print debug string temp storage


void setup() {
    // take control of the LED
    RGB.control(true);
    RGB.brightness(255);
    RGB.color(255, 255, 0); 

    // define the spark data sets
    Spark.variable("config", &configStr, STRING);
    Spark.function("config", processConfig);
    // Spark.function("timezone",processTimezone);
    // Spark.variable("timezone",&timezone, INT);
    
    // pins
    pinMode(relayPin, OUTPUT);
    pinMode(buttonPin, INPUT_PULLUP); 

    // initialize configuration (TODO: Read and initialize from EEPROM)
    for (int i=0; i<DAYS_IN_WEEK; i++)
    {
        days[i].enabled = true;
        days[i].onHour = 0;
        days[i].offHour = 0;
        days[i].onMinute = 0;
        days[i].offMinute = 0;
    }
    
    // start serial communication
    Serial.begin(9600);
    Serial.println("Remote timed control spark core unit ready.");

    // start     
    active = 0;
    Time.zone(3); // Jerusalem time (should be a command)
    generateJSONfromCurrentConfig();
}



void blinkConnectionLED()
{
    // show connection by green LED beacon
    if (Spark.connected())
        RGB.color(0,255,0);
    else
        RGB.color(255,0,0);
    delay(50);
    handleActivation();
}


void blinkBlueLED()
{
    // blink it blue 4 times when accepting new configuration from client
    for (int j=0; j<4; j++)
    {
        RGB.color(0,0,255);
        delay(100);
        RGB.color(0,0,0);
        delay(100);
    }
    handleActivation();
}


void handleActivation()
{
 	if (active == 1) 
	{
	    Serial.println("active: ON");
        digitalWrite(relayPin, HIGH);
        RGB.color(255, 0, 20); 
	}
    else
    {
	    Serial.println("active: OFF");
        digitalWrite(relayPin, LOW);
        RGB.color(0, 0, 0); 

    }   
}



void buttonPressCheck()
{
  int reading = digitalRead(buttonPin);

  // check to see if you just pressed the button  (i.e. the input went from LOW to HIGH),  
  // and you've waited  long enough since the last press to ignore any noise.
  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  } 
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so take it as the actual current state:
    if (reading != buttonState) {
      buttonState = reading;

      // only toggle activation if the new button state is LOW
      if (buttonState == LOW) 
      {
        // if button is debounced + depressed for more than 5 seconds - cause reset (emergency)
        long longPressTime = millis();
        while (digitalRead(buttonPin) == LOW);
        if ((millis() - longPressTime) > 5000) // long press for more than 5 secs
        {
//          Serial.println(F("* Long press detected, reseting..."));
          blinkBlueLED();
          Spark.sleep(SLEEP_MODE_DEEP,1); // will make the spark reset after 1 second
        }
        else // it's been shorter than 5 seconds - so just toggle activation
        {
          if (active) active = 0; 
          else active = 1;
          handleActivation();
        }
      }
    }
  }
  
  // save the reading.  Next time through the loop,
  // it'll be the lastButtonState:
  lastButtonState = reading;
}



void loop() 
{
    buttonPressCheck();

    if (millis() - lastSync > ONE_DAY_MILLIS) 
    {
      // Request time synchronization from the Spark Cloud
      Spark.syncTime();
      lastSync = millis();
    }
    
    // sample interval and blink LED
    if (millis() - lastBlink > 5000) 
    {
        lastBlink = millis();
        blinkConnectionLED();
        currentDayConfig = days[Time.weekday()-1];
        
        // sprintf(debugStr, "* Today is %s ", dayNames[Time.weekday()-1]);
        // Serial.println(debugStr);
        if (currentDayConfig.enabled)
        {
            // Serial.println("- enabled");

            if ((Time.hour() == currentDayConfig.onHour) && (Time.minute() == currentDayConfig.onMinute))
            {
                active = 1;
                handleActivation();
                generateJSONfromCurrentConfig();
            }
            
            if ((Time.hour() == currentDayConfig.offHour) && (Time.minute() == currentDayConfig.offMinute))
            {
                active = 0;
                handleActivation();
                generateJSONfromCurrentConfig();
            }

		  //  sprintf(debugStr,"On : %02d:%02d, Off : %02d:%02d",currentDayConfig.onHour, currentDayConfig.onMinute, currentDayConfig.offHour, currentDayConfig.offMinute);
    //         Serial.println(debugStr);
		  //  sprintf(debugStr,"Now : %02d:%02d",Time.hour(), Time.minute());
    //         Serial.println(debugStr);
        }
        
    }
}


void generateJSONfromCurrentConfig()
{
     // generate string from current config for exposing variable
    char dayStr[128];
    char boolStr[8];

    sprintf(configStr, "{\"active\":");
    if (active)
        sprintf(boolStr, "true");
    else
        sprintf(boolStr, "false");
    strcat(configStr,boolStr);
    sprintf(dayStr,",\"serverTime\":{\"hour\":%d,\"minute\":%d},",Time.hour(),Time.minute());
    strcat(configStr,dayStr);
        
    for (int i=0; i<DAYS_IN_WEEK; i++)
	{
        if (days[i].enabled)
            sprintf(boolStr, "true");
        else
            sprintf(boolStr, "false");
        
        sprintf(dayStr,"\"%s\":{\"enabled\":%s,\"onHour\":%d,\"onMinute\":%d,\"offHour\":%d,\"offMinute\":%d}",dayNames[i], boolStr, days[i].onHour, days[i].onMinute, days[i].offHour, days[i].offMinute);
        strcat(configStr, dayStr);
        if (i!=6)
            strcat(configStr,",");

	} 
	strcat(configStr, "}");
    
    // Serial.print("Generated JSON from configuration:");
    // Serial.println(configStr);  
}


int processTimezone(String command)
{
    // TBD
    return 0;
}


int processConfig(String command)
{
	JsonParser<16> parser;
	char* jsonStr = configStr;
	
	//jsonStr = (char *)malloc(command.length()+2);
	command.toCharArray(jsonStr,command.length()+1);
	
	Serial.print("Got command setConfig");
	Serial.println(jsonStr);
	
    JsonHashTable rootHashTable = parser.parseHashTable(jsonStr);

    if (!rootHashTable.success())
    {
		// error parsing json
		//free (jsonStr);
        return -1;
    }
	
	Serial.println("Parsed command");
	
	if (rootHashTable.containsKey("active"))
	{
 	    active = (rootHashTable.getBool("active") ? 1 : 0);
	}
 	

    JsonHashTable dayHashTable;
	
	for (int i=0; i<DAYS_IN_WEEK; i++)
	{
	    if (rootHashTable.containsKey(dayNames[i]))
	    {
    		dayHashTable = rootHashTable.getHashTable(dayNames[i]);
    		if (dayHashTable.success())
    		{
    		    if (dayHashTable.containsKey("enabled"))
    			    days[i].enabled = dayHashTable.getBool("enabled");

                if (dayHashTable.containsKey("onHour"))
    			    days[i].onHour = (dayHashTable.getLong("onHour") < 24) ? dayHashTable.getLong("onHour") : days[i].onHour;

                if (dayHashTable.containsKey("offHour"))
    			    days[i].offHour = (dayHashTable.getLong("offHour") < 24) ? dayHashTable.getLong("offHour") : days[i].offHour;
        			
                if (dayHashTable.containsKey("onMinute"))
        			days[i].onMinute = (dayHashTable.getLong("onMinute") < 60) ? dayHashTable.getLong("onMinute") : days[i].onMinute;

                if (dayHashTable.containsKey("offMinute"))
        			days[i].offMinute = (dayHashTable.getLong("offMinute") < 60) ? dayHashTable.getLong("offMinute") : days[i].offMinute;
        			
		        sprintf(debugStr,"Parsed: %s is %s, On : %02d:%02d, Off : %02d:%02d",dayNames[i],(days[i].enabled ? "enabled" : "not enabled"), days[i].onHour, days[i].onMinute, days[i].offHour, days[i].offMinute);
		        Serial.println(debugStr);
		        

    		}
	    }
		
	}
	
	//free(jsonStr);
	
	generateJSONfromCurrentConfig();

	blinkBlueLED();
	handleActivation();

    // add read()/write() calls for storing config in EEPROM

    return 0;
   
}