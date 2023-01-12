#include <Adafruit_SGP30.h>
#include <LiquidCrystal_PCF8574.h>
#include <Adafruit_BME280.h>
#include <Wire.h>

LiquidCrystal_PCF8574 lcd(0x27);  // set the LCD address to 0x27 
char lcdDisplay[4][20];           // 4 lines of 20 character buffer

/*
 * TO DO:
 * 
 * 
 */

/************************** Configuration ***********************************/

// edit the config.h tab and enter your Adafruit IO credentials
// and any additional configuration needed for WiFi, cellular,
// or ethernet clients.
#include "config.h"

/************************ Example Starts Here *******************************/


// Instanciate the sensors
Adafruit_BME280 bme; // I2C
Adafruit_SGP30 sgp;

// this int will hold the current count for our sketch
int counter = 0;
// These floats will hold the data from the sensors
float Temp = 0;
float Humidity = 0;


/* return absolute humidity [mg/m^3] with approximation formula
* @param temperature [°C]
* @param humidity [%RH]
*/
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
}


// set up IO feeds
AdafruitIO_Feed *vocTemp = io.feed("vocTemp");
AdafruitIO_Feed *vocHumidity = io.feed("vocHumidity");
AdafruitIO_Feed *TVOC = io.feed("TVOC");
AdafruitIO_Feed *eCO2 = io.feed("eCO2");


void setup() {

  // start the serial connection
  Serial.begin(9600);

  // wait for serial monitor to open
  while(! Serial);

  Serial.print("Connecting to Adafruit IO");


  unsigned status;
    
    // default settings
    status = bme.begin(0x76);  
    // You can also pass in a Wire library object like &Wire2
    // status = bme.begin(0x76, &Wire2)
    if (!status) {
        Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
        Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
        Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
        Serial.print("        ID of 0x60 represents a BME 280.\n");
        Serial.print("        ID of 0x61 represents a BME 680.\n");
        while (1) delay(10);
    }

  // If you have a baseline measurement from before you can assign it to start, to 'self-calibrate'
  //sgp.setIAQBaseline(0x8E68, 0x8F41);  // Will vary for each sensor!


    if (! sgp.begin()){
    Serial.println("Sensor not found :(");
    while (1);
  }
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);

  // connect to io.adafruit.com
  io.connect();

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());

  // initialize LCD
  lcd.begin(16, 4);

  // initialize the sgp30
  sgp.begin();

}

void loop() {

  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

  // save count to the 'counter' feed on Adafruit IO
//  Serial.print("sending -> ");
//  Serial.println(count);
//  counter->save(count);


  // If you have a temperature / humidity sensor, you can set the absolute humidity to enable the humditiy compensation for the air quality signals
  //float temperature = 22.1; // [°C]
  //float humidity = 45.2; // [%RH]
  //sgp.setHumidity(getAbsoluteHumidity(temperature, humidity));

  if (! sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    return;
  }
  Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print(" ppb\t");
  Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.println(" ppm");

  if (! sgp.IAQmeasureRaw()) {
    Serial.println("Raw Measurement failed");
    return;
  }
  Serial.print("Raw H2 "); Serial.print(sgp.rawH2); Serial.print(" \t");
  Serial.print("Raw Ethanol "); Serial.print(sgp.rawEthanol); Serial.println("");

  // Read the temperature from the bme280
  Temp = bme.readTemperature();
  Humidity = bme.readHumidity();


  Serial.print("bmeTemp: ");
  Serial.print(bme.readTemperature());
  Serial.println("C");

  Serial.print("Humidity: ");
  Serial.print(bme.readHumidity());
  Serial.println(" %");

  Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print(" ppb\t");
  Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.println(" ppm"); Serial.println("");

  //Example for LCD
  // Write to LCD line 1
  lcd.setBacklight(225);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TVOC: ");
  lcd.print(sgp.TVOC);
  lcd.print(" ppb-t");
  lcd.setCursor(0, 1);
  lcd.print("eCO2: ");
  lcd.print(sgp.eCO2);
  lcd.print(" ppm");

  delay(1000);

  counter++;
  if (counter == 30) {
    counter = 0;

    
    // Send data to IO
    Serial.println("Sending Temperature to IO");
    vocTemp->save(Temp);
    Serial.println("Sending Humidity to IO");
    vocHumidity->save(Humidity);
    Serial.println("Sending TVOC to IO");
    TVOC->save(sgp.TVOC);
    Serial.println("Sending eCO2 to IO");
    eCO2->save(sgp.eCO2);

    uint16_t TVOC_base, eCO2_base;
    if (! sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
      Serial.println("Failed to get baseline readings");
      return;
    }
    Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX);
    Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);
  
  }

  // Adafruit IO is rate limited for publishing, so a delay is required in
  // between feed->save events. In this example, we will wait three seconds
  // (1000 milliseconds == 1 second) during each loop.
//  delay(1000);

}
