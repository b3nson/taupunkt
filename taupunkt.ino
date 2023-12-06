#include "DHT.h"
#include <RunningMedian.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoHttpClient.h>
#include "conf.h"

#define DHTPIN_0 2
#define DHTPIN_I 4
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321

DHT dht_O(DHTPIN_0, DHTTYPE);
DHT dht_I(DHTPIN_I, DHTTYPE);

const int SWITCHPIN   = 12;
const int MOSFETPIN_O =  9;
const int MOSFETPIN_I = 10;
const int analogInPin =  0; // TMP

// --- Internal ------------------------------------------------------
int RUNMODE = 0;  //-1=OFF | 0=IDLE | 1=ENTFEUCHTUNG | 2=LUEFTUNG
float humi_I;
float humi_O;
float temp_I;
float temp_O;
float taup_I;
float taup_O;
float taup_delta;
RunningMedian humi_I_MED = RunningMedian(19);
RunningMedian humi_O_MED = RunningMedian(19);
RunningMedian temp_I_MED = RunningMedian(19);
RunningMedian temp_O_MED = RunningMedian(19);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

WiFiSSLClient wifi;
int status = WL_IDLE_STATUS;
HttpClient client = HttpClient(wifi, server, port);
HttpClient wttr = HttpClient(wifi, server, port);

boolean NOW;
long lastTime;

// --- FanValues -----------------------------------------------------
String FANMODE;
const float fan_speedMin = 0.03;
const float fan_speedMax = 1.00;
float fan_speedNow   = 0.0; // REMEMBER TO CHECK
float fan_O_speedNow = 0.0; // REMEMBER TO CHECK
float fan_I_speedNow = 0.0; // REMEMBER TO CHECK

String FAN_I,FAN_O;

// --- WeatherValues -------------------------------------------------
float temp_O_CORRECTION =  -3.0;
float temp_I_CORRECTION =   0.0;

float temp_I_MIN     =  10.5; // min. Temperatur Innen
float temp_O_MIN     = -10.0; // min. Temperatur Außen
float humi_O_MAX     =  85.0; // max. Luftfeuchte Außen

float taup_DIF       =   6.0; // minimaler Taupunktunterschied, bei dem das Relais schaltet
float HYSTERESE      =   3.0; // Abstand von Ein- und Ausschaltpunkt

// --- Logging ------------------------------------------------------
bool p = true;                        // Do serial print


void setup() {

  //TCCR1B = TCCR1B & 0b11111000 | 0x01;
  dht_O.begin();
  dht_I.begin();

  pinMode(SWITCHPIN, INPUT_PULLUP);
  pinMode(MOSFETPIN_O,OUTPUT);
  pinMode(MOSFETPIN_I,OUTPUT);

  if(p) Serial.begin(9600);

  while (status != WL_CONNECTED) {
    if(p) Serial.print("Attempting to connect to Network named: ");
    if(p) Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }
  if(p) Serial.print("SSID: ");
  if(p) Serial.println(WiFi.SSID());
  IPAddress ip = WiFi.localIP();
  IPAddress gateway = WiFi.gatewayIP();
  if(p) Serial.print("IP Address: ");
  if(p) Serial.println(ip);

  timeClient.begin();
  lastTime = WiFi.getTime();

}

void(* resetFunc) (void) = 0; // declare reset function @ address 0

void loop() {

  // --- checkSwitch -------------------------------------------
  if ( digitalRead(SWITCHPIN) == HIGH ) {
    if(p && RUNMODE != -1) Serial.println("--- SILENT ON ------------------");
    RUNMODE = -1;
  } else {
    if(p && RUNMODE == -1) Serial.println("--- SILENT OFF -----------------");
    RUNMODE = 0;
  }

  if ( RUNMODE != -1 ) {
  
    // --- check lastTime ----------------------------------------
    if ( WiFi.getTime()-lastTime > 300 ) { // SET INTERVALL
      NOW=true; 
    } else { 
      NOW=false; 
    }
    // -----------------------------------------------------------

    // --- checkSensors -------------------------------------------    
    humi_I = dht_I.readHumidity();
    temp_I = dht_I.readTemperature() + temp_I_CORRECTION;

    humi_I_MED.add(humi_I);
    temp_I_MED.add(temp_I);

    humi_I = humi_I_MED.getMedian();
    temp_I = temp_I_MED.getMedian();

   if ( NOW == true ) {

    int attempt = 0;
    String response = "";
    while ( response == "" && attempt < 10 ) {
      wttr.get(meteorequest);
      int statusCode = wttr.responseStatusCode();
      response = wttr.responseBody();
      //if(p) Serial.print("Response: ");Serial.println(response);
      //if(p) Serial.print("Status code: ");Serial.println(statusCode);
      int sep = response.indexOf(' ');
      humi_O = stringToFloat(response.substring(sep+1));
      temp_O = stringToFloat(response.substring(0, sep));
      delay(5000);
      wttr.stop();
      attempt++;
    }

    if  ( isnan(humi_O) || isnan(temp_O) ) {
        humi_O = dht_O.readHumidity();
        temp_O = dht_O.readTemperature() + temp_O_CORRECTION;
        humi_O_MED.add(humi_O);
        temp_O_MED.add(temp_O);
        humi_O = humi_O_MED.getMedian();
        temp_O = temp_O_MED.getMedian();
    }

    taup_O = taupunkt(temp_O,humi_O);
    taup_I = taupunkt(temp_I,humi_I);
    taup_delta = taup_I - taup_O;

    // --- collect postData ----------------------------------------------
    String postData = String(temp_O)
                    + ";"
                    + String(temp_I)
                    + ";"
                    + String(humi_O)
                    + ";"
                    + String(humi_I)
                    + ";"
                    + String(taup_O)
                    + ";"
                    + String(taup_I);
    // -------------------------------------------------------------------

    timeClient.update();
    //int H = timeClient.getHours();
    int M = timeClient.getMinutes();

    // --- checkModeConditions -------------------------------------------
    if ( (temp_I < temp_I_MIN) && (temp_O < temp_O_MIN) ) { // too cold
      RUNMODE = 0;                                          //   IDLE/WAIT (M0)
    } else {                                                // not too cold
      if ( taup_delta >= taup_DIF ) {                       //   taup_delta ok
        RUNMODE = 1;                                        //     ENTFEUCHTUNG (M1)
      } else if (  RUNMODE == 1 &&                                             
                  (taup_delta < taup_DIF) && 
                  (taup_delta > (taup_DIF - HYSTERESE)) ) { //   taup_delta still ok
        RUNMODE = 1;                                        //     ENTFEUCHTUNG Keep running (M1)
      } else {                                              //   otherwise
        RUNMODE = 2;                                        //     INTERVALLLUEFTUNG (M2)
      }
    }
    if ( (humi_O > humi_O_MAX) && RUNMODE == 1 ) RUNMODE = 2;  // Too wet outside

  // --- activateModes-------------------------------------------------------
    if ( RUNMODE == 0 ) {          // Switch or stay IDLE (M0)
      fan(MOSFETPIN_I, 0.0);FAN_I="0";
      fan(MOSFETPIN_O, 0.0);FAN_O="0";
    } 
    else if ( RUNMODE == 1) {      // Switch or stay ENTFEUCHTUNG (M1)
      fan(MOSFETPIN_I, 1.0);FAN_I="1";
      fan(MOSFETPIN_O, 1.0);FAN_O="1";
    } 
    else if ( RUNMODE == 2 ) {       // Switch or stay LUEFTUNG (M2)
      if ( M < 20 ) { // RUN FOR EVERY FIRST 20 MINUTES OF EACH HOUR
        fan(MOSFETPIN_I, 0.0);FAN_I="0";
        fan(MOSFETPIN_O, 1.0);FAN_O="1";
      } else {
        fan(MOSFETPIN_I, 0.0);FAN_I="0";
        fan(MOSFETPIN_O, 0.0);FAN_O="0";
      }
    }
    // ------------------------------------------------------------------------
    String contentType = "application/x-www-form-urlencoded";
    postData = String(RUNMODE)
             + ";"
             + postData
             + ";"
             + String(FAN_O)
             + ";"
             + String(FAN_I);
  
    // --- post to Server -----------------------------------------------------
    if(p) Serial.println(postData);
    postData = "dht=" + postData + "&token=" + token;
    client.post("/dht/log.php", contentType, postData);
    // show the status code and body of the response
    int statusCode = client.responseStatusCode();
    if(p) Serial.print("Status code: ");Serial.println(statusCode);
    if ( statusCode != 200 ) resetFunc(); // call reset
    delay(5000);
    client.stop();

    // --- remember Time  -----------------------------------------------------
    lastTime = WiFi.getTime();
   } else {
     delay(5000);
   }
  } else { // SILENT
    fan(MOSFETPIN_I, 0.0);
    fan(MOSFETPIN_O, 0.0);
    delay(1000);
  }

} // END loop