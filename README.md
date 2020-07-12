OctoWifi-LEDs Controller
============

https://antoine.doussaud.org/esp32_LED

### You must install https://platformio.org/

## Feature
 - Can wok with x8 WS2812b LEDs strip at the same time
 - Can drive 8000 WS2812 LEDs at 30Hz or 4000 WS2812 LEDs at 60Hz
 - Compatible Art-Net ( DMX512 )
 - Compatible RGB888
 - Compatible RGB565
 - Compatible Z888

## Setting
You can edit  setting in `src/main.cpp`

### Wifi setting
#### Wifi Mode: ( use only one )
```C
#define USE_AP            // The driver start has a WiFi Acess point
#define USE_WIFI          // The driver use WIFI_SSID and WIFI_PASSWORD
#define USE_WIFI_MANAGER  // The driver use Wifi manager
```
- AP: the driver is an Wifi Acces Point. You don't need a router
- WIFI: you must write the SSID and the Password of your wifi directly in the code
- WIFI_MANAGER: The driver start in AP mode. You can then connect directly to it and select your wifi network using a web page

#### In AP mode you can edit the AP SSID and password with

```C
#define AP_SSID       "ESP32_LEDs_AP"
#define AP_PASSWORD   "WIFI_PASSWORD"
```

#### In Wifi  mode you can edit the router SSID and password with

```C
#define WIFI_SSID        "SSID"
#define WIFI_PASSWORD    "PASSWORD"
```

#### in Wifi manager mode you can use a button to reset the Wifi config

```C
#define USE_RESET_BUTTON  // Can reset Wifi manager with button'
const int RESET_WIFI_PIN = 23; // GPIO use for Reset button
```

## LEDs setting

#### You can change the LEDs type and color order
```C
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB
```

#### You can change the max brightness ( 0-255 )
```C
#define BRIGHTNESS  255
```
#### You can activate and set the current limitation here
```C
#define USE_POWER_LIMITER

#define LED_VCC          5    // 5V
#define LED_MAX_CURRENT  2400 // 2400mA
```

#### If you want to use 8 strip output you must activate

```C
#define USE_8_OUTPUT // Activate 8 LEDs output
```

#### You can edit the LEDs port here:
```C
const int LED_PORT_0 = 16;
const int LED_PORT_1 =  4;
const int LED_PORT_2 =  2;
const int LED_PORT_3 = 22;
const int LED_PORT_4 = 19;
const int LED_PORT_5 = 18;
const int LED_PORT_6 = 21;
const int LED_PORT_7 = 17;
```

#### If you don't care about art-net you can set simply set how many LEDs by strip here
```C
const int LED_BY_STRIP = 150;
```

#### If you want to use the driver as an art-net node you can edit
```C
const int START_UNI    = 0;
const int UNI_BY_STRIP = 4;
const int LEDS_BY_UNI  = 170;


const int LED_BY_STRIP = (UNI_BY_STRIP*LEDS_BY_UNI);
const int LED_TOTAL    = (LED_BY_STRIP*NUM_STRIPS);
```
