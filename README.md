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
