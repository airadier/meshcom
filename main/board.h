/**
 * MeshCom - Board definitions
 * M5StickC Plus 2 (default) vs generic DevKit
 */
#pragma once

/* Define BOARD_DEVKIT externally to target generic ESP32 DevKit */

#ifdef BOARD_DEVKIT

/* ---- Generic ESP32 DevKit ---- */
#define BOARD_NAME          "DevKit"
#define HAS_DISPLAY         0
#define LED_GPIO            2       /* onboard LED (most DevKits) */
#define LED_ACTIVE_LOW      0
#define BTN_A_GPIO          0       /* BOOT button */
#define BTN_B_GPIO          (-1)    /* no second button */

#else

/* ---- M5StickC Plus 2 ---- */
#define BOARD_NAME          "M5StickCPlus2"
#define HAS_DISPLAY         1
#define LED_GPIO            10
#define LED_ACTIVE_LOW      1
#define BTN_A_GPIO          37
#define BTN_B_GPIO          39

/* Display: ST7789V2, 135x240, SPI */
#define DISP_WIDTH          135
#define DISP_HEIGHT         240
#define DISP_SPI_HOST       SPI2_HOST
#define DISP_PIN_MOSI       15
#define DISP_PIN_CLK        13
#define DISP_PIN_CS         5
#define DISP_PIN_DC         14
#define DISP_PIN_RST        12
#define DISP_PIN_BL         27  /* backlight */

#endif /* BOARD_DEVKIT */

/* Common */
#define MESHCOM_VERSION     "0.1"
