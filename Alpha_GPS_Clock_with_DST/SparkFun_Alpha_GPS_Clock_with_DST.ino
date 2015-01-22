/*
 2-9-2011
 Spark Fun Electronics 2011
 Nathan Seidle
 
 This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).
 
 This code takes the GPS NMEA strings from a Locosys LS20031 module and outputs the current
 time to a 6 panel alphanumeric display. Note: To reprogram the Arduino, I have to unplug the GPS
 module from the RX line on the Arduino. I would use the newsoftserial lib from Mikal but I don't
 think it is working at 57600bps with the flood of serial GPS data coming in from the LS20031.
 
 The time should be correctly adjusted for daylight savings time in the MST timezone until 2014.
 
 We also display current day of the week and date on 15s and 45s of every minute.
 There's a neat day of the week function in here!
 
 We also display current altitude and uptime in hours on every hour.
 
 3-23-2014: This is what happens when you assume a project won't last for more than a couple years.
 Here I am fixing DST on a clock I never thought would make it to 2014. Man was I wrong... Time to
 program in a (nearly) permanent daylight saving time fix.
 
 12-31-2014: Updated to count down to midnight (New Years).
 
 1-1-2015: Updated to not use the GPS library. This removes the requirement to have a lock. All we
 need/want is the date and time which takes very few satellite signals.
 
 */

#include <AlphaNumeric_Driver.h>  // Include AlphaNumeric Display Driver Library
#define NUMBER_OF_DISPLAYS 6  // This value is currently only used by the library for the clear() command.

int local_hour_offset = 7; //Boulder Colorado (MST) is -7 hours from UTC

// --------- Alphanumeric Pin Definitions -----------
byte SDIpin = 6;
byte CLKpin = 5;
byte LEpin = 4;
byte OEpin = 3;
// -------------------------------------
byte statLED = 13;

alphaNumeric myDisplay(SDIpin, CLKpin, LEpin, OEpin, NUMBER_OF_DISPLAYS);  // Create an instance of Alpha Numeric Displays

char buffer[20]; //"Tuesday" and stuff
char time[7]; //145205 = 2:52:05
char date[7]; //123114 = Dec 31, 2014
char sats[3]; //08 = 8 satellites in view
byte SIV = 0;

void setup()
{
  Serial.begin(9600);
  Serial.print("GPS Clock Mini");
  
  pinMode(statLED, OUTPUT);

  //Splash screen
  digitalWrite(LEpin, LOW); //Disable the screen while we write to it
  myDisplay.scroll(" NOGPS", 0); //Push to alphanumeric display
  digitalWrite(LEpin, HIGH); //Now display new data
}

void loop()
{
  if(checkGPS() == true) //Checks for new serial data
  {
    //Crack strings into usable numbers
    int year;
    byte day, month;
    crackDate(&day, &month, &year);
    
    byte hour, minute, second;
    crackTime(&hour, &minute, &second);

    SIV = 0;
    crackSatellites(&SIV);
    
    //Convert time to local time using daylight savings if necessary
    convertToLocal(&day, &month, &year, &hour);
    
    //For debugging
    Serial.print("Date: "); 
    Serial.print(month); 
    Serial.print("/"); 
    Serial.print(day); 
    Serial.print("/"); 
    Serial.print(year);
    Serial.print(" ");

    Serial.print("Time: "); 
    Serial.print(hour); 
    Serial.print(":"); 
    Serial.print(minute); 
    Serial.print(":"); 
    Serial.print(second);
    Serial.print(" ");
    
    Serial.print("SIV: "); 
    Serial.print(SIV);
   
    Serial.println();

    //Update display with stuff
    //Let's just display the time, but on 15 seconds and 45 seconds, display something else
    if( (second == 15) || (second == 45))
    { 
      if(second == 15) //On every 0:15, Day of week and Go home!
      {
        byte DoW = day_of_week(year, month, day);
        if(DoW == 0 || DoW == 6) //Sunday or Saturday
        {
          sprintf(buffer, " GoHome");
        }
        else //Display the day of the week
        {
          if(DoW == 1) sprintf(buffer, " Monday");
          if(DoW == 2) sprintf(buffer, " Tuesday");
          if(DoW == 3) sprintf(buffer, " Wednesday");
          if(DoW == 4) sprintf(buffer, " Thursday");
          if(DoW == 5) sprintf(buffer, " Friday!");
        }
      }
      else if(second == 45) //On every 0:45, Date
      {
        sprintf(buffer, " Date %02d%02d%02d", month, day, year % 100);
      }

      myDisplay.scroll(buffer, 200); //Push to alphanumeric display
      delay(1000); //Display this message for 1 second
    }
    else //Just print the time
    {
      //Let's count down to new years!
      //sprintf(buffer, "%02d%02d%02d", 11 - hour, 59 - minute, 59 - second);

      sprintf(buffer, "%02d%02d%02d", hour, minute, second);

      //Scrolling causes the display to flash, so turn it off
      digitalWrite(LEpin, LOW); //Disable the screen while we write to it
      myDisplay.scroll(buffer, 0); //Push to alphanumeric display
      digitalWrite(LEpin, HIGH); //Now display new data
    } 
    
  }
  
  //Turn on stat LED if we have enough sats for a lock
  if(SIV > 3)
    digitalWrite(statLED, HIGH);
  else
    digitalWrite(statLED, LOW);

} //End loop()

//Given date and hours, convert to local time using DST
void convertToLocal(byte* day, byte* month, int* year, byte* hours)
{
  //Since 2007 DST starts on the second Sunday in March and ends the first Sunday of November
  //Let's just assume it's going to be this way for awhile (silly US government!)
  //Example from: http://stackoverflow.com/questions/5590429/calculating-daylight-savings-time-from-only-date

  boolean dst = false; //Assume we're not in DST

  if(*month > 3 && *month < 11) dst = true; //DST is happening!

  byte DoW = day_of_week(*year, *month, *day); //Get the day of the week. 0 = Sunday, 6 = Saturday

  //In March, we are DST if our previous Sunday was on or after the 8th.
  int previousSunday = *day - DoW;

  if (*month == 3)
  {
    if(previousSunday >= 8) dst = true; 
  } 

  //In November we must be before the first Sunday to be dst.
  //That means the previous Sunday must be before the 1st.
  if(*month == 11)
  {
    if(previousSunday <= 0) dst = true;
  }
  if(dst == true) *hours = *hours + 1; //If we're in DST add an extra hour


  //Convert UTC hours to local current time using local_hour
  if(*hours < local_hour_offset)
  {
    //Go back a day in time
    *day = *day - 1;
    
    if(*day == 0)
    {
      //Jeesh. Figure out what month this drops us into
      *month = *month - 1;
      
      if(*month == 1) *day = 31;
      if(*month == 2) *day = 28; //Not going to deal with it
      if(*month == 3) *day = 31;
      if(*month == 4) *day = 30;
      if(*month == 5) *day = 31;
      if(*month == 6) *day = 30;
      if(*month == 7) *day = 31;
      if(*month == 8) *day = 31;
      if(*month == 9) *day = 30;
      if(*month == 10) *day = 31;
      if(*month == 11) *day = 30;
      if(*month == 0)
      {
        *year = *year - 1;
        *month = 12;
        *day = 31;
      }
    }
    
    *hours = *hours + 24; //Add 24 hours before subtracting local offset
  }
  *hours = *hours - local_hour_offset;
  if(*hours > 12) *hours = *hours - 12; //Get rid of military time
  
}

//Given the date string return day, month, year
void crackDate(byte* day, byte* month, int* year)
{
  for(byte x = 0 ; x < 3 ; x++)
  {
    byte temp = (date[ (x*2) ] - '0') * 10;
    temp += (date[ (x*2)+1 ] - '0');
    
    if(x == 0) *day = temp;
    if(x == 1) *month = temp;
    if(x == 2)
    {
      *year = temp;
      *year += 2000;
    }
  }
}

//Given the time string return hours, minutes, seconds
void crackTime(byte* hours, byte* minutes, byte* seconds)
{
  for(byte x = 0 ; x < 3 ; x++)
  {
    byte temp = (time[ (x*2) ] - '0') * 10;
    temp += (time[ (x*2)+1 ] - '0');
    
    if(x == 0) *hours = temp;
    if(x == 1) *minutes = temp;
    if(x == 2) *seconds = temp;
  }
}

//Given the sats string return satellites in view
void crackSatellites(byte* SIV)
{
  *SIV = (sats[0] - '0') * 10;
  *SIV += (sats[1] - '0');
}

//Looks at the incoming serial stream and tries to find time, date and sats in view
boolean checkGPS()
{
  unsigned long start = millis();

  //Give up after a second of polling
  while (millis() - start < 1500)
  {
    if(Serial.available())
    {
      if(Serial.read() == '$') //Spin until we get to the start of a sentence
      {
        //Get "GPGGA,"
        
        char sentenceType[6];
        for(byte x = 0 ; x < 6 ; x++)
        {
          while(Serial.available() == 0) delay(1); //Wait
          sentenceType[x] = Serial.read();
        }
        sentenceType[5] = '\0';
        //Serial.println(sentenceType);
          
        if(sentenceType[3] == 'G' && sentenceType[4] == 'A')
        {
          //We are now listening to the GPGGA sentence for time and number of sats
          //$GPGGA,145205.00,3902.08127,N,10415.90019,W,2,08,2.68,1611.4,M,-21.3,M,,0000*5C
          
          //Grab time
          for(byte x = 0 ; x < 6 ; x++)
          {
            while(Serial.available() == 0) delay(1); //Wait
            time[x] = Serial.read(); //Get time characters

            //Error check for a time that is too short
            if(time[x] == ',')
            {
              Serial.println("No time found");
              return(false);
            }
          }
          time[6] = '\0';
          //Serial.println(time);
          
          //Wait for 6 commas to go by
          for(byte commaCounter = 6 ; commaCounter > 0 ; )
          {
            while(Serial.available() == 0) delay(1); //Wait
            char incoming = Serial.read(); //Get time characters
            if(incoming == ',') commaCounter--;
          }

          //Grab sats in view
          for(byte x = 0 ; x < 2 ; x++)
          {
            while(Serial.available() == 0) delay(1); //Wait
            sats[x] = Serial.read(); //Get sats in view characters
          }
          sats[2] = '\0';
          //Serial.println(sats);
          
          //Once we have GPGGA, we should already have GPRMC so return
          return(true);
        }
        else if(sentenceType[3] == 'M' && sentenceType[4] == 'C')
        {
          //We are now listening to GPRMC for the date
          //$GPRMC,145205.00,A,3902.08127,N,10415.90019,W,0.772,,010115,,,D*6A

          //Wait for 8 commas to go by
          for(byte commaCounter = 8 ; commaCounter > 0 ; )
          {
            while(Serial.available() == 0) delay(1); //Wait
            char incoming = Serial.read(); //Get time characters
            if(incoming == ',') commaCounter--;
          }

          //Grab date
          for(byte x = 0 ; x < 6 ; x++)
          {
            while(Serial.available() == 0) delay(1); //Wait
            date[x] = Serial.read(); //Get date characters
            
            //Error check for a date too short
            if(date[x] == ',')
            {
              Serial.println("No date found");
              return(false);
            }
          }
          date[6] = '\0';
          //Serial.println(date);
        }

      }
    }
  }
  
  Serial.println("No valid GPS serial data");
  return(false);
}

//Given the current year/month/day
//Returns 0 (Sunday) through 6 (Saturday) for the day of the week
//Assumes we are operating in the 2000-2099 century
//From: http://en.wikipedia.org/wiki/Calculating_the_day_of_the_week
char day_of_week(int year, byte month, byte day) {

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
