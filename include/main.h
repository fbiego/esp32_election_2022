
#define BL 5


char * WIFI_SSID = "xxxxxxxxxx";
char * WIFI_PASS = "xxxxxxxxxx";

int mn = 0;

// backlight pin
const int ledPin = BL; //

// setting PWM properties
const int freq = 1000;
const int ledChannel = 15;
const int resolution = 8;

void ui_Screen1_screen_init();
void getTime();
void getResults();