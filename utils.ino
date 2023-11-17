void fan(int MOSFETPIN, float speed ) {

  if ( MOSFETPIN == 9 ) {
       String FANMODE = "OUT";
       float fan_speedNow = fan_O_speedNow;
  } else {
       String FANMODE = "IN";
       float fan_speedNow = fan_I_speedNow;
  }

  if ( speed > 0.0 &&
       speed < fan_speedMin ) {
       speed = fan_speedMin;
  }
  if ( speed > fan_speedMax ) {
       speed = fan_speedMax;
  }
  if ( speed != fan_speedNow &&
       speed >= fan_speedMin ) {

    if ( speed < fan_speedMin && speed > 0.0 ) {
         // SPIN UP
         //Serial.println("SPIN UP FAN_" + FANMODE);
         analogWrite(MOSFETPIN, 0.1);
         delay(2000);
    }
    //Serial.print("SET SPEED FAN_" + FANMODE + "= ");
    //Serial.println(speed);
    fan_O_speedNow = speed; // REMEMBER
    
    analogWrite(MOSFETPIN, speed * 255);
    
  } else if ( speed < fan_speedMin &&
              speed != fan_O_speedNow ) {
    
    analogWrite(MOSFETPIN, 0.0);
    //
    if ( FANMODE == "OUT" ) {
         fan_O_speedNow = speed; // REMEMBER
    } else {
         fan_I_speedNow = speed; // REMEMBER
    }
  }
}

// https://github.com/MakeMagazinDE/Taupunktluefter/blob/1eb6624/Taupunkt_Lueftung.ino
float taupunkt(float t, float r) {
  
  float a, b;
  if (t >= 0) {
    a = 7.5;
    b = 237.3;
  } else if (t < 0) {
    a = 7.6;
    b = 240.7;
  }
  
  // Sättigungsdampfdruck in hPa
  float sdd = 6.1078 * pow(10, (a*t)/(b+t));
  
  // Dampfdruck in hPa
  float dd = sdd * (r/100);
  
  // v-Parameter
  float v = log10(dd/6.1078);
  
  // Taupunkttemperatur (°C)
  float tt = (b*v) / (a-v);
  return { tt };  

}
