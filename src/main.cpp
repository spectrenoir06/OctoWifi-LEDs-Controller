#include <WiFi.h>

// #define FASTLED_ESP32_I2S
// #define FASTLED_ALLOW_INTERRUPTS
#include <FastLED.h>

// #define USE_AP				// The driver start has a WiFi Acess point
// #define USE_WIFI				// The driver use WIFI_SSID and WIFI_PASSWORD
#define USE_WIFI_MANAGER		// The driver use Wifi manager

//#define USE_RESET_BUTTON		// Can reset Wifi manager with button

#define USE_POWER_LIMITER	// Activate power limitaton ( edit: LED_VCC and LED_MAX_CURRENT )
// #define USE_OTA				// Activate Over the Air Update
#define USE_ANIM				// Activate animation in SPI filesysteme (need BROTLI)
#define USE_FTP				// Activate FTP server (need USE ANIM)
// #define USE_8_OUTPUT			// Activate 8 LEDs output
// #define USE_CONFIG			// Activate config menu on WifiManger

#define USE_UDP
#define USE_ZLIB
// #define USE_BROTLI

#define PRINT_FPS
// #define PRINT_DEBUG
// #define PRINT_DMX
// #define PRINT_RLE

#define FIRMWARE_VERSION	"1.0"

#define DEFAULT_HOSTNAME	"ESP32_LEDs"

#define AP_SSID				"ESP32_LEDs_AP"
#define AP_PASSWORD			"WIFI_PASSWORD"

#define WIFI_SSID			""
#define WIFI_PASSWORD		""

#define FTP_USER			"LED"
#define FTP_PASS			"LED"

#define ART_NET_PORT		6454
#define UDP_PORT			ART_NET_PORT
#define OTA_PORT			3232

#define LED_TYPE			WS2812B
#define COLOR_ORDER			GRB
#define BRIGHTNESS			255

#ifdef USE_8_OUTPUT
	#define NUM_STRIPS	8
#else
	#define NUM_STRIPS	1
#endif

const int BUFFER_SIZE (4096*3);

const int START_UNI = 0;
const int UNI_BY_STRIP		= 4;
const int LEDS_BY_UNI		= 170;
const int LED_BY_STRIP		= 512;	//(UNI_BY_STRIP*LEDS_BY_UNI)
const int LED_TOTAL			= 4096;//(LED_BY_STRIP*NUM_STRIPS);

#define LED_VCC				5	// 5V
#define LED_MAX_CURRENT		800  // 2000mA

const int RESET_WIFI_PIN	= 39;//23;

const int LED_PORT_0 		= 13;//16;
const int LED_PORT_1 		= 4;
const int LED_PORT_2 		= 2;
const int LED_PORT_3 		= 22;
const int LED_PORT_4 		= 19;
const int LED_PORT_5 		= 18;
const int LED_PORT_6 		= 21;
const int LED_PORT_7 		= 17;

#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27

#define R2_PIN 14
#define G2_PIN 12
#define B2_PIN 21

#define A_PIN 23
#define B_PIN 19
#define C_PIN 5
#define D_PIN 33
#define CLK_PIN 32
#define LAT_PIN 4
#define OE_PIN 22

#define E_PIN -1

#define MATRIX_WIDTH 128

// #define USE_GFX_ROOT 1
#define PIXEL_COLOR_DEPTH_BITS 6

#include <ESP32-RGB64x32MatrixPanel-I2S-DMA.h>

#ifdef USE_WIFI_MANAGER
	#include <WiFiManager.h>
#endif

#if defined(USE_CONFIG) || defined(USE_FTP) || defined(USE_ANIM)
	#include "SPIFFS.h"
#endif

#ifdef USE_CONFIG
	#include <ArduinoJson.h>
#endif

#ifdef USE_FTP
	#include <ESP8266FtpServer.h>
#endif

#ifdef USE_OTA
	#include <ArduinoOTA.h>
#endif

#ifdef USE_UDP
	#include <Artnet.h>
	#include <AsyncUDP_big.h>
#endif

#ifdef USE_ZLIB
	#include <miniz.h>
#endif

#if defined(USE_BROTLI)
	#include <brotli/decode.h>
#endif

#ifdef PRINT_FPS
	#include <SimpleTimer.h>
#endif

enum UDP_PACKET
{
	LED_RGB_888 = 0,
	LED_RGB_888_UPDATE = 1,

	LED_RGB_565 = 2,
	LED_RGB_565_UPDATE = 3,

	LED_UPDATE = 4,
	GET_INFO = 5,

	LED_TEST = 6,
	LED_RGB_SET = 7,
	LED_LERP = 8,

	SET_MODE = 9,
	REBOOT = 10,

	LED_RLE_888 = 11,
	LED_RLE_888_UPDATE = 12,

	LED_BRO_888 = 13,
	LED_BRO_888_UPDATE = 14,

	LED_Z_888 = 15,
	LED_Z_888_UPDATE = 16,

	LED_Z_565 = 17,
	LED_Z_565_UPDATE = 18,

};

enum ANIM {
	ANIM_UDP = 0,
	ANIM_START = 1,
	ANIM_PLAY = 2
};

#ifdef USE_UDP
	AsyncUDP_big	udp;
	Artnet			artnet;
#endif

uint8_t			led_state = 0;
uint16_t		paquet_count = 0;

#ifdef USE_FTP
	FtpServer ftpSrv;
#endif

#if defined(USE_CONFIG) || defined(USE_ANIM)
	File file;
	File root;
#endif

#ifdef USE_ANIM	
	// size_t un_size = sizeof(buff);
	uint16_t wait;
	uint8_t head[5];
	uint16_t fps;
	uint16_t nb;
	unsigned long previousMillis = 0;
	uint8_t		anim = ANIM_START;
#else
	uint8_t		anim = ANIM_UDP;
#endif

char 		hostname[50] = DEFAULT_HOSTNAME;
char 		firmware[20] = FIRMWARE_VERSION;

struct Config {
	int		udp_port;
};

const char *filename = "/config.txt";
Config config;

// CRGB		leds[4096*2];
// uint8_t		buffer[8192*3];
// uint16_t 	buffer2[8192];

CRGB *leds;// = (CRGB *)malloc(4096 * 4); //(CRGB*)malloc(sizeof(CRGB) * LED_TOTAL);
uint8_t *buffer;// = (uint8_t *)malloc(BUFFER_SIZE);

#ifdef USE_WIFI_MANAGER
	WiFiManager	wifiManager;
	#ifdef USE_CONFIG
			WiFiManagerParameter param_udp_port("udp_port", "UDP port", "6454", 6);
		// WiFiManagerParameter param_udp_port("LEDs type", "UDP port", "6454", 5);

		// WiFiManagerParameter custom_text("<select name=\"LEDs type\" id=\"leds_type\"><option value=\"WS2811 800kHz\">WS2811 800kHz</option><option value=\"WS2811 400kHz\">WS2811 400kHz</option><option value=\"WS2812\">WS2812</option><option value=\"WS2813\">WS2813</option><option value=\"SK6822\">SK6822</option></select>");
	#endif
#endif

RGB64x32MatrixPanel_I2S_DMA dma_display(true);

#ifdef PRINT_FPS
	SimpleTimer	timer;
#endif
unsigned long frameCounter = 0;
unsigned long frameLastCounter = frameCounter;

#ifdef PRINT_FPS
	void timerCallback(){
		if (frameLastCounter != frameCounter) {
			Serial.printf("FPS: %lu Frames received: %lu\n", (frameCounter-frameLastCounter)/5, frameCounter);
			frameLastCounter = frameCounter;
		}
	}
#endif


#ifdef USE_ANIM
	void load_anim()
	{
		Serial.print("  FILE: ");
		Serial.print(file.name());
		Serial.print("\tSIZE: ");
		Serial.println(file.size());

		Serial.println("Open animation");

		file.read(head, 5);

		fps = (head[2] << 8) | head[1];
		nb = (head[4] << 8) | head[3];
		wait = 1000.0 / fps;
		// compress_size = 0;

		Serial.printf("  FPS: %02d; LEDs: %04d format: %d\n", fps, nb, head[0]);
		anim = ANIM_PLAY;
	}

	// void save_anim()
	// {
	// 	file = SPIFFS.open("/test.Z565", FILE_WRITE);
	// 	// Serial.print("  FILE: ");
	// 	// Serial.print(file.name());
	// 	// Serial.print("\tSIZE: ");
	// 	// Serial.println(file.size());

	// 	Serial.println("write animation");

	// 	head[0] = 17;
	// 	(uint16_t)head[1] = 60;
	// 	(uint16_t)head[3] = 4096;

	// 	file.write(head, 5);

	// 	fps = (head[2] << 8) | head[1];
	// 	nb = (head[4] << 8) | head[3];
	// 	wait = 1000.0 / fps;
	// 	compress_size = 0;

	// 	Serial.printf("  FPS: %02d; LEDs: %04d format: %d\n", fps, nb, head[0]);
	// 	anim = ANIM_PLAY;
	// }

	void read_anim_frame()
	{
		uint16_t compress_size;
		unsigned long int un_size;
		uint8_t buff_test[4096*2];

		file.read((uint8_t *)&compress_size, 2);
		uint16_t r = file.read(buffer, compress_size);

		// Serial.printf("%x %x %x\n", buffer[0], buffer[1], buffer[2]);

		// LEDS.show();

		int ret = uncompress(
			(uint8_t *)buff_test,
			(long unsigned int *)&un_size,
			(const uint8_t *)buffer,
			BUFFER_SIZE
		);

		if (ret) 
			Serial.printf("ret: %d == %s, compress: %d, uncompress: %d, r: %d\n", ret, mz_error(ret), compress_size, un_size, r);

		for (int i = 0; i < LED_TOTAL; i++) {
			dma_display.drawPixel(i % 128, i / 128, ((uint16_t*)buff_test)[i]);
		}

		dma_display.showDMABuffer();
		dma_display.flipDMABuffer();
		frameCounter++;

		if (!file.available())
		{
			anim = ANIM_START;
			file.close();
			file = root.openNextFile();
			if (!file)
			{
				root.close();
				root = SPIFFS.open("/");
				file = root.openNextFile();
			}
		}
	}
#endif

#ifdef USE_CONFIG
	void saveConfiguration(const char *filename, const Config &config) {
		// Delete existing file, otherwise the configuration is appended to the file
		SPIFFS.remove(filename);

		// Open file for writing
		File file = SPIFFS.open(filename, FILE_WRITE);
		if (!file) {
			Serial.println(F("Failed to create file"));
			return;
		}

		// Allocate a temporary JsonDocument
		// Don't forget to change the capacity to match your requirements.
		// Use arduinojson.org/assistant to compute the capacity.
		StaticJsonDocument<256> doc;

		// Set the values in the document
		doc["udp_port"] = config.udp_port | UDP_PORT;

		// Serialize JSON to file
		if (serializeJson(doc, file) == 0) {
			Serial.println(F("Failed to write to file"));
		}

		// Close the file
		file.close();
	}

	void printFile(const char *filename) {
	// Open file for reading
	File file = SPIFFS.open(filename);
	if (!file) {
		Serial.println(F("Failed to read file"));
		return;
	}

	// Extract each characters by one by one
	while (file.available()) {
		Serial.print((char)file.read());
	}
	Serial.println();

	// Close the file
	file.close();
}
#endif

#ifdef USE_WIFI_MANAGER
	//flag for saving data
	bool shouldSaveConfig = false;

	//callback notifying us of the need to save config
	void saveConfigCallback () {
		Serial.println("Should save config");
		shouldSaveConfig = true;
	}
#endif

#ifdef USE_UDP

	void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP) {

		static uint8_t dmx_counter = 0;

		#ifdef PRINT_DMX
			Serial.print("DMX: Univ: ");
			Serial.print(universe, DEC);
			Serial.print(", Seq: ");
			Serial.println(sequence, DEC);
		#endif

		if (sequence != dmx_counter + 1 && sequence) {
			// Serial.println("LOST frame: ");
		}
		paquet_count++;

		if (sequence == 255) {
			Serial.printf("%d / 256 = %02f%%\n", paquet_count, paquet_count/256.0 * 100.0 );
			paquet_count = 0;
		}

		dmx_counter = sequence;

		if (length<3 || universe < START_UNI || universe >  (START_UNI + UNI_BY_STRIP * NUM_STRIPS))
			return;

		uint16_t off  = (universe - START_UNI) * LEDS_BY_UNI * 3;

		memcpy(((uint8_t*)leds) + off, data, length);
		// if (universe==3)
			// LEDS.show();
	}

	void onSync(IPAddress remoteIP) {
		#ifdef PRINT_DMX
			Serial.println("DMX: Sync");
		#endif
		LEDS.show();
	}

	void udp_receive(AsyncUDP_bigPacket packet) {
	int len = packet.length();
	if (len) {
		#ifdef PRINT_DEBUG
			Serial.printf("Received %d bytes from %s, port %d\n", len, packet.remoteIP().toString().c_str(), packet.remotePort());
		#endif
		uint8_t		*pqt		= packet.data();
		uint8_t 	type		= pqt[0];
		uint8_t		seq			= pqt[1];
		uint16_t	leds_off	= *(uint16_t *)(pqt+2);
		uint16_t	leds_nb		= *(uint16_t *)(pqt+4);
		uint8_t		*data		= (uint8_t*)&pqt[6];
		uint16_t	*data16		= (uint16_t*)&pqt[6];
		size_t		un_size		= LED_TOTAL*3;

		anim = ANIM_UDP;

		if (type != 'A') {
			paquet_count++;
			if (seq == 255) {
				Serial.printf("%d / 256 = %02f%%\n", paquet_count, paquet_count/256.0 * 100.0 );
				paquet_count = 0;
			}
		}
		#ifdef PRINT_DEBUG
			Serial.printf("receive: type:%d, seq:%d, off:%d, nb:%d\n", type, seq, leds_off, leds_nb);
		#endif
		switch (type) {
			case 'A': // Art-Net
				artnet.read(&packet);
				break;
			case LED_RGB_888:
			case LED_RGB_888_UPDATE:
				if (len - 6 == (leds_nb*3))
					memcpy(((uint8_t*)leds)+leds_off*3, data, leds_nb*3);
				else
					Serial.println("Invalid LED_RGB_888_UPDATE len");
				if (type == LED_RGB_888_UPDATE) {
					LEDS.show();
					frameCounter++;
				}
				break;
			case LED_RGB_565:
			case LED_RGB_565_UPDATE:
				if (len - 6 == (leds_nb*2)) {
					for (uint16_t i=0; i<leds_nb; i++) {
						dma_display.drawPixelRGB565((i + leds_off) % MATRIX_WIDTH, (i + leds_off) / MATRIX_WIDTH, data16[i]);
						// uint8_t r = ((((data16[i] >> 11) & 0x1F) * 527) + 23) >> 6;
						// uint8_t g = ((((data16[i] >> 5) & 0x3F) * 259) + 33) >> 6;
						// uint8_t b = (((data16[i] & 0x1F) * 527) + 23) >> 6;
						// leds[i] = r << 16 | g << 8 | b;
					}
				}
				else
					Serial.println("Invalid LED_RGB_565 len");
				if (type == LED_RGB_565_UPDATE) {
					// LEDS.show();
					dma_display.showDMABuffer();
					dma_display.flipDMABuffer();
					frameCounter++;
				}
			case LED_UPDATE:
				LEDS.show();
				frameCounter++;
				break;
			case REBOOT:
				Serial.println("Receive REBOOT");
				ESP.restart();
				break;
			case LED_RLE_888:
			case LED_RLE_888_UPDATE:
				for (int i=0; i<(len-6); i+=4) {
					#ifdef PRINT_RLE
						Serial.printf("  nb:%d {%02d, %02d, %02d}\n", data[i],data[i+1],data[i+2],data[i+3]);
					#endif
					for (int j=0;j<data[i]; j++) {
						// Serial.printf("  off:%d {%02d}\n", leds_off, data[i+2]);
						memcpy(leds+leds_off, data+i+1, 3);
						leds_off++;
					}
				}
				if (type == LED_RLE_888_UPDATE) {
					// LEDS.show();
					frameCounter++;
				}
				break;
			#ifdef USE_BROTLI
			case LED_BRO_888_UPDATE:
				BrotliDecoderDecompress( // need xTaskCreateUniversal(_udp_task, "async_udp", 8160,
					len-6,
					(const uint8_t *)data,
					&un_size,
					(uint8_t*)leds
				);
				LEDS.show();
				frameCounter++;
				break;
			#endif
			#ifdef USE_ZLIB
			case LED_Z_888_UPDATE:
				uncompress(
					(uint8_t*)(leds+leds_off),
					(long unsigned int*)&un_size,
					(const uint8_t *)data,
					len-6
				);
				LEDS.show();
				frameCounter++;
				break;
			case LED_Z_565:
			case LED_Z_565_UPDATE:
				memcpy(((uint8_t *)buffer) + leds_off, data, leds_nb);

				if (type==LED_Z_565_UPDATE) {
					uint16_t size = leds_off + leds_nb;
					file.write((uint8_t *)&size, 2);
					file.write(buffer, leds_off + leds_nb);
					int ret = uncompress(
						(uint8_t *)leds,
						(long unsigned int *)&un_size,
						(const uint8_t *)buffer,
						BUFFER_SIZE
					);

					Serial.printf("ret = %d, compress: %d, uncompress: %d\n", ret, 0, un_size);
				
					for (int i = 0; i < LED_TOTAL; i++)
						dma_display.drawPixel(i % 128, i / 128, ((uint16_t *)leds)[i]);
					dma_display.showDMABuffer();
					dma_display.flipDMABuffer();
					frameCounter++;
				}
				break;
			#endif
			default:
				Serial.printf("UDP packet type unknown: %d\n", type);
				break;
		}
	}
}

#endif

void taskTwo(void *parameter)
{

	#ifdef USE_ANIM
	for (;;) {
		switch (anim) {
		case ANIM_START:
			load_anim();
			break;
		case ANIM_PLAY:
			if (millis() - previousMillis >= wait)
			{
				previousMillis = millis();
				read_anim_frame();
			}
			break;
		case ANIM_UDP:
			break;
		}
		vTaskDelay(1 / portTICK_PERIOD_MS);
	}
	#endif

	Serial.println("Ending task 1");
	vTaskDelete(NULL);
}

void setup() {
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);

	#if defined(USE_WIFI_MANAGER) && defined(USE_RESET_BUTTON)
		pinMode(RESET_WIFI_PIN, INPUT);
		digitalWrite(RESET_WIFI_PIN, HIGH);
	#endif

	Serial.println("\n------------------------------");
	Serial.printf("  LEDs driver V%s\n", FIRMWARE_VERSION);
	Serial.printf("  Hostname: %s\n", hostname);
	int core = xPortGetCoreID();
	Serial.print("  Main code running on core ");
	Serial.println(core);
	Serial.println("------------------------------");

	#if defined(USE_CONFIG) || defined(USE_FTP) || defined(USE_ANIM)
		if(!SPIFFS.begin(true)){
			Serial.println("An Error has occurred while mounting SPIFFS");
			ESP.restart();
		}
		Serial.println("mounting SPIFFS OK");
	#endif
	
	root = SPIFFS.open("/");
	file = root.openNextFile();

	#ifdef USE_CONFIG


		printFile(filename);

		saveConfiguration(filename, config);
	#endif

		leds = (CRGB *)malloc(BUFFER_SIZE); //(CRGB*)malloc(sizeof(CRGB) * LED_TOTAL);
		buffer = (uint8_t *)malloc(BUFFER_SIZE);

#ifdef USE_8_OUTPUT
		LEDS.addLeds<LED_TYPE,LED_PORT_0,COLOR_ORDER>(leds, 0 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE,LED_PORT_1,COLOR_ORDER>(leds, 1 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE,LED_PORT_2,COLOR_ORDER>(leds, 2 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE,LED_PORT_3,COLOR_ORDER>(leds, 3 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE,LED_PORT_4,COLOR_ORDER>(leds, 4 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE,LED_PORT_5,COLOR_ORDER>(leds, 5 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE,LED_PORT_6,COLOR_ORDER>(leds, 6 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE,LED_PORT_7,COLOR_ORDER>(leds, 7 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
	#else
		LEDS.addLeds<LED_TYPE,LED_PORT_0,COLOR_ORDER>(leds, 0 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
	#endif
	LEDS.setBrightness(BRIGHTNESS);
	#ifdef USE_POWER_LIMITER
		LEDS.setMaxPowerInVoltsAndMilliamps(LED_VCC, LED_MAX_CURRENT);
	#endif
	Serial.println("LEDs driver start");

	dma_display.begin(R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN);
	dma_display.setPanelBrightness(32);
	dma_display.clearScreen();

	xTaskCreateUniversal(
		taskTwo,   /* Task function. */
		"TaskTwo", /* String with name of task. */
		8192*4,	   /* Stack size in bytes. */
		NULL,	   /* Parameter passed as input of the task */
		1,		   /* Priority of the task. */
		NULL,		   /* Task handle. */
		1
	);

#ifdef USE_WIFI_MANAGER

		wifiManager.setDebugOutput(false);
		wifiManager.setTimeout(180);
		wifiManager.setConfigPortalTimeout(180); // try for 3 minute
		wifiManager.setMinimumSignalQuality(15);
		wifiManager.setRemoveDuplicateAPs(true);
		wifiManager.setSaveConfigCallback(saveConfigCallback);

		#ifdef USE_CONFIG
			wifiManager.addParameter(&param_udp_port);
			// wifiManager.addParameter(&custom_text);
		#endif

		wifiManager.setClass("invert"); // dark theme

		// std::vector<const char *> menu = {"wifi","info","param","sep","restart","exit"};
		#ifdef USE_CONFIG
			std::vector<const char *> menu = {"param","wifi","info","sep","restart"};
		#else
			std::vector<const char *> menu = {"wifi","info","sep","restart"};
		#endif
		wifiManager.setMenu(menu);

		// wifiManager.setParamsPage(true);
		wifiManager.setCountry("US");
		wifiManager.setHostname(hostname);

		#ifdef USE_RESET_BUTTON

		if (!digitalRead(RESET_WIFI_PIN)) {
			Serial.printf("Start config\n");
			wifiManager.startConfigPortal("ESP32_LEDs");
		}
		else
		#endif
		{
			bool rest = wifiManager.autoConnect("ESP32_LEDs");

			if (rest) {
				Serial.println("Wifi connected");
			}
			else
			ESP.restart();
		}
	#elif defined(USE_AP)
		Serial.println("Setting AP (Access Point)");
		WiFi.softAP(AP_SSID, AP_PASSWORD);
		IPAddress IP = WiFi.softAPIP();
		Serial.print("AP IP address: ");
		Serial.println(IP);
	#elif defined(USE_WIFI)
		WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
		uint8_t retryCounter = 0;
		while (WiFi.status() != WL_CONNECTED) {
			delay(1000);
			Serial.println("Establishing connection to WiFi..");
			retryCounter++;
			if (retryCounter>5) {
				Serial.println("Could not connect, restarting");
				delay(10);
				ESP.restart();
			}
		}
	#endif

	Serial.print("Connected to:\t");
	Serial.println(WiFi.SSID());
	Serial.print("IP address:\t");
	Serial.println(WiFi.localIP());
	WiFi.setSleep(false);

	#ifdef USE_WIFI_MANAGER
		wifiManager.setConfigPortalBlocking(false);
		wifiManager.startWebPortal();
	#endif

	#ifdef USE_UDP
		if(udp.listen(UDP_PORT)) {
			Serial.printf("UDP server started on port %d\n", UDP_PORT);
			udp.onPacket(udp_receive);

			artnet.begin(NUM_STRIPS, UNI_BY_STRIP);
			artnet.setArtDmxCallback(onDmxFrame);
			artnet.setArtSyncCallback(onSync);

			#ifdef USE_AP
				artnet.setIp(WiFi.softAPIP());
			#endif
		}
	#endif

	#ifdef USE_OTA
		ArduinoOTA.setPort(OTA_PORT);
		ArduinoOTA.setHostname(hostname);

		ArduinoOTA.onStart([]() {
			anim = ANIM_UDP;
			String type;
			if (ArduinoOTA.getCommand() == U_FLASH)
				type = "sketch";
			else // U_SPIFFS
				type = "filesystem";

			// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
			Serial.println("Start updating " + type);
		});

		ArduinoOTA.onEnd([]() {
			Serial.println("\nEnd");
		});

		ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
			Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
			led_state+=1;
		});

		ArduinoOTA.onError([](ota_error_t error) {
			Serial.printf("Error[%u]: ", error);
			if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
			else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
			else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
			else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
			else if (error == OTA_END_ERROR) Serial.println("End Failed");
			ESP.restart();
		});

		ArduinoOTA.begin();
		Serial.printf("OTA server started on port %d\n", OTA_PORT);
	#endif

	#ifdef USE_FTP
		ftpSrv.begin(FTP_USER, FTP_PASS);
		Serial.println("FTP Server Start");
	#endif

	#ifdef PRINT_FPS
		timer.setTimer(5000, timerCallback, 6000); // Interval to measure FPS  (millis, function called, times invoked for 1000ms around 1 hr and half)
	#endif

	// file = SPIFFS.open("/test.Z565", FILE_WRITE);
}

void loop(void) {
	#ifdef USE_WIFI_MANAGER
		wifiManager.process();
	#endif

	#ifdef PRINT_FPS
		timer.run();
	#endif

	#ifdef USE_OTA
		ArduinoOTA.handle();
	#endif

	#ifdef USE_FTP
		ftpSrv.handleFTP();
	#endif

	#if defined(USE_WIFI_MANAGER) && defined(USE_RESET_BUTTON)
		if (!digitalRead(RESET_WIFI_PIN)) {
			Serial.printf("Press reset Wifi\nStart config\n");
			wifiManager.setConfigPortalBlocking(true);
			wifiManager.startConfigPortal("ESP32_LEDs");
			Serial.printf("Finish config reset\n");
			ESP.restart();
		}
	#endif


}

