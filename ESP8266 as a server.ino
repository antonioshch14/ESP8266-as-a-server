//working scetch of a server with OLED 64x128 that provides connection to 3 devices 
//bidirectional communication works well
//this is a new branch for the test reason


//#include <WiFiClient.h>
//#include <WiFi.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
//#include <ESP8266WebServer.h>
#include "DHTesp.h"
#include <WiFiUdp.h>
#include <FS.h>
#include "Network.h"
#include "Sys_Variables.h"
#include "CSS.h"
#include <SPI.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>


WiFiUDP UDP;                     // Create an instance of the WiFiUDP class to send and receive

IPAddress timeServerIP;          // time.nist.gov NTP server address
const char* NTPServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message

byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets


int     LED0 = 16;         // WIFI Module LED
int sendTriger = 5; // triger to send data to client 3
int sendTriger2 = 4; // triger to send data to client 2

bool sendBegin= false;
bool sendBegin2 = false;
//***************************************************************************************
  // Authentication Variables
char*       ssid;              // SERVER WIFI NAME
char*       password;          // SERVER PASSWORD

//------------------------------------------------------------------------------------
  // WiFi settings
#define     MAXSC     6           // MAXIMUM NUMBER OF CLIENTS

IPAddress APlocal_IP(192, 168, 4, 1);
IPAddress APgateway(192, 168, 4, 1);
IPAddress APsubnet(255, 255, 255, 0);
IPAddress IPdev1(192, 168, 4, 101);
IPAddress IPdev2(192, 168, 4, 102);
IPAddress IPdev3(192, 168, 4, 103);
IPAddress IPdev4(192, 168, 4, 104); //bathroom
unsigned int TCPPort = 2390;

WiFiServer  TCP_SERVER(TCPPort);      // THE SERVER AND THE PORT NUMBER
WiFiClient  TCP_Clients[MAXSC];        // THE SERVER CLIENTS Maximum number
//------------------------------------------------------------------------------------
  // Some Variables
char result[10];
float temp, humid;
//***********************************************************************************
unsigned int port=0;
DHTesp dht;

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, 12, 14);

void process_Message(String);
unsigned long now3 = millis();
unsigned long now4 = millis();//data timer sending to clients
int conected = 0;
unsigned long dev1 = 0, dev2 = 0, dev3 = 0, dev4 = 0;

bool started;
int Day=0, Time=0, Hour=0,Min=0,Sec=0;
//void getTime();
void getTime(int *Day,int *Hour,int*Min,int *Sec, unsigned long Now);
bool get_field_value(String Message, String field, unsigned long* value, int* index );//reads a value from fields of message
bool sentToClientNew(int Client, String data);//new send to client, sending data to a clinet, returns whther sending was succesfull
class Device{
public:
	int name=0;
	unsigned long time=0;
	int signal=0;
	bool connected=false;
	float temp = 0;
	float humid = 0;
	unsigned long lastRecieved = 0;
	int status = 0;
	float field1 = 0;//spare field, for bathroom used as humidAver
	String logFileName;
	bool newNameAssignedAfteShift;
};
class Timed { //time of messages from devices
public:
	int Day=0;
	int Hour=0;
	int Min=0;
	int Sec=0;
};
Timed Time1dev;
Timed Time2dev;
Timed Time3dev;
Timed Time4dev;
Device Device0; //server
Device Device1;
Device Device2;
Device Device3;
Device Device4; //bathroom

Device Device_undidentified;//assigneed for data if device name is unindentified
int devToShow = 0;//counter which device to show
float h = 0;
float t = 0;
long timeToCheckConnected = 100000;//time interval to check connected
void checkConnected();
class task {
public:
	unsigned long period;
	bool ignor=false;
	void reLoop() {
		taskLoop = millis();
	};
	bool check() {
		if (!ignor) {
			if (millis() - taskLoop > period) {
				taskLoop = millis();
				return true;
			}
		}
			return false;
	}
	void StartLoop(unsigned long shift) {
		taskLoop = millis()+ shift;
	}
	task(unsigned long t) {
		period = t;
	}
private:
	unsigned long taskLoop;

};
task task1(15000);//connection to NTP
task taskSendNTPrequest(100);//sending NTP request
//--------------------------NTP
task taskMemmoryClear(5000);//check and clear memmory
task checkconnection(60000);//check connected clients
unsigned long intervalNTP = 60000; // Request NTP time every minute
unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();
uint32_t timeUNIX = 0;
unsigned long prevActualTime = 0;
bool NTPconnected = false;
uint32_t actualTime;
//IPAddress staticIP(192, 168, 1, 22);

//IPAddress gateway(192, 168, 1, 1);
//
//IPAddress subnet(255, 255, 255, 0);

class TimeSet {
	int monthDays[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 }; // API starts months from 1, this array starts from 0
	int *shift;
	time_t lastUpdate;//time when the time was updated, used for current time calculation
	time_t actual_Time;
	time_t UNIXtime;
public:
	int NowMonth;
	int NowYear;
	int NowDay;
	int NowWeekDay;
	int lastShift;
	int NowHour;
	int NowMin;
	int NowSec;
	int Now10Min;
	String FileNewManeParameter; //defines file name assingnement whether: min, 10min, hour, day
	int sec() {
		updateCurrecnt();
		return actual_Time % 60;
	};
	int min() {
		updateCurrecnt();
		return actual_Time / 60 % 60;
	};
	int hour() {
		updateCurrecnt();
		return actual_Time / 3600 % 24;
	};

	time_t SetCurrentTime(time_t timeToSet) { //call each time after recieving updated time
		lastUpdate = millis();
		UNIXtime = timeToSet;
	}//call each time after recieving updated time
	void updateDay() {
		updateCurrecnt();
		breakTime(&NowYear, &NowMonth, &NowDay, &NowWeekDay);
		Serial.println(String(NowYear) + ":" + String(NowMonth) + ":" + String(NowDay) + ":" + String(NowWeekDay));
	}// call in setup and each time when new day comes 
	bool Shift() {  //check whether there is a new day comes, first call causes alignement of tracked and checked day 
		//Serial.println("lastShift " + String(lastShift) + "  *shift " + String(*shift));
		if (lastShift != *shift) {
			lastShift = *shift;
			return true;
		}
		else return false;
	} //call to check whether the day is changed
	void begin(int shiftType=1) //1-day,2-hiur,3-10 minutes
	{
		switch (shiftType)
		{
		case 1:shift = &NowDay;
			FileNewManeParameter = "day";
			break;
		case 2:shift = &NowHour;
			FileNewManeParameter = "hour";
			break;
		case 3:shift = &Now10Min;
			FileNewManeParameter = "10min";
			break;
		case 4:shift = &NowMin;
			FileNewManeParameter = "min";
			break;
		default:shift = &NowDay;
			FileNewManeParameter = "day";
			break;
		}
	};
private:

	void updateCurrecnt() {
		actual_Time = UNIXtime + (millis() - lastUpdate) / 1000;

	}

	bool LEAP_YEAR(time_t Y) {
		//((1970 + (Y)) > 0) && !((1970 + (Y)) % 4) && (((1970 + (Y)) % 100) || !((1970 + (Y)) % 400)) ;
		if ((1970 + (Y)) > 0) {
			if ((1970 + (Y)) % 4 == 0) {
				if ((((1970 + (Y)) % 100) != 0) || (((1970 + (Y)) % 400) == 0)) return true;
			}
			else return false;
		}
		else return false;
	}

	void breakTime(int *Year, int *Month, int *day, int *week) {
		// break the given time_t into time components
		// this is a more compact version of the C library localtime function
		// note that year is offset from 1970 !!!

		int year;
		int month, monthLength;
		uint32_t time;
		unsigned long days;

		time = (uint32_t)actual_Time;
		NowSec = time % 60;
		time /= 60; // now it is minutes
		NowMin = time % 60;
		Now10Min = time % 600;
		time /= 60; // now it is hours
		NowHour = time % 24;
		time /= 24; // now it is days
		*week = int(((time + 4) % 7));  // Monday is day 1 

		year = 0;
		days = 0;
		while ((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
			year++;
		}
		*Year = year + 1970; // year is offset from 1970 

		days -= LEAP_YEAR(year) ? 366 : 365;
		time -= days; // now it is days in this year, starting at 0

		days = 0;
		month = 0;
		monthLength = 0;
		for (month = 0; month < 12; month++) {
			if (month == 1) { // february
				if (LEAP_YEAR(year)) {
					monthLength = 29;
				}
				else {
					monthLength = 28;
				}
			}
			else {
				monthLength = monthDays[month];
			}

			if (time >= monthLength) {
				time -= monthLength;
			}
			else {
				break;
			}
		}
		*Month = month + 1;  // jan is month 1  
		*day = time + 1;     // day of month
	}
};
TimeSet Time_set;
String fileName = "/temp.csv";
File fsUploadFile;                                    // a File variable to temporarily store the received file
task writeToLog(10000);
class fileSt {
	
public:
	String name;
	unsigned long nameFirst;
	String size;
	bool checked;//true if the file checked on the number
	unsigned long number_csv;
	void checkNameOnNumber( String beginer, String end) {//check a file on name that is a 
	//number: beginer, end, there to save if 0 to name, if 1 to nameFirst
		
		int fieldBegin = name.indexOf(beginer) + beginer.length()-1;
		int check_field = name.indexOf(beginer);
		int filedEnd = name.indexOf(end);
		number_csv = 0;
		if (check_field != -1 && filedEnd != -1) {
			for (int i = filedEnd - 1; i > fieldBegin; i--) {
				char ch = name[i];
				if (isDigit(ch)) {
					int val = ch - 48;
					number_csv += ((val * pow(10, filedEnd - 1 - i)));
				}
				if (number_csv > 4294967293)break;
			}
			checked = false;
		}
		else {
			//Serial.println("Fail to find a begginer or end");
			checked = true;
		}
		//Serial.println(" *numbrPtr= " +String(*numbrPtr)+ String(number_csv));
	}
};

int filesStoredIndex;
const int maxFiles = 20;
fileSt filesStored[maxFiles];
ESP8266WebServer server(80);             // create a web server on port 80
byte packetBuffer[NTP_PACKET_SIZE];      // A buffer to hold incoming and outgoing packets
const char* mdnsName = "esp8266";
unsigned long  usedBytes;
void memoryStatus() {

	uint32_t realSize = ESP.getFlashChipRealSize();
	uint32_t ideSize = ESP.getFlashChipSize();
	FlashMode_t ideMode = ESP.getFlashChipMode();

	Serial.printf("Flash real id:   %08X\n", ESP.getFlashChipId());
	Serial.printf("Flash real size: %u bytes\n\n", realSize);

	Serial.printf("Flash ide  size: %u bytes\n", ideSize);
	Serial.printf("Flash ide speed: %u Hz\n", ESP.getFlashChipSpeed());
	Serial.printf("Flash ide mode:  %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));

	if (ideSize != realSize) {
		Serial.println("Flash Chip configuration wrong!\n");
	}
	else {
		Serial.println("Flash Chip configuration ok.\n");
	}
	FSInfo sppifs;
	SPIFFS.info(sppifs);
	unsigned long totalBytes = sppifs.totalBytes;
	Device0.time = sppifs.usedBytes;
	Serial.printf("totalBytes: %u \n", totalBytes);
	Serial.printf("usedBytes: %u \n", Device0.time);
	Serial.printf("blockSize: %u \n", sppifs.blockSize);
	Serial.printf("pageSize: %u \n", sppifs.pageSize);
	Serial.printf("maxOpenFiles: %u \n", sppifs.maxOpenFiles);
	Serial.printf("maxPathLength: %u \n", sppifs.maxPathLength);
	if (100 * (totalBytes - Device0.time) / totalBytes < 47)if (deleteFile(findOldest())) refreshSPIFS();
}

void setup(void) {
	Serial.begin(115200);
  //dht.begin();
  dht.setup(0, DHTesp::DHT11); 
  u8g2.begin();
  u8g2.setFont(u8g2_font_crox1c_tf);
  delay(700);
  SetWifi("DataTransfer","BelovSer");// setting up a Wifi AccessPoint
  pinMode(LED0, INPUT);
  pinMode(sendTriger, INPUT);
  pinMode(sendTriger2, INPUT);
  Serial.setDebugOutput(true);
  //---------------------------------
  startWiFi();                   // Try to connect to some given access points. Then wait for a connection
  startUDP();
  SPIFFS.begin();
  checkFileOverFloodAndDelete();              // Start the SPIFFS and list all contents

  startMDNS();                 // Start the mDNS responder
  Time_set.begin(1);               //start time 1-day,2-hour,3-10 min,4-min

  server.on("/", HomePage);
  server.on("/download", File_Download);
  server.on("/Delete", File_Delete);
  server.begin();
  
}
void startWiFi() { // Try to connect to some given access points. Then wait for a connection
	
	char ssid[] = "Keenetic-4574"; //  Your network SSID (name)
	char pass[] = "Gfmsd45kaxu69$"; // Your network password
	WiFi.begin(ssid, pass);
	WiFiClient client; // Initialize the WiFi client library
	Serial.printf("Connection %s to begin \n\r", ssid);
}
void stopWiFi() {
	WiFi.disconnect();
	Serial.println("Disconnect from WiFi router ");
}
//----------------------NTP--------------
void startUDP() {
	Serial.println("Starting UDP");
	UDP.begin(123);                          // Start listening for UDP messages on port 123
	Serial.print("Local port:\t");
	Serial.println(UDP.localPort());
	Serial.println();
}
uint32_t getTime() {
	if (UDP.parsePacket() == 0) { // If there's no response (yet)
		return 0;
	}
	UDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
	// Combine the 4 timestamp bytes into one 32-bit number
	uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
	// Convert NTP time to a UNIX timestamp:
	// Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
	const uint32_t seventyYears = 2208988800UL;
	// subtract seventy years:
	uint32_t UNIXTime = NTPTime - seventyYears+60*60*3;
	return UNIXTime;
}
/*
void sendNTPpacket(IPAddress& address) {
	memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
	
	NTPBuffer[0] = 0b11100011;   // LI, Version, Mode 
	NTPBuffer[1] = 0;     // Stratum, or type of clock
	NTPBuffer[2] = 6;     // Polling Interval
	NTPBuffer[3] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	NTPBuffer[12] = 49;
	NTPBuffer[13] = 0x4E;
	NTPBuffer[14] = 49;
	NTPBuffer[15] = 52;
	UDP.beginPacket(address, 123); // NTP requests are to port 123
	UDP.write(NTPBuffer, NTP_PACKET_SIZE);
	UDP.endPacket();
}
*/
inline int getSeconds(uint32_t UNIXTime) {
	return UNIXTime % 60;
}
inline int getMinutes(uint32_t UNIXTime) {
	return UNIXTime / 60 % 60;
}
inline int getHours(uint32_t UNIXTime) {
	return UNIXTime / 3600 % 24;
}
void getTime(int *Day, int *Hour, int*Min, int *Sec,unsigned long Now) {
	*Day = (Now / (1000 * 60 * 60 * 24));
	Now = Now - (*Day*(1000 * 60 * 60 *24));
	*Hour = (Now / (1000 * 60 * 60));
	Now = Now - (*Hour*(1000 * 60 * 60 ));
	*Min = (Now / (1000 * 60));
	Now = Now - (*Min*(1000*60 ));
	*Sec = (Now / (1000));

}
void display() {
	{
		String con = "";
		String time = "";
		now3 = millis();
		u8g2.clearBuffer();
		u8g2.drawStr(0, 15, "lt");
		u8g2.drawStr(0, 32, "lh");
		u8g2.setCursor(20, 32);
		u8g2.print(h);
		u8g2.setCursor(20, 15);
		u8g2.print(t);
		u8g2.drawStr(67, 15, "gt");
		u8g2.drawStr(67, 32, "gh");
		
		if (devToShow == 0) {			
				getTime(&Day, &Hour, &Min, &Sec, millis());
				time = "d" + String(Day) + "h" + String(Hour) + "m" + String(Min) + "s" + String(Sec) + "con " + String(conected);
				unsigned long total = dev1 + dev2 + dev3 + dev4;
				con = String(getHours(actualTime))+":"+ String(getMinutes(actualTime))+":"+ String(getSeconds(actualTime))+ "m" + String(total);
		}
		if (devToShow == 1) {
			if (Device1.connected) {
				getTime(&Time1dev.Day, &Time1dev.Hour, &Time1dev.Min, &Time1dev.Sec, Device1.time);
				time = "d" + String(Time1dev.Day) + "h" + String(Time1dev.Hour) + "m" + String(Time1dev.Min) + "s" + String(Time1dev.Sec);
				con = "1Str " + String(Device1.signal) + "." + String(dev1);
				u8g2.setCursor(85, 15);
				u8g2.print(Device1.temp);
				u8g2.setCursor(85, 32);
				u8g2.print(Device1.humid);
			}
			else devToShow++;
		}
		if (devToShow == 2) {
			if (Device2.connected) {
				getTime(&Time2dev.Day, &Time2dev.Hour, &Time2dev.Min, &Time2dev.Sec, Device2.time);
				time = "d" + String(Time2dev.Day) + "h" + String(Time2dev.Hour) + "m" + String(Time2dev.Min) + "s" + String(Time2dev.Sec);
				con = "2Str " + String(Device2.signal) + "." + String(dev2);
				u8g2.setCursor(85, 15);
				u8g2.print(Device2.temp);
				u8g2.setCursor(85, 32);
				u8g2.print(Device2.humid);
			}
			else devToShow++;
		}
		if (devToShow == 3){
			if (Device3.connected) {
				getTime(&Time3dev.Day, &Time3dev.Hour, &Time3dev.Min, &Time3dev.Sec, Device3.time);
				time = "d" + String(Time3dev.Day) + "h" + String(Time3dev.Hour) + "m" + String(Time3dev.Min) + "s" + String(Time3dev.Sec);
				con = "3Str " + String(Device3.signal) + "." + String(dev3);
				u8g2.setCursor(85, 15);
				u8g2.print(Device3.temp);
				u8g2.setCursor(85, 32);
				u8g2.print(Device3.humid);
			}
			else devToShow ++;
		}
		if (devToShow == 4) {
			if (Device4.connected) {
				getTime(&Time4dev.Day, &Time4dev.Hour, &Time4dev.Min, &Time4dev.Sec, Device4.time);
				time = "d" + String(Time4dev.Day) + "h" + String(Time4dev.Hour) + "m" + String(Time4dev.Min) + "s" + String(Time4dev.Sec);
				con = "4D" + String(Device4.signal) + "|" + String(dev4)+"|"+String(Device4.status)+ "|" + String(Device4.field1);
				u8g2.setCursor(85, 15);
				u8g2.print(Device4.temp);
				u8g2.setCursor(85, 32);
				u8g2.print(Device4.humid);
			}
			devToShow=0;
		}
		else devToShow++;
		u8g2.setCursor(0, 46);
		u8g2.print(con);
		u8g2.setCursor(0, 61);
		u8g2.print(time);
		u8g2.sendBuffer();

	}
}
bool connectToNtp() {// replies true if lookup is Ok, otherwise false
	if (WiFi.hostByName(NTPServerName, timeServerIP)) { // Get the IP address of the NTP server

		Serial.print("Time server IP:\t");
		Serial.println(timeServerIP);

		Serial.println("\r\nSending NTP request ...");
		sendNTPpacket(timeServerIP);
		return true;
	}
	Serial.println("DNS lookup failed.");
	startUDP();
	return false;
}
void loop(void) {
	if (task1.check()) {Serial.println("Check() WiFi connection ");
		if (WiFi.status() == WL_CONNECTED && !NTPconnected) {
			Serial.println("WL_CONNECTED");
			NTPconnected = connectToNtp(); // replies true if lookup is Ok, otherwise false
			task1.ignor = NTPconnected;
		}
		else {
			stopWiFi();
			startWiFi();
		}
	}
	if (NTPconnected) {
		unsigned long currentMillis = millis();

		if (taskSendNTPrequest.check()) { // If a minute has passed since last NTP request
			//prevNTP = currentMillis;
			Serial.println("\r\nSending NTP request ...");
			if (WiFi.status() != WL_CONNECTED) { //initiate time obtaining again
				startWiFi(); //connect back to WiFi router
				task1.ignor = false;//Start task1 again
				NTPconnected = false;
			}
			sendNTPpacket(timeServerIP);               // Send an NTP request
			taskSendNTPrequest.period = 100;//hurry up to obtain time from NTP
		}
		yield();
		uint32_t time = getTime();                   // Check if an NTP response has arrived and get the (UNIX) time
		if (time) {                                  // If a new timestamp has been received
			Serial.println("time " + String(time));
			timeUNIX = time;
			Serial.print("NTP response:\t");
			Serial.println(timeUNIX);
			lastNTPResponse = currentMillis;
			//taskSendNTPrequest.period=1000*60*3;//test for each 3 minutes
			taskSendNTPrequest.period=1000*60*60*24;//slow down requestes when the time is obtained
			Time_set.SetCurrentTime(timeUNIX);
			stopWiFi();
		}

		actualTime = timeUNIX + (currentMillis - lastNTPResponse) / 1000;
		
	}
	//-----------------------END NTP--------------------
	server.handleClient();
	yield();
	HandleClients();
	if (millis() > (now3 + 2000)) {
		 h = dht.getHumidity();
		 t = dht.getTemperature();
		 Device0.temp = t;
		 Device0.humid = h;
		display();
	}
	
	/*while(digitalRead(LED0) == HIGH){
		if(!started)Serial.print("reading stopped  ");
		started = true;
		delay(500);
		Serial.print(".");
	}
		if (started) {
			Serial.println("reading began"); started = false;
		}*/
		if (Device3.connected) {
			if (!sendBegin && digitalRead(sendTriger) == HIGH) {

				if (!sentToClientNew(2, "ON")) {
					Serial.println("Client 3 not connected for data read ");
				}
				else sendBegin = true;
			}
			else if (sendBegin && digitalRead(sendTriger) == LOW) {

				if (sentToClientNew(2, "OFF"))sendBegin = false;
			}
		}
		if (Device2.connected) {
			if (!sendBegin2 && digitalRead(sendTriger2) == HIGH) {

				if (!sentToClientNew(1, "ON")) {
					Serial.println("Client 2 not connected for data read ");
				}
				else sendBegin2 = true;
			}
			else if (sendBegin2 && digitalRead(sendTriger2) == LOW) {

				if (sentToClientNew(1, "OFF"))sendBegin2 = false;
			}
		}

		if (millis() > (now4 + 2000)) {//send continues messages to clients
			now4 = millis();
			if (Device4.connected) {
				sentToClientNew(3, "humid:"+String(h+7)+";");
			}
		}
			/*
			if (Device2.connected) {
				sentToClientNew(1, String(millis()));
			}
			if (Device3.connected) {
				sentToClientNew(2, String(millis()));
			}
		}
		*/
		if (checkconnection.check()) { if (conected > 0) checkConnected();
							conected = WiFi.softAPgetStationNum();
							}
		if (timeUNIX && writeToLog.check()) logDeviceGotData(0);
		if (Serial.available()) {// read serial for command
			String SerialCommand = Serial.readStringUntil('\r');
			if (SerialCommand == "memmory") memoryStatus();
			else if (SerialCommand == "list") startSPIFFS();
			else if (SerialCommand == "format") format();
			else Serial.println("Print: memmory, list");
		}
		if (taskMemmoryClear.check()) {
			memoryStatus();
			taskMemmoryClear.period=60 * 1000;
		}
}
void format() {
	if (SPIFFS.format())  Serial.println("fs formated");
	else Serial.println("fs formate failed");
	memoryStatus();
	startSPIFFS();
}
void checkConnected() {//check the real status of sent message
	unsigned long Tnow = millis();
	Device*Deviceptr;
	for (int i = 0; i < 4; i++) {
		if (i == 0)Deviceptr = &Device1;
		else if (i == 1)Deviceptr = &Device2;
		else if (i == 2)Deviceptr = &Device3;
		else if (i == 3)Deviceptr = &Device4;
		if (Deviceptr->lastRecieved + timeToCheckConnected < Tnow) {
			Deviceptr->connected = false;
			TCP_Clients[i].flush();
			TCP_Clients[i].stop();
			Serial.println("disconected  "+String(i));
		}
		else Deviceptr->connected = true;
	}
}
void SetWifi(char* Name, char* Password) {
	// Stop any previous WIFI
	//WiFi.disconnect();

	// Setting The Wifi Mode
	WiFi.mode(WIFI_AP_STA);
	Serial.println("WIFI Mode : AccessPoint");

	// Setting the AccessPoint name & password
	ssid = Name;
	password = Password;

	// Starting the access point
	WiFi.softAPConfig(APlocal_IP, APgateway, APsubnet);                 // softAPConfig (local_ip, gateway, subnet)
	WiFi.softAP(ssid, password, 9, 0, 8);                           // WiFi.softAP(ssid, password, channel, hidden, max_connection)     
	Serial.println("WIFI < " + String(ssid) + " > ... Started");
	Serial.println("password: " + String(password));
	// wait a bit
	delay(50);

	// getting server IP
	IPAddress IP = WiFi.softAPIP();

	// printing the server IP address
	Serial.print("AccessPoint IP : ");
	Serial.println(IP);

	// starting server
	TCP_SERVER.begin();                                                 // which means basically WiFiServer(TCPPort);

	Serial.println("Server Started");
}


bool sentToClientNew(int Client, String data) {
	if (TCP_Clients[Client].connected()) {

	TCP_Clients[Client].setNoDelay(1);
		TCP_Clients[Client].println(data);
		Serial.println("data stent " + data);
		return true;
	}
	else return false;
}
//====================================================================================

void HandleClients() {

	unsigned long tNow;
	unsigned long tNow2 = millis();
	if (TCP_SERVER.hasClient()) {
		conected = WiFi.softAPgetStationNum();
		Device0.field1 = conected;// to store in log file
		Serial.printf("Stations connected to soft-AP = %d\n", conected);
		WiFiClient Temp= TCP_SERVER.available();
		IPAddress IP = Temp.remoteIP();
		bool Nown = false;
		if (IP == IPdev1) {
			if (!Device1.connected)	Device1.connected = true;
			else Nown = true;
			TCP_Clients[0] = Temp;
			TCP_Clients[0].setNoDelay(1);
		}
		else if (IP == IPdev2) {
			if (!Device2.connected)	Device2.connected = true;
			else Nown = true;
			TCP_Clients[1] = Temp;
			TCP_Clients[1].setNoDelay(1);
		}
		else if (IP == IPdev3) {
			if (!Device3.connected)	Device3.connected = true;
			else Nown = true;
			TCP_Clients[2] = Temp;
			TCP_Clients[2].setNoDelay(1);
		}
		else if (IP == IPdev4) {
			if (!Device4.connected)	Device4.connected = true;
			else Nown = true;
			TCP_Clients[3] = Temp;
			TCP_Clients[3].setNoDelay(1);
		}
		
		//String readMess = Temp.readStringUntil('\r\n');
		String readMess = Temp.readStringUntil('\n');
		Serial.println(" First message of a new client : " + readMess);
		
			
	}
	
	String Message;
	yield();
	for(int i=0;i<4;++i){
		if (TCP_Clients[i].connected() && TCP_Clients[i].available()) {
			Message = TCP_Clients[i].readStringUntil('\n');
			Serial.print(" Content: ");
			Serial.println(Message);
			new_process_Msessage(Message);
			break;
		}
		
	}

			
			                          
}
	

void new_process_Msessage(String Message) {
	int action = 0;;//1- get time,
	int device = 0,index=0;
	unsigned long value;
	Device*Deviceptr= &Device_undidentified;
	if (get_field_value(Message, "Device:", &value, &index)) {
		device=int(value);
	
	switch (device) {// conunt message recieved from clients
	case 1: Deviceptr=&Device1; dev1++;   break;
	case 2: Deviceptr = &Device2; dev2++;   break;
	case 3: Deviceptr = &Device3;  dev3++; break;
	case 4: Deviceptr = &Device4;  dev4++; break;
	default:	break;
	}
	Deviceptr->name = value;
	}
	Deviceptr->lastRecieved = millis();
	if (get_field_value(Message, "get:", &value, &index)) {
		action = value;
	}
	if (!action) {
		if (get_field_value(Message, "time:", &value, &index)) {
			Deviceptr->time = value;
		}
		if (get_field_value(Message, "signal:", &value, &index)) {
			Deviceptr->signal = int(value);
		}
		if (get_field_value(Message, "temp:", &value, &index)) {//index means that the value is float and it determens numbers after ','
			Deviceptr->temp = value / pow(10, index);
		}
		if (get_field_value(Message, "humid:", &value, &index)) {
			Deviceptr->humid = value / pow(10, index);
		}
		if (Deviceptr->name == 4) {
			if (get_field_value(Message, "humidAv:", &value, &index)) {
				Deviceptr->field1 = int(value / pow(10, index));
			}
			if (get_field_value(Message, "status:", &value, &index)) {
				Deviceptr->status = int(value / pow(10, index));
			}
		}
		logDeviceGotData(Deviceptr->name);
	}
	switch (action)
	{
	case 1: sentToClientNew(device,"time:"+String(actualTime)+";");
		break;
	default:
		break;
	}
	//Serial.println("device: " + String(Deviceptr->name) + " time: " + String(Deviceptr->time) +
	//	" signal: " + String(Deviceptr->signal));
}
void logDeviceGotData(int deviceDataGot) {
	bool assignNewName = false;
	if (timeUNIX ) {
		Device*Deviceptr ;
			switch (deviceDataGot) {
			case 0: Deviceptr = &Device0;    break;
			case 1: Deviceptr = &Device1;    break;
			case 2: Deviceptr = &Device2;    break;
			case 3: Deviceptr = &Device3;   break;
			case 4: Deviceptr = &Device4;   break;
			default:	Deviceptr = &Device_undidentified; break;
			}
		Time_set.updateDay();
		Serial.println("current time" + String(Time_set.NowYear) + String(Time_set.NowMonth) +
			String(Time_set.NowDay) + String(Time_set.NowHour) + String(Time_set.NowMin) + String(Time_set.NowSec));
		String Time = String(Time_set.NowHour) + ":" + String(Time_set.NowMin) + ":" + String(Time_set.NowSec);


		Serial.println(temp + String("  ") + humid);
		String Fmonth, Fday, Fhour, Fmin;
		if (Time_set.Shift()) {
			Device0.newNameAssignedAfteShift = false;
			Device1.newNameAssignedAfteShift = false;
			Device2.newNameAssignedAfteShift = false;
			Device3.newNameAssignedAfteShift = false;
			Device4.newNameAssignedAfteShift = false;
			Serial.println("Time shitf occured");
		}
		if (!Deviceptr->newNameAssignedAfteShift) {
			Deviceptr->newNameAssignedAfteShift = true;
			if (Time_set.NowMonth < 10)Fmonth = "0" + String(Time_set.NowMonth);
			else Fmonth = String(Time_set.NowMonth);
			if (Time_set.NowDay < 10)Fday = "0" + String(Time_set.NowDay);
			else Fday = String(Time_set.NowDay);
			if (Time_set.FileNewManeParameter != "day") {  //defines file name assingnement whether: min, 10min, hour, day
				if (Time_set.NowHour < 10)Fhour = "0" + String(Time_set.NowHour);
				else Fhour = String(Time_set.NowHour);
				if (Time_set.FileNewManeParameter != "hour") {
					if (Time_set.NowMin < 10)Fmin = "0" + String(Time_set.NowMin);
					else Fmin = String(Time_set.NowMin);
					if (Time_set.FileNewManeParameter != "10min") {
						Fmin = Fmin.substring(0, 1);
					}
				}
			}
			int fileLatest = findLatest(deviceDataGot);//find latest log file of such device
			if (fileLatest >=0 && fileLatest <=maxFiles) {
				Serial.printf("fileLatest = %d \n\r", fileLatest);
				//filesStored[fileLatest].checkNameOnNumber("/","_",1); 
				//name.indexOf(beginer) + beginer.length()-1;
				String fileNemeTemp = filesStored[fileLatest].name;
				String cuttedName = fileNemeTemp.substring(fileNemeTemp.indexOf("/")+1, fileNemeTemp.indexOf("_"));
				Serial.println("file found"+ cuttedName +"  file comoared to "+ String(deviceDataGot) + String(Time_set.NowYear) + Fmonth + Fday +
					Fhour + Fmin);
				if (cuttedName == String(deviceDataGot) + String(Time_set.NowYear) + Fmonth + Fday +
					Fhour + Fmin) {
					Deviceptr->logFileName = filesStored[fileLatest].name;
					Serial.println(" find existing file to store Deviceptr->logFileName: " + Deviceptr->logFileName);
				}
				else {
					assignNewName = true;
					Serial.println(" file not found");
				}
			}
			else assignNewName = true;
			if (assignNewName){
				Deviceptr->logFileName = "/" + String(deviceDataGot) + String(Time_set.NowYear) + Fmonth + Fday +
					Fhour + Fmin + '_' + String(actualTime) + ".csv";
				Serial.println("Appending temperature to file: " + Deviceptr->logFileName);
				File tempLog = SPIFFS.open(Deviceptr->logFileName, "a"); // Write the time and the temperature to the csv file
				tempLog.print(Time);
				tempLog.println(",DeviceTime,Signal,Temp,Humid,Status,Field1");
				tempLog.close();
				checkFileOverFloodAndDelete();               // Start the SPIFFS and list all contents
			}
		}
		//Serial.println("Appending temperature to file: " + Deviceptr->logFileName);
		File tempLog = SPIFFS.open(Deviceptr->logFileName, "a"); // Write the time and the temperature to the csv file
		tempLog.print(Time);
		tempLog.print(',');
		tempLog.print(Deviceptr->time);
		tempLog.print(',');
		tempLog.print(Deviceptr->signal);
		tempLog.print(',');
		tempLog.print(Deviceptr->temp);
		tempLog.print(',');
		tempLog.print(Deviceptr->humid);
		tempLog.print(',');
		tempLog.print(Deviceptr ->status);
		tempLog.print(',');
		tempLog.println(Deviceptr->field1);
		tempLog.close();

	}
}
bool get_field_value(String Message, String field, unsigned long* value,int* index) {
	int fieldBegin = Message.indexOf(field)+ field.length();
	int check_field = Message.indexOf(field);
	int ii = 0;
	*value = 0;
	*index = 0;
	bool indFloat = false;
	if (check_field != -1) {
		int filedEnd = Message.indexOf(';', fieldBegin);
		if (filedEnd == -1) { return false; }
		int i = 1;
		char ch = Message[filedEnd - i];
		while (ch != ' ' && ch != ':') {
			if (isDigit(ch)) {
				int val= ch - 48;
				if (!indFloat)ii = i-1;
				else ii = i-2;
				*value = *value +((val * pow(10,ii)));
			}
			else if (ch == '.') { *index = i-1; indFloat = true; }
			i++;
			if (i > (filedEnd- fieldBegin+1)||i>10)break;
			ch = Message[filedEnd - i];
		}
		
	}
	else return false;
	return true;
}
	/*void process_Message(String Message) {
		if (Message[0] == 'D') { new_process_Msessage(Message); return;
		}

		bool tempEnd = false;
		
		int length = Message.length();
		int SensorDataBegining = Message.indexOf('{');
		if (SensorDataBegining == -1) { return; }
		humid = "";
		temp = "";
		for (int i = SensorDataBegining+1;i<length;++i){
			if (Message.charAt(i) == '}')break;
			if (Message.charAt(i) == ',') {
				tempEnd = true; continue;
			}
			if (!tempEnd) temp += Message.charAt(i);
			else humid += Message.charAt(i);
		}
		temp.remove(4);
		humid.remove(4);
		

		
	}
*/
String findOldest() {
	unsigned long min = 4294967293;
	int indexOfMin = 0;
	bool found = false;
	for (int i = 0; i <= maxFiles - 1; i++) {
		//	Serial.printf("min %d <= filesStored[i].number_csv %d && !filesStored[i].checked %b",
		//		min, filesStored[i].number_csv); Serial.println(filesStored[i].checked);
		if (filesStored[i].number_csv && min >= filesStored[i].number_csv && !filesStored[i].checked) {
			indexOfMin = i;
			min = filesStored[i].number_csv;
			found = true;
		}
	}

	Serial.printf(" File name %s position %d\n\r", filesStored[indexOfMin].name.c_str(), indexOfMin);
	if (found) {
		filesStored[indexOfMin].checked = true;
		return filesStored[indexOfMin].name;
	}
	else return "Not_found";
};
int findLatest(int Dev) {
	unsigned long max = 0;
	int indexOfMax = 0;
	String fileName;
	bool found = false;
	for (int i = 0; i <maxFiles ; i++) {
		//Serial.printf("max %d <= filesStored[i].number_csv %d ",
		//	max, filesStored[i].number_csv); Serial.println(filesStored[i].checked);
		if (filesStored[i].number_csv && max <= filesStored[i].number_csv && !filesStored[i].checked) {
		//	Serial.println("filesStored[i].name: "+ filesStored[i].name);
			//fileName = filesStored[i].name;
			//charDev = fileName[1] ;
			//Serial.println("filesStored[i].name[1] = "+ charDev);
			if (filesStored[i].name[1]  == Dev+48) {
				indexOfMax = i;
				max = filesStored[i].number_csv;
				found = true;
			}
		}
	}

	Serial.printf(" File latest %d device has name %s position %d\n\r", Dev,filesStored[indexOfMax].name.c_str(), indexOfMax);
	if (found) {
		filesStored[indexOfMax].checked = true;
		return indexOfMax;
	}
	else return -1;
};
void checkFileOverFloodAndDelete() {
	while (!startSPIFFS()) {
		if (deleteFile(findOldest())) refreshSPIFS();
		else break;
	};               // Start the SPIFFS and list all contents
}
bool startSPIFFS() { // Start the SPIFFS and list all contents
	//SPIFFS.begin();                             // Start the SPI Flash File System (SPIFFS)
	filesStoredIndex = -1;
	Serial.println("SPIFFS started. Contents:");
	{
		Dir dir = SPIFFS.openDir("/");
		while (dir.next()) {                      // List the file system contents
			filesStoredIndex++;
			String fileName = dir.fileName();
			size_t fileSize = dir.fileSize();
			Serial.printf("\tFS File: %s, size: %s  ", fileName.c_str(), formatBytes(fileSize).c_str()); //\r\n
			if (filesStoredIndex < maxFiles) {
				filesStored[filesStoredIndex].name = fileName;
				filesStored[filesStoredIndex].size = formatBytes(fileSize);
				filesStored[filesStoredIndex].checkNameOnNumber("_", ".csv");
				Serial.print(filesStored[filesStoredIndex].number_csv);
				Serial.printf(" filesStoredIndex %d \r\n", filesStoredIndex);
			}
		}
		Serial.printf("\n");
	}
	SPIFFS.gc();//Performs a quick garbage collection operation on SPIFFS, possibly making writes perform faster/better in the future.
	Serial.printf("filesStoredIndex = %d >= maxFiles = %d \n\r", filesStoredIndex, maxFiles);
	if (filesStoredIndex >= maxFiles)return false;
	else return true;
}

void startMDNS() { // Start the mDNS responder
	MDNS.begin(mdnsName);                        // start the multicast domain name server
	Serial.print("mDNS responder started: http://");
	Serial.print(mdnsName);
	Serial.println(".local");
}

void startServer() { // Start a HTTP server with a file read handler and an upload handler
	server.on("/edit.html", HTTP_POST, []() {  // If a POST request is sent to the /edit.html address,
		server.send(200, "text/plain", "");
	}, handleFileUpload);                       // go to 'handleFileUpload'

	server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'
	// and check if the file exists

	//server.begin();                             // start the HTTP server
	Serial.println("HTTP server started.");
}

/*__________________________________________________________SERVER_HANDLERS__________________________________________________________*/

void handleNotFound() { // if the requested file or page doesn't exist, return a 404 not found error
	if (!handleFileRead(server.uri())) {        // check if the file exists in the flash memory (SPIFFS), if so, send it
		server.send(404, "text/plain", "404: File Not Found");
	}
}
bool handleFileRead(String path) { // send the right file to the client (if it exists)
	Serial.println("handleFileRead: " + path);
	if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
	String contentType = getContentType(path);             // Get the MIME type
	String pathWithGz = path + ".gz";
	if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
		if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
			path += ".gz";                                         // Use the compressed verion
		File file = SPIFFS.open(path, "r");                    // Open the file
		size_t sent = server.streamFile(file, contentType);    // Send it to the client

		file.close();                                          // Close the file again
		Serial.println(String("\tSent file: ") + path);
		return true;
	}
	Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
	return false;
}
void handleFileUpload() { // upload a new file to the SPIFFS
	HTTPUpload& upload = server.upload();
	String path;
	if (upload.status == UPLOAD_FILE_START) {
		path = upload.filename;
		if (!path.startsWith("/")) path = "/" + path;
		if (!path.endsWith(".gz")) {                         // The file server always prefers a compressed version of a file
			String pathWithGz = path + ".gz";                  // So if an uploaded file is not compressed, the existing compressed
			if (SPIFFS.exists(pathWithGz))                     // version of that file must be deleted (if it exists)
				SPIFFS.remove(pathWithGz);
		}
		Serial.print("handleFileUpload Name: "); Serial.println(path);
		fsUploadFile = SPIFFS.open(path, "w");               // Open the file for writing in SPIFFS (create if it doesn't exist)
		path = String();
	}
	else if (upload.status == UPLOAD_FILE_WRITE) {
		if (fsUploadFile)
			fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
	}
	else if (upload.status == UPLOAD_FILE_END) {
		if (fsUploadFile) {                                   // If the file was successfully created
			fsUploadFile.close();                               // Close the file again
			Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
			server.sendHeader("Location", "/success.html");     // Redirect the client to the success page
			server.send(303);
		}
		else {
			server.send(500, "text/plain", "500: couldn't create file");
		}
	}
}
/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/
String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
	if (bytes < 1024) {
		return String(bytes) + "B";
	}
	else if (bytes < (1024 * 1024)) {
		return String(bytes / 1024.0) + "KB";
	}
	else if (bytes < (1024 * 1024 * 1024)) {
		return String(bytes / 1024.0 / 1024.0) + "MB";
	}
	return "";
}
String getContentType(String filename) { // determine the filetype of a given filename, based on the extension
	if (filename.endsWith(".html")) return "text/html";
	else if (filename.endsWith(".css")) return "text/css";
	else if (filename.endsWith(".js")) return "application/javascript";
	else if (filename.endsWith(".ico")) return "image/x-icon";
	else if (filename.endsWith(".gz")) return "application/x-gzip";
	return "text/plain";
}
/*
unsigned long getTime() { // Check if the time server has responded, if so, get the UNIX time, otherwise, return 0
	if (UDP.parsePacket() == 0) { // If there's no response (yet)
		return 0;
	}
	UDP.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
	// Combine the 4 timestamp bytes into one 32-bit number
	uint32_t NTPTime = (packetBuffer[40] << 24) | (packetBuffer[41] << 16) | (packetBuffer[42] << 8) | packetBuffer[43];
	// Convert NTP time to a UNIX timestamp:
	// Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
	const uint32_t seventyYears = 2208988800UL;
	// subtract seventy years:
	int32_t UNIXTime = NTPTime - seventyYears + 60 * 60 * 3;

	return UNIXTime;
}
*/
void sendNTPpacket(IPAddress& address) {
	Serial.println("Sending NTP request");
	memset(packetBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
	// Initialize values needed to form NTP request
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode

	// send a packet requesting a timestamp:
	UDP.beginPacket(address, 123); // NTP requests are to port 123
	UDP.write(packetBuffer, NTP_PACKET_SIZE);
	UDP.endPacket();
}

void HomePage() {
	Serial.println("Home page begins");

	SendHTML_Header();
	webpage += F("<a href='/download'><button>Download</button></a>");
	append_page_footer();
	SendHTML_Content();
	SendHTML_Stop(); // Stop is needed because no content length was sent
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Header() {
	//server.send(200, "text/plain", "Home page begins"); 
	server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "-1");
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	// server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
	append_page_header();
	SendHTML_Content();
	webpage = "";
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Stop() {
	server.sendContent("");
	server.client().stop(); // Stop is needed because no content length was sent
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void ReportFileNotPresent(String target) {
	SendHTML_Header();
	webpage += F("<h3>File does not exist</h3>");
	webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
	append_page_footer();
	SendHTML_Content();
	SendHTML_Stop();
}
//===========================================================================================

void File_Download() { // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments
	if (server.args() > 0) { // Arguments were received
		if (server.hasArg("download")) SD_file_download(server.arg(0));

	}
	else SelectInput("File Download", "Enter filename to download", "download", "download");
}
void File_Delete() { // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments
	if (server.args() > 0) { // Arguments were received
		if (server.hasArg("Delete")) if (deleteFile(server.arg(0))) {
			checkFileOverFloodAndDelete();
		}
		server.sendHeader("Connection", "close");
	}
	else SelectInput("File Delete", "Enter filename to delete", "Delete", "Delete");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SD_file_download(String filename) {
	//File download = SPIFFS.open("/" + filename, "r");
	File download = SPIFFS.open(filename, "r");
	if (download) {
		server.sendHeader("Content-Type", "text/text");
		server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
		server.sendHeader("Connection", "close");
		server.streamFile(download, "application/octet-stream");
		download.close();
	}
	else ReportFileNotPresent("download");
}
bool deleteFile(String filename) {
	bool filefound = false;
	Serial.println("Tring to delete file  .... " + filename);
	//File download = SPIFFS.open("/" + filename, "r");
	File download = SPIFFS.open(filename, "r");
	if (download) filefound = true;
	download.close();
	if (filefound) {
		//SPIFFS.remove("/"+filename);
		SPIFFS.remove(filename);
		Serial.println("File deleted " + filename);

		return true;
	}
	return false;
}
void refreshSPIFS() {
	for (int i = 0; i < maxFiles; i++) {
		filesStored[i].name = "";
		filesStored[i].size = "";
		filesStored[i].checked = false;
		filesStored[i].number_csv = 0;
	}
	startSPIFFS();
   //  filesStoredIndex = 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Content() {
	Serial.println("webpage: " + String(webpage.length()));
	server.sendContent(webpage);
	webpage = "";
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SelectInput(String heading1, String heading2, String command, String arg_calling_name) {
	char quote = '"';
	SendHTML_Header();
	webpage += F("<h3 class='rcorners_m'>"); webpage += heading1 + "</h3><br>";
	webpage += F("<h3>"); webpage += heading2 + "</h3>";
	webpage += F("<ul>");
		
	for (int i = 0; i < maxFiles; i++) {
		webpage += F("<li>"); webpage += "<span id=" + String(quote)+"file" + i + String(quote);
		webpage += F(">"); webpage += filesStored[i].name + "</span> <span>" + filesStored[i].size + "</span></li></br>";
		// webpage += F("<h6>"); webpage += filesStored[i].name +"  "+ filesStored[i].size + "</h6>";
	}
	webpage += F("</ul>");
	webpage += F("<script type = \"text/javascript\">");
	// 20 - максимальное количество файлов, увеличить, если нужно больше
	webpage += "for (let i = 0; i < "+String(maxFiles)+"; i++) {";
	webpage +=		"var h6 = document.getElementById(`file${i}`);";
	webpage +=			"if (!h6) continue;";
	webpage +=			"let fileName = h6.innerText;";
	webpage +=			"h6.onclick = function() {";
	webpage +=				"document.getElementById('fileNameField').value = fileName;";
	webpage +=			"}";
	webpage += "}";
	webpage += "</script>";
	webpage += F("<FORM action='/"); webpage += command + "' method='post'>"; // Must match the calling argument e.g. '/chart' calls '/chart' after selection but with arguments!
	webpage += F("<input type='text' name='"); webpage += arg_calling_name; webpage += F("' value='' id='fileNameField'><br>");
	webpage += F("<type='submit' name='"); webpage += arg_calling_name; webpage += F("' value=''><br><br>");

	append_page_footer();
	SendHTML_Content();
	SendHTML_Stop();
}
