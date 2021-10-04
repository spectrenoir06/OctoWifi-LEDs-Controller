#include <WiFi.h>

#ifdef USE_FASTLED
	// #define FASTLED_ESP32_I2S
	// #define FASTLED_ALLOW_INTERRUPTS
	#include <FastLED.h>
#endif

// #define USE_AP				// The driver start has a WiFi Access point
// #define USE_WIFI				// The driver use WIFI_SSID and WIFI_PASSWORD
// #define USE_WIFI_MANAGER		// The driver use Wifi manager

//#define USE_RESET_BUTTON		// Can reset Wifi manager with button

// #define USE_POWER_LIMITER	// Activate power limitaton ( edit: LED_VCC and LED_MAX_CURRENT )
// #define USE_OTA				// Activate Over the Air Update
// #define USE_ANIM				// Activate animation in SPI filesysteme (need BROTLI)
// #define USE_FTP				// Activate FTP server (need USE ANIM)
// #define USE_CONFIG			// Activate config menu on WifiManger

// #define USE_8_OUTPUT			// Activate 8 LEDs output
// #define USE_HUB75
// #define USE_1_OUTPUT

// #define USE_UDP
// #define USE_ZLIB
// #define USE_SPIFFS
// #define USE_SD
// #define USE_SD_MMC

// #define MINIZ_USE_PSRAM

// #define PRINT_DEBUG

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
const int START_UNI = 0;
const int UNI_BY_STRIP = 4;
const int LEDS_BY_UNI = 170;

#define LED_VCC				5	// 5V
// #define LED_MAX_CURRENT		1000  // 2000mA

// const int RESET_WIFI_PIN = 17;//23;

#if defined(USE_8_OUTPUT) || defined(USE_1_OUTPUT)
	#define USE_FASTLED
#endif

#if defined(USE_HUB75)
	#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
	MatrixPanel_I2S_DMA *display = nullptr;
#endif

#if defined(USE_BAR)
	// #include "I2SClocklessLedDriver.h"
	#include "I2SClocklessVirtualLedDriver.h"
	int pins[3] = {
		LED_PORT_0,
		LED_PORT_1,
		LED_PORT_2
	};
	I2SClocklessVirtualLedDriver driver;
#endif

#ifdef USE_WIFI_MANAGER
	#include <WiFiManager.h>
#endif

#if defined(USE_CONFIG) || defined(USE_FTP) || defined(USE_ANIM)
	#ifdef USE_SD
		#include "FS.h"
		#include "SD.h"
		#include "SPI.h"
		const int SD_CS = 13;
		const int SD_MOSI = 15;
		const int SD_SCK = 14;
		const int SD_MISO = 2;
		#define filesyteme SD
		#ifdef USE_FTP
			#include "ESP32FtpServer.h"
		#endif
	#endif
	#ifdef USE_SD_MMC
		#include "FS.h"
		#include "SD_MMC.h"
		#define filesyteme SD_MMC
	#endif
	#ifdef USE_SPIFFS
		#include "SPIFFS.h"
		#define filesyteme SPIFFS
		#ifdef USE_FTP
			#include "ESP8266FtpServer.h"
		#endif
	#endif
#endif

#ifdef USE_CONFIG
	#include <ArduinoJson.h>
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

#ifdef PRINT_FPS
	#include <SimpleTimer.h>
#endif

enum UDP_PACKET {
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

	STOP = 11,

	// LED_RLE_888 = 11,
	// LED_RLE_888_UPDATE = 12,

	LED_BRO_888 = 13,
	LED_BRO_888_UPDATE = 14,

	LED_Z_888 = 15,
	LED_Z_888_UPDATE = 16,

	LED_Z_565 = 17,
	LED_Z_565_UPDATE = 18,
	LED_Z_8888 = 19,

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
uint8_t			anim_on = false;
static TaskHandle_t animeTaskHandle = NULL;


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
	uint8_t anim = ANIM_START;
	uint8_t next_anim = 0;
#else
	uint8_t		anim = ANIM_UDP;
#endif

char	hostname[50] = DEFAULT_HOSTNAME;
char	firmware[20] = FIRMWARE_VERSION;

struct Config {
	int	udp_port;
};

const char* filename = "/config.txt";
Config config;

uint8_t* leds;
uint8_t* buffer;
uint8_t brightness = BRIGHTNESS;

#ifdef USE_WIFI_MANAGER
	WiFiManager	wifiManager;
	#ifdef USE_CONFIG
		WiFiManagerParameter param_udp_port("udp_port", "UDP port", "6454", 6);
		// WiFiManagerParameter param_udp_port("LEDs type", "UDP port", "6454", 5);

	// WiFiManagerParameter custom_text("<select name=\"LEDs type\" id=\"leds_type\"><option value=\"WS2811 800kHz\">WS2811 800kHz</option><option value=\"WS2811 400kHz\">WS2811 400kHz</option><option value=\"WS2812\">WS2812</option><option value=\"WS2813\">WS2813</option><option value=\"SK6822\">SK6822</option></select>");
	#endif
#endif

#ifdef PRINT_FPS
	SimpleTimer	timer;
#endif

#ifdef USE_HUB75
	void flip_matrix() {
		display->flipDMABuffer();
	}
#endif

void set_brightness(int b) {
	brightness = constrain(b, 0, 255);
	Serial.printf("Brightness set to %d\n", brightness);
	#if defined(USE_FASTLED)
		LEDS.setBrightness(brightness);
	#elif defined(USE_BAR)
		driver.setBrightness(brightness);
	#elif defined(USE_HUB75)
		display->setBrightness8(brightness); //0-255
	#endif
}

void set_all_pixel(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
	anim = ANIM_UDP;
	delay(100);
	#if defined(USE_BAR)
		for (int i=0; i<LED_TOTAL; i++) {
			driver.setPixel(i, r, g, b, w);
		}
		driver.showPixels();
	#elif USE_HUB75
		display->fillScreenRGB888(r,g,b);
		flip_matrix();
	#endif
}


#ifdef USE_BLE
	#include <BLEDevice.h>
	#include <BLEUtils.h>
	#include <BLEServer.h>
	#include <BLE2902.h>

	#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
	#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
	#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

	BLEServer* pServer = NULL;
	BLECharacteristic* pTxCharacteristic;
	bool deviceConnected = false;
	bool oldDeviceConnected = false;
	uint8_t txValue = 0;

	class MyServerCallbacks : public BLEServerCallbacks {
		void onConnect(BLEServer* pServer) {
			deviceConnected = true;
		};

		void onDisconnect(BLEServer* pServer) {
			deviceConnected = false;
		}
	};

	class MyCallbacks : public BLECharacteristicCallbacks {
		void onWrite(BLECharacteristic* pCharacteristic) {
			std::string rxValue = pCharacteristic->getValue();

			if (rxValue.length() > 0 && rxValue[0] == '!') {
				switch (rxValue[1]) {
					case 'B':
						switch (rxValue[2]) {
							case '1':
								if (rxValue[3] == '1')
									next_anim = 1;
								break;
							case '5':
								if (rxValue[3] == '1') {
									set_brightness(brightness+10);
								}	
								break;
							case '6':
								if (rxValue[3] == '1') {
									set_brightness(brightness-10);
								}	
								break;
							case '2':
								if (rxValue[3] == '1') {
									set_all_pixel(0, 0, 0, 255);
								}	
								break;
							default:
								break;
						}
						break;
					case 'C':
						set_all_pixel(rxValue[2], rxValue[3], rxValue[4], 0);
						break;
					default:
						break;
				}
				Serial.print("Received Value: ");
				for (int i = 0; i < rxValue.length(); i++)
					Serial.print(rxValue[i]);
				Serial.println();
			}
		}
	};
#endif

unsigned long frameCounter = 0;
unsigned long frameLastCounter = frameCounter;

#ifdef PRINT_FPS
void timerCallback() {
	if (frameLastCounter != frameCounter) {
		Serial.printf("FPS: %lu Frames received: %lu\n", (frameCounter - frameLastCounter) / 5, frameCounter);
		frameLastCounter = frameCounter;
	}
}
#endif

#ifdef USE_ANIM
void load_anim() {
	Serial.println("Open animation");

	Serial.print("  FILE: ");
	Serial.print(file.name());
	Serial.print("\tSIZE: ");
	Serial.print(file.size() / (1024.0 * 1024.0));
	Serial.println(" Mo");

	if (file.size() > 0) {
		file.read(head, 5);

		fps = (head[2] << 8) | head[1];
		nb = (head[4] << 8) | head[3];
		wait = 1000.0 / fps;
		if (wait > 2000)
			wait = 100;

		Serial.printf("  FPS: %02d; nb: %04d format: %d\n", fps, nb, head[0]);
		anim = ANIM_PLAY;
	} else {
		anim = ANIM_UDP;
	}

}
#define MIN(a,b) (((a)<(b))?(a):(b))
#define BUF_SIZE 1024

void read_anim_frame() {
	uint16_t compress_size;
	unsigned long int un_size = 0;

	static uint8_t s_inbuf[BUF_SIZE];
	static uint8_t s_outbuf[BUF_SIZE];

	mz_stream stream;
	memset(&stream, 0, sizeof(stream));
	stream.next_in = s_inbuf;
	stream.avail_in = 0;
	stream.next_out = s_outbuf;
	stream.avail_out = BUF_SIZE;

	
	file.read((uint8_t*)&compress_size, 2);
	uint16_t infile_remaining = compress_size;

	if (mz_inflateInit(&stream)) {
		Serial.printf("inflateInit() failed!\n");
		return;
	}

	for (;;){
		int status;
		if (!stream.avail_in){
			uint n = MIN(BUF_SIZE, infile_remaining);

			if (file.read(s_inbuf, n) != n){
				printf("Failed reading from input file!\n");
				return;
			}

			stream.next_in = s_inbuf;
			stream.avail_in = n;
			infile_remaining -= n;
		}

		status = mz_inflate(&stream, MZ_SYNC_FLUSH);

		if ((status == MZ_STREAM_END) || (!stream.avail_out)){
			uint n = BUF_SIZE - stream.avail_out;
			#ifdef USE_HUB75
				memcpy(leds + un_size, s_outbuf, n);
			#else
				if (head[0] == LED_Z_565)
					memcpy(buffer + un_size, s_outbuf, n);
				else
					memcpy(leds + un_size, s_outbuf, n);
			#endif
			un_size+=n;
			stream.next_out = s_outbuf;
			stream.avail_out = BUF_SIZE;
		}

		if (status == MZ_STREAM_END)
			break;
		else if (status != MZ_OK){
			printf("inflate() failed with status %i!\n", status);
			return;
		}
	}

	if (mz_inflateEnd(&stream) != MZ_OK){
		printf("inflateEnd() failed!\n");
		return;
	}

	#if defined(USE_HUB75)
		if (head[0] == LED_Z_888)
			for (int i = 0; i < LED_TOTAL; i++)
				display->drawPixelRGB888(i % MATRIX_W, i / MATRIX_W, leds[i * 3], leds[i * 3 + 1], leds[i * 3 + 2]);
		else if (head[0] == LED_Z_565)
			for (int i = 0; i < LED_TOTAL; i++)
				display->drawPixel(i % MATRIX_W, i / MATRIX_W, ((uint16_t*)leds)[i]);
	#else
		if (head[0] == LED_Z_565) {
			for (int i = 0; i < LED_TOTAL; i++){
				uint8_t r = ((((((uint16_t*)buffer)[i] >> 11) & 0x1F) * 527) + 23) >> 6;
				uint8_t g = ((((((uint16_t*)buffer)[i] >> 5) & 0x3F) * 259) + 33) >> 6;
				uint8_t b = (((((uint16_t*)buffer)[i] & 0x1F) * 527) + 23) >> 6;
				#if defined(USE_BAR)
					driver.setPixel(i, r, g, b, 0);
				#elif defined(USE_FASTLED)
					leds[i] = r << 16 | g << 8 | b;
				#endif
			}
		}
	#endif

	#if defined(USE_HUB75)
		flip_matrix();
	#elif defined(USE_BAR)
		driver.showPixels();
	#elif defined(USE_FASTLED)
		LEDS.show();
	#endif

	frameCounter++;

	if (!file.available()) { // loop
		file.seek(0);
		anim = ANIM_START;
	}


	// if (!file.available()) {
	// 	anim = ANIM_START;
	// 	file.close();
	// 	file = root.openNextFile();
	// 	if (!file) {
	// 		root.close();
	// 		root = filesyteme.open("/");
	// 		file = root.openNextFile();
	// 	}
	// }
}
#endif

#ifdef USE_CONFIG
void saveConfiguration(const char* filename, const Config& config) {
	// Delete existing file, otherwise the configuration is appended to the file
	filesyteme.remove(filename);

	// Open file for writing
	File file = filesyteme.open(filename, FILE_WRITE);
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

void printFile(const char* filename) {
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
	void saveConfigCallback() {
		Serial.println("Should save config");
		shouldSaveConfig = true;
	}
#endif

uint8_t button_isPress = 0;

void playAnimeTask(void* parameter) {
	#ifdef USE_ANIM
		for (;;) {
			switch (anim) {
				case ANIM_START:
					load_anim();
					break;
				case ANIM_PLAY:
					if (millis() - previousMillis >= wait) {
						previousMillis = millis();
						read_anim_frame();
						// vTaskDelay(wait / portTICK_PERIOD_MS);
					}
					break;
				case ANIM_UDP:
					break;
			}
			if (digitalRead(0) == LOW || next_anim) {
				if (!button_isPress || next_anim) {
					button_isPress = 1;
					next_anim = 0;
					Serial.println("next_anim()");
					anim = ANIM_START;
					file.close();
					file = root.openNextFile();
					if (!file) {
						root.close();
						root = filesyteme.open("/");
						file = root.openNextFile();
					}
					vTaskDelay(20 / portTICK_PERIOD_MS);
				}
			} else {
				button_isPress = 0;
			}
			vTaskDelay(1 / portTICK_PERIOD_MS);
		}
	#endif

	Serial.println("Ending task playAnimeTask");
	vTaskDelete(NULL);
}

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
		Serial.printf("%d / 256 = %02f%%\n", paquet_count, paquet_count / 256.0 * 100.0);
		paquet_count = 0;
	}

	dmx_counter = sequence;

	if (length < 3 || universe < START_UNI) // || universe >(START_UNI + UNI_BY_STRIP * NUM_STRIPS)
		return;

	uint16_t off = (universe - START_UNI) * LEDS_BY_UNI * 3;

	memcpy(((uint8_t*)leds) + off, data, length);
}

void onSync(IPAddress remoteIP) {
	#ifdef PRINT_DMX
		Serial.println("DMX: Sync");
	#endif
		// LEDS.show();
}

void udp_receive(AsyncUDP_bigPacket packet) {
	int len = packet.length();
	if (len) {
		#ifdef PRINT_DEBUG
			Serial.printf("Received %d bytes from %s, port %d\n", len, packet.remoteIP().toString().c_str(), packet.remotePort());
		#endif
		uint8_t		*pqt = packet.data();
		uint8_t		type = pqt[0];
		uint8_t		seq = pqt[1];
		uint16_t	leds_off = *(uint16_t*)(pqt + 2);
		uint16_t	size     = *(uint16_t*)(pqt + 4);
		uint8_t*	data     =  (uint8_t*)&pqt[6];
		uint16_t*	data16   =  (uint16_t*)&pqt[6];
		size_t		un_size  = LED_TOTAL * 3;

		anim = ANIM_UDP;
		if (animeTaskHandle) {
			vTaskDelete(animeTaskHandle);
			animeTaskHandle = NULL;
		}

		if (type != 'A') {
			paquet_count++;
			if (seq == 255) {
				Serial.printf("%d / 256 = %02f%%\n", paquet_count, paquet_count / 256.0 * 100.0);
				paquet_count = 0;
			}
		}

		#ifdef PRINT_DEBUG
			Serial.printf("receive: type:%d, seq:%d, off:%d, size:%d\n", type, seq, leds_off, size);
		#endif

		switch (type) {
		case 'A': // Art-Net
			artnet.read(&packet);
			break;
		case LED_RGB_888:
		case LED_RGB_888_UPDATE:
			if (len - 6 == size) {
				#if defined(USE_HUB75)
					for (uint16_t i = 0; i < (size/3); i++)
						display->drawPixelRGB888((i + leds_off) % MATRIX_W, (i + leds_off) / MATRIX_W, data[i*3], data[i*3+1], data[i*3+2]);
				#else
					memcpy(((uint8_t*)leds) + leds_off * 3, data, size);
				#endif
			}
			else
				Serial.println("Invalid LED_RGB_888_UPDATE len");

			if (type == LED_RGB_888_UPDATE) {
				#if defined(USE_HUB75)
					flip_matrix();
				#elif defined (USE_FASTLED)
					LEDS.show();
				#endif
				frameCounter++;
			}
			break;
		case LED_RGB_565:
		case LED_RGB_565_UPDATE:
			if (len - 6 == size) {
				for (uint16_t i = 0; i < (size/2); i++) {
					#if defined(USE_HUB75)
						display->drawPixel((i + leds_off) % MATRIX_W, (i + leds_off) / MATRIX_W, data16[i]);
					#else
						uint8_t r = ((((data16[i] >> 11) & 0x1F) * 527) + 23) >> 6;
						uint8_t g = ((((data16[i] >> 5) & 0x3F) * 259) + 33) >> 6;
						uint8_t b = (((data16[i] & 0x1F) * 527) + 23) >> 6;
						#if defined(USE_BAR)
							driver.setPixel(i, r, g, b, 0);
						#elif defined(USE_FASTLED)
							leds[i] = r << 16 | g << 8 | b;
						#endif
					#endif
				}
			}
			else
				Serial.println("Invalid LED_RGB_565 len");

			if (type == LED_RGB_565_UPDATE) {
				#if defined(USE_HUB75)
					flip_matrix();
				#elif defined(USE_BAR)
					for (int j = 1; j < 48; j++)
						memcpy(((uint8_t*)leds) + 256 * j * 4, leds, 256 * 4);
					driver.showPixels();
				#elif defined(USE_FASTLED)
					LEDS.show();
				#endif
				frameCounter++;
			}
			break;
		case LED_UPDATE:
			#if defined(USE_HUB75)
				flip_matrix();
			#elif defined(USE_FASTLED)
				LEDS.show();
			#elif defined(USE_BAR)
				for (int j = 1; j < 48; j++)
					memcpy(((uint8_t*)leds) + 256 * j * 4, leds, 256 * 4);
				driver.showPixels();
			#endif
			frameCounter++;
			break;
		case REBOOT:
			Serial.println("Receive REBOOT");
			ESP.restart();
			break;


		#ifdef USE_ZLIB
			case LED_Z_888:
			case LED_Z_888_UPDATE:
				memcpy(((uint8_t*)buffer) + leds_off, data, size);

				if (type == LED_Z_888_UPDATE) {
					int ret = mz_uncompress(
						(uint8_t*)leds,
						(long unsigned int*) &un_size,
						(const uint8_t*)buffer,
						LED_TOTAL*3
					);

					if (ret) {
						Serial.printf("ret: %d == %s, compress: %d, uncompress: %d, r: %d\n", ret, mz_error(ret), 0, un_size, 0);
					} else {
						#if defined(USE_HUB75)
							for (int i = 0; i < LED_TOTAL; i++)
								display->drawPixelRGB888(i % MATRIX_W, i / MATRIX_W, leds[i * 3 + 0], leds[i * 3 + 1], leds[i * 3 + 2]);
							flip_matrix();
						#elif defined(USE_FASTLED)
							LEDS.show();
						#endif
						frameCounter++;
					}
				}
				break;
			case LED_Z_565:
			case LED_Z_565_UPDATE:
				#if defined(USE_HUB75)
					memcpy(((uint8_t*)buffer) + leds_off, data, size);
					if (type == LED_Z_565_UPDATE) {
						int ret = mz_uncompress(
							(uint8_t*)leds,
							(long unsigned int*) &un_size,
							(const uint8_t*)buffer,
							LED_TOTAL*3
						);

						if (ret) {
							Serial.printf("ret: %d == %s, compress: %d, uncompress: %d, r: %d\n", ret, mz_error(ret), 0, un_size, 0);
						} else {
							for (int i = 0; i < LED_TOTAL; i++) {
								display->drawPixel(i%128, i/128, ((uint16_t*)leds)[i]);
							}
							flip_matrix();
							frameCounter++;
						}
					}
				#else
					printf("Z565 is not supported with 24Bit LEDs\n");
				#endif
				break;
		#endif

		case STOP:
			#ifdef USE_ANIM
				if (anim_on) {
					anim = ANIM_PLAY;
					xTaskCreate(
						playAnimeTask,   /* Task function. */
						"playAnimeTask", /* String with name of task. */
						8192 * 4,  /* Stack size in bytes. */
						NULL,	   /* Parameter passed as input of the task */
						1,		   /* Priority of the task. */
						&animeTaskHandle	   /* Task handle. */
					);

				}
			#endif
			Serial.printf("stop: %d\n", type);
			break;


		default:
			Serial.printf("UDP packet type unknown: %d\n", type);
			break;
		}
	}
}

#endif // USE_UDP

void setup() {
	psramInit();
	Serial.begin(115200);
	// esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
	// WiFi.mode(WIFI_STA);

	pinMode(21, OUTPUT);
	digitalWrite(21, 1);
	pinMode(0, INPUT);

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


	#ifdef USE_CONFIG
		printFile(filename);
		saveConfiguration(filename, config);
	#endif

	leds = (uint8_t*)malloc(LED_TOTAL * LED_SIZE); //(CRGB*)ps_malloc(sizeof(CRGB) * LED_TOTAL);
	if (!leds)
		Serial.println("Can't allocate leds");
	buffer = (uint8_t*)malloc(LED_TOTAL*2);
	if (!buffer)
		Serial.println("Can't allocate buffer");

	#ifdef USE_8_OUTPUT
		LEDS.addLeds<LED_TYPE, LED_PORT_0, COLOR_ORDER>((CRGB*)leds, 0 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_1, COLOR_ORDER>((CRGB*)leds, 1 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_2, COLOR_ORDER>((CRGB*)leds, 2 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_3, COLOR_ORDER>((CRGB*)leds, 3 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_4, COLOR_ORDER>((CRGB*)leds, 4 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_5, COLOR_ORDER>((CRGB*)leds, 5 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_6, COLOR_ORDER>((CRGB*)leds, 6 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_7, COLOR_ORDER>((CRGB*)leds, 7 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
	#endif

	#ifdef USE_1_OUTPUT
		LEDS.addLeds<LED_TYPE, LED_PORT_0, COLOR_ORDER>((CRGB*)leds, 0 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
	#endif

	#ifdef USE_2_OUTPUT
		LEDS.addLeds<LED_TYPE, LED_PORT_0, COLOR_ORDER>((CRGB*)leds, 0 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_1, COLOR_ORDER>((CRGB*)leds, 1 * LED_BY_STRIP, LED_BY_STRIP).setCorrection(TypicalLEDStrip);
	#endif

	#ifdef USE_POWER_LIMITER
		LEDS.setMaxPowerInVoltsAndMilliamps(LED_VCC, LED_MAX_CURRENT);
	#endif

	Serial.println("LEDs driver start");
	
	#ifdef USE_BAR
		driver.initled((uint8_t*)leds, pins, CLOCK_PIN, LATCH_PIN);
	#endif

	// testFileIO(SD, "/music.Z565", 40, 1);

	#if defined(USE_HUB75)
		HUB75_I2S_CFG::i2s_pins _pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
		
		HUB75_I2S_CFG mxconfig(
			MATRIX_W,     // Module width
			MATRIX_H,     // Module height
			MATRIX_CHAIN, // chain length
			_pins         // pin mapping
		);

		mxconfig.double_buff = false;                   // use DMA double buffer (twice as much RAM required)
		// #ifndef DMA_DOUBLE_BUFF
		// #endif
		mxconfig.driver          = HUB75_I2S_CFG::SHIFTREG; // Matrix driver chip type - default is a plain shift register
		mxconfig.i2sspeed        = HUB75_I2S_CFG::HZ_10M;   // I2S clock speed
		mxconfig.clkphase        = true;                    // I2S clock phase
		mxconfig.latch_blanking  = MATRIX_LATCH_BLANK;      // How many clock cycles to blank OE before/after LAT signal change, default is 1 clock

		display = new MatrixPanel_I2S_DMA(mxconfig);

		display->begin();  // setup display with pins as pre-defined in the library
		// flip_matrix();
	#endif

	set_brightness(BRIGHTNESS);

	#if defined(USE_CONFIG) || defined(USE_FTP) || defined(USE_ANIM)

		#ifdef USE_SD
			// Initialize SD card
			SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
			for (int i=0; i<20; i++) {
				if (!filesyteme.begin(SD_CS, SPI)) {
					Serial.println("Card Mount Failed");
					anim_on = false;
					delay(10);
				} else {
					root = filesyteme.open("/");
					file = root.openNextFile();
					anim_on = true;
					break;
				}
			}
		#endif

		#ifdef USE_SD_MMC
			pinMode(2, INPUT_PULLUP);
			if (!filesyteme.begin("/sdcard", true)) {
				Serial.println("Card Mount Failed");
				anim_on = false;
			} else {
				root = filesyteme.open("/");
				file = root.openNextFile();
				anim_on = true;
			}
		#endif

		#ifdef USE_SPIFFS
			if (!filesyteme.begin(true)) {
				Serial.println("An Error has occurred while mounting SPIFFS");
				// ESP.restart();
				anim_on = false;
			} else {
				Serial.println("mounting SPIFFS OK");
				root = filesyteme.open("/");
				file = root.openNextFile();
				anim_on = true;
			}
		#endif
	#endif

	#ifdef USE_ANIM
		xTaskCreate(
			playAnimeTask,   /* Task function. */
			"playAnimeTask", /* String with name of task. */
			8192 * 5,  /* Stack size in bytes. */
			NULL,	   /* Parameter passed as input of the task */
			1,		   /* Priority of the task. */
			&animeTaskHandle	   /* Task handle. */
		);
	#endif


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
			std::vector<const char*> menu = { "param","wifi","info","sep","restart" };
		#else
			std::vector<const char*> menu = { "wifi","info","sep","restart" };
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
			if (retryCounter > 5) {
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
		if (udp.listen(UDP_PORT)) {
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

	#ifdef USE_FTP
		if (anim_on) {
			ftpSrv.begin(FTP_USER, FTP_PASS);
			Serial.println("FTP Server Start");
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
			led_state += 1;
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

	#ifdef PRINT_FPS
		timer.setTimer(5000, timerCallback, 6000); // Interval to measure FPS  (millis, function called, times invoked for 1000ms around 1 hr and half)
	#endif

	#ifdef USE_BLE
		Serial.println("Start BLE");
		// Create the BLE Device
		BLEDevice::init("Spectre Hat");

		// Create the BLE Server
		pServer = BLEDevice::createServer();
		pServer->setCallbacks(new MyServerCallbacks());

		// Create the BLE Service
		BLEService* pService = pServer->createService(SERVICE_UUID);

		// Create a BLE Characteristic
		pTxCharacteristic = pService->createCharacteristic(
			CHARACTERISTIC_UUID_TX,
			BLECharacteristic::PROPERTY_NOTIFY
		);

		pTxCharacteristic->addDescriptor(new BLE2902());

		BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
			CHARACTERISTIC_UUID_RX,
			BLECharacteristic::PROPERTY_WRITE
		);

		pRxCharacteristic->setCallbacks(new MyCallbacks());

		// Start the service
		pService->start();

		// Start advertising
		pServer->getAdvertising()->start();
		Serial.println("Waiting a client connection to notify...");
	#endif
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

	#ifdef USE_BLE
		// disconnecting
		if (!deviceConnected && oldDeviceConnected) {
			delay(500); // give the bluetooth stack the chance to get things ready
			pServer->startAdvertising(); // restart advertising
			Serial.println("start advertising");
			oldDeviceConnected = deviceConnected;
		}
		// connecting
		if (deviceConnected && !oldDeviceConnected) {
		// do stuff here on connecting
			oldDeviceConnected = deviceConnected;
		}
	#endif
}

