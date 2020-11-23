#include "esp_camera.h"
#include <WiFi.h>

// SD
#include "FS.h"
#include "SD_MMC.h"
#include <EEPROM.h>
#define EEPROM_SIZE 1 // define number of bytes to set aside in persistant memory
int pictureNumber = 0;
// !SD

// Deep Sleep
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10      /* Time ESP32 will go to sleep (in mins) */
// !Deep Sleep


#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

const char* ssid = "heh";
const char* password = "peopeo22";

void startCameraServer();

static esp_err_t get_image(fs::FS &fs, const char * path) {
    // Variable definitions for camera frame buffer
    camera_fb_t * fb = NULL;
    int64_t fr_start = esp_timer_get_time();

    // Get the contents of the camera frame buffer
    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return ESP_FAIL;
    }

    // Save image to file
    size_t fb_len = 0;
    fb_len = fb->len;
    File file = fs.open(path, FILE_WRITE);
    if(!file){
      Serial.println("Failed to open file in writing mode");
    } else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.printf("Saved file to path: %s\n", path);
    }
    file.close();

    // Release the camera frame buffer
    esp_camera_fb_return(fb);
    int64_t fr_end = esp_timer_get_time();
    Serial.printf("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));
    return ESP_OK;
}



void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }
  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  WiFi.begin(ssid, password);

  int count = 0;
  
  while (WiFi.status() != WL_CONNECTED && count < 23) {
    delay(500);
    Serial.print(".");
    ++count;
  }

   if(WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFiconnected");
    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");
    startCameraServer();
    
  } else {
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * 60 * uS_TO_S_FACTOR);
      
      if(!SD_MMC.begin()){
        Serial.println("Card Mount Failed");
        return;
      }
      uint8_t cardType = SD_MMC.cardType();
      if(cardType == CARD_NONE){
        Serial.println("No SD_MMC card attached");
        return;
      }
      Serial.println("SD Card Initialized");
    
      // initialize EEPROM with predefined size
      EEPROM.begin(EEPROM_SIZE);
      pictureNumber = EEPROM.read(0) + 1;
    
      // Call function to capture the image and save it as a file
      String path = "/picture" + String(pictureNumber) + ".jpg";
      if(get_image(SD_MMC, path.c_str()) != ESP_OK ) {
        Serial.println("Failed to capture picture");
      } else{
        Serial.println("Captured picture");
        EEPROM.write(0, pictureNumber);
        EEPROM.commit();
      }      
      // Deep Sleep    
      Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
      Serial.println("Going to sleep now");
      Serial.flush(); 
      esp_deep_sleep_start();
  }
  
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(10000);
}
