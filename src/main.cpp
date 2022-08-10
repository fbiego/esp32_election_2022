#include <Arduino.h>

#include <lvgl.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Time.h>
#include <Arduino_GFX_Library.h>
#include "main.h"

///////////////////// VARIABLES ////////////////////
lv_obj_t *ui_Screen1;
lv_obj_t *ui_Bar1;
lv_obj_t *ui_Bar2;
lv_obj_t *ui_Bar3;
lv_obj_t *ui_Bar4;
lv_obj_t *ui_Label1;
lv_obj_t *ui_Label2;
lv_obj_t *ui_Label3;
lv_obj_t *ui_Label4;
lv_obj_t *ui_Label5;
lv_obj_t *ui_Label6;
lv_obj_t *ui_Label7;
lv_obj_t *ui_Label8;
lv_obj_t *ui_Label9;

Arduino_DataBus *bus = new Arduino_ESP32SPI(21 /* DC */, 22 /* CS */, 18 /* SCK */, 23 /* MOSI */, -1 /* MISO */, VSPI /* spi_num */);
Arduino_GC9A01 *gfx = new Arduino_GC9A01(bus, 19, 0 /* rotation */, true /* IPS */);

WiFiMulti wifiMulti;
ESP32Time rtc(3600 * 3);

/* Change to your screen resolution */
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

lv_color_t convertColor(String hexstring);

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
  lv_disp_flush_ready(disp);
}

void setup()
{
  Serial.begin(115200);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASS);
  gfx->begin();
  gfx->fillScreen(BLACK);

  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(ledPin, ledChannel);
  ledcWrite(ledChannel, 10);

  screenWidth = gfx->width();
  screenHeight = gfx->height();

  Serial.print("Width: ");
  Serial.print(screenWidth);
  Serial.print("\tHeight: ");
  Serial.println(screenHeight);

  lv_init();

  disp_draw_buf = (lv_color_t *)malloc(sizeof(lv_color_t) * screenWidth * 10);
  if (!disp_draw_buf)
  {
    Serial.println("LVGL disp_draw_buf allocate failed!");
  }
  else
  {
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * 10);

    /* Initialize the display */
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Initialize the (dummy) input device driver */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);

    lv_disp_t *display = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(display, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                              true, LV_FONT_DEFAULT);
    lv_disp_set_theme(display, theme);
    ui_Screen1_screen_init();
    lv_disp_load_scr(ui_Screen1);
    Serial.println("Setup done");
  }

  while ((wifiMulti.run() != WL_CONNECTED))
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println("Connected");
  getTime();
}

void loop()
{
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);

  lv_label_set_text(ui_Label9, rtc.getTime("%H:%M").c_str());

  if (rtc.getMinute() != mn)
  {
    mn = rtc.getMinute();
    Serial.println("Checking results");
    getResults();
  }
}

void getTime()
{
  long t = 0;
  HTTPClient http;
  http.begin("https://iot.fbiego.com/api/v1/time");
  int httpCode = http.GET();
  String payload = http.getString();
  Serial.println(payload);
  DynamicJsonDocument json(100);
  deserializeJson(json, payload);
  t = json["timestamp"].as<long>();
  rtc.setTime(t);

  http.end();
}

void getResults()
{
  HTTPClient http;
  http.begin("https://api.fbiego.com/v1/election/");
  http.setTimeout(20000);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    String payload = http.getString();
    Serial.println(httpCode);
    // Serial.println(payload);
    DynamicJsonDocument json(2048);
    DeserializationError error = deserializeJson(json, payload);
    if (!error)
    {
      for (int x = 0; x < 4; x++)
      {
        Serial.println(json[x]["candidate_popular_name"].as<String>());
        Serial.println(json[x]["party_abbreviation"].as<String>());
        Serial.println(json[x]["party_color"].as<String>());
        Serial.println(json[x]["total_votes"].as<int>());
        Serial.println(json[x]["percentage"].as<float>());
        Serial.println();
      }
      lv_label_set_text(ui_Label1, json[0]["party_abbreviation"].as<String>().c_str());
      lv_label_set_text(ui_Label5, String(json[0]["total_votes"].as<int>()).c_str());
      lv_bar_set_value(ui_Bar1, int(json[0]["percentage"].as<float>() * 100.0), LV_ANIM_OFF);
      lv_obj_set_style_bg_color(ui_Bar1, convertColor(json[0]["party_color"].as<String>()), LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_bg_color(ui_Bar1, convertColor(json[0]["party_color"].as<String>()), LV_PART_INDICATOR | LV_STATE_DEFAULT);

      lv_label_set_text(ui_Label2, json[1]["party_abbreviation"].as<String>().c_str());
      lv_label_set_text(ui_Label6, String(json[1]["total_votes"].as<int>()).c_str());
      lv_bar_set_value(ui_Bar2, int(json[1]["percentage"].as<float>() * 100.0), LV_ANIM_OFF);
      lv_obj_set_style_bg_color(ui_Bar2, convertColor(json[1]["party_color"].as<String>()), LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_bg_color(ui_Bar2, convertColor(json[1]["party_color"].as<String>()), LV_PART_INDICATOR | LV_STATE_DEFAULT);

      lv_label_set_text(ui_Label3, json[2]["party_abbreviation"].as<String>().c_str());
      lv_label_set_text(ui_Label7, String(json[2]["total_votes"].as<int>()).c_str());
      lv_bar_set_value(ui_Bar3, int(json[2]["percentage"].as<float>() * 100.0), LV_ANIM_OFF);
      lv_obj_set_style_bg_color(ui_Bar3, convertColor(json[2]["party_color"].as<String>()), LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_bg_color(ui_Bar3, convertColor(json[2]["party_color"].as<String>()), LV_PART_INDICATOR | LV_STATE_DEFAULT);

      lv_label_set_text(ui_Label4, json[3]["party_abbreviation"].as<String>().c_str());
      lv_label_set_text(ui_Label8, String(json[3]["total_votes"].as<int>()).c_str());
      lv_bar_set_value(ui_Bar4, int(json[3]["percentage"].as<float>() * 100.0), LV_ANIM_OFF);
      lv_obj_set_style_bg_color(ui_Bar4, convertColor(json[3]["party_color"].as<String>()), LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_bg_color(ui_Bar4, convertColor(json[3]["party_color"].as<String>()), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
  }

  http.end();
}

lv_color_t convertColor(String hexstring)
{
  // Get rid of '#' and convert it to integer
  int number = (int)strtol(&hexstring[1], NULL, 16);

  // Split them up into r, g, b values
  int r = number >> 16;
  int g = number >> 8 & 0xFF;
  int b = number & 0xFF;
  return lv_color_make(r, g, b);
}

void ui_Screen1_screen_init(void)
{

  // ui_Screen1

  ui_Screen1 = lv_obj_create(NULL);

  lv_obj_clear_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_Screen1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

  // ui_Bar1

  ui_Bar1 = lv_bar_create(ui_Screen1);
  lv_bar_set_range(ui_Bar1, 0, 10000);
  lv_bar_set_value(ui_Bar1, 25, LV_ANIM_OFF);

  lv_obj_set_width(ui_Bar1, 170);
  lv_obj_set_height(ui_Bar1, 10);

  lv_obj_set_x(ui_Bar1, 0);
  lv_obj_set_y(ui_Bar1, -55);

  lv_obj_set_style_bg_opa(ui_Bar1, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_Bar1, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

  lv_obj_set_align(ui_Bar1, LV_ALIGN_CENTER);

  // ui_Bar2

  ui_Bar2 = lv_bar_create(ui_Screen1);
  lv_bar_set_range(ui_Bar2, 0, 10000);
  lv_bar_set_value(ui_Bar2, 25, LV_ANIM_OFF);

  lv_obj_set_width(ui_Bar2, 170);
  lv_obj_set_height(ui_Bar2, 10);

  lv_obj_set_x(ui_Bar2, 0);
  lv_obj_set_y(ui_Bar2, -15);

  lv_obj_set_style_bg_opa(ui_Bar2, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_Bar2, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

  lv_obj_set_align(ui_Bar2, LV_ALIGN_CENTER);

  // ui_Bar3

  ui_Bar3 = lv_bar_create(ui_Screen1);
  lv_bar_set_range(ui_Bar3, 0, 10000);
  lv_bar_set_value(ui_Bar3, 25, LV_ANIM_OFF);

  lv_obj_set_width(ui_Bar3, 170);
  lv_obj_set_height(ui_Bar3, 10);

  lv_obj_set_x(ui_Bar3, 0);
  lv_obj_set_y(ui_Bar3, 25);

  lv_obj_set_style_bg_opa(ui_Bar3, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_Bar3, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

  lv_obj_set_align(ui_Bar3, LV_ALIGN_CENTER);

  // ui_Bar4

  ui_Bar4 = lv_bar_create(ui_Screen1);
  lv_bar_set_range(ui_Bar4, 0, 10000);
  lv_bar_set_value(ui_Bar4, 25, LV_ANIM_OFF);

  lv_obj_set_width(ui_Bar4, 170);
  lv_obj_set_height(ui_Bar4, 10);

  lv_obj_set_x(ui_Bar4, 0);
  lv_obj_set_y(ui_Bar4, 65);

  lv_obj_set_style_bg_opa(ui_Bar4, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_Bar4, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

  lv_obj_set_align(ui_Bar4, LV_ALIGN_CENTER);

  // ui_Label1

  ui_Label1 = lv_label_create(ui_Screen1);

  lv_obj_set_width(ui_Label1, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label1, LV_SIZE_CONTENT);

  lv_obj_set_x(ui_Label1, 40);
  lv_obj_set_y(ui_Label1, -70);

  lv_obj_set_align(ui_Label1, LV_ALIGN_LEFT_MID);

  lv_label_set_text(ui_Label1, "");

  // ui_Label2

  ui_Label2 = lv_label_create(ui_Screen1);

  lv_obj_set_width(ui_Label2, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label2, LV_SIZE_CONTENT);

  lv_obj_set_x(ui_Label2, 40);
  lv_obj_set_y(ui_Label2, -30);

  lv_obj_set_align(ui_Label2, LV_ALIGN_LEFT_MID);

  lv_label_set_text(ui_Label2, "");

  // ui_Label3

  ui_Label3 = lv_label_create(ui_Screen1);

  lv_obj_set_width(ui_Label3, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label3, LV_SIZE_CONTENT);

  lv_obj_set_x(ui_Label3, 40);
  lv_obj_set_y(ui_Label3, 10);

  lv_obj_set_align(ui_Label3, LV_ALIGN_LEFT_MID);

  lv_label_set_text(ui_Label3, "");

  // ui_Label4

  ui_Label4 = lv_label_create(ui_Screen1);

  lv_obj_set_width(ui_Label4, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label4, LV_SIZE_CONTENT);

  lv_obj_set_x(ui_Label4, 40);
  lv_obj_set_y(ui_Label4, 50);

  lv_obj_set_align(ui_Label4, LV_ALIGN_LEFT_MID);

  lv_label_set_text(ui_Label4, "");

  // ui_Label5

  ui_Label5 = lv_label_create(ui_Screen1);

  lv_obj_set_width(ui_Label5, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label5, LV_SIZE_CONTENT);

  lv_obj_set_x(ui_Label5, -40);
  lv_obj_set_y(ui_Label5, -70);

  lv_obj_set_align(ui_Label5, LV_ALIGN_RIGHT_MID);

  lv_label_set_text(ui_Label5, "0");

  // ui_Label6

  ui_Label6 = lv_label_create(ui_Screen1);

  lv_obj_set_width(ui_Label6, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label6, LV_SIZE_CONTENT);

  lv_obj_set_x(ui_Label6, -40);
  lv_obj_set_y(ui_Label6, -30);

  lv_obj_set_align(ui_Label6, LV_ALIGN_RIGHT_MID);

  lv_label_set_text(ui_Label6, "0");

  // ui_Label7

  ui_Label7 = lv_label_create(ui_Screen1);

  lv_obj_set_width(ui_Label7, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label7, LV_SIZE_CONTENT);

  lv_obj_set_x(ui_Label7, -40);
  lv_obj_set_y(ui_Label7, 10);

  lv_obj_set_align(ui_Label7, LV_ALIGN_RIGHT_MID);

  lv_label_set_text(ui_Label7, "0");

  // ui_Label8

  ui_Label8 = lv_label_create(ui_Screen1);

  lv_obj_set_width(ui_Label8, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label8, LV_SIZE_CONTENT);

  lv_obj_set_x(ui_Label8, -40);
  lv_obj_set_y(ui_Label8, 50);

  lv_obj_set_align(ui_Label8, LV_ALIGN_RIGHT_MID);

  lv_label_set_text(ui_Label8, "0");

  // ui_Label9

  ui_Label9 = lv_label_create(ui_Screen1);

  lv_obj_set_width(ui_Label9, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label9, LV_SIZE_CONTENT);

  lv_obj_set_x(ui_Label9, 0);
  lv_obj_set_y(ui_Label9, -10);

  lv_obj_set_align(ui_Label9, LV_ALIGN_BOTTOM_MID);

  lv_label_set_text(ui_Label9, "12:56");
}
