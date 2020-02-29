OctoWifi-LEDs Controller
============

https://antoine.doussaud.org/esp32_LED

## Feature
 - Can wok with x8 WS2812b LEDs strip at the same time
 - Can drive 8000 WS2812 LEDs at 30Hz or 4000 WS2812 LEDs at 60Hz
 - Compatible Art-Net ( DMX512 )


## Roadmap

- Work
  - Protocol
    - [x] Art-net
    - [x] RGB888
    - [x] RGB565
    - [x] RLE888
    - [x] BRO888 (brotli)
    - [x] Z888 (zlib)


#
// #define USE_AP				// The driver start has a WiFi Acess point
// #define USE_WIFI				// The driver use WIFI_SSID and WIFI_PASSWORD
#define USE_WIFI_MANAGER		// The driver use Wifi manager

// #define USE_RESET_BUTTON		// Can reset Wifi manager with button

// #define USE_OTA					// Activate Over the Air Update
#define USE_ANIM				// activate animation in SPI filesysteme (need BROTLI)
#define USE_FTP					// activate FTP server (need USE ANIM)
// #define USE_8_OUTPUT			// active 8 LEDs output

#define USE_UDP
#define USE_BROTLI
#define USE_ZLIB

#define PRINT_FPS
// #define PRINT_DEBUG
// #define PRINT_DMX
// #define PRINT_RLE

#define FIRMWARE_VERSION	"1.0"

#define DEFAULT_HOSTNAME	"ESP32_LEDs"
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

const int START_UNI			= 0;
const int UNI_BY_STRIP		= 4;
const int LEDS_BY_UNI		= 170;
const int LED_BY_STRIP		= 512;	//(UNI_BY_STRIP*LEDS_BY_UNI)
const int LED_TOTAL			= (LED_BY_STRIP*NUM_STRIPS);

#define LED_VCC				5	// 5V
#define LED_MAX_CURRENT		500  // 2000mA

const int RESET_WIFI_PIN	= 23;

const int LED_PORT_0 		= 13; // 16
const int LED_PORT_1 		= 4;
const int LED_PORT_2 		= 2;
const int LED_PORT_3 		= 22;
const int LED_PORT_4 		= 19;
const int LED_PORT_5 		= 18;
const int LED_PORT_6 		= 21;
const int LED_PORT_7 		= 17;
