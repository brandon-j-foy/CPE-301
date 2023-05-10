// Brandon Foy
// CPE 301, Spring 2023

#include <LiquidCrystal.h>
#include <RTClib.h>
#include <DHT.h>
#include <Stepper.h>

// Temperature and Humidity sensor
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
RTC_DS1307 RTC;

#define StepperPin1 26
#define StepperPin2 28
#define StepperPin3 30
#define StepperPin4 32

#define RDA 0x80
#define TBE 0x20


LiquidCrystal lcd(8, 7, 6, 5, 4, 3);

Stepper stepper(2048, StepperPin1, StepperPin2, StepperPin3, StepperPin4);
int stepperRate = 2048;

// Digital Registers
volatile unsigned char* port_b = (unsigned char*) 0x25;
volatile unsigned char* ddr_b = (unsigned char*) 0x24;
volatile unsigned char* pin_b = (unsigned char*) 0x23;

// ANALOG
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADCH_DATA = (unsigned int*) 0x79;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

// Water Level Port
unsigned char WaterA0 = 0;

// Min and Max Thresholds
#define TempMax 80
#define TempMin 25
#define WaterLevel 80

// Flags depicting what state we are in.
enum state {
   off, idle, fan, error
};

// Begin in off state.
enum state stat = off;

void TimeStamp(DateTime now){

  U0putchar(' ');

  int year = now.year();
  int month = now.month();
  int day = now.day();
  int hour = now.hour();
  int minute = now.minute();
  int second = now.second();
  char numbers[10] = {'0','1','2','3','4','5','6','7','8','9'};
  int onesYear = year % 10;
  int tensYear = year / 10 % 10;
  int onesMonth = month % 10;
  int tensMonth = month / 10 % 10;
  int onesDay = day % 10;
  int tensDay = day / 10 % 10;
  int onesHour = hour % 10;
  int tensHour = hour / 10 % 10;
  int onesMinute = minute % 10;
  int tensMinute = minute / 10 % 10;
  int onesSecond = second % 10;
  int tensSecond = second / 10 % 10;
  
  U0putchar('M');
  U0putchar(':');
  U0putchar('D');
  U0putchar(':');
  U0putchar('Y');

  U0putchar(' ');
  
  U0putchar('H');
  U0putchar(':');
  U0putchar('M');
  U0putchar(':');
  U0putchar('S');

  U0putchar(' ');

  U0putchar(numbers[tensMonth]);
  U0putchar(numbers[onesMonth]);
  U0putchar(':');
  U0putchar(numbers[tensDay]);
  U0putchar(numbers[onesDay]);
  U0putchar(':');
  U0putchar(numbers[tensYear]);
  U0putchar(numbers[onesYear]);
  
  U0putchar(' ');

  U0putchar(numbers[tensHour]);
  U0putchar(numbers[onesHour]);
  U0putchar(':');
  U0putchar(numbers[tensMinute]);
  U0putchar(numbers[onesMinute]);
  U0putchar(':');
  U0putchar(numbers[tensSecond]);
  U0putchar(numbers[onesSecond]);

  U0putchar('\n');
}

void setup() {
  //Initialize analog ports
  adc_init();

  // Initialzie Serial Port
  Serial.begin(9600);

  // Initialize LCD
  lcd.begin(16, 2);

  // Initialize DHT
  dht.begin();

  PORTD |= ((1 << PD0) || (1 << PD1));
  DateTime now = DateTime(2023, 05, 0, 0, 0);
  RTC.adjust(now);
  stepper.setSpeed(10);

  *port_b &= 0b01111111;
  *ddr_b &= 0b01111111;
  *ddr_b |= 0b01111110;

}

void loop() {
  delay(2000);
        
  unsigned int w = adc_read(WaterA0);
  float f = temperatureRead(true);
  float h = humidity();

  DateTime now = RTC.now();

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F(" Temperature: "));
  Serial.print(f);
  Serial.print(F(" Water Level: "));
  Serial.print(w);
  Serial.print('\n');

  switch(stat) {
    case off:
      U0putchar('O');
      TimeStamp(now);
      DisabledState();
      break;
    case idle:
      U0putchar('I');
      TimeStamp(now);
      IdleState();
      break;
    case error:
      U0putchar('E');;
      TimeStamp(now);
      ErrorState();
      break;
    case fan:
      U0putchar('F');;
      TimeStamp(now);
      RunningState();
      break;
    default:
      break;
  }
  
}

void DisabledState() { // Or off state
  lcd.clear();
  lcd.noDisplay();

  *port_b &= 0b10000001;
  *port_b |= 0b00001000;
  
  while ( (*pin_b & (1 << 7)) == 0) { }

  stat = idle;
  lcd.display();
}

void IdleState(){
  *port_b |= 0b01000000;
  *port_b &= 0b01000000; 
  
  unsigned int wLevel = water_level();
  float tRead = temperatureRead(true);
  float hRead = humidity();

  lcd_function(tRead, hRead);


  if (wLevel < WaterLevel) stat = error;

  else if (tRead > TempMax) stat = fan;
}

void ErrorState(){
    *port_b |= 0b00100000; // Turn on red LED
    *port_b &= 0b00100000;
    
    lcd.clear();
    lcd.print("Low Water!");

    unsigned int WL = water_level();

    while (WL < WaterLevel) {
      delay(1000);
      WL = water_level();
      lcd.setCursor(0, idle);
      lcd.print("Level:");
      lcd.setCursor(7, idle);
      lcd.print(WL);
  }
  
  stat = idle;
  lcd.clear();
}

void RunningState(){
  *port_b |= 0b00010000; // Enable fan and running LED  
  *port_b &= 0b00010000;
  //*ddr_b |= 0b01000000; // Meant to enable stepper motors
  float temp = temperatureRead(true);
  float humid = humidity();
  //setStepperMotor(stepperDirection);

  // Check water level and temperature
  if (water_level() < WaterLevel) stat = error;
  else if (temp > TempMax) {
    delay(1000);
    Serial.print("Temp: ");
    Serial.print(temp);
    Serial.print('\n');
    lcd_function(temp, humid);
    return RunningState();
  }
  else {
    lcd.clear();
    stat = idle;
  }
}


unsigned int water_level() {
  return adc_read(WaterA0);
}

float temperatureRead(bool F) {
  return dht.readTemperature(true);
}

float humidity() {
  return dht.readHumidity();
}

void lcd_function(float temp, float humidity) {
  lcd.setCursor(0, 0);
  lcd.print("Temp:  Humidity:");
  lcd.setCursor(0, 1);
  lcd.print(temp);
  lcd.setCursor(7, 1);
  lcd.print(humidity);
}

void adc_init(){

  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 0-2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111; // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111; // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000; // clear bit 4-0 to 0 to reset the channel and gain bits
}
unsigned int adc_read(unsigned char adc_channel_num)
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  // set the channel number
  if(adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;
    // set MUX bit 5
    *my_ADCSRB |= 0b00001000;
  }
  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register
  return *my_ADC_DATA;
}

void U0init(int U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}
unsigned char U0kbhit()
{
  return *myUCSR0A & RDA;
}
unsigned char U0getchar()
{
  return *myUDR0;
}
void U0putchar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}
