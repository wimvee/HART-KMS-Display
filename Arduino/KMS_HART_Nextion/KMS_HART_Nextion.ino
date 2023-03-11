/*
   KMS HART Nextion Display
   Made by: Wim Veenstra
   Version: V1.3
   Date: 01-08-2022

   To do:
   - Gear position test, prob won't work (accurasy / slip on wheels)
      > hall switches would be better




*/


#include <SPI.h>                   //Library for using SPI Communication
#include <mcp2515.h>               //Library for using CAN Communication
#include <EasyNextionLibrary.h>    //Library for the Nextion_display
#include <trigger.h>
#include <EEPROM.h>


MCP2515 mcp2515(10);      //SPI CS Pin 10
struct can_frame canMsg;
float speed;
float rpm;

EasyNex myNex(Serial); // Create an object of EasyNex class with the name < myNex > 
// Set as parameter the Hardware Serial you are going to use

/*interval Every x ms the battery voltage will be updated.*/
const unsigned long FUELPRESSURE_eventInterval = 1000; 
const unsigned long FUELLEVEL_eventInterval = 5000; 
const unsigned long GearPositionCalculated_eventInterval = 250;
const unsigned long processbar_eventInterval = 250;
const unsigned long voltage_eventInterval = 1000;

/* Start value. -Don't change- */
unsigned long FUELPRESSURE_previousTime = 0;
unsigned long FUELLEVEL_previousTime = 50;
unsigned long GearPositionCalculated_previousTime = 100;
unsigned long processbar_previousTime = 150;
unsigned long voltage_previousTime = 200;

/* Fuel */
int Fuel_potPin = 6;      // select the input pin for the Fuel level sensor (potentiometer).
float Fuel_LRead = 0;      // variable to store the value coming from the sensor. -Don't change-
float Fuel_LVoltage ;
float Fuel_L;

int Fuel_PressurePin = 5;      // select the input pin for the Fuel prussure sensor.
float Fuel_PRead = 0;      // variable to store the value coming from the sensor. -Don't change-
float Fuel_PVoltage ;
float Fuel_P;

/* Progress bars  */
int rpm_pb;
int rpm_pb_30;   // transition green to yellow (max Torque)
int rpm_pb_69;   // transition yellow to red (max Power)
int rpm_pb_100;  // maximum rpm

int water_pb;
int water_pb_25;   // water temp from cold to acceptable operating temp
int water_pb_75;  // water temp is getting to hot

int voltage_pb;
int voltage_pb_25;  // battery voltage low
int voltage_pb_75;  // battery voltage high

bool functions_ON_OFF = true;

void setup()
{
  SPI.begin();                                //Begins SPI Communication
  myNex.begin(115200);                        // Begin the object with a baud rate of 115200
  mcp2515.reset();
  mcp2515.setBitrate(CAN_1000KBPS, MCP_8MHZ); //Sets CAN at speed 1 Mbps and Clock 8MHz
  mcp2515.setNormalMode();                    //Sets CAN at normal mode


/* Get progress bars values from EEPROM */
  EEPROM.get(0, rpm_pb_30);
  EEPROM.get(5, rpm_pb_69);
  EEPROM.get(10, rpm_pb_100);
  EEPROM.get(15, water_pb_25);
  EEPROM.get(20, water_pb_75);
  EEPROM.get(25, voltage_pb_25);
  EEPROM.get(30, voltage_pb_75);
}


void loop()
{
  myNex.NextionListen(); // WARNING: This function must be called repeatedly to response touch events.

  CANKMS();

  // make less inportant
  if ( millis() - FUELLEVEL_previousTime >= FUELLEVEL_eventInterval)
  {
    FUELLEVEL();
    FUELLEVEL_previousTime = millis();
  }
  if ( millis() - FUELPRESSURE_previousTime >= FUELPRESSURE_eventInterval)
  {
    FUELPRESSURE();
    FUELPRESSURE_previousTime = millis();
  }

/* function in test fase, comment out to turn it off.*/
//  if ( millis() - GearPositionCalculated_previousTime >= GearPositionCalculated_eventInterval)
//  {
//    GearPositionCalculated(); // function in test fase, comment out to turn it off.
//    GearPositionCalculated_previousTime = millis();
//  }

}


/*-----------------------------------------------------------------------------------------------------------------------READ KMS CAN----------------------------------------------------------------------------------------------------------------------------*/
void CANKMS()
{


  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK)
  {
    /*-----------------------------------------------------------------------------------------------------------------------Read can ID 2D---------------------------------------------------------------------------------------------------------------------------*/
    if (canMsg.can_id == 0x02D) //MD35_6
    {
      int MD35_6_0 = canMsg.data[0]; //Speed Factor 0.01    Offset    Min    Max     Unit kmh
      int MD35_6_1 = canMsg.data[1]; //Speed Factor 0.01    Offset    Min    Max     Unit kmh
      /*
          Vehicle speed
      */
      speed = (MD35_6_0 * 256 + MD35_6_1) * 0.01;
      myNex.writeNum("speed.val", speed);
    }
    /*-----------------------------------------------------------------------------------------------------------------------Read can ID 28---------------------------------------------------------------------------------------------------------------------------*/

    else if (canMsg.can_id == 0x028) //MD35_1
    {
      int MD35_1_0 = canMsg.data[0]; //rpm
      int MD35_1_1 = canMsg.data[1]; //rpm
      int MD35_1_2 = canMsg.data[2]; //water_t Offset -20 Min -20 Max 135 Unit Deg C
      int MD35_1_3 = canMsg.data[3]; //Air_t   Offset -20 Min -20 Max 135 Unit Deg C
      int MD35_1_6 = canMsg.data[6]; //Oil_P   Offset 1   Min 0   Max 0   Unit Kpa
      int MD35_1_7 = canMsg.data[7]; //Oil_P

      /*
          rpm
                Sending the variable to the nextion
      */


      rpm = (MD35_1_0 * 256 + MD35_1_1) * 10;
      if (rpm < rpm_pb_30) {
        rpm_pb = map(rpm, 0, rpm_pb_30, 0, 30);
      }
      else if (rpm >= rpm_pb_30 & rpm < rpm_pb_69) {
        rpm_pb = map(rpm, rpm_pb_30, rpm_pb_69, 30, 69);
      }
      else if (rpm >= rpm_pb_69 & rpm < rpm_pb_100) {
        rpm_pb = map(rpm, rpm_pb_69, rpm_pb_100, 69, 100);
      }
      myNex.writeNum("rpm_pb.val", rpm_pb);
      myNex.writeNum("rpm.val", rpm);

      if ( millis() - processbar_previousTime >= processbar_eventInterval)
      {
        /*
            Water temperature

        */

        int water_t = (MD35_1_2) - 20;
        // sending to progresbar
        if (water_t < water_pb_25) {
          water_pb = map(water_t, 0, water_pb_25, 0, 25);
        }
        else if (water_t >= water_pb_25 & water_t < water_pb_75) {
          water_pb = map(water_t, water_pb_25, water_pb_75, 0, 75);
        }
        else if (water_t >= water_pb_75) {
          water_pb = map(water_t, water_pb_75, (water_pb_75 + 50), 0, 100);
        }

        myNex.writeNum("water_pb.val", water_pb);
        myNex.writeNum("water_t.val", water_t);

        if (functions_ON_OFF)
        {
        /*
            Air temperature
        */
        float air_t = (MD35_1_3) - 20;
        myNex.writeNum("air_t.val", air_t);
        }
        /*
           Oil Pressure
        */
        float Oil_P = (MD35_1_6 * 256 + MD35_1_7) * 0.01; // Unit Bar
        myNex.writeNum("oil_pb.val", Oil_P * 10);
        String oilString = String(Oil_P, 1); // Convert the float value to String, with 1 decimal place
        myNex.writeStr("oil.txt", oilString);


        processbar_previousTime = millis();
      }
    }
    /*-----------------------------------------------------------------------------------------------------------------------Read can ID 29---------------------------------------------------------------------------------------------------------------------------*/
    else if (canMsg.can_id == 0x029) //MD35_2
    {
      int MD35_2_0 = canMsg.data[0]; //TPS LOAD
      int MD35_2_1 = canMsg.data[1]; //MAP Load
      int MD35_2_2 = canMsg.data[2]; //voltage Factor 0.25 Offset 8   Min 8   Max 22   Unit Volt
      int MD35_2_3 = canMsg.data[3]; //EGT_1 Factor 5    Offset 0   Min 0   Max 1250    Unit Deg_C
      int MD35_2_4 = canMsg.data[4]; //EGT_2 Factor 5    Offset 0   Min 0   Max 1250    Unit Deg_C
      int MD35_2_6 = canMsg.data[6]; //gear Factor 1    Offset 0   Min 0   Max 13    Unit Nr

      if (functions_ON_OFF)
      {
        /*
            Throttle position sensor
        */
        float TPS = MD35_2_0 * 100 * (1 / 15.9) * 0.1; //TPS in %
        String throttleString = String(TPS, 0); // Convert the float value to String, with 0 decimal place
        myNex.writeStr("throttle.txt", throttleString);
  
        /*
            MAP Sensor
        */
        float MAP_LOAD = MD35_2_1 * 100 * 0.1; //MAP in unknown unit
        String map_loadString = String(MAP_LOAD, 0); // Convert the float value to String, with 0 decimal place
        myNex.writeStr("map_load.txt", map_loadString);
  
        /*
            Battery voltage
        */
  
        float voltage = (MD35_2_2 * 0.25) + 8;
        if ( millis() - voltage_previousTime >= voltage_eventInterval)
        {
          if (voltage < voltage_pb_25) {
            voltage_pb = map(voltage, 0, voltage_pb_25, 0, 25);
          }
          else if (voltage >= voltage_pb_25 & voltage < voltage_pb_75) {
            voltage_pb = map(voltage, voltage_pb_25, voltage_pb_75, 0, 75);
          }
          else if (voltage >= voltage_pb_75) {
            voltage_pb = map(voltage, voltage_pb_75, (voltage_pb_75 + 5), 0, 100);
          }
          myNex.writeNum("voltage_pb.val", voltage_pb);
  
          /* Update the timing for the next time around */
          voltage_previousTime = millis();
         }

        String voltageString = String(voltage, 1); // Convert the float value to String, with 1 decimal place
        myNex.writeStr("voltage.txt", voltageString);
       }

      ///*
      // *  Oil temperature
      // */
              float EGT_1 = MD35_2_3*5; //MAP in unknown unit
              String EGT_1String = String(EGT_1, 0); // Convert the float value to String, with 0 decimal place
              myNex.writeStr("oil_t.txt",EGT_1String);


      /*
          Gear position CAN
      */

      //        int gearReading = MD35_2_6;
      //        char gear[gearReading]={'X', 'R', 'N', '1', '2', '3','4','5','6','7','8','X','X','X'};
      //        String gearString = String(gear[gearReading],0); // Convert to String, with 0 decimal place
      //        myNex.writeStr("gear.txt",gearString);

    }

    /*-----------------------------------------------------------------------------------------------------------------------Read can ID 2A---------------------------------------------------------------------------------------------------------------------------*/
    else if (canMsg.can_id == 0x02A) //MD35_3
    {
      int MD35_3_4 = canMsg.data[4]; //Lambda Factor 0.005    Offset 0.65   Min 0.65   Max 1.455    Unit Lambda

      /*
          Lamda
      */
      float Lambda = (MD35_3_4 * 0.005) + 0.65;
      String lambdaString = String(Lambda, 2); // Convert the float value to String, with 2 decimal place
      myNex.writeStr("lambda.txt", lambdaString);

    }

    
  }



}

/*-----------------------------------------------------------------------------------------------------------------------Fuel Level------------------------------------------------------------------------------------------------------------------------------------------*/
void FUELLEVEL()
{
   if (functions_ON_OFF)
   {
    /*
       Fuel Level
  
    */
    Fuel_LRead = analogRead(Fuel_potPin);    // read the value from the sensor
    Fuel_LVoltage = ((Fuel_LRead / 1024) * 5); // Converting binary to decimal for a 5 volt system.
  
    String Fuel_LVoltageString = String(Fuel_LVoltage, 3); // Convert the float value to String, with 3 decimal place
    myNex.writeStr("fuel_lvoltage.txt", Fuel_LVoltageString); // Shows the measured voltage of the fuel level pot
  
    Fuel_L = (float)Fuel_LVoltage * -20.075 + 49.807; // y = -20.075x+49.807 // formule from excel measering voltage @ every 1L added to fueltank.
  
    String Fuel_LString = String(Fuel_L, 0); // Convert the float value to String, with 1 decimal place
    myNex.writeStr("fuel_l.txt", Fuel_LString); // Shows the fuel level in Liters
   }
}

/*-----------------------------------------------------------------------------------------------------------------------Gear Position calculated----------------------------------------------------------------------------------------------------------------------------*/
void GearPositionCalculated()
{


  //
  //if (!Gear_ON_OFF) {
  //    return;
  //  }
  //else { }
  //int iR=-2.09;         // reverse 11/23
  int i1 = 2.9;           // 1st gear 2.9
  int i2 = 2;             // 2nd gear 2.0
  int i3 = 1.66;          // 3rd gear 1.66
  int i4 = 1.41;          // 4th gear 1.41
  int i5 = 1.2;           // 5th gear 1.2
  int i6 = 1;             // 6th gear 1.0
  int iD = 5;             // differantial 5.0
  int rd = 1.9644;        // wheel circumference 196.44cm (215/45/R17)
  int corr = 0.1;         // Correction
  int gear_i = (rd * rpm) / (speed / 3.6 * iD); //    gear.ratio = (wheel circumference * rpm)/(speed * diff.ratio)

  if (gear_i >= (i1 - corr)&gear_i <= (i1 + corr)) {
    myNex.writeStr("gear.txt", "1");
  }
  else if (gear_i >= (i2 - corr)&gear_i <= (i2 + corr)) {
    myNex.writeStr("gear.txt", "2");
  }
  else if (gear_i >= (i3 - corr)&gear_i <= (i3 + corr)) {
    myNex.writeStr("gear.txt", "3");
  }
  else if (gear_i >= (i4 - corr)&gear_i <= (i4 + corr)) {
    myNex.writeStr("gear.txt", "4");
  }
  else if (gear_i >= (i5 - corr)&gear_i <= (i5 + corr)) {
    myNex.writeStr("gear.txt", "5");
  }
  else if (gear_i >= (i6 - corr)&gear_i <= (i6 + corr)) {
    myNex.writeStr("gear.txt", "6");
  }
  else {
    myNex.writeStr("gear.txt", "N");
  }

}

/*-----------------------------------------------------------------------------------------------------------------------Fuel Pressure----------------------------------------------------------------------------------------------------------------------------*/
void FUELPRESSURE()
{
  /*
     Fuel Pressure

  */
  Fuel_PRead = analogRead(Fuel_PressurePin);    // read the value from the sensor
  Fuel_PVoltage = ((Fuel_PRead / 1024) * 5);
  Fuel_P = (float)(Fuel_PVoltage / (5 * 0.0016)) - (0.1 / 0.0016); // Ua = (C1 * Prel + C0) * Us    C0 = 0.1, C1 = 0.0016 /bar, Ua = SIGNAL OUTPUT VOLTAGE IN V, Us = SUPPLY VOLTAGE IN V, Prel = PRESSURE (RELATIVE; bar)
  String Fuel_PVoltageString = String(Fuel_PVoltage, 3); // Convert the float value to String, with 3 decimal place <<<
  myNex.writeStr("fuel_pvoltage.txt", Fuel_PVoltageString);
  String Fuel_PString = String(Fuel_P, 0); // Convert the float value to String, with 0 decimal place <<<
  myNex.writeStr("fuel_p.txt", Fuel_PString);
}


/*
    eeprom for progressbars
    trigger0 will run every time the following sequence of bytes (in HEX format) 23 02 54 00 comes to Arduino's Serial.

*/
void trigger0() {

  rpm_pb_30 = myNex.readNumber("rpm_pb_30.val");
  rpm_pb_69 = myNex.readNumber("rpm_pb_69.val");
  rpm_pb_100 = myNex.readNumber("rpm_pb_100.val");
  water_pb_25 = myNex.readNumber("water_pb_25.val");
  water_pb_75 = myNex.readNumber("water_pb_75.val");
  voltage_pb_25 = myNex.readNumber("voltage_pb_25.val");
  voltage_pb_75 = myNex.readNumber("voltage_pb_75.val");

  EEPROM.put(0, rpm_pb_30);
  EEPROM.put(5, rpm_pb_69);
  EEPROM.put(10, rpm_pb_100);
  EEPROM.put(15, water_pb_25);
  EEPROM.put(20, water_pb_75);
  EEPROM.put(25, voltage_pb_25);
  EEPROM.put(30, voltage_pb_75);

}

void trigger1() {

  myNex.writeNum("options.rpm_pb_30.val", rpm_pb_30);
  myNex.writeNum("options.rpm_pb_69.val", rpm_pb_69);
  myNex.writeNum("options.rpm_pb_100.val", rpm_pb_100);
  myNex.writeNum("options.water_pb_25.val", water_pb_25);
  myNex.writeNum("options.water_pb_75.val", water_pb_75);
  myNex.writeNum("options.voltage_pb_25.val", voltage_pb_25);
  myNex.writeNum("options.voltage_pb_75.val", voltage_pb_75);

}

void trigger2()
{
    functions_ON_OFF = false;
}
void trigger3()
{
    functions_ON_OFF = true;
}
