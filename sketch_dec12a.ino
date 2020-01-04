//working scetch of a server with OLED 64x128 that provides connection to 3 devicec.
//works stable after exlusion of IP obtaining from mesage sent


#include <U8g2lib.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
//#include <ESP8266WebServer.h>
#include "DHTesp.h"
int     LED0 = 16;         // WIFI Module LED

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
unsigned int TCPPort = 2390;

WiFiServer  TCP_SERVER(TCPPort);      // THE SERVER AND THE PORT NUMBER
WiFiClient  TCP_Client[MAXSC];        // THE SERVER CLIENTS Maximum number
//------------------------------------------------------------------------------------
  // Some Variables
char result[10];
float temp, humid;
//***********************************************************************************

DHTesp dht;

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, 12, 14);

void process_Message(String);
unsigned long now3 = millis();
int conected = 0;
unsigned long dev1 = 0, dev2 = 0, dev3 = 0;

bool started;
int Day=0, Time=0, Hour=0,Min=0,Sec=0;
//void getTime();
void getTime(int *Day,int *Hour,int*Min,int *Sec, unsigned long Now);
bool get_field_value(String Message, String field, unsigned long* value, int* index );//reads a value from fields of message
class Device{
public:
	int name=0;
	unsigned long time=0;
	int signal=0;
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
Device Device1;
Device Device2;
Device Device3;
Device Device_undidentified;//assigneed for data if device name is unindentified
int devToShow = 0;
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

  Serial.setDebugOutput(true);
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


void loop(void) {
	String con="";
	String time = "";
	HandleClients();
	if (millis() > (now3 + 2000)) {
		now3 = millis();
		//delay(dht.getMinimumSamplingPeriod());
		float h = dht.getHumidity();
		float t = dht.getTemperature();
		
		//Serial.println(h);
		//Serial.println(t);
		u8g2.clearBuffer();
		u8g2.drawStr(0, 15, "lt");
		u8g2.drawStr(0, 32, "lh");
		u8g2.setCursor(20, 32);
		u8g2.print(h);
		u8g2.setCursor(20, 15);
		u8g2.print(t);
		u8g2.drawStr(67, 15, "gt");
		u8g2.drawStr(67, 32, "gh");
		u8g2.setCursor(85, 15);
		u8g2.print(temp);
		u8g2.setCursor(85, 32);
		u8g2.print(humid);
		if (devToShow == 0) {
			getTime(&Day, &Hour, &Min, &Sec, millis());
			time= "d" + String(Day) + "h" + String(Hour) + "m" + String(Min) + "s" + String(Sec)+ "con "+String(conected);
			unsigned long total = dev1 + dev2 + dev3;
			con = "mes tot"+String(total) ;
			devToShow++;
		}
		else if (devToShow == 1) {
			 getTime(&Time1dev.Day, &Time1dev.Hour, &Time1dev.Min, &Time1dev.Sec, Device1.time);
			 time = "d" + String(Time1dev.Day) + "h" + String(Time1dev.Hour) + "m" + String(Time1dev.Min) + "s" + String(Time1dev.Sec) ;
			con = "1Str "+String(Device1.signal) + "."  +String(dev1)  ;
			devToShow++;
		}
		else if ((devToShow == 2)){
			getTime(&Time2dev.Day, &Time2dev.Hour, &Time2dev.Min, &Time2dev.Sec, Device2.time);
			time = "d" + String(Time2dev.Day) + "h" + String(Time2dev.Hour) + "m" + String(Time2dev.Min) + "s" + String(Time2dev.Sec);
			con = "2Str " + String(Device2.signal) + "." + String(dev2) ;
			 devToShow++;
		}
		else {
		getTime(&Time3dev.Day, &Time3dev.Hour, &Time3dev.Min, &Time3dev.Sec, Device3.time);
		time = "d" + String(Time3dev.Day) + "h" + String(Time3dev.Hour) + "m" + String(Time3dev.Min) + "s" + String(Time3dev.Sec);
		con = "3Str " + String(Device3.signal) + "." + String(dev3);

			devToShow = 0;
		}
		u8g2.setCursor(0, 46);
		u8g2.print(con);
		u8g2.setCursor(0, 61);
		u8g2.print(time);
		u8g2.sendBuffer();

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
}


void SetWifi(char* Name, char* Password) {
	// Stop any previous WIFI
	WiFi.disconnect();

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

//====================================================================================

void HandleClients() {
	unsigned long tNow;
	unsigned long tNow2 = millis();
	if (TCP_SERVER.hasClient()) {
		conected = WiFi.softAPgetStationNum();
		Serial.printf("Stations connected to soft-AP = %d\n", conected);
		WiFiClient TCP_Client = TCP_SERVER.available();
		TCP_Client.setNoDelay(1);    
		int i = 0;
		// enable fast communication
		while (tNow2 > (millis() - 50)) {
			++i;
			//---------------------------------------------------------------
			// If clients are connected
			//---------------------------------------------------------------
			if (TCP_Client.available() > 0) {
				String Message;
				//int aval=0;
				//do {// read the message
					//Serial.println("1TCP_Client.available()" + String(TCP_Client.available()));
					Message = TCP_Client.readStringUntil('\r\n');
					
					Serial.print(" Content: ");
					Serial.print(Message);
					new_process_Msessage(Message);
					//break;
					/*if (!TCP_SERVER.hasClient()) { break; };
					WiFiClient TCP_Client = TCP_SERVER.available();
					Serial.println("2TCP_Client.available()" + String(TCP_Client.available()));
					aval = TCP_Client.available();*/
				//	} while (aval > 0);
				// print the message on the screen
				

				
				/*if (i > 2) { //check whether there data is strored in the buffer for so long because of error 28
					Serial.print(" Received packet of size ");
					Serial.print(sizeof(Message));
					Serial.print(" From ");
					Serial.print(TCP_Client.remoteIP());
					Serial.print(", remot port ");
					Serial.print(TCP_Client.remotePort());
					//Serial.print(" ,local IP ");
					//Serial.print(TCP_Client.localIP());
					Serial.print(", local port ");
					Serial.print(TCP_Client.localPort());
					Serial.print(" i= ");
					Serial.println(i);
					if (TCP_Client.remoteIP() == IPdev1) {
						dev1++;
					}
					else if (TCP_Client.remoteIP() == IPdev2) {
						dev2++;
					}
					else if (TCP_Client.remoteIP() == IPdev3) {
						dev3++;
					}
				}*/
				// content
				//Serial.print("Content: ");
				//Serial.println(Message);

				// generate a response - current run-time -> to identify the speed of the response

				//tNow = millis();
				//dtostrf(tNow, 8, 0, result);

				// reply to the client with a message     
				//TCP_Client.println(result);                             // important to use println instead of print, as we are looking for a '\r' at the client
				//TCP_Client.flush();
			//	break;
			}

			//---------------------------------------------------------------
			// If clients are disconnected                                            // does not realy work....
			//---------------------------------------------------------------     
			//if (!TCP_Client || !TCP_Client.connected()) {
				// Here We Turn Off The LED To Indicated The Its Disconnectted
			//	digitalWrite(LED0, LOW);
				
			//}
		}
		}
	else {
		
		// the LED blinks if no clients are available
		//digitalWrite(LED0, HIGH);
		//delay(250);
		//digitalWrite(LED0, LOW);
		//delay(250);
	}
	
}
void new_process_Msessage(String Message) {
	int device = 0,index=0;
	unsigned long value;
	Device*Deviceptr= &Device_undidentified;
	if (get_field_value(Message, "Device:", &value, &index)) {
		device=int(value);

	
	switch (device) {
	case 1: Deviceptr=&Device1; dev1++;   break;
	case 2: Deviceptr = &Device2; dev2++;   break;
	case 3: Deviceptr = &Device3;  dev3++; break;
	default:	break;
	}
	Deviceptr->name = value;
	}
	if (get_field_value(Message, "time:", &value, &index)) {
		Deviceptr->time = value;
	}
	if (get_field_value(Message, "signal:", &value, &index)) {
		Deviceptr->signal = int(value);
	}
	if (get_field_value(Message, "temp:", &value,&index)) {
		temp = value/pow(10,index);
		}
	if (get_field_value(Message, "humid:", &value, &index)) {
		humid = value / pow(10, index);
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

