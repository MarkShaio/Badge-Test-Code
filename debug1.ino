//#include <Mirf.h>
//#include <MirfHardwareSpiDriver.h>
//#include <MirfSpiDriver.h>
//#include <nRF24L01.h>
//#include <SPI.h>
/*
Turns on LED1 and LED2 is toggled by the switch
 */
 
#define RED_LED P1_0
#define LED2 P1_1
#define BUTTON P1_2
//---------------
#define SS P2_0
#define CE P2_1
#define SCK P3_0
#define MOSI P3_1
#define MISO P3_2
#define IRQ P2_2
//---------------
#define test0 P2_4//random pin that is not connected
#define test1 P1_1//pin that is connected to LED2
//SPI pin configuration


int buttonState = 0;
int val = 0;
void setup() {                
  // initialize the digital pin as an output.
  // Pin 14 has an LED connected on most Arduino boards:
  //Serial.begin(115200);
  //SPI.begin();
  //pinMode(SS,OUTPUT);
  pinMode(test0,INPUT);
  pinMode(test1,INPUT);
  pinMode(CE,OUTPUT);
  pinMode(SS,OUTPUT);
  pinMode(IRQ,OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(LED2,OUTPUT);  
  pinMode(BUTTON, INPUT);
  //digitalWrite(RED_LED,LOW);
  //digitalWrite(LED2,HIGH);
}

void loop() {
  //Serial.println(bitRead(PORTA,0));
  //val = bitRead(P2OUT,IRQ);
  //digitalWrite(LED2,HIGH);
  //val = P2IN & 0b00000100;
  //digitalWrite(RED_LED,val);
  //P1OUT &= 0b00000001;
  //P1OUT &= 0b00000001;
  //if(val)
    //P1OUT |= 0b00000001;
    //digitalWrite(RED_LED,val);
  //else{
    //P1OUT |= 0b00000000;
 
  //}
 
  
  buttonState = digitalRead(BUTTON);
  if(buttonState == LOW){
    digitalWrite(LED2,HIGH);
  }
  else{
    digitalWrite(LED2,LOW);
    digitalWrite(RED_LED,HIGH);
  }
  //digitalWrite(SS,LOW);
  //SPI.transfer(0);//to send register locaton
  //SPI.transfer(255);//to send value to register
  //digitalWrite(SS,HIGH);
  
  //digitalWrite(RED_LED, HIGH);   // set the LED on
  //delay(1000);              // wait for a second
  //digitalWrite(RED_LED, LOW);    // set the LED off
  //delay(1000);              // wait for a second
}
