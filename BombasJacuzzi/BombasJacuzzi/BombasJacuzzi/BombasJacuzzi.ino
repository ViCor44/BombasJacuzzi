#include <SD.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Wire.h>
#include <DS1307RTC.h>
#include <SPI.h>
#include <string.h>
#include <Time.h>
#include <EEPROM.h>
#include <EEPROMAnything.h>
#include <avr/pgmspace.h>

// Definição dos pins a conectar às bombas
#define counterpin1 7
#define counterpin2 8

long lastTime1 = 0;
long lastTime2 = 0;
int hoursPump1;
int hoursPump2;
int minutesPump1;
int minutesPump2;
int secondsPump1;
int secondsPump2;

/***** Configurações Ethernet *****/
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xF3 };
IPAddress ip(191,188,127, 77); //Substituir pelo ip a implementar
IPAddress dns(8,8,8,8);
IPAddress gateway(191,188,127,254);
IPAddress subnet(255,255,254,0);
EthernetServer server(80);

/***** Configurações NTP *****/
unsigned int localPort = 8888;          // local port to listen for UDP packets
IPAddress timeServer(128, 138, 141, 172); //NIST time server IP address: for more info
                                        //see http://tf.nist.gov/tf-cgi/servers.cgi
 
const int NTP_PACKET_SIZE= 48; //NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
EthernetUDP Udp;

unsigned long lastIntervalTime = 0; //The time the last measurement occured.
#define MEASURE_INTERVAL 600000 //10 minute intervals between measurements (in ms)
unsigned long newFileTime;          //The time at which we should create a new week's file
#define FILE_INTERVAL 604800        //One week worth of seconds

char newFilename[18] = "";
unsigned long rawTime;
 
//A structure that stores file config variables from EEPROM
typedef struct{                    
    unsigned long newFileTime;      //Keeps track of when a newfile should be made.
    char workingFilename[19];    //The path and filename of the current week's file
} configuration;

configuration config;               //Actually make our config struct

void setup() {    
    Serial.begin(9600);  
    pinMode(10, OUTPUT);          // set the SS pin as an output (necessary!)
    digitalWrite(10, HIGH);       // but turn off the W5100 chip!
     
    // Verificação da existência de cartão
    if (!SD.begin(4)) {
        Serial.println("Card failed, or not present");        
        return;
    }
    Serial.println("card initialized.");
   
    // Inicialização de Ethernet
    Ethernet.begin(mac, ip, dns, gateway, subnet);    
    server.begin();

    Udp.begin(localPort);
    EEPROM_readAnything(0,config); // make sure our config struct is syncd with EEPROM

    // Inicialização e escrita das variaveis na EEPROM(descomentar o que for necessário)
    //hoursPump1 = 131;
    //hoursPump2 = 131;
    //minutesPump1 = 33;
    //minutesPump2 = 33;
    //secondsPump1 = 30;
    //secondsPump2 = 30;
    //EEPROM.put(100,hoursPump1);
    //EEPROM.put(200,minutesPump1);
    //EEPROM.put(300,secondsPump1);
    //EEPROM.put(400,hoursPump2);
    //EEPROM.put(500,minutesPump2);
    //EEPROM.put(600,secondsPump2);

    //Leitura das variaveis na EEPROM
    EEPROM.get(100,hoursPump1);
    EEPROM.get(200,minutesPump1);
    EEPROM.get(300,secondsPump1);
    EEPROM.get(400,hoursPump2);
    EEPROM.get(500,minutesPump2);
    EEPROM.get(600,secondsPump2);
    Serial.println("Bomba 1: ");
    Serial.print("  Horas: ");
    Serial.println(hoursPump1);
    Serial.print("  Minutos: ");
    Serial.println(minutesPump1);
    Serial.print("  Segundos: ");
    Serial.println(secondsPump1);
    Serial.println("Bomba 2: ");
    Serial.print("  Horas: ");
    Serial.println(hoursPump2);
    Serial.print("  Minutos: ");
    Serial.println(minutesPump2);
    Serial.print("  Segundos: ");
    Serial.println(secondsPump2);

    //Inicialização dos pins de contagem
    pinMode(counterpin1, INPUT); 
    digitalWrite(counterpin1, HIGH);
    pinMode(counterpin2, INPUT);
    digitalWrite(counterpin2, HIGH);    
}

unsigned long getTime(){
  sendNTPpacket(timeServer); // send an NTP packet to a time server
 
  // wait to see if a reply is available
  delay(1000); 
  if ( Udp.parsePacket() ) { 
    // We've received a packet, read the data from it
    Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer
 
    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:
 
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]); 
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord; 
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;    
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears; 
    // return Unix time:
    return epoch;
  }
}
 
// send an NTP request to the time server at the given address,
// necessary for getTime().
unsigned long sendNTPpacket(IPAddress& address){   
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
 
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:        
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket();
}

int adjustDstEurope(int rawYear, int rawMounth, int rawDay){
 
 Serial.println(rawYear);
 int beginDSTMonth = 3;
 int beginDSTDate =  (31 - (4 + 5 * rawYear / 4) % 7);// last sunday of march
 Serial.println(beginDSTDate);
 
 int endDSTMonth = 10;
 int endDSTDate = (31 - (1 + 5 * rawYear / 4) % 7);//last sunday of october
 Serial.println(endDSTDate);
 
 // DST is valid as:
 if (((rawMounth < beginDSTMonth) && (rawMounth > endDSTMonth))
     || ((rawMounth == beginDSTMonth) && (rawDay <= beginDSTDate)) 
     || ((rawMounth == endDSTMonth) && (rawDay >= endDSTDate))){      
      return 0; // DST europe = utc +2 hour
     }
 else {    
    return 3600; // nonDST europe = utc +1 hour
  }
}

void ListHours1(EthernetClient client) {
   
  File workingDir = SD.open("/data");   
  client.println("<ul>");   
  while(true) {
      File entry =  workingDir.openNextFile();
       if (! entry) {
         break;
       }
       client.print("<li><a href=\"/H1.htm?file=");
       client.print(entry.name());
       client.print("\">");
       client.print(entry.name());
       client.println("</a></li>");
       entry.close();
  }
  client.println("</ul>");
  workingDir.close();
}

void ListHours2(EthernetClient client) {
   
  File workingDir = SD.open("/data");   
  client.println("<ul>");   
  while(true) {
      File entry =  workingDir.openNextFile();
       if (! entry) {
         break;
       }
       client.print("<li><a href=\"/H2.htm?file=");
       client.print(entry.name());
       client.print("\">");
       client.print(entry.name());
       client.println("</a></li>");
       entry.close();
  }
  client.println("</ul>");
  workingDir.close();
}

// HTTP/1.1 200 OK Função
void HtmlHeaderOK(EthernetClient client) {    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Cache-Control: no-cache");
    client.println("Cache-Control: no-store");
    client.println();
}

// HTTP/1.1 200 OK para XML
void HtmlHeaderOK_XML(EthernetClient client) {    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/xml");    
    client.println("Cache-Control: no-cache");
    client.println("Cache-Control: no-store");
    client.println();
} 

// HTTP/1.1 404 Not Found Função
void HtmlHeader404(EthernetClient client) {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/html");
    client.println("");
    client.println("<h2>File Not Found!</h2>");    
    client.println();
}

// Função que devolve o conteudo XML
void XML_response(EthernetClient client)
{     
    client.print("<?xml version = \"1.0\" ?>");
    client.print("<inputs>");  
    client.print("<counterPump1>");
    client.print(hoursPump1);
    client.print("</counterPump1>");
    client.print("<counterPump2>");
    client.print(hoursPump2);
    client.print("</counterPump2>");   
    client.print("</inputs>");
} 

// Função de conexão Ethernet
#define BUFSIZE 75
void Connection() {
    char clientline[BUFSIZE];
    int index = 0;
     
    EthernetClient client = server.available();
    if (client) {        
        // reset the input buffer
        index = 0;
       
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
           
                // If it isn't a new line, add the character to the buffer
                if (c != '\n' && c != '\r') {
                    clientline[index] = c;
                    index++;
                    // are we too big for the buffer? start tossing out data
                    if (index >= BUFSIZE)
                        index = BUFSIZE -1;             
                    // continue to read more data!
                    continue;
                }
           
                // got a \n or \r new line, which means the string is done
                clientline[index] = 0;
           
                // Print it out for debugging
                Serial.println(clientline);
           
                // Look for substring such as a request to get the root file
          
                if (strstr(clientline, "ajax_inputs")) {
                    HtmlHeaderOK_XML(client);
                    // send XML file containing input states
                    XML_response(client);                    
                }
                else if (strstr(clientline, "GET /horas1")) {
                    // send a standard http response header
                    HtmlHeaderOK(client);
                    // print all the data files, use a helper to keep it clean
                    client.println("<h1 style=\"font-family:arial; color: #ffffff;\">Graficos de Horas Trabalhadas</h1>");
                    client.println("<h2 style=\"font-family:arial; color: #ffffff;\">Ver graficos da semana(dd-mm-aa):</h2>");
                    ListHours1(client);
                }
                else if (strstr(clientline, "GET /horas2")) {
                    // send a standard http response header
                    HtmlHeaderOK(client);
                    // print all the data files, use a helper to keep it clean
                    client.println("<h1 style=\"font-family:arial; color: #ffffff;\">Graficos de Horas Trabalhadas</h1>");
                    client.println("<h2 style=\"font-family:arial; color: #ffffff;\">Ver graficos da semana(dd-mm-aa):</h2>");
                    ListHours2(client);
                }
                else if (strstr(clientline, "GET /COUNTER1=0")) {
                      hoursPump1 = 0;                     
                      minutesPump1 = 0;                      
                      secondsPump1 = 0;                      
                      EEPROM.put(100,hoursPump1);
                      EEPROM.put(200,minutesPump1);
                      EEPROM.put(300,secondsPump1);              
                  }
                  else if (strstr(clientline, "GET /COUNTER2=0")) {
                      hoursPump2 = 0;                     
                      minutesPump2 = 0;                      
                      secondsPump2 = 0;                      
                      EEPROM.put(400,hoursPump2);
                      EEPROM.put(500,minutesPump2);
                      EEPROM.put(600,secondsPump2);              
                  }                 
                else if (strstr(clientline, "GET /")) {
                    // this time no space after the /, so a sub-file!
                    char *filename;
             
                    filename = strtok(clientline + 5, "?"); // look after the "GET /" (5 chars) but before
                    // the "?" if a data file has been specified. A little trick, look for the " HTTP/1.1"
                    // string and turn the first character of the substring into a 0 to clear it out.
                    (strstr(clientline, " HTTP"))[0] = 0;            
                    
                    Serial.println(filename);
                    File file = SD.open(filename,FILE_READ);
                    if (!file) {
                        HtmlHeader404(client);
                        break;
                    }
            
                    Serial.println("Opened!");
                       
                    HtmlHeaderOK(client);
             
                    int16_t c;
                    while ((c = file.read()) > 0) {
                        // uncomment the serial to debug (slow!)
                        //Serial.print((char)c);
                        client.print((char)c);
                    }
                    file.close();
                }          
                else {
                    // Tudo o resto é um 404
                    HtmlHeader404(client);
                }
                break;
            }
        }
        // Tempo para o Web Browser receber os dados
        delay(1);
        client.stop();
    }
}
void loop(){   
  
  if(digitalRead(counterpin1) == 0){
    EEPROM.get(100,hoursPump1);
    EEPROM.get(200,minutesPump1); 
    EEPROM.get(300,secondsPump1);

    if(millis()-lastTime1 > 1000){
      secondsPump1++;    
      lastTime1 = millis();
      Serial.println(secondsPump1);
      EEPROM.put(300, secondsPump1);    
    }
    if(secondsPump1 > 60){
      minutesPump1++;
      secondsPump1 = 0;
      Serial.println(minutesPump1);
      EEPROM.put(300, secondsPump1);   
      EEPROM.put(200, minutesPump1);
    }
    if(minutesPump1 > 60){
      hoursPump1++;
      minutesPump1 = 0;
      Serial.println(minutesPump1);
      Serial.println(hoursPump1);
      EEPROM.put(100, hoursPump1);
      EEPROM.put(200, minutesPump1);
    }    
  }
  
  if(digitalRead(counterpin2) == 0){
    EEPROM.get(400,hoursPump2);
    EEPROM.get(500,minutesPump2); 
    EEPROM.get(600,secondsPump2);

    if(millis()-lastTime2 > 1000){
      secondsPump2++;    
      lastTime2 = millis();
      Serial.println(secondsPump2);
      EEPROM.put(600, secondsPump2);    
    }
    if(secondsPump2 > 60){
      minutesPump2++;
      secondsPump2 = 0;
      Serial.println(minutesPump2);
      EEPROM.put(600, secondsPump2);   
      EEPROM.put(500, minutesPump2);
    }
    if(minutesPump2 > 60){
      hoursPump2++;
      minutesPump2 = 0;
      Serial.println(minutesPump2);
      Serial.println(hoursPump2);
      EEPROM.put(400, hoursPump2);
      EEPROM.put(500, minutesPump2);
    }    
  }
  if ((millis() % lastIntervalTime) >= MEASURE_INTERVAL){ //Is it time for a new measurement?
     
    char dataString[20] = "";
    int count = 0;
    unsigned long rawTime;
    rawTime = getTime();
    Serial.print(rawTime); 
    int dayInt = day(rawTime);
    int monthInt = month(rawTime);
    int yearInt = year(rawTime);
    rawTime += adjustDstEurope(yearInt, monthInt, dayInt);   
    Serial.print(rawTime); 
     while((rawTime == 39 || rawTime < 1546300800) && (count < 12)){     //server seems to send 39 as an error code
      delay(5000);                              //we want to retry if this happens. I chose
      rawTime = getTime();                      //12 retries because I'm stubborn/persistent.
      count += 1;                               //NIST considers retry interval of <4s as DoS
    }                                           //attack, so fair warning.                                   //attack, so fair warning.
    
    if (rawTime != 39 || rawTime > 1546300800){                         //If that worked, and we have a real time
      
      //Decide if it's time to make a new file or not. Files are broken
      //up like this to keep loading times for each chart bearable.
      //Lots of string stuff happens to make a new filename if necessary.
      if (rawTime >= config.newFileTime){

        int dayInt = day(rawTime);
        int monthInt = month(rawTime);
        int yearInt = year(rawTime);
        Serial.println("______");
        Serial.print(yearInt);
        Serial.println("______");
        rawTime += adjustDstEurope(yearInt, monthInt, dayInt);
        char newFilename[18] = "";
        char dayStr[3];
        char monthStr[3];
        char yearStr[5];
        char subYear[3];
        strcat(newFilename,"data/");
        itoa(dayInt,dayStr,10);
        if (dayInt < 10){
          strcat(newFilename,"0");
        }
        strcat(newFilename,dayStr);
        strcat(newFilename,"-");
        itoa(monthInt,monthStr,10);
        if (monthInt < 10){
          strcat(newFilename,"0");
        }
        strcat(newFilename,monthStr);
        strcat(newFilename,"-");
        itoa(yearInt,yearStr,10);
        //we only want the last two digits of the year
        memcpy( subYear, &yearStr[2], 3 );
        strcat(newFilename,subYear);
        strcat(newFilename,".csv");
        
        //make sure we update our config variables:
        config.newFileTime += FILE_INTERVAL;
        strcpy(config.workingFilename,newFilename);
        //Write the changes to EEPROM. Bad things may happen if power is lost midway through,
        //but it's a small risk we take. Manual fix with EEPROM_config sketch can correct it.
        EEPROM_writeAnything(0, config); 
      }
        
      //get the values and setup the string we want to write to the file
        
      char timeStr[12];
      char sensorStr[6];
      int working1;
      int working2;
      
      
      ultoa(rawTime,timeStr,10);       
      strcat(dataString,timeStr);
      strcat(dataString,",");     
      
      //open the file we'll be writing to.
      File dataFile = SD.open(config.workingFilename, FILE_WRITE);
  
      // if the file is available, write to it:
      if (dataFile) {
        if(digitalRead(counterpin1) == 0){
          working1 = 1;
        }
        else{
          working1 = 0;
        }
        if(digitalRead(counterpin2) == 0){
          working2 = 1;
        }
        else{
          working2 = 0;
        }
        dataFile.print(dataString);       
        dataFile.print(working1);
        dataFile.print(",");
        dataFile.print(working2);
        dataFile.println();
        dataFile.close();
        // print to the serial port too:
        Serial.println(dataString);
        Serial.println(digitalRead(counterpin1));
        Serial.println(digitalRead(counterpin2));
      }  
      // if the file isn't open, pop up an error:
      else {
        Serial.println("Error opening datafile for writing");
      }
    }
    else{
      Serial.println("Couldn't resolve a time from the Ntp Server.");
    }
    //Update the time of the last measurment to the current timer value
    lastIntervalTime = millis();
  }
  else{
  Connection();
  }  
}
