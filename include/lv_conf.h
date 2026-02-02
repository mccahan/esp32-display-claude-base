#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_INFO

/* Color settings */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Memory settings */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (128U * 1024U)

/* Display settings */
#define LV_HOR_RES_MAX 480
#define LV_VER_RES_MAX 480
#define LV_DPI_DEF 130

/* Feature usage */
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

/* Input device settings */
#define LV_INDEV_DEF_READ_PERIOD 30

/* Drawing */
#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_USE_GPU_STM32_DMA2D 0

/* Themes */
#define LV_USE_THEME_DEFAULT 1
#define LV_USE_THEME_BASIC 1

/* Fonts - enable commonly used sizes */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Text settings */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* Widget usage */
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_CANVAS 1
#define LV_USE_CHECKBOX 1
#define LV_USE_DROPDOWN 1
#define LV_USE_IMG 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_ROLLER 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TEXTAREA 1
#define LV_USE_TABLE 1
#define LV_USE_SPINNER 1

/* Layout */
#define LV_USE_ANIMATION 1
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* Extra libs */
#define LV_USE_QRCODE 1

#endif /* LV_CONF_H */
