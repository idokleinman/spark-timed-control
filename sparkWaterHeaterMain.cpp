#include "JsonParser.h"
#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
#define DAYS_IN_WEEK 7
//char json[] = "{\"Name\":\"Blanchon\",\"Skills\":[\"C\",\"C++\",\"C#\"],\"Age\":32,\"Online\":true}";

typedef struct 
{
	bool enabled;
	int onHour, onMinute, offHour, offMinute;
} dayConfig;

// globals
int relayPin = D0;  

char *dayNames[DAYS_IN_WEEK]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
dayConfig days[DAYS_IN_WEEK];
int active = 0;
long lastSync, lastBlink;
int timezone = 3;
dayConfig currentDayConfig;

#define CONFIG_STR_MAX_SIZE 600
char configStr[CONFIG_STR_MAX_SIZE]; // mallocs?
char debugStr[60];
bool blinkLEDOnce = false;

void setup() {
    // take control of the LED
    RGB.control(true);
    Spark.variable("config", &configStr, STRING);
    Spark.function("config", processConfig);
    pinMode(relayPin, OUTPUT);
    // Spark.function("timezone",processTimezone);
    // Spark.variable("timezone",&timezone, INT);

    for (int i=0; i<DAYS_IN_WEEK; i++)
    {
        days[i].enabled = true;
        days[i].onHour = 0;
        days[i].offHour = 0;
        days[i].onMinute = 0;
        days[i].offMinute = 0;
    }
    
    RGB.brightness(255);
    active = 0;
    Serial.begin(9600);
    Serial.println("Remote timed control spark core unit ready.");
    
    Time.zone(3); // Israel time (should be a command)
    //configStr = (char *)malloc(CONFIG_STR_SIZE);
    generateJSONfromCurrentConfig();
    RGB.color(255, 255, 0); 
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



void loop() 
{

    if (millis() - lastSync > ONE_DAY_MILLIS) {
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