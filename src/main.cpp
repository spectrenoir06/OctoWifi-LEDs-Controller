#include "miniz.h"
#include <AnimatedGIF.h>


#include <LuaWrapper.h>

#if defined(USE_WIFI_MANAGER) || defined(USE_AP)
	#include <WiFi.h>
#endif

// #define PRINT_DEBUG


#ifdef USE_FASTLED
	// #define FASTLED_ESP32_I2S
	// #define FASTLED_ALLOW_INTERRUPTS
	#include <FastLED.h>
#endif

#define FIRMWARE_VERSION	"1.0"

#define DEFAULT_HOSTNAME	HOSTNAME

#define AP_SSID				HOSTNAME
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

#ifdef USE_WIFI_MANAGER
	#include <WiFiManager.h>
#endif

const int SD_CS = 13;
const int SD_MOSI = 15;
const int SD_SCK = 14;
const int SD_MISO = 2;

#if defined(USE_CONFIG) || defined(USE_FTP) || defined(USE_ANIM)
	#ifdef USE_SD
		#include "FS.h"
		#include "SD.h"
		#include "SPI.h"
		#define filesystem SD
		#ifdef USE_FTP
			#include "ESP32FtpServer.h"
		#endif
	#endif
	#ifdef USE_SD_MMC
		#include "FS.h"
		#include "SD_MMC.h"
		#define filesystem SD_MMC
		#ifdef USE_FTP
			#include "ESP32FtpServer.h"
		#endif
	#endif
	#ifdef USE_SPIFFS
		#include "SPIFFS.h"
		#define filesystem SPIFFS
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
static TaskHandle_t runLuaTaskHandle = NULL;


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
	uint8_t next_anim = 1;
	AnimatedGIF gif;
	uint32_t next_frame=0;
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

#include <NimBLEDevice.h>

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static NimBLEServer* pServer = NULL;
NimBLECharacteristic* pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;
uint8_t image_receive_mode = false;
uint8_t gif_receive_mode = false;
uint8_t lua_receive_mode = false;
uint32_t byte_to_store = 0;
uint32_t data_size = 0;
uint32_t img_receive_width = 0;
uint32_t img_receive_height = 0;
uint32_t img_receive_color_depth = 0;
File f_tmp;
uint8_t change_anim = 0;
int timeout_var = 0;
#define timeout_time 3000; // ms
int time_reveice = 0;
String lua_script = "";
uint8_t run_lua = false;
LuaWrapper *lua;
uint8_t list_send_mode = false;

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
	#elif defined(USE_HUB75)
		display->setBrightness8(brightness); //0-255
	#endif
}

void set_all_pixel(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
	anim = ANIM_UDP;
	delay(100);
	#if defined(USE_FASTLED)
		for (int i=0; i<LED_TOTAL; i++)
			((CRGB*)leds)[i] = r << 16 | g << 8 | b;
		LEDS.show();
	#elif USE_HUB75
		display->fillScreenRGB888(r,g,b);
		flip_matrix();
	#endif
}


static int lua_wrapper_updateDisplay(lua_State *lua_state) {
	flip_matrix();
	return 0;
}

static int lua_wrapper_drawPixel(lua_State *lua_state) {
	int x = luaL_checkinteger(lua_state, 1);
	int y = luaL_checkinteger(lua_state, 2);
	int r = luaL_checkinteger(lua_state, 3);
	int g = luaL_checkinteger(lua_state, 4);
	int b = luaL_checkinteger(lua_state, 5);
	display->drawPixelRGB888(x, y, r, g, b);
	return 0;
}

static int lua_wrapper_delay(lua_State *lua_state) {
	int a = luaL_checkinteger(lua_state, 1);
	delay(a);
	return 0;
}

static int lua_wrapper_millis(lua_State *lua_state) {
	lua_pushnumber(lua_state, (lua_Number) millis());
	return 1;
}

static int lua_wrapper_printBLE(lua_State *lua_state) {
	if (deviceConnected) {
		size_t len = 0;
		const char *str = luaL_checklstring(lua_state, 1, &len);
		pTxCharacteristic->setValue((uint8_t*)str, len);
		pTxCharacteristic->notify();
	}
	return 0;
}

static int lua_wrapper_clearDisplay(lua_State *lua_state) {
	display->clearScreen();
	return 0;
}

static int lua_wrapper_setTextColor(lua_State *lua_state) {
	int r = luaL_checkinteger(lua_state, 1);
	int g = luaL_checkinteger(lua_state, 2);
	int b = luaL_checkinteger(lua_state, 3);
	display->setTextColor(display->color565(r,g,b));
	return 0;
}

static int lua_wrapper_printText(lua_State *lua_state) {
	size_t len = 0;
	const char *str = luaL_checklstring(lua_state, 1, &len);
	display->print(str);
	return 0;
}

static int lua_wrapper_setCursor(lua_State *lua_state) {
	int x = luaL_checkinteger(lua_state, 1);
	int y = luaL_checkinteger(lua_state, 2);
	display->setCursor(x,y);
	return 0;
}

static int lua_wrapper_setTextSize(lua_State *lua_state) {
	int size = luaL_checkinteger(lua_state, 1);
	display->setTextSize(size);
	return 0;
}

static int lua_wrapper_fillRect(lua_State *lua_state) {
	int x = luaL_checkinteger(lua_state, 1);
	int y = luaL_checkinteger(lua_state, 2);

	int w = luaL_checkinteger(lua_state, 3);
	int h = luaL_checkinteger(lua_state, 4);

	int r = luaL_checkinteger(lua_state, 5);
	int g = luaL_checkinteger(lua_state, 6);
	int b = luaL_checkinteger(lua_state, 7);
	display->fillRect(x, y, w, h, r, g, b);
	return 0;
}

static int lua_wrapper_colorWheel(lua_State *lua_state) {
	uint8_t pos = luaL_checkinteger(lua_state, 1);
	uint8_t r,g,b;
	if(pos < 85) {
		r = pos * 3;
		g = 255 - pos * 3;
		b = 0;
	} else if(pos < 170) {
		pos -= 85;
		r = 255 - pos * 3;
		g = 0;
		b = pos * 3;
	} else {
		pos -= 170;
		r = 0;
		g = pos * 3;
		b = 255 - pos * 3;
	}
	lua_pushinteger(lua_state, (lua_Integer)r);
	lua_pushinteger(lua_state, (lua_Integer)g);
	lua_pushinteger(lua_state, (lua_Integer)b);
	return 3;
}


static int lua_wrapper_setTextWrap(lua_State *lua_state) {
	if (lua_isboolean(lua_state, 1))
		display->setTextWrap(lua_toboolean(lua_state, 1));
	return 0;
}




#ifdef USE_BLE


void kill_lua_task() {
	if (runLuaTaskHandle) {
		vTaskDelete(runLuaTaskHandle);
		runLuaTaskHandle = NULL;
	}
}


void runLuaTask(void* parameter) {
	{
		lua = new LuaWrapper();
		lua->Lua_register("clearDisplay", (const lua_CFunction) &lua_wrapper_clearDisplay);
		lua->Lua_register("updateDisplay", (const lua_CFunction) &lua_wrapper_updateDisplay);
		lua->Lua_register("drawPixel", (const lua_CFunction) &lua_wrapper_drawPixel);
		lua->Lua_register("fillRect", (const lua_CFunction) &lua_wrapper_fillRect);
		lua->Lua_register("colorWheel", (const lua_CFunction) &lua_wrapper_colorWheel);

		lua->Lua_register("delay", (const lua_CFunction) &lua_wrapper_delay);
		lua->Lua_register("millis", (const lua_CFunction) &lua_wrapper_millis);

		lua->Lua_register("setTextColor", (const lua_CFunction) &lua_wrapper_setTextColor);
		lua->Lua_register("setTextWrap", (const lua_CFunction) &lua_wrapper_setTextWrap);
		lua->Lua_register("printText", (const lua_CFunction) &lua_wrapper_printText);
		lua->Lua_register("setCursor", (const lua_CFunction) &lua_wrapper_setCursor);
		lua->Lua_register("setTextSize", (const lua_CFunction) &lua_wrapper_setTextSize);
		
		lua->Lua_register("printBLE", (const lua_CFunction) &lua_wrapper_printBLE);
		
		

		Serial.println("Start task runLuaTask");
		String str = lua_script;
		String ret = lua->Lua_dostring(&str);
		Serial.println(ret);
		if (deviceConnected) {
			char str[512];
			memset(str, 0, 512);
			strcat(str, "!S");
			strcat(str, ret.c_str());
			pTxCharacteristic->setValue((uint8_t*)str, strlen(str));
			pTxCharacteristic->notify();
		}
		delete lua;
		lua = NULL;
	}
	for(;;) {
		vTaskDelay(1 / portTICK_PERIOD_MS);
	};
	Serial.println("Ending task runLuaTask (should not happen oh no)");
	runLuaTaskHandle = NULL;
	vTaskDelete(NULL);
}

void print_progress(const char *str) {
	display->fillScreen(0);
	display->setCursor(4, 4);
	display->setTextSize(1);
	display->setTextColor(display->color565(255,255,255));
	display->printf(str);
	display->fillRect(4, 16, 64 - 4 * 2, 8, 0xFFFF);
	display->fillRect(
		4+1,
		16+1,
		map(byte_to_store, 0, data_size, 0, (64 - 4 * 2 - 2)),
		8 - 2,
		display->color444(0, 0x7b, 0xFF)
	);
	flip_matrix();
}


	class MyServerCallbacks : public NimBLEServerCallbacks {
		void onConnect(NimBLEServer* pServer, ble_gap_conn_desc *desc) {
			deviceConnected = true;
			pServer->updateConnParams(desc->conn_handle, 0x6, 0x6, 0, 100);
		};

		void onDisconnect(NimBLEServer* pServer) {
			deviceConnected = false;
		}

		void onMTUChange (uint16_t MTU, ble_gap_conn_desc *desc) {
			Serial.printf("MTU change: %d\n", MTU);
		}
	};
	

	class MyCallbacks : public NimBLECharacteristicCallbacks {
		void onWrite(NimBLECharacteristic* pCharacteristic) {
			std::string rxValue = pCharacteristic->getValue();
			Serial.printf("Received Value: %d:\n", rxValue.length());
			// for (int i = 0; i < rxValue.length(); i++)
				// Serial.print(rxValue[i]);
			// Serial.println();

			if (rxValue.length() > 0 && rxValue[0] == '!' && !image_receive_mode && !gif_receive_mode) {
				switch (rxValue[1]) {
					case 'B':
						switch (rxValue[2]) {
							case '1':
								#ifdef USE_ANIM
									if (rxValue[3] == '1')
										next_anim = 1;
								#endif
								break;
							case '5':
									set_brightness(brightness+2);
								break;
							case '6':
								if (rxValue[3] == '1')
									set_brightness(brightness-2);
								break;
							case '2':
								if (rxValue[3] == '1')
									set_all_pixel(0, 0, 0, 255);
								break;
							default:
								break;
						}
						break;
					case 'C':
						kill_lua_task();
						set_all_pixel(rxValue[2], rxValue[3], rxValue[4], 0);
						break;
					case 'I':
						{
							kill_lua_task();
							anim = ANIM_UDP;
							img_receive_color_depth = rxValue[2];
							img_receive_width = rxValue[3] + (rxValue[4] << 8);
							img_receive_height = rxValue[5] + (rxValue[6] << 8);
							Serial.printf("Image: depth: %d, %dX%d\n", img_receive_color_depth, img_receive_width, img_receive_height);
							image_receive_mode = true;
							byte_to_store = 0;
							data_size = img_receive_width * img_receive_height * (img_receive_color_depth / 8);
							for (int i = 7; i < rxValue.length(); i++) {
								leds[byte_to_store++] = rxValue[i];

							}
							timeout_var = millis() + timeout_time;
						}
						break;
					case 'L': // List files
						{
							list_send_mode = true;

							// memset(str, 0, 255);
							// strcat(str, "HELLLO !!!");
							// pTxCharacteristic->setValue((uint8_t*)str, strlen(str));
							// pTxCharacteristic->notify();
							// Serial.printf("send end !L!\n");
						}
						break;
					case 'D': // delete file
						{
							const char* data = rxValue.c_str();
							char str[255];
							memset(str, 0, 255);
							strcat(str, "/GIF/");
							strcat(str, data+2);
							Serial.printf("Remove %s\n", str);
							filesystem.remove(str);
						}
						break;
					case 'G': // add file
						{
							kill_lua_task();
							anim = ANIM_UDP;
							gif_receive_mode = true;
							const char* data = rxValue.c_str();
							char str[255];
							memset(str, 0, 255);
							strcat(str, "/GIF/");
							strcat(str, data + 2 + 4);
							Serial.printf("add %s\n", data + 2 + 4);
							f_tmp = filesystem.open(str, "w", true);
							int len = strlen(data + 2 + 4);
							data_size = *(uint32_t*)(data + 2);
							byte_to_store = 0;
							Serial.printf("gif size = %d\n", data_size);
							for (int i = 2 + 4 + len + 1; i < rxValue.length(); i++) {
								f_tmp.write(rxValue[i]);
								byte_to_store++;
							}
							timeout_var = millis() + timeout_time;
							time_reveice = millis();
							flip_matrix();

						}
						break;
					case 'P':
						{
							const char* data = rxValue.c_str();
							char str[255];
							memset(str, 0, 255);
							strcat(str, "/GIF/");
							strcat(str, data+2);
							char *ptr = strchr(str, '\n');
							if (ptr)
								*ptr = 0;
							Serial.printf("Open %s\n", str);
							file.close();
							file = filesystem.open(str);
							anim = ANIM_START;
						}
						break;
					case 'S':
						{
							anim = ANIM_UDP;
							lua_receive_mode = true;
							const char* data = rxValue.c_str();
							data_size = *(uint32_t*)(data + 2);
							Serial.printf("load lua size:%d\n", data_size);
							byte_to_store = 0;
							kill_lua_task();
							lua_script = "";
							for (int i = 2 + 4; i < rxValue.length(); i++) {
								lua_script += rxValue[i];
								byte_to_store++;
							}
							timeout_var = millis() + timeout_time;
							time_reveice = millis();
						}
						break;
					default:
						break;
				}
			}
			else if (image_receive_mode) {
				for (int i = 0; i < rxValue.length(); i++) {
					if (byte_to_store < (LED_TOTAL * LED_SIZE))
						leds[byte_to_store] = rxValue[i];
					byte_to_store++;
				}
			}
			else if (gif_receive_mode) {
				for (int i = 0; i < rxValue.length(); i++) {
					f_tmp.write(rxValue[i]);
					byte_to_store++;
				}
			}
			else if (lua_receive_mode) {
				for (int i = 0; i < rxValue.length(); i++) {
					lua_script += rxValue[i];
					byte_to_store++;
				}
			}

			if (image_receive_mode) {
				Serial.printf("Byte receive: %d, wait: %d\n", byte_to_store, data_size - byte_to_store);
				timeout_var = millis() + timeout_time;
				if (byte_to_store >= data_size) {
					Serial.printf("Image complete\n");
					display->fillScreenRGB888(0, 0, 0);
					for (int i = 0; i < (img_receive_width * img_receive_height); i++) {
						if (img_receive_color_depth == 16)
							display->drawPixel((i) % img_receive_width, (i) / img_receive_width, leds[i * 2] + (leds[i * 2 + 1] << 8));
						else
							display->drawPixelRGB888((i) % img_receive_width, (i) / img_receive_width, leds[i*3], leds[i*3+1], leds[i*3+2]);
					}
					flip_matrix();
					image_receive_mode = false;
					timeout_var = 0;
				}
				else {
					print_progress("load img:");
				}
			}

			if (gif_receive_mode) {
				Serial.printf("Byte receive: %d, wait: %d\n", byte_to_store, data_size - byte_to_store);
				timeout_var = millis() + timeout_time;
				if (byte_to_store >= data_size) {
					gif_receive_mode = false;
					Serial.printf("receive GIF OK\n");
					const char* tmp = f_tmp.path();
					file = filesystem.open(tmp);
					anim = ANIM_START;
					f_tmp.close();
					timeout_var = 0;
					Serial.printf("time to receive gif: %dms\n", millis() - time_reveice);
				}
				else {
					print_progress("load gif:");
				}
			}
			

			if (lua_receive_mode) {
				Serial.printf("Byte receive: %d, wait: %d\n", byte_to_store, data_size - byte_to_store);
				timeout_var = millis() + timeout_time;
				if (byte_to_store >= data_size) {
					lua_receive_mode = false;
					Serial.printf("receive Lua OK\n");
					Serial.printf("time to receive Lua: %dms\n", millis() - time_reveice);
					timeout_var = 0;
					if (lua) {
						delete lua;
						lua = NULL;
					}
					Serial.println("[APP] Free memory: " + String(esp_get_free_heap_size()) + " bytes");
					BaseType_t result = xTaskCreatePinnedToCore(
						runLuaTask,   /* Task function. */
						"runLuaTask", /* String with name of task. */
						1024 * 20,  /* Stack size in bytes. */
						NULL,	   /* Parameter passed as input of the task */
						1,		   /* Priority of the task. */
						&runLuaTaskHandle,	   /* Task handle. */
						1
					);
					Serial.printf("xTaskCreatePinnedToCore returned %d\n", result);
				}
				else {
					print_progress("load lua:");
				}
			}
		}
	};
#endif


#ifdef USE_GIF

	float gif_scale;
	int gif_off_x;
	int gif_off_y;
	File f;

	// Draw a line of image directly on the LED Matrix
	void GIFDraw(GIFDRAW* pDraw) {
		uint8_t* s;
		uint16_t* d, * usPalette, usTemp[320];
		int x, y, iWidth;

		iWidth = pDraw->iWidth;
		if (iWidth > MATRIX_WIDTH)
			iWidth = MATRIX_WIDTH;

		usPalette = pDraw->pPalette;
		y = pDraw->iY + pDraw->y; // current line

		s = pDraw->pPixels;
		if (pDraw->ucDisposalMethod == 2) // restore to background color
		{
			for (x = 0; x < iWidth; x++) {
				if (s[x] == pDraw->ucTransparent)
					s[x] = pDraw->ucBackground;
			}
			pDraw->ucHasTransparency = 0;
		}
		// Apply the new pixels to the main image
		if (pDraw->ucHasTransparency) // if transparency used
		{
			uint8_t* pEnd, c, ucTransparent = pDraw->ucTransparent;
			int x, iCount;
			pEnd = s + pDraw->iWidth;
			x = 0;
			iCount = 0; // count non-transparent pixels
			while (x < pDraw->iWidth) {
				c = ucTransparent - 1;
				d = usTemp;
				while (c != ucTransparent && s < pEnd) {
					c = *s++;
					if (c == ucTransparent) // done, stop
					{
						s--; // back up to treat it like transparent
					}
					else // opaque
					{
						*d++ = usPalette[c];
						iCount++;
					}
				} // while looking for opaque pixels
				if (iCount) // any opaque pixels?
				{
					for (int xOffset = 0; xOffset < iCount; xOffset++) {
						display->drawPixel(x + xOffset, y, usTemp[xOffset]); // 565 Color Format
					}
					x += iCount;
					iCount = 0;
				}
				// no, look for a run of transparent pixels
				c = ucTransparent;
				while (c == ucTransparent && s < pEnd) {
					c = *s++;
					if (c == ucTransparent)
						iCount++;
					else
						s--;
				}
				if (iCount) {
					x += iCount; // skip these
					iCount = 0;
				}
			}
		}
		else // does not have transparency
		{
			s = pDraw->pPixels;
			// Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
			for (x = 0; x < pDraw->iWidth; x++) {
				display->drawPixel(x, y, usPalette[*s++]); // color 565
			}
		}
	} /* GIFDraw() */

	void* GIFOpenFile(const char* fname, int32_t* pSize) {
		f = filesystem.open(fname);
		if (f) {
			*pSize = f.size();
			return (void*)&f;
		}
		return NULL;
	} /* GIFOpenFile() */

	void GIFCloseFile(void* pHandle) {
		File* f = static_cast<File*>(pHandle);
		if (f != NULL)
			f->close();
		} /* GIFCloseFile() */

	int32_t GIFReadFile(GIFFILE* pFile, uint8_t* pBuf, int32_t iLen) {
		int32_t iBytesRead;
		iBytesRead = iLen;
		File* f = static_cast<File*>(pFile->fHandle);
		// Note: If you read a file all the way to the last byte, seek() stops working
		if ((pFile->iSize - pFile->iPos) < iLen)
			iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
		if (iBytesRead <= 0)
			return 0;
		iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
		pFile->iPos = f->position();
		return iBytesRead;
	} /* GIFReadFile() */

	int32_t GIFSeekFile(GIFFILE* pFile, int32_t iPosition) {
		int i = micros();
		File* f = static_cast<File*>(pFile->fHandle);
		f->seek(iPosition);
		pFile->iPos = (int32_t)f->position();
		i = micros() - i;
	  //  Serial.printf("Seek time = %d us\n", i);
		return pFile->iPos;
	} /* GIFSeekFile() */

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
	Serial.printf("Open animation: '%s'\n", file.path());

	display->clearScreen();
	if (gif.open(file.path(), GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
		Serial.print("load anim: ");
		Serial.print(file.name());
		Serial.print("\tsize: ");
		Serial.print(file.size() / (1024.0 * 1024.0));
		Serial.println(" Mo");
		// gif_scale = 1;//(float)min(MATRIX_W, MATRIX_H) / max(gif.getCanvasWidth(), gif.getCanvasHeight());
		// gif_off_x = ((int)MATRIX_W - gif.getCanvasWidth() * gif_scale) / 2;
		// gif_off_y = ((int)MATRIX_H - gif.getCanvasHeight() * gif_scale) / 2;

		// if (gif_off_x < 0) gif_off_x = 0;
		// if (gif_off_y < 0) gif_off_y = 0;

		// Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
		// Serial.flush();

		// Serial.printf("  FPS: %02d; nb: %04d format: %d\n", fps, nb, head[0]);

		#ifdef USE_BLE
			if (deviceConnected) {
				char str[100];
				memset(str, 0, 100);
				strcat(str, "!P");
				strcat(str, file.name());
				strcat(str, "\r\n");
				pTxCharacteristic->setValue((uint8_t*)str, strlen(str));
				pTxCharacteristic->notify();
			}
		#endif

		anim = ANIM_PLAY;
	} else {
		anim = ANIM_UDP;
	}

}
#define MIN(a,b) (((a)<(b))?(a):(b))
#define BUF_SIZE (256*1)

void read_anim_frame() {
	

	int t = millis();
	int i;
	// display->clearScreen();
	if (gif.playFrame(false, &i)) {
		next_frame = t + i;
		flip_matrix();
	}
	else {
		gif.reset();
		// temp.close();
		// temp = root.openNextFile();
	}


	frameCounter++;

	// if (!file.available()) { // loop
	// 	file.seek(0);
	// 	anim = ANIM_START;
	// }


	// if (!file.available()) {
	// 	anim = ANIM_START;
	// 	file.close();
	// 	file = root.openNextFile();
	// 	if (!file) {
	// 		root.close();
	// 		root = filesystem.open("/");
	// 		file = root.openNextFile();
	// 	}
	// }
}
#endif

#ifdef USE_CONFIG
void saveConfiguration(const char* filename, const Config& config) {
	// Delete existing file, otherwise the configuration is appended to the file
	filesystem.remove(filename);

	// Open file for writing
	File file = filesystem.open(filename, FILE_WRITE);
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
		gif.begin(LITTLE_ENDIAN_PIXELS);
		File root;
		root = filesystem.open("/GIF");

		if (root) {
			for (;;) {
				if (digitalRead(0) == LOW || next_anim) {
					if (!button_isPress || next_anim) {
						button_isPress = 1;
						next_anim = 0;
						anim = ANIM_START;
						file.close();
						file = root.openNextFile();
						if (!file) {
							root.close();
							root = filesystem.open("/GIF");
							file = root.openNextFile();
						}
						// vTaskDelay(20 / portTICK_PERIOD_MS);
					}
				} else
					button_isPress = 0;

				switch (anim) {
					case ANIM_START:
						load_anim();
						break;
					case ANIM_PLAY:
						if (millis() >= next_frame ) {
							previousMillis = millis();
							read_anim_frame();
						}
						break;
					case ANIM_UDP:
						if (timeout_var != 0 && millis() > timeout_var) {
							anim = ANIM_START;
							image_receive_mode = false;
							gif_receive_mode = false;
							Serial.printf("timemout\n");
							timeout_var = 0;
						}
						break;
				}
				if (list_send_mode) {
					list_send_mode = false;
					Serial.printf("Print list files:\n");
					File tmp_root = filesystem.open("/GIF");
					File tmp_file = tmp_root.openNextFile();
					char str[255];
					while(tmp_file) {
						memset(str, 0, 255);
						strcat(str, "!L");
						strcat(str, tmp_file.name());
						pTxCharacteristic->setValue((uint8_t*)str, strlen(str));
						pTxCharacteristic->notify();
						Serial.println(str);
						tmp_file = tmp_root.openNextFile();
					}
					memset(str, 0, 255);
					strcat(str, "!L!");
					pTxCharacteristic->setValue((uint8_t*)str, strlen(str));
					pTxCharacteristic->notify();
				}
				vTaskDelay(1 / portTICK_PERIOD_MS);
			}
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
				for (uint32_t i = 0; i < (size/2); i++) {
					#if defined(USE_HUB75)
						display->drawPixel((i + leds_off) % MATRIX_W, (i + leds_off) / MATRIX_W, data16[i]);
					#else
						uint8_t r = ((((data16[i] >> 11) & 0x1F) * 527) + 23) >> 6;
						uint8_t g = ((((data16[i] >> 5) & 0x3F) * 259) + 33) >> 6;
						uint8_t b = (((data16[i] & 0x1F) * 527) + 23) >> 6;
						#if defined(USE_FASTLED)
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
							#if defined(USE_HUB75)
								for (int i = 0; i < LED_TOTAL; i++) {
									display->drawPixel(i%160, i/160, ((uint16_t*)leds)[i]);
								}
								flip_matrix();
							#endif
							frameCounter++;
						}
					}
				// #else
					// printf("Z565 is not supported with 24Bit LEDs\n");
				// #endif
				break;
		#endif

		case STOP:
			#ifdef USE_ANIM
				if (anim_on) {
					anim = ANIM_PLAY;
					xTaskCreatePinnedToCore(
						playAnimeTask,   /* Task function. */
						"playAnimeTask", /* String with name of task. */
						8192 * 2,  /* Stack size in bytes. */
						NULL,	   /* Parameter passed as input of the task */
						1,		   /* Priority of the task. */
						&animeTaskHandle,	   /* Task handle. */
						1
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

	// pinMode(21, OUTPUT);
	// digitalWrite(21, 1);
	// pinMode(0, INPUT);
	// digitalWrite(0, 1);

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

	leds = (uint8_t*)malloc(LED_TOTAL * LED_SIZE); //(CRGB*)malloc(sizeof(CRGB) * LED_TOTAL);
	if (!leds)
		Serial.println("Can't allocate leds");
	buffer = (uint8_t*)malloc(LED_TOTAL * 2);
	if (!buffer)
		Serial.println("Can't allocate buffer");

	#ifdef USE_8_OUTPUT
		LEDS.addLeds<LED_TYPE, LED_PORT_0, COLOR_ORDER>((CRGB*)leds, 0 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_1, COLOR_ORDER>((CRGB*)leds, 1 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_2, COLOR_ORDER>((CRGB*)leds, 2 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_3, COLOR_ORDER>((CRGB*)leds, 3 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_4, COLOR_ORDER>((CRGB*)leds, 4 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_5, COLOR_ORDER>((CRGB*)leds, 5 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_6, COLOR_ORDER>((CRGB*)leds, 6 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_7, COLOR_ORDER>((CRGB*)leds, 7 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
	#endif

	#ifdef USE_1_OUTPUT
		LEDS.addLeds<LED_TYPE, LED_PORT_0, COLOR_ORDER>((CRGB*)leds, 0 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
	#endif

	#ifdef USE_2_OUTPUT
		LEDS.addLeds<LED_TYPE, LED_PORT_0, COLOR_ORDER>((CRGB*)leds, 0 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
		LEDS.addLeds<LED_TYPE, LED_PORT_1, COLOR_ORDER>((CRGB*)leds, 1 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
	#endif

	#ifdef USE_POWER_LIMITER
		LEDS.setMaxPowerInVoltsAndMilliamps(LED_VCC, LED_MAX_CURRENT);
	#endif

	Serial.println("LEDs driver start");

	#if defined(USE_HUB75)
		HUB75_I2S_CFG::i2s_pins _pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
		
		HUB75_I2S_CFG mxconfig(
			MATRIX_W,     // Module width
			MATRIX_H,     // Module height
			MATRIX_CHAIN, // chain length
			_pins         // pin mapping
		);

		mxconfig.double_buff = true;                   // use DMA double buffer (twice as much RAM required)
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
				if (!filesystem.begin(SD_CS, SPI)) {
					Serial.println("Card Mount Failed");
					anim_on = false;
						display->fillScreen(0);
						display->setCursor(4, 4);
						display->setTextSize(1);
						display->setTextColor(display->color565(255,255,255));
						display->printf("Can't mnt");
						display->setCursor(4, 4+9);
						display->printf("SD Card!");
						flip_matrix();
					delay(10);
				} else {
					// root = filesystem.open("/");
					// file = root.openNextFile();
					anim_on = true;
					break;
				}
			}
		#endif

		#ifdef USE_SD_MMC
			pinMode(2, INPUT_PULLUP);
			pinMode(15, PULLUP);
			pinMode(14, PULLUP);
			pinMode(13, PULLUP);
			if (!filesystem.begin("/sdcard", true)) {
				Serial.println("Card Mount Failed");
				anim_on = false;
			} else {
				// root = filesystem.open("/");
				// file = root.openNextFile();
				anim_on = true;
			}
		#endif

		#ifdef USE_SPIFFS
			if (!filesystem.begin(true)) {
				Serial.println("An Error has occurred while mounting SPIFFS");
				// ESP.restart();
				anim_on = false;
			} else {
				Serial.println("mounting SPIFFS OK");
				// root = filesystem.open("/");
				// file = filesystem.open("/start.Z565", "r");
				// if (!file.available())
				// 	file = root.openNextFile();
				anim_on = true;
			}
		#endif
	#endif

	#ifdef USE_ANIM
			xTaskCreatePinnedToCore(
			playAnimeTask,   /* Task function. */
			"playAnimeTask", /* String with name of task. */
			8192 * 2,  /* Stack size in bytes. */
			NULL,	   /* Parameter passed as input of the task */
			5,		   /* Priority of the task. */
			&animeTaskHandle,	   /* Task handle. */
			1
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
				wifiManager.startConfigPortal(HOSTNAME);
			}
			else
		#endif
		{
			bool rest = wifiManager.autoConnect(HOSTNAME);

			if (rest) {
				Serial.println("Wifi connected");
			}
			else
				ESP.restart();
		}
	#elif defined(USE_AP)
		Serial.println("\nSetting AP (Access Point)");
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

	#if defined(USE_WIFI_MANAGER)
		Serial.print("\nConnected to:\t");
		Serial.println(WiFi.SSID());
		Serial.print("IP address:\t");
		Serial.println(WiFi.localIP());
		WiFi.setSleep(false);
	#endif

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
		NimBLEDevice::init(HOSTNAME);
		NimBLEDevice::setPower(ESP_PWR_LVL_P9);

		// NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);

		// Create the BLE Server
		pServer = NimBLEDevice::createServer();
		pServer->setCallbacks(new MyServerCallbacks());

		// Create the BLE Service
		NimBLEService* pService = pServer->createService(SERVICE_UUID);

		// Create a BLE Characteristic
		pTxCharacteristic = pService->createCharacteristic(
			CHARACTERISTIC_UUID_TX,
			NIMBLE_PROPERTY::NOTIFY
		);

		BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
			CHARACTERISTIC_UUID_RX,
			NIMBLE_PROPERTY::WRITE
		);

		pRxCharacteristic->setCallbacks(new MyCallbacks());

		// Start the service
		pService->start();

		BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
		// pAdvertising->setAppearance(0x7<<6); // glasses
		pAdvertising->setAppearance(0x01F << 6 | 0x06); // LEDs 

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
			wifiManager.startConfigPortal(HOSTNAME);
			Serial.printf("Finish config reset\n");
			ESP.restart();
		}
	#endif
}

