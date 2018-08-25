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

//*******************************************
// Weather Reminder 상태
// 1. SETUP 상태. 블루투스를 사용하는 상태. 즉 어플로 부터 와이파이 정보를 받기 위한 상태
//  (1) 블루투스 모듈이 켜져있는지 AT명령어로 확인
//	(2) 어플에서 블루투스 연결이 되고 블루투스 응답을 요청하면 응답
//  (3) 어플쪽에게 주변 와이파이 스캔 목록을 전송
//  (4) 어플에서 선택된 와이파이의 SSID와 비밀번호 정보를 받음
//  (5) 어플에서 선택된 사용자 위치 정보를 받음
//  (6) 받은 정보를 eeprom에 저장 
// 2. RUN 상태. 날씨 데이터를 사용자에게 제공하기 위해 작동하는 상태
//  (1) setup() 에서 eeprom에 저장된 데이터들을 불러오고 와이파이 연결한다
//  (2) 이후 작동...
//*******************************************


//분리할수 있는 기능들은 가능한 다른 소스파일로 만들자



// WeatherReminder GPIO핀 정의
// D7 : 소프트웨어 시리얼 RX
// D8 : 소프트웨어 시리얼 TX
// D5 : 디버깅용 LED 

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

typedef struct
{
	String temp;  //온도
	String wfEn;  //구름
	String reh;   //습도
} INFO_WEATHER;




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





const int httpPort = 80;

//동백동 기상청RSS 주소
//http://www.kma.go.kr/wid/queryDFSRSS.jsp?zone=4146358500

//상윤원룸 죽전1동
//http://www.kma.go.kr/wid/queryDFSRSS.jsp?zone=4146554000

const String KMA_url = "/wid/queryDFSRSS.jsp?zone=4146358500";
const char* SERVER = "www.kma.go.kr";


//하드웨어 변수 초기화
SoftwareSerial BlueToothSerial(D7, D8);

// WeatherReminder 블루투스 켜져있으면 SETUP, 꺼져있으면 RUN
const int SETUP = 1;
const int RUN = 0;
int state_WeatherReminder;
void SetupWeatherReminder();
void RunWeatherReminder();


void parsing_data(char str[], char ssid[], char pwd[]) ; // 와이파이 정보 파싱용
void wifi_scan();	// 주변 wifi 스캐닝
void WiFiSetUp();	// wifi 연결을 위한 초기화

void WiFiSetUp() {
	// Connect to WiFi network
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);

	WiFi.mode(WIFI_STA);  //esp12를 AP에 연결하고 외부 네트워크와 통신하므로 스테이션모드
	
	//테스트용 변수임...
	const char* wifiSSID = "KT_GiGA_2G_C62E";
	const char* wifiPassword = "fzfd8ed535";
	//const char* ssid = "TP-LINK_E59A";
	//const char* password = "23498129";
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

	state_WeatherReminder = 0;
	flag.Serial = 0 ;
	flag.menu = 0 ;	// 기본모드
	flag.read = 0 ;

	pinMode(D5, OUTPUT);
	digitalWrite(D5, LOW);
}

void loop()
{
	BlueToothSerial.write("AT");  //시리얼 모니터 내용을 블루추스 측에 WRITE
	delay(3000);
	if (BlueToothSerial.available())
	{
		String str = BlueToothSerial.readString();
		// 블루투스 모듈이 OK 응답하면 SETUP 상태로 설정한다
		if (str == "OK")
			state_WeatherReminder = SETUP;
		
	}
	// state_WeatherReminder에 따라 진입할 기능이 나뉜다
	switch (state_WeatherReminder)
	{
	case SETUP :
		SetupWeatherReminder();
		
		state_WeatherReminder = 0;
		break;
	case RUN :
		RunWeatherReminder();
		break;
	default :
		break;	
	}

#if 0
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

#endif

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

void SetupWeatherReminder()
{
	digitalWrite(D5, HIGH);
	Serial.println("SetupWeatherReminder");
	while (1)
	{
		
		delay(1000);
	}
}

void RunWeatherReminder()
{
	digitalWrite(D5, LOW);
	Serial.println("RunWeatherReminder");
	//RUN으로 진입하면 가장 첫번째로 eeprom에 와이파이 정보가 저장되어있는지 체크한다. 없으면 기능 작동 못하니깐 빠져나온다
	//if (와이파이 정보 있으면)
	//{
		//Serial.println("RunWeatherReminder");
		//RUN 상태일때 와이파이, 도트매트릭스, 센서 등등 사용하니깐 그런거 셋업하는거 추가
		WiFiSetUp();
		//ex) DotMatrixSetUp();
		INFO_WEATHER infoWeather;

		WiFiClient client;
		int i;
		String tmp_str;
		while (1)
		{
	
			if (client.connect(SERVER, httpPort))
			{

				client.print(String("GET ") + KMA_url + " HTTP/1.1\r\n" +
					"Host: " + SERVER + "\r\n" +
					"Connection: close\r\n\r\n");

				delay(1000);


				while (client.available())
				{
					String line = client.readStringUntil('\n');

					i = line.indexOf("</temp>"); //온도

					if (i > 0)
					{
						tmp_str = "<temp>";
						infoWeather.temp = line.substring(line.indexOf(tmp_str) + tmp_str.length(), i);
						Serial.print("온도 : ");
						Serial.println(infoWeather.temp);
					}

					i = line.indexOf("</wfEn>"); //구름정보(영문)

					if (i>0)
					{
						tmp_str = "<wfEn>";
						infoWeather.wfEn = line.substring(line.indexOf(tmp_str) + tmp_str.length(), i);
						Serial.print("구름 : ");
						Serial.println(infoWeather.wfEn);
					}

					i = line.indexOf("</reh>"); //습도

					if (i>0)
					{
						tmp_str = "<reh>";
						infoWeather.reh = line.substring(line.indexOf(tmp_str) + tmp_str.length(), i);
						Serial.print("습도 : ");
						Serial.println(infoWeather.reh);
						break;
					}
				}
			}
		}
	//}

}

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

