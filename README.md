# Animation format:

| type | FPS | Frame size | Frame nb | frames data |
|-----:|----:|-----------:|---------:|:-----------:|
| u8   | u16 | u16        | u16      |     ?       |

## Frame Data type:
### Brotli:
 | size | brotli data   |
 |-----:|:-------------:|
 | u16  |compress RGB888|

 #### RGB888:
 | Red | Green | Blue | ... |
 |----:|------:|-----:|:---:|
 | u8  |  u8   | u8   | ... |

  #### RGB565:
 | Red | Green | Blue | ... |
 |----:|------:|-----:|:---:|
 | u5  |  u6   | u5   | ... |

### Prerequisites

sudo apt-get install luajit lua-socket luarocks

### Love

LÖVE is an *awesome* framework you can use to make 2D games in Lua.

To try this first get the love stable latest version. For Ubuntu/Debian just use the PPA:

sudo add-apt-repository ppa:bartbes/love-stable
sudo apt-get update

Edition note: This is an attempt to make this compile in Platformio. Still a work in progress