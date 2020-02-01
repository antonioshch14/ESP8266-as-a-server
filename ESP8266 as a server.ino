//working scetch of a server with OLED 64x128 that provides connection to 3 devices 
//bidirectional communication works well
//this is a new branch for the test reason


#include <U8g2lib.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
//#include <ESP8266WebServer.h>
#include "DHTesp.h"
#include <WiFiUdp.h>



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
};
class Timed {
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
Device Device1;
Device Device2;
Device Device3;
Device Device4; //bathroom
Device Device_undidentified;//assigneed for data if device name is unindentified
int devToShow = 0;//counter which device to show
float h = 0;
float t = 0;
int unsigned long timers[10];
bool timer(int tNamber, unsigned long tDelay);
long timeToCheckConnected = 60000;//time interval to check connected
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
task task1(10000);//connection to NTP
task taskSendNTPrequest(100);//sending NTP request
//--------------------------NTP
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

void setup(void) {
	Serial.begin(115200);
  //dht.begin();
  dht.setup(0, DHTesp::DHT11); 
  u8g2.begin();
  u8g2.setFont(u8g2_font_crox1c_tf);
  
  
  delay(700);
  // setting up a Wifi AccessPoint
  SetWifi("DataTransfer","BelovSer");
  pinMode(LED0, INPUT);
  pinMode(sendTriger, INPUT);
  pinMode(sendTriger2, INPUT);
  Serial.setDebugOutput(true);
  //---------------------------------
  startWiFi();                   // Try to connect to some given access points. Then wait for a connection
  startUDP();



}
void startWiFi() { // Try to connect to some given access points. Then wait for a connection
	
	char ssid[] = "Keenetic-4574"; //  Your network SSID (name)
	char pass[] = "Gfmsd45kaxu69$"; // Your network password
	WiFi.begin(ssid, pass);
	//WiFi.config(staticIP, gateway, subnet);
	WiFiClient client; // Initialize the WiFi client library
	//WiFi.mode(WIFI_STA);
	/*while (WiFi.status() != WL_CONNECTED) {
		Serial.print("Attempting to connect to SSID: ");
		Serial.println(ssid);
		
		Serial.print("WiFimode is "); Serial.println(WiFi.getMode());// Connect to WPA/WPA2 network. Change this line if using open or WEP network
		delay(1000);  // Wait 1 second to connect
	}*/
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

void sendNTPpacket(IPAddress& address) {
	memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
	// Initialize values needed to form NTP request
	//NTPBuffer[0] = 0b11100011;   // LI, Version, Mode
	// send a packet requesting a timestamp:
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
			}
			else devToShow++;
		}
		if (devToShow == 3){
			if (Device3.connected) {
				getTime(&Time3dev.Day, &Time3dev.Hour, &Time3dev.Min, &Time3dev.Sec, Device3.time);
				time = "d" + String(Time3dev.Day) + "h" + String(Time3dev.Hour) + "m" + String(Time3dev.Min) + "s" + String(Time3dev.Sec);
				con = "3Str " + String(Device3.signal) + "." + String(dev3);
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
bool connectToNtp() {
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
	if (task1.check()) {Serial.println("Check()");
		if (WiFi.status() == WL_CONNECTED && !NTPconnected) {
			Serial.println("WL_CONNECTED");
			NTPconnected = connectToNtp();
			task1.ignor = NTPconnected;
		}
	}
	if (NTPconnected) {
		unsigned long currentMillis = millis();

		if (taskSendNTPrequest.check()) { // If a minute has passed since last NTP request
			//prevNTP = currentMillis;
			Serial.println("\r\nSending NTP request ...");
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
			taskSendNTPrequest.period=1000*60*60*24;//slow down requestes when the time is obtained
			WiFi.disconnect;

		}

		actualTime = timeUNIX + (currentMillis - lastNTPResponse) / 1000;
		
	}
	//-----------------------END NTP--------------------
	
	HandleClients();
	if (millis() > (now3 + 2000)) {
		 h = dht.getHumidity();
		 t = dht.getTemperature();
		display();
	}
	
		while(digitalRead(LED0) == HIGH){
		if(!started)Serial.print("reading stopped  ");
		started = true;
		delay(500);
		Serial.print(".");
	}
		if (started) {
			Serial.println("reading begined"); started = false;
		}
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
				sentToClientNew(3, "humid:"+String(h)+";");
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
		if (timer(1, 60000)) { if (conected > 0) checkConnected(); }
		
}
bool timer(int tNamber, unsigned long tDelay) {
	unsigned long current = millis();
	if (current > (timers[tNamber] + tDelay)) {
		timers[tNamber] = current; return true;
	}
	else return false;
}
void checkConnected() {//check the real status of sent message
	unsigned long Tnow = millis();
	Device*Deviceptr;
	for (int i = 0; i < 4; i++) {
		if (i == 0)Deviceptr = &Device1;
		else if (i == 1)Deviceptr = &Device2;
		else if (i == 2)Deviceptr = &Device3;
		else if (i == 3)Deviceptr = &Device4;
		if (Deviceptr->lastRecieved + timeToCheckConnected < Tnow )Deviceptr->connected = false;
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
	WiFi.softAP(ssid, password, 1, 0, MAXSC);                           // WiFi.softAP(ssid, password, channel, hidden, max_connection)     
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
		
		String readMess = Temp.readStringUntil('\r\n');
		Serial.println(" First message of a new client : " + readMess);
		
			
	}
	
	String Message;
	yield();
	for(int i=0;i<4;++i){
		if (TCP_Clients[i].connected() && TCP_Clients[i].available()) {
			Message = TCP_Clients[i].readStringUntil('\r\n');
			Serial.print(" Content: ");
			Serial.println(Message);
			new_process_Msessage(Message);
			break;
		}
		
	}

			
			                          
}
	

void new_process_Msessage(String Message) {
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
	if (get_field_value(Message, "time:", &value, &index)) {
		Deviceptr->time = value;
	}
	if (get_field_value(Message, "signal:", &value, &index)) {
		Deviceptr->signal = int(value);
	}
	if (get_field_value(Message, "temp:", &value,&index)) {//index means that the value is float and it determens numbers after ','
		Deviceptr->temp = value/pow(10,index);
		}
	if (get_field_value(Message, "humid:", &value, &index)) {
		Deviceptr->humid = value / pow(10, index);
		}
	if (Deviceptr->name == 4) {
		if (get_field_value(Message, "humidAv:", &value, &index)) {
			Deviceptr->field1 = int(value / pow(10, index));
		}
		if (get_field_value(Message,"status:", &value, &index)) {
			Deviceptr->status = int(value / pow(10, index));
		}
	}
	//Serial.println("device: " + String(Deviceptr->name) + " time: " + String(Deviceptr->time) +
	//	" signal: " + String(Deviceptr->signal));
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

