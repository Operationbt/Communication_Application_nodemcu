// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
Name:       Communication_Application_nodemcu.ino
Created:	2018-08-06 17:27
Author:     JHW

한것 :		1. SAVE_ROM
			2. wifi connected
			3. wifi infomation send ( Serial com )

*/

//전현우 밥사라 

#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <SoftwareSerial/SoftwareSerial.h>
#define NULL 0

typedef struct flag_variable {

	int Serial ;
	int menu ;
	int read;

}flag_var;

flag_var flag;

const int EEPROM_MIN_ADDR = 0;
const int EEPROM_MAX_ADDR = 511;

const int BUFSIZE = 32;		//32bit
char buf[BUFSIZE];
char temp_buf[BUFSIZE];

char ssid[BUFSIZE] = { 0, };
char pwd[BUFSIZE] = { 0, };
char WiFi_info[BUFSIZE] = { 0, };
char *save_data;

String temp_str;

int addr = 0x00;

boolean eeprom_is_addr_ok(int addr);
boolean eeprom_write_bytes(int startAddr, const byte* array, int numBytes);
boolean eeprom_write_string(int addr, const char* string);
boolean eeprom_read_string(int addr, char* buffer, int bufSize);

const char* wifiSSID = "KT_GiGA_2G_C62E";
const char* wifiPassword = "fzfd8ed535";

//const char* ssid = "TP-LINK_E59A";
//const char* password = "23498129";


const int httpPort = 80;

//동백동 기상청RSS 주소
//http://www.kma.go.kr/wid/queryDFSRSS.jsp?zone=4146358500

//상윤원룸 죽전1동
//http://www.kma.go.kr/wid/queryDFSRSS.jsp?zone=4146554000

const String KMA_url = "/wid/queryDFSRSS.jsp?zone=4146358500";
const char* SERVER = "www.kma.go.kr";


//하드웨어 변수 초기화
SoftwareSerial BlueToothSerial(D7, D8);


void parsing_data(char str[], char ssid[], char pwd[]) ; // 와이파이 정보 파싱용
void wifi_scan();	// 주변 wifi 스캐닝
void WiFiSetUp();	// wifi 연결을 위한 초기화

void WiFiSetUp() {
	// Connect to WiFi network
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);

	WiFi.mode(WIFI_STA);  //esp12를 AP에 연결하고 외부 네트워크와 통신하므로 스테이션모드

	WiFi.begin(wifiSSID, wifiPassword);

	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}

	Serial.println("");
	Serial.println("WiFi connected");
}

void setup() {

	EEPROM.begin(512);

	Serial.begin(9600);
	BlueToothSerial.begin(9600);
	
	delay(1000);
	Serial.println("Serial SetUp begin");

	WiFiSetUp();

	flag.Serial = 0 ;
	flag.menu = 0 ;	// 기본모드
	flag.read = 0 ;

}

void loop() {

	if (Serial.available() > 0) {

		temp_str = Serial.readStringUntil('\n');		// read 한 문자열 

		/* 조합논리 */
		temp_str.equals("0") ? flag.menu = 0 :			// 아무것도 아닌 상태 
		temp_str.equals("1") ? flag.menu = 1 :			// wifi 정보를 받는 상태
		temp_str.equals("11") ? flag.menu = 11 : 0;		// wifi 를 연결할건지 선택하는 모드.

		temp_str.equals("21") ? flag.menu = 21 :		// eepron save
		temp_str.equals("22") ? flag.menu = 22 : 0 ; 	// eepron read

		Serial.flush();
	}
	else {

		switch ( flag.menu ) {

			case 1:
				/* WiFi set mode */
				if (!flag.read) {
					temp_str.toCharArray(WiFi_info, BUFSIZE);
				}

				save_data = WiFi_info;
 
				Serial.println("no input sector");
				Serial.print("wifi_info : ");
				Serial.println( WiFi_info );

				parsing_data(WiFi_info, ssid, pwd);

				Serial.print("ssid : ");
				Serial.println(ssid);
				Serial.print("pwd : ");
				Serial.println(pwd);

				Serial.flush();

				break;
			case 11:
				/* WiFI 연결부 */
				WiFi.begin(ssid, pwd);

				while (WiFi.status() != WL_CONNECTED) {
					Serial.println("not connected");
					delay(500);
				}

				Serial.println("");
				Serial.println("WiFi connected ");
				Serial.print("IP Add : ");
				Serial.println(WiFi.localIP());

				flag.menu = 0;

				break;
			case 21:
				/* eeprom save */
				eeprom_write_string( 0, save_data );
				Serial.println("SAVE!!");

				break;
			case 22:
				/* eeprom read */
				eeprom_read_string(0, WiFi_info, BUFSIZE);
				Serial.print("read data : ");
				Serial.println( WiFi_info );

				flag.read = 1;
				break;


		}

			
	}

	/* wifi scan info */

	wifi_scan();

	Serial.print("flag.menu : ");
	Serial.println(flag.menu);



	delay(1000);

}

/* eeprom 함수 */

boolean eeprom_is_addr_ok(int addr) {
	return ((addr >= EEPROM_MIN_ADDR) && (addr <= EEPROM_MAX_ADDR));
}
// Writes a sequence of bytes to eeprom starting at the specified address.
// Returns true if the whole array is successfully written.
// Returns false if the start or end addresses aren't between
// the minimum and maximum allowed values.
// When returning false, nothing gets written to eeprom.

boolean eeprom_write_bytes(int startAddr, const byte* array, int numBytes) {
	// counter
	int i;
	// both first byte and last byte addresses must fall within
	// the allowed range 
	if (!eeprom_is_addr_ok(startAddr) || !eeprom_is_addr_ok(startAddr + numBytes)) {
		return false;
	}
	for (i = 0; i < numBytes; i++) {
		EEPROM.write(startAddr + i, array[i]);
	}

	EEPROM.commit(); // 기존소스에서 추가한것.

	return true;
}	

// Writes a string starting at the specified address.
// Returns true if the whole string is successfully written.
// Returns false if the address of one or more bytes fall outside the allowed range.
// If false is returned, nothing gets written to the eeprom.

boolean eeprom_write_string(int addr, const char* string) {

	int numBytes; // actual number of bytes to be written
				  //write the string contents plus the string terminator byte (0x00)
	numBytes = strlen(string) + 1;
	return eeprom_write_bytes(addr, (const byte*)string, numBytes);
}

// Reads a string starting from the specified address.
// Returns true if at least one byte (even only the string terminator one) is read.
// Returns false if the start address falls outside the allowed range or declare buffer size is zero.
// 
// The reading might stop for several reasons:
// - no more space in the provided buffer
// - last eeprom address reached
// - string terminator byte (0x00) encountered.

boolean eeprom_read_string(int addr, char* buffer, int bufSize) {
	byte ch; // byte read from eeprom
	int bytesRead; // number of bytes read so far
	if (!eeprom_is_addr_ok(addr)) { // check start address
		return false;
	}

	if (bufSize == 0) { // how can we store bytes in an empty buffer ?
		return false;
	}
	// is there is room for the string terminator only, no reason to go further
	if (bufSize == 1) {
		buffer[0] = 0;
		return true;
	}
	bytesRead = 0; // initialize byte counter
	ch = EEPROM.read(addr + bytesRead); // read next byte from eeprom
	buffer[bytesRead] = ch; // store it into the user buffer
	bytesRead++; // increment byte counter
				 // stop conditions:
				 // - the character just read is the string terminator one (0x00)
				 // - we have filled the user buffer
				 // - we have reached the last eeprom address
	while ((ch != 0x00) && (bytesRead < bufSize) && ((addr + bytesRead) <= EEPROM_MAX_ADDR)) {
		// if no stop condition is met, read the next byte from eeprom
		ch = EEPROM.read(addr + bytesRead);
		buffer[bytesRead] = ch; // store it into the user buffer
		bytesRead++; // increment byte counter
	}
	// make sure the user buffer has a string terminator, (0x00) as its last byte
	if ((ch != 0x00) && (bytesRead >= 1)) {
		buffer[bytesRead - 1] = 0;
	}
	return true;
}

/* eeprom 끝 */

/* 파싱 */
void parsing_data(char str[], char ssid[], char pwd[]) {

	int i = 0, j = 0;
	int section = 0 ; 

	while ( str[i] != '\0' ) {
		
		if( str[i] == '|' ){
			section = 1 ;
		}
		i++;
	}

	i = 0 ;

	if (section) {
		while (str[i] != '|') {
			ssid[i] = str[i];
			i++;
		}
		ssid[i] = '\0';
		i = i + 1;

		while (str[i] != '\0') {

			pwd[j] = str[i];
			i++;
			j++;

			pwd[j] = '\0';
		}
	}
	else {
		return;
	}
	
}
void wifi_scan() {

	int numberOfNetworks = WiFi.scanNetworks();
	Serial.println("ListStart");
	for (int i = 0; i < numberOfNetworks; i++) {

		Serial.print("Network name[");
		Serial.print(i); Serial.print("]: ");
		Serial.println(WiFi.SSID(i));
		//Serial.print("Signal strength: ");
		//Serial.println(WiFi.RSSI(i));
		//Serial.println("-----------------------");

	}
	Serial.println("ListEnd");

}

