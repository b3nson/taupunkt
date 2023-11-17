#include "DHT.h"

#define DHTPIN_0 2
#define DHTPIN_I 4
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321

DHT dht_O(DHTPIN_0, DHTTYPE);
DHT dht_I(DHTPIN_I, DHTTYPE);

const int RELAISPIN   =  7;
const int MOSFETPIN_O =  9;
const int MOSFETPIN_I = 10;

const int analogInPin = 0; // TMP
int light = 0;             // TMP

String FANMODE;
const float fan_speedMin = 0.03;
const float fan_speedMax = 1.00;
float fan_speedNow   = 0.0; // REMEMBER TO CHECK
float fan_O_speedNow = 0.0; // REMEMBER TO CHECK
float fan_I_speedNow = 0.0; // REMEMBER TO CHECK

float loop_Interval  =   4; // Messinterval in Sekunden

float taup_DIF       =   5.0; // minimaler Taupunktunterschied, bei dem das Relais schaltet
float HYSTERESE      =   2.0; // Abstand von Ein- und Ausschaltpunkt

//float humi_MAX       =  62.0; // max. Luftfeuchte
float temp_I_MIN     =  10.0; // min. Temperatur Innen
float temp_O_MIN     = -10.0; // min. Temperatur Außen

float temp_O_CORRECTION =  -3.0;
float temp_I_CORRECTION =   0.0;

bool RUN;
int RUNMODE = 0; 
//-1=SILENT / MANUALOFF
// 0=IDLE / WAIT
// 1=ENTFEUCHTUNG
// 2=LUEFTUNG

int mode2RunInterval = 5;     //in Seconds
int mode2WaitInterval = 15;   //in Seconds

int mode2CountWaitInterval = 0;
int mode2CountRunInterval = 0;
int mode2Status = 0; //0=passive. 1=waiting. 2=running

float humi_I;
float humi_O;
float temp_I;
float temp_O;

void setup() {

//TCCR1B = TCCR1B & 0b11111000 | 0x01;

  dht_O.begin();
  dht_I.begin();

  pinMode(RELAISPIN, OUTPUT);
  pinMode(MOSFETPIN_O,OUTPUT);
  pinMode(MOSFETPIN_I,OUTPUT);

  Serial.begin(9600);

}

void loop() {

// --- checkSensors -------------------------------------------

  if(RUNMODE != -1) {
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float humi_O = dht_O.readHumidity();
    float humi_I = dht_I.readHumidity();
    // Read temperature as Celsius (the default)
    float temp_O = dht_O.readTemperature() + temp_O_CORRECTION;
    float temp_I = dht_I.readTemperature() + temp_I_CORRECTION;

    float taup_O = taupunkt(temp_O,humi_O);
    float taup_I = taupunkt(temp_I,humi_I);
    float taup_delta = taup_I - taup_O;
    

  // --- checkConditions -------------------------------------------

    
    if ( (temp_I < temp_I_MIN ) || (temp_O < temp_O_MIN ) ) {  // zu kalt
      RUNMODE = 0;                                                            //IDLE/WAIT
    } else {                                                // nicht zu kalt
      if (taup_delta >= taup_DIF)                            { RUNMODE = 1; } //ENTFEUCHTUNG
      else if ( (taup_delta < taup_DIF) && 
                (taup_delta > (taup_DIF - HYSTERESE)) )      { RUNMODE = 1; } //ENTFEUCHTUNG  Keep running
      else                                                   { RUNMODE = 2; } //INTERVALLLUEFTUNG
    }

    //if (humi_O > humi_MAX+10 )                 RUN = false;
    //light = analogRead(analogInPin);  // TMP

  // ----------------------------------------------------------

    if ( RUNMODE == 0 ) {         // Switch or stay IDLE
      fan(MOSFETPIN_I, 0.0);
      fan(MOSFETPIN_O, 0.0); 
      mode2Status = 0;
    } 
    else if ( RUNMODE == 1) {      // Switch or stay ENTFEUCHTUNG
      fan(MOSFETPIN_I, 1.0);
      fan(MOSFETPIN_O, 1.0);
      mode2Status = 0;
    } 
    else if (RUNMODE == 2) {
      if(mode2Status == 0) {          //passive
        mode2Status = 1;              
        mode2CountWaitInterval = 0;
        mode2CountRunInterval = 0;
        fan(MOSFETPIN_I, 0.0);
        fan(MOSFETPIN_O, 0.0); 
      } 
      else if (mode2Status == 1) {    //waiting
        mode2CountWaitInterval++;
        if( (loop_Interval*mode2CountWaitInterval) >= mode2WaitInterval ) {
          mode2Status = 2;
          mode2CountWaitInterval = 0;
        }
        fan(MOSFETPIN_I, 0.0);
        fan(MOSFETPIN_O, 0.0); 
      } 
      else if (mode2Status == 2) {    //runnning
        mode2CountRunInterval++;
        if( (loop_Interval*mode2CountRunInterval) >= mode2RunInterval ) {
          mode2Status = 1;
          mode2CountRunInterval = 0;
        }
        fan(MOSFETPIN_I, 0.0);
        fan(MOSFETPIN_O, 1.0); 
      }
//      mode2Status = 0; //0=passive. 1=waiting. 2=running


    }


    // else {
    //     fan(MOSFETPIN_I, 0.0);
    //     if ( humi_I < humi_MAX-5 ) {
    //         fan(MOSFETPIN_O, 0.0);
    //     } else if ( temp_I > temp_I_MIN &&
    //                 humi_I < humi_MAX ) {
    //         fan(MOSFETPIN_O, 1.0);
    //     } else if ( humi_I > humi_MAX+2 ) {
    //         fan(MOSFETPIN_O, 1.0);
    //     } 
    // }

    //if ( temp_I < temp_I_MIN-4) { // EMERGENCY HALT
    //     fan(MOSFETPIN_O, 0.0);
    //     fan(MOSFETPIN_I, 0.0);
    //}
    Serial.print("RUNMODE:");
    Serial.print(RUNMODE);
    Serial.print("|");
    Serial.print("humi_O:");
    Serial.print(humi_O);
    Serial.print("|");
    Serial.print("temp_O:");
    Serial.print(temp_O);
    Serial.print("|");
    Serial.print("taup_O:");
    Serial.print(taup_O);
    Serial.print("|");
    Serial.print("humi_I:");
    Serial.print(humi_I);
    Serial.print("|");
    Serial.print("temp_I:");
    Serial.print(temp_I);
    Serial.print("|");
    Serial.print("taup_I:");
    Serial.print(taup_I);
    Serial.print("|");
    Serial.print("RUN:");
    Serial.print(RUN);
    Serial.print("|");
    Serial.print("LIGHT:");
    Serial.print(light); // TMP
    Serial.println();
  } else {
    mode2Status = 0;
    Serial.print("SILENT");
  }
  // Wait a few seconds between measurements.
  //delay(300000); // 5 Minuten
  delay(loop_Interval*1000);
} // END loop


