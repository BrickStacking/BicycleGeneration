#include <Arduino.h>
#include <WiFiClient.h>
WiFiClient client;
#if defined(ESP8266)
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#else
#include <WiFi.h>          //https://github.com/esp8266/Arduino
#endif
//needed for library
#include <DNSServer.h>
#if defined(ESP8266)
#include <ESP8266WebServer.h>
#else
#include <WebServer.h>
#endif
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <Ticker.h>

/******** DECLARE FOR LIBRARY **********************/
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME  "thienlc"
#define AIO_KEY       "84a4aa2dfb2b4c288a1cb870a7eb330b"
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish all_data_collection = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/all_data_collection");
Adafruit_MQTT_Publish speed_encoder = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/Speed");
Adafruit_MQTT_Subscribe onoff_relay1 = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/Relay 1");
Adafruit_MQTT_Subscribe onoff_relay2 = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/Relay 2");
Adafruit_MQTT_Subscribe heartrate = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/HeartRate");
Ticker ticker, sent_speed;
Ticker blinker;
#include <FPM.h>
FPM finger(&Serial2);
FPM_System_Params params;
int16_t p = -1;
uint16_t fid, score; //Với FID là id của vân tay và score là độ tin cậy của vân tay
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 20, 4);
#define nut1 14
#define nut2 26
#define nut3 27
#define nut4 25
#define BUILTIN_LED 2
#define Relay1 4
#define Relay2 15

/******** ENDING DECLARE **********************/

/******** DECLARE FOR VARIABLES **********************/
struct Button
{ //Cấu trúc Struct
  const uint8_t PIN;
  uint32_t numberKeyPresses;
  bool pressed;
};
Button button1 = {13, 0, false};
unsigned long int i = (button1.numberKeyPresses / 1600 * 1.5);
void IRAM_ATTR isr2()
{ //Chương trình ngắt đếm encoder
  //button1.numberKeyPresses += 1;
  if (digitalRead(12) == 0)
    button1.numberKeyPresses += 1;
  if (digitalRead(12) == 1)
    button1.numberKeyPresses -= 1;
  button1.pressed = true;
}
int state_lcd = 0, last_state_lcd;
long int rfid_card = 0, last_rfid_card = 0;
uint8_t user_state = 0;
volatile unsigned long int encoderPosA = 0;
volatile unsigned long int last_encoderPosA;
float v_encoder = 0;
float k;
uint16_t real_last_value_save;
uint8_t real_user_tranfer;
float calories = 0;
unsigned long int t;
const float togglePeriod = 1; //seconds
bool Connected2Blynk = false;
int dem1, dem2, dem3, dem4;
/******** ENDING DECLARE **********************/
void caculate_v();
void tick()
{
  //toggle state
  int state = digitalRead(BUILTIN_LED); // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);    // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void setup()
{
  Serial.begin(115200);
  pinMode(BUILTIN_LED, OUTPUT);
  ticker.attach(0.6, tick);
  lcd.clear();
  lcd.init();
  lcd.backlight(); //Bật đèn nền
  lcd.home();
  lcd.setCursor(2, 0);
  lcd.print("Staring Program !");
  lcd.setCursor(1, 1);
  for (int i = 0; i < 12; i++)
  {
    lcd.print(".");
    delay(70);
  }
  pinMode(button1.PIN, INPUT_PULLUP);
  pinMode(nut1, INPUT_PULLUP);
  pinMode(nut2, INPUT_PULLUP);
  pinMode(nut3, INPUT_PULLUP);
  pinMode(nut4, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  pinMode(Relay1, OUTPUT); //2 chan relay kích bằng Esp32
  pinMode(Relay2, OUTPUT);
  Serial.println("Trigger Relay");
  digitalWrite(Relay1, LOW);
  digitalWrite(Relay2, LOW);
  Serial.print(digitalRead(Relay1));
  Serial.print(" ");
  Serial.print(digitalRead(Relay2));
  Serial.print(" ");
  delay(4000);
  digitalWrite(Relay1,  HIGH);
  digitalWrite(Relay2, HIGH);
  Serial.print(digitalRead(Relay1));
  Serial.print(" ");
  Serial.print(digitalRead(Relay2));
  Serial.print(" ");
  t = millis();
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setTimeout(180);
  if (!wifiManager.autoConnect("Esp32 Bicycle", "1234554320"))
  {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }

  //For Finger Print ///
  Serial.println("FingerPrint Begin");
  Serial2.begin(57600);
  if (finger.begin()) {
    finger.readParams(&params);
    Serial.println("Found fingerprint sensor!");
    Serial.print("Capacity: "); Serial.println(params.capacity);
    Serial.print("Packet length: "); Serial.println(FPM::packet_lengths[params.packet_len]);
  }
  else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) yield();
  }
  //////////////////////////////////////

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  ticker.detach();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //keep LED on
  digitalWrite(BUILTIN_LED, HIGH);


  lcd.clear();
  attachInterrupt(button1.PIN, isr2, FALLING);
  Serial.println("Ok Start");
  blinker.attach(0.2, caculate_v); //Ticker for calculate speed
  sent_speed.attach(10, push_data_to_server);
  mqtt.subscribe(&onoff_relay1);
  mqtt.subscribe(&onoff_relay2);
  mqtt.subscribe(&heartrate);
  button1.numberKeyPresses = 0;
  fid = 0;
  state_lcd = 0;
}

void loop()
{
  MQTT_connect();

  // this is our 'wait for incoming subscription packets' busy subloop
  // try to spend your time here

  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    // Check if its the onoff button feed
    if (subscription == &onoff_relay1) { //onoff_relay1
      Serial.print(F("On-Off button: "));
      Serial.println((char *)onoff_relay1.lastread);

      if (strcmp((char *)onoff_relay1.lastread, "ON") == 0) {
        digitalWrite(Relay1, LOW);
      }
      if (strcmp((char *)onoff_relay1.lastread, "OFF") == 0) {
        digitalWrite(Relay1, HIGH);
      }
    }

    // check if its the slider feed
    if (subscription == &onoff_relay2) { //onoff_relay1
      Serial.print(F("On-Off button: "));
      Serial.println((char *)onoff_relay2.lastread);

      if (strcmp((char *)onoff_relay2.lastread, "ON") == 0) {
        digitalWrite(Relay2, LOW);
      }
      if (strcmp((char *)onoff_relay2.lastread, "OFF") == 0) {
        digitalWrite(Relay2, HIGH);
      }
    }
  }

  // ping the server to keep the mqtt connection alive
  if (! mqtt.ping()) {
    mqtt.disconnect();
  }
  docnut();
  lcd_display();

  if ((millis() - t) > 1000 && state_lcd == 2)
  { //Clear man hinh
    t = millis();
    lcd.clear();
  }
  //  Serial.print(button1.numberKeyPresses);
  //  Serial.print(" ");
  //         Serial.print(digitalRead(nut1));
  //  Serial.print(" ");
  //  Serial.print(digitalRead(nut2));
  //  Serial.print(" ");
  //  Serial.print(digitalRead(nut3));
  //  Serial.print(" ");
  //  Serial.print(digitalRead(nut4));
  //  Serial.println(" ");
}

void caculate_v()
{
  v_encoder = (button1.numberKeyPresses - last_encoderPosA) * 5;
  last_encoderPosA = button1.numberKeyPresses;
  calories = button1.numberKeyPresses * 40 / 1000 / 1600 * 1.5;
  Serial.print("B:");
  Serial.print(digitalRead(2));
  Serial.print(" S:");
  Serial.print(button1.numberKeyPresses / 1600 * 1.5);
  Serial.print(" V:");
  Serial.println(v_encoder);
}

void docnut()
{

  if (digitalRead(14) == 0)
    dem1++;
  if (dem1 == 2 && state_lcd == 0)
  { //Nếu state_lcd =0 mới cho quét vân tay lại
    //sent_data1();
    state_lcd = 1;
    lcd_display(); //Chuyển về giao diện để ấn
    delay(300);
    search_database(); //Đọc vân tay

    Serial.println("Read Fingerprint!");
  }
  if (digitalRead(14) == 1)
    dem1 = 0;
  //================================//
  if (digitalRead(26) == 0)
    dem2++;
  if (dem2 == 2 && fid != 0)
  {
    Serial.println("Nut 3 an!"); //Ấn nút này,mọi trạng thái sẽ quay về ban đầu sau khi đẩy dữ liệu lên internet
    state_lcd = 3;
    lcd_display();
    sent_data1();
    delay(3000);
    lcd.clear();
  }
  if (digitalRead(26) == 1)
    dem2 = 0;
  //================================//
  if (digitalRead(27) == 0)
    dem3++;
  if (dem3 == 2 && fid != 0)
  { //Nếi đã ấn nút 2, và fid khác 0 rồi, thì tiếp tục cho access vào
    Serial.println("Nut 2 an!");
    lcd.clear();
    state_lcd = 2; //Sau khi ấn nút 2, chuyển từ màn hình chờ sang màn hình hiển thị tập
  }
  if (digitalRead(27) == 1)
    dem3 = 0;
  //================================//
  //================================//
  if (digitalRead(25) == 0)
    dem4++;
  if (dem4 == 2)
  {
    Serial.println("Nut 4 an!"); //Nút để break hết ra ngoài
    fid = 0;
    state_lcd = 0;
    lcd.clear();
  }
  if (digitalRead(25) == 1)
    dem4 = 0;
  //================================//
}

void sent_data1()
{
  //real_last_value_save  ; calories
  if (!client.connect("maker.ifttt.com", 80))
  {
    Serial.println(F("Connection failed"));
    return;
  }
  Serial.println(F("Connected!"));
  // Send HTTP request
  String sufix = " HTTP/1.1";
  String b = String(button1.numberKeyPresses / 1600 * 1.5);
  String c = String(calories);
  String a = "GET https://maker.ifttt.com/trigger/fbchat/with/key/dz_0EAp6pGiE4DAvPiJ1DG/?value1=" + b + "&value2=" + c;
  client.println(a);
  //client.println(("GET https://maker.ifttt.com/trigger/homefeed/with/key/dz_0EAp6pGiE4DAvPiJ1DG/?value1=thienlccd HTTP/1.1"));
  client.println(F("Host: maker.ifttt.com"));
  client.println(F("Connection: close"));
  if (client.println() == 0)
  {
    Serial.println(F("Failed to send request"));
    return;
  }
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0)
  {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    return;
  }
  client.stop();
}

int search_database()
{
  p = -1;
  /* first get the finger image */
  Serial.println("Waiting for valid finger");
  while (p != FPM_OK)
  {
    p = finger.getImage();
    switch (p)
    {
      case FPM_OK:
        Serial.println("Image taken");
        break;
      case FPM_NOFINGER:
        Serial.println(".");
        break;
      case FPM_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FPM_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      case FPM_TIMEOUT:
        Serial.println("Timeout!");
        break;
      case FPM_READ_ERROR:
        Serial.println("Got wrong PID or length!");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
    yield();
    if (digitalRead(25) == 0)
    {
      Serial.println("Break find finger print");
      state_lcd = 0;
      lcd.clear();
      break;
    }
  }

  /* convert it */
  p = finger.image2Tz();
  switch (p)
  {
    case FPM_OK:
      Serial.println("Image converted");
      break;
    case FPM_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FPM_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FPM_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FPM_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    case FPM_TIMEOUT:
      Serial.println("Timeout!");
      return p;
    case FPM_READ_ERROR:
      Serial.println("Got wrong PID or length!");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Remove finger");
  p = 0;
  while (p != FPM_NOFINGER)
  {
    p = finger.getImage();
    yield();
  }
  Serial.println();

  /* search the database for the converted print */

  p = finger.searchDatabase(&fid, &score);
  if (p == FPM_OK)
  {
    Serial.println("Found a print match!");
  }
  else if (p == FPM_PACKETRECIEVEERR)
  {
    Serial.println("Communication error");
    return p;
  }
  else if (p == FPM_NOTFOUND)
  {
    Serial.println("Did not find a match");
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Press your finger!");
    lcd.setCursor(1, 1);
    lcd.print("Wrong finger print!");
    lcd.setCursor(2, 2);
    lcd.print("Please try again!");
    lcd.setCursor(3, 3);
    lcd.print("X-Tech Group");
    last_state_lcd = state_lcd;
    delay(4000);
    lcd.clear();
    return p;
  }
  else if (p == FPM_TIMEOUT)
  {
    Serial.println("Timeout!");
    return p;
  }
  else if (p == FPM_READ_ERROR)
  {
    Serial.println("Got wrong PID or length!");
    return p;
  }
  else
  {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  Serial.print("Found ID #");
  Serial.print(fid);
  state_lcd = 1; //Đưa màn hình sang trạng thái hiển thị người dùng
  Serial.print(" with confidence of ");
  Serial.println(score);
}

void lcd_display()
{
  //  Serial.print("Doc:");
  //  Serial.println(encoderPosA);
  //  docnut();
  if (state_lcd == 0)
  {
    lcd.setCursor(5, 0);
    lcd.print("Welcome !");
    lcd.setCursor(2, 1);
    lcd.print("Bicycle Training");
    lcd.setCursor(4, 3);
    lcd.print("X-Tech Group");
    last_state_lcd = state_lcd;
    button1.numberKeyPresses = 0; //Ban đầu khi chưa log in bằng vân tay, sẽ không đếm quãng đường
  }
  if (state_lcd != last_state_lcd)
    lcd.clear();
  if (last_rfid_card != fid)
  {
    lcd.clear();
    last_rfid_card = fid;
  }
  if (state_lcd == 1)
  {
    button1.numberKeyPresses = 0;
    lcd.setCursor(1, 0);
    lcd.print("Press your finger!");
    lcd.setCursor(2, 1);

    if (fid == 7 || fid == 5 || fid == 6 || fid == 8)
    {
      //      Serial.println("");
      //      Serial.println("User A");
      lcd.print("Nguyen Tien Dam");
      user_state = 1;
    }
    if (fid == 9)
    {
      //      Serial.println("");
      //      Serial.println("User C");
      lcd.print("Hoang Trong Nho");
      user_state = 3;
    }
    if (fid == 1 || fid == 2 || fid == 3 || fid == 4)
    {
      //      Serial.println("");
      //      Serial.println("User E");
      lcd.print("Cao Duc Thien");
      user_state = 2;
    }

    lcd.setCursor(8, 2);
    lcd.print("ID:");
    lcd.print(fid);
    lcd.setCursor(3, 3);
    lcd.print("X-Tech Group");
    last_state_lcd = state_lcd;
  }

  if (state_lcd == 2)
  {
    lcd.setCursor(2, 0);
    lcd.print("Training Start!");
    lcd.setCursor(1, 1);
    lcd.print("Quang duong:");
    i = (button1.numberKeyPresses / 1600 * 1.5);
    lcd.print(i);
    lcd.print("m");
    lcd.setCursor(1, 2);
    lcd.print("Van toc  :");
    k = (v_encoder / 1600 * 1.5 * 3.6); //Chuyển từ m/s sang km/h
    lcd.print(k, 2);
    lcd.print("km/h");
    lcd.setCursor(3, 3);
    lcd.print("X - Tech Group");
    last_state_lcd = state_lcd;
  }

  if (state_lcd == 3)
  {
    state_lcd = 0;
    lcd.setCursor(4, 0);
    lcd.print("Training End!");
    lcd.setCursor(0, 1);
    lcd.print("Quang duong:");
    i = (button1.numberKeyPresses / 1600 * 1.5);
    lcd.print(i);
    lcd.print("m");
    lcd.setCursor(0, 2);
    float calories = button1.numberKeyPresses * 40 / 1000 / 1600 * 1.5;
    lcd.print("Calories Burn:");
    lcd.print(calories);
    lcd.setCursor(4, 3);
    lcd.print("Thanks you !");
    fid = 0;
  }
}
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
    }
  }
  Serial.println("MQTT Connected!");
}
//sent_speed.attach(0.2, push_data_to_server);


void push_data_to_server() {
  Serial.print(F("\nSending speed calculator "));
  Serial.print(v_encoder / 1600 * 1.5 * 3.6);
  Serial.print("...");
  if (! speed_encoder.publish(v_encoder / 1600 * 1.5 * 3.6)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!")); 
  }

  Serial.print(F("\nSending all data to feed "));
  Serial.print(calories);
  Serial.print("...");
  if (! all_data_collection.publish(calories)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }
}

//  v_encoder = (button1.numberKeyPresses - last_encoderPosA) * 5;
//  last_encoderPosA = button1.numberKeyPresses;
//  calories = button1.numberKeyPresses * 40 / 1000 / 1600 * 1.5;
//  Serial.print("B:");
//  Serial.print(digitalRead(2));
//  Serial.print(" S:");
//  Serial.print(button1.numberKeyPresses / 1600 * 1.5);
//  Serial.print(" V:");
//  Serial.println(v_encoder);
