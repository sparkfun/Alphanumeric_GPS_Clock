/*
 2-9-2011
 Spark Fun Electronics 2011
 Nathan Seidle
 
 This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).
 
 NOTE: This is the older, kind of flawed code to see how we used to hardcode DST. Please see
 Alpha_GPS_Clock_with_DST.ino for the latest and greatest.
 
 This code takes the GPS NMEA strings from a Locosys LS20031 module and outputs the current
 time to a 6 panel alphanumeric display. Note: To reprogram the Arduino, I have to unplug the GPS
 module from the RX line on the Arduino. I would use the newsoftserial lib from Mikal but I don't
 think it is working at 57600bps with the flood of serial GPS data coming in from the LS20031.
 
 The time should be correctly adjusted for daylight savings time in the MST timezone until 2014.
 
 We also display current day of the week and date on 15s and 45s of every minute.
 There's a neat day of the week function in here!
 
 We also display current altitude and uptime in hours on every hour.
 
 */

#include <TinyGPS.h>

#include <AlphaNumeric_Driver.h>  // Include AlphaNumeric Display Driver Library
#define NUMBER_OF_DISPLAYS 6  // This value is currently only used by the library for the clear() command.

int local_hour_offset = 7; //Boulder Colorado (MST) is -7 hours from GMT

// --------- Alphanumeric Pin Definitions -----------
int SDIpin = 6;
int CLKpin = 5;
int LEpin = 4;
int OEpin = 3;
// -------------------------------------
alphaNumeric myDisplay(SDIpin, CLKpin, LEpin, OEpin, NUMBER_OF_DISPLAYS);  // Create an instance of Alpha Numeric Displays

TinyGPS gps;

void gpsdump(TinyGPS &gps);
bool feedgps();
void printFloat(double f, int digits = 2);

char buffer[50];

void setup()
{
  Serial.begin(9600);
  Serial.print("GPS Clock Mini"); 

}

void loop()
{
  bool newdata = false;
  unsigned long start = millis();

  //Every second we print an update
  while (millis() - start < 1000)
  {
    if (feedgps())
    {
      newdata = true;
    }
  }

  if (newdata)
  {
    unsigned long age, date, time;
    int year;
    byte month, day, hour, minute, second, hundredths;
    gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);

    //For debugging
    Serial.print("Date: "); 
    Serial.print((int)month); 
    Serial.print("/"); 
    Serial.print((int)day); 
    Serial.print("/"); 
    Serial.print(year);
    Serial.print(" ");

    //Check for daylight savings time

    //Works fine until you get to the year you never expected your project to make it to
    if(year == 2011) {
     if(month == 3 && day > 12) hour++; //DST begins March 13th
     else if(month > 3 && month < 11) hour++;
     else if(month == 11 && day < 6) hour++; //DST ends November 6th
     }
     if(year == 2012) {
     if(month == 3 && day > 10) hour++; //DST begins March 11th
     else if(month > 3 && month < 11) hour++;
     else if(month == 11 && day < 4) hour++; //DST ends November 4th
     }
     if(year == 2013) {
     if(month == 3 && day > 9) hour++; //DST begins March 10th
     else if(month > 3 && month < 11) hour++;
     else if(month == 11 && day < 3) hour++; //DST ends November 3th
     }

    //Convert UTC hours to local current time using local_hour
    if(hour < local_hour_offset)
      hour += 24; //Add 24 hours before subtracting local offset
    hour -= local_hour_offset;
    if(hour > 12) hour -= 12; //Get rid of military time

    //Let's just display the time, but on 15 seconds and 45 seconds, display something else
    if( (second == 15) || (second == 45)) { 
      if( (minute == 0) && (second == 15) ){ //Display uptime every hour on 0:15 seconds
        sprintf(buffer, " Uptime hours %ld", millis() / (long)(1000 * 3600));
        myDisplay.scroll(buffer, 200); //Push to alphanumeric display
        delay(3000); //Display this message for 3 seconds
      }
      else if( (minute == 0) && (second == 45) ) { //Display altitude every hour on 0:45 seconds
        sprintf(buffer, " Altitude %ldm", gps.altitude() / 100);
        myDisplay.scroll(buffer, 200); //Push to alphanumeric display
        delay(3000); //Display this message for 3 seconds
      }

      else if(second == 15) { //On every 0:15, Day of week and Go home!
        byte DoW = day_of_week(year, month, day);
        if(DoW == 0 || DoW == 6) { //Sunday or Saturday
          sprintf(buffer, " GoHome");
        }
        else { //Display the day of the week
          if(DoW == 1) sprintf(buffer, " Monday");
          if(DoW == 2) sprintf(buffer, " Tuesday");
          if(DoW == 3) sprintf(buffer, " Wednesday");
          if(DoW == 4) sprintf(buffer, " Thursday");
          if(DoW == 5) sprintf(buffer, " Friday!");
        }
      }
      else if(second == 45) { //On every 0:45, Date
        sprintf(buffer, " Date %02d%02d%02d", month, day, year % 100);
      }

      myDisplay.scroll(buffer, 200); //Push to alphanumeric display
      delay(1000); //Display this message for 1 second
    }
    else { //Just print the time

        //Let's count down to new years!
      sprintf(buffer, "%02d%02d%02d", 11 - hour, 59 - minute, 59 - second);
      Serial.println(buffer);

      //Scrolling causes the display to flash, so turn it off
      digitalWrite(LEpin, LOW); //Disable the screen while we write to it
      myDisplay.scroll(buffer, 0); //Push to alphanumeric display
      digitalWrite(LEpin, HIGH); //Now display new data
    } 

  } //End gps newdata

} //End loop()

//Given the current year/month/day
//Returns 0 (Sunday) through 6 (Saturday) for the day of the week
//Assumes we are operating in the 2000-2099 century
//From: http://en.wikipedia.org/wiki/Calculating_the_day_of_the_week
char day_of_week(int year, int month, int day) {

  //offset = centuries table + year digits + year fractional + month lookup + date
  int centuries_table = 6; //We assume this code will only be used from year 2000 to year 2099
  int year_digits;
  int year_fractional;
  int month_lookup;
  int offset;

  //Example Feb 9th, 2011

  //First boil down year, example year = 2011
  year_digits = year % 100; //year_digits = 11
  year_fractional = year_digits / 4; //year_fractional = 2

  switch(month) {
  case 1: 
    month_lookup = 0; //January = 0
    break; 
  case 2: 
    month_lookup = 3; //February = 3
    break; 
  case 3: 
    month_lookup = 3; //March = 3
    break; 
  case 4: 
    month_lookup = 6; //April = 6
    break; 
  case 5: 
    month_lookup = 1; //May = 1
    break; 
  case 6: 
    month_lookup = 4; //June = 4
    break; 
  case 7: 
    month_lookup = 6; //July = 6
    break; 
  case 8: 
    month_lookup = 2; //August = 2
    break; 
  case 9: 
    month_lookup = 5; //September = 5
    break; 
  case 10: 
    month_lookup = 0; //October = 0
    break; 
  case 11: 
    month_lookup = 3; //November = 3
    break; 
  case 12: 
    month_lookup = 5; //December = 5
    break; 
  default: 
    month_lookup = 0; //Error!
    return(-1);
  }

  offset = centuries_table + year_digits + year_fractional + month_lookup + day;
  //offset = 6 + 11 + 2 + 3 + 9 = 31
  offset %= 7; // 31 % 7 = 3 Wednesday!

  return(offset); //Day of week, 0 to 6

  //Example: May 11th, 2012
  //6 + 12 + 3 + 1 + 11 = 33
  //5 = Friday! It works!
}

void printFloat(double number, int digits) {
  // Handle negative numbers
  if (number < 0.0)
  {
    Serial.print('-');
    number = -number;
  }

  // Round correctly so that print(1.999, 2) prints as "2.00"
  double rounding = 0.5;
  for (uint8_t i=0; i<digits; ++i)
    rounding /= 10.0;

  number += rounding;

  // Extract the integer part of the number and print it
  unsigned long int_part = (unsigned long)number;
  double remainder = number - (double)int_part;
  Serial.print(int_part);

  // Print the decimal point, but only if there are digits beyond
  if (digits > 0)
    Serial.print("."); 

  // Extract digits from the remainder one at a time
  while (digits-- > 0)
  {
    remainder *= 10.0;
    int toPrint = int(remainder);
    Serial.print(toPrint);
    remainder -= toPrint; 
  } 
}

void gpsdump(TinyGPS &gps)
{
  long lat, lon;
  float flat, flon;
  unsigned long age, date, time, chars;
  int year;
  byte month, day, hour, minute, second, hundredths;
  unsigned short sentences, failed;

  feedgps();

  gps.get_datetime(&date, &time, &age);
  Serial.print("Date(ddmmyy): "); 
  Serial.print(date); 
  Serial.print(" Time(hhmmsscc): "); 
  Serial.print(time);
  Serial.print(" Fix age: "); 
  Serial.print(age); 
  Serial.println("ms.");

  feedgps();

  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
  Serial.print("Date: "); 
  Serial.print(static_cast<int>(month)); 
  Serial.print("/"); 
  Serial.print(static_cast<int>(day)); 
  Serial.print("/"); 
  Serial.print(year);
  Serial.print("  Time: "); 
  Serial.print(static_cast<int>(hour)); 
  Serial.print(":"); 
  Serial.print(static_cast<int>(minute)); 
  Serial.print(":"); 
  Serial.print(static_cast<int>(second)); 
  Serial.print("."); 
  Serial.print(static_cast<int>(hundredths));
  Serial.print("  Fix age: ");  
  Serial.print(age); 
  Serial.println("ms.");

  feedgps();

  Serial.print("Alt(cm): "); 
  Serial.print(gps.altitude()); 
  Serial.print(" Course(10^-2 deg): "); 
  Serial.print(gps.course()); 
  Serial.print(" Speed(10^-2 knots): "); 
  Serial.println(gps.speed());
  Serial.print("Alt(float): "); 
  printFloat(gps.f_altitude()); 
  Serial.print(" Course(float): "); 
  printFloat(gps.f_course()); 
  Serial.println();
  Serial.print("Speed(knots): "); 
  printFloat(gps.f_speed_knots()); 
  Serial.print(" (mph): ");  
  printFloat(gps.f_speed_mph());
  Serial.print(" (mps): "); 
  printFloat(gps.f_speed_mps()); 
  Serial.print(" (kmph): "); 
  printFloat(gps.f_speed_kmph()); 
  Serial.println();
}

bool feedgps()
{
  while (Serial.available()) {
    if (gps.encode(Serial.read()))
      return true;
  }
  return false;
}


