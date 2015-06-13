// Update Every 15 mins
#define UpdateTime 15

// Setup the thermometer
#include <OneWire.h>
#include <DallasTemperature.h>
OneWire oneWire(7);
DallasTemperature sensors(&oneWire);
DeviceAddress thermometer;
int16_t tempC = 0;

// GSM Serial connection
#define HWSERIAL Serial1
char incomingByte;

// Sleep
#include <Snooze.h>
SnoozeBlock config;

// Timing
elapsedMillis sinceWake;
elapsedMillis waitTime;

void setup() {
    HWSERIAL.begin(9600);
    //Serial.begin(9600);
    delay(1000);
    //Serial.println("Starting Up");

    // Locate the termometer
    sensors.begin();
    sensors.getAddress(thermometer, 0);
    sensors.setResolution(thermometer, 12);
}

void loop() {
    sinceWake = 0;
    //Serial.println("Awake");

    // Give the Sim800 time to boot
    gsmOn();
    
    // Make sure it works right
    HWSERIAL.println("AT");
    if (waitForOK()) {
        //Serial.println("Sim800l Responding");
        
        // Send the current temp
        setupAndSend();
    }
    
    // Shutdown Sim800l
    gsmOff();
    
    // Sleep until next reading
    uint8_t sleepTime = 60 - sinceWake/1000;
    //Serial.print("Going to sleep for ");
    //Serial.print(UpdateTime - 1);
    //Serial.print(" minutes, ");
    //Serial.print(sleepTime);
    //Serial.println("seconds");
    delay(100);
    config.setAlarm(0, UpdateTime - 1, sleepTime);
    Snooze.deepSleep(config);
}

/**
 * Press the power key on the GSM module
 */
void gsmOn() {
    HWSERIAL.println("AT");
    if (waitForOK()) {
        // Already on
        return;
    }

    // Pull PWRKEY low
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);
    
    // For at least one second
    delay(1100);
    
    // Then let it pull high again
    pinMode(2, INPUT);

    // Send data to the the Sim800 so it auto bauds
    delay(4000);
    HWSERIAL.print("\r\n\r\n\r\n");

    // Wait for the Sim800l to become ready
    waitFor("SMS Ready", 15000);
    getChars();
}

/**
 * Shut down the GSM module
 */
void gsmOff() {
    //Serial.println("Shutdown Sim800l");  
    HWSERIAL.println("AT+CPOWD=1");
    waitFor("NORMAL POWER DOWN", 5000);
}


void setupAndSend() {
    // Get the temp
    //Serial.println("Get the temp");
    sensors.requestTemperatures();
    float tempCf = sensors.getTempC(thermometer);      
    
    // Get the battery voltage
    getChars();
    //Serial.println("Get the battery voltage");
    HWSERIAL.println("AT+CBC");

    String batt;
    waitTime = 0;
    while (waitTime < 4000) {
        if (!HWSERIAL.available()) {
            delay(5);
            continue;
        }
        incomingByte = HWSERIAL.read();
        //Serial.print(incomingByte);
        batt += incomingByte;
        
        bool found = batt.lastIndexOf("CBC:") != -1;
        if (incomingByte == '\n' && !found) {
            // Not the line we're looking for
            batt = "";
        }
        if (incomingByte == '\n' && found) {
            // Found the response
            batt = batt.substring(batt.lastIndexOf(",") + 1, batt.lastIndexOf(",") + 5);
            break;
        }
    }
    //Serial.println(batt);
    
    // Configure the data settings
    //Serial.println("Configure the data settings");
    HWSERIAL.println("AT+CGATT=1");
    waitForOK();
    HWSERIAL.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
    waitForOK();
    HWSERIAL.println("AT+SAPBR=3,1,\"APN\",\"internet\"");
    waitForOK();
    HWSERIAL.println("AT+SAPBR=1,1");
    waitForOK();
    
    // Make the HTTP request
    //Serial.println("Make the HTTP request");
    HWSERIAL.println("AT+HTTPINIT");
    waitForOK();

    // Set the context ID
    HWSERIAL.println("AT+HTTPPARA=\"CID\",1");
    waitForOK();

    // Set the User agent
    HWSERIAL.println("AT+HTTPPARA=\"UA\",\"Sensor 002\"");
    waitForOK();

    // Set the URL
    HWSERIAL.print("AT+HTTPPARA=\"URL\",\"http://oceanswims.nz/temp/?temp=");
    HWSERIAL.print(tempCf);
    HWSERIAL.print("&battery=");
    HWSERIAL.print(batt);
    HWSERIAL.println("\"");
    waitForOK();    

    // We want a GET request
    HWSERIAL.println("AT+HTTPACTION=0");
    waitForOK();
    
    // Wait for the response
    HWSERIAL.println("AT+HTTPREAD");
    waitForOK();
    waitFor("\r\n+HTTPACTION:", 15000);
    getChars(); 

    // All done
    HWSERIAL.println("AT+HTTPTERM");
    waitForOK();
}	

/**
 * Wait for a define length of time for a particular response from the Sim800l
 */
bool waitFor(String searchString, int waitTimeMS) {
    //Serial.println("Waiting for string");
    waitTime = 0;
    String foundText;
    while (waitTime < waitTimeMS) {
        if (!HWSERIAL.available()) {
            delay(5);
            continue;
        }
        incomingByte = HWSERIAL.read();
        //Serial.print(incomingByte);
        foundText += incomingByte;
        
        if (foundText.lastIndexOf(searchString) != -1) {
            //Serial.print("Found string after ");
            //Serial.print(waitTime);
            //Serial.println("ms.");
            return true;
        }
    }
    
    // Timed out before finding it
    //Serial.println("'OK' not found, timed out");
    return false;
}

/**
 * Wait for "OK" from the Sim800l
 */
bool waitForOK() {
    return waitFor("\nOK\r\n", 4000);
}

/**
 * Get any characters in the serial buffer
 */
void getChars() {
    while (HWSERIAL.available() > 0) {
        incomingByte = HWSERIAL.read();
        //Serial.print(incomingByte);
        delay(5);
    }  
}