
#include <M5Stack.h>

#include "src/MHZ/MHZ.h"


// pin for pwm reading

MHZ co2(&Serial2, 0, MHZ19B);

void setup() {
  M5.begin();
  M5.Lcd.drawJpgFile(SD, "/prepare.jpg");
  
  Serial.begin(115200);

  delay(100);
  Serial.println("MHZ 19B");

  Serial2.begin(9600);

  // enable debug to get addition information
  co2.setDebug(false);

  M5.Lcd.setTextColor(ORANGE,BLACK);
  M5.Lcd.setTextSize(2);

  int i = 3*60;
  char str[6];
  while (co2.isPreHeating()) {

    M5.Lcd.setCursor(273,160);
    sprintf(str,"%3d", i);
    M5.Lcd.print(str);
    delay(1000);
    i--;

    if(M5.BtnC.isPressed())break;  // Skip preheat
    M5.update();
  }
}

String img_path="";

void loop() {


  int co2_ppm = co2.readCO2UART();
  
  if (co2_ppm > 0) {

    String new_image = "";

  
    if(co2_ppm <= 1000){
      new_image = "/co2-low.jpg";  // 0 - 1000
    }else if(co2_ppm <= 2000){    
      new_image = "/co2-mid1.jpg";   // 1000 - 2000
    }else if(co2_ppm <= 3000){   
      new_image = "/co2-mid2.jpg";   // 2000 - 3000
    }else{
      new_image = "/co2-high1.jpg";   // 3000 -
    }

    // Refresh background image
    if(new_image != img_path){
        M5.Lcd.drawJpgFile(SD, new_image.c_str());
        img_path = new_image;
    }  

    char str[6];
    sprintf(str,"%5d", co2_ppm);
    
    M5.Lcd.setTextColor(BLACK,WHITE);
    M5.Lcd.setTextSize(4);
    M5.Lcd.setCursor(70,185);
    M5.Lcd.print(str);
    
    
    //M5.Lcd.println(ppm_uart);
  }

  int temperature = co2.getLastTemperature();
  if (temperature > 0) {
    Serial.print(", Temperature: ");
    Serial.println(temperature);
  }

//  Serial.println("\n------------------------------");
  delay(1000);
}
