/****************************************************************************************************************************
 *  An example of bot that receives commands and turns on and off                                                           *
 *  an LED.                                                                                                                 *
 *  written by Christos Prassas (https://github.com/prassaschr)                                                             *
 * https://telegram.me/get_id_bot                                                                                           *        
 * Find chat_id: http://stackoverflow.com/questions/32423837/telegram-bot-how-to-get-a-group-chat-id-ruby-gem-telegram-bot  *
 *                                                                                                                          *
 * https://randomnerdtutorials.com/esp8266-ota-updates-with-arduino-ide-over-the-air/                                       *                                                                                  
 * https://elementztechblog.wordpress.com/2016/06/28/over-the-air-update-for-esp8266-using-arduino-ide/                     *    
 * http://esp8266.github.io/Arduino/versions/2.0.0/doc/ota_updates/ota_updates.html#http-server                             *
 * http://esp8266.github.io/Arduino/versions/2.0.0/doc/ota_updates/ota_updates.html#arduinoota-configuration                *
 * http://iot-playground.com/blog/2-uncategorised/92-esp8266-internet-connected-4-relay-switch#materials                    *
 * https://github.com/vshymanskyy/TinyGSM                                                                                   *
 * https://github.com/skorokithakis/A6lib
 * https://www.bakke.online/index.php/2017/06/02/self-updating-ota-firmware-for-esp8266/
 * https://community.blynk.cc/t/self-updating-from-web-server-http-ota-firmware-for-esp8266/18544/3
 *  IO index ESP8266 pin IO index  ESP8266 pin                                                                              *
 *   0 [*] GPIO16     7 GPIO13                                                                                              *
 *   1 GPIO5          8 GPIO15                                                                                              *
 *   2 GPIO4          9 GPIO3                                                                                               *
 *   3 GPIO0          10  GPIO1                                                                                             *
 *   4 GPIO2          11  GPIO9                                                                                             *
 *   5 GPIO14         12  GPIO10                                                                                            * 
 *   6 GPIO12                                                                                                               *
 * **************************************************************************************************************************
 * **************************************************************************************************************************
 ***************************************************************************************************************************/ 
#include <FS.h>                   //this needs to be first, or it all crashes and burns...#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <UniversalTelegramBot.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <DNSServer.h> //Local DNS Server used for redirecting all requests to the configuration portal
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
// Wifi connection configuration data
//char ssid[] = "XXXXXX";     // your network SSID (name)
//char password[] = "XXXXXXXXXX"; // your network key

const int FW_VERSION = 201801;
const char* fwUrlBase = "https://your_site.com/fota/";
bool HTTP_OTA = false;


volatile bool telegramButtonPressedFlag = false;
volatile char bootFlag = 'a';
//define your default values here, if there are different values in config.json, they are overwritten.
// Telegram BOT configuration data
#define BOTtoken "XXXXXXXX:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"  // your Bot Token (Get from Botfather)
#define CHAT_IDD "XXXXXXXXXX"

bool shouldSaveConfig = false;


void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

//To the server add 2 files. One **.bin file with ** the MAC address of the device. The other file is **.version with ** the MAC address of the device.
//Inside the version file add the next version.
void checkForUpdates() {
  String mac = getMAC();
  String fwURL = String( fwUrlBase );
  fwURL.concat( mac );
  String fwVersionURL = fwURL;
  fwVersionURL.concat( ".version" );

  Serial.println( "Checking for firmware updates." );
  Serial.print( "MAC address: " );
  Serial.println( mac );
  Serial.print( "Firmware version URL: " );
  Serial.println( fwVersionURL );

  HTTPClient httpClient;
  httpClient.begin( fwVersionURL );
  int httpCode = httpClient.GET();
  if( httpCode == 200 ) {
    String newFWVersion = httpClient.getString();

    Serial.print( "Current firmware version: " );
    Serial.println( FW_VERSION );
    Serial.print( "Available firmware version: " );
    Serial.println( newFWVersion );

    int newVersion = newFWVersion.toInt();

    if( newVersion > FW_VERSION ) {
      Serial.println( "Preparing to update" );

      String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" );
      t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );

      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;
      }
    }
    else {
      Serial.println( "Already on latest version" );
    }
  }
  else {
    Serial.print( "Firmware version check failed, got HTTP response code " );
    Serial.println( httpCode );
  }
  httpClient.end();
}

String getMAC()
{
  uint8_t mac[6];
  char result[14];

 snprintf( result, sizeof( result ), "%02x%02x%02x%02x%02x%02x", mac[ 0 ], mac[ 1 ], mac[ 2 ], mac[ 3 ], mac[ 4 ], mac[ 5 ] );

  return String( result );
}
  
  
  //#define TELEGRAM_BUTTON_PIN D5 //14

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

int Bot_mtbs = 1000; //mean time between scan messages
long Bot_lasttime;   //last time messages' scan has been done
bool Start = false;

// GPIO configuration
const int ledPin = 12; //D6
int ledStatus = 0;
const int buzzerPin = 15; //D8
int buzzerStatus = 0;
const int switchPin = 13; //D7
//int switchStatus = 0;

//Handle Telegram Messages
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "") from_name = "Guest";

    if (text == "/ledon") {
      digitalWrite(ledPin, HIGH);   // turn the LED on (HIGH is the voltage level)
      ledStatus = 1;
      bot.sendMessage(chat_id, "Led is ON", "");
    }

    if (text == "/ledoff") {
      ledStatus = 0;
      digitalWrite(ledPin, LOW);    // turn the LED off (LOW is the voltage level)
      bot.sendMessage(chat_id, "Led is OFF", "");
    }

    if (text == "/buzzeron") {
      digitalWrite(buzzerPin, HIGH);   // turn the BUZZER on (HIGH is the voltage level)
      buzzerStatus = 1;
      bot.sendMessage(chat_id, "Buzzer is ON", "");
    }

    if (text == "/buzzeroff") {
      buzzerStatus = 0;
      digitalWrite(buzzerPin, LOW);    // turn the BUZZER off (LOW is the voltage level)
      bot.sendMessage(chat_id, "Buzzer is OFF", "");
    }

    if (text == "/status") {
      if(ledStatus){
        bot.sendMessage(chat_id, "Led is ON", "");
      } else {
        bot.sendMessage(chat_id, "Led is OFF", "");
      }
      if(buzzerStatus){
        bot.sendMessage(chat_id, "Buzzer is ON", "");
      } else {
        bot.sendMessage(chat_id, "Buzzer is OFF", "");
      }
    }

    if (text == "/internalipaddress") {
      bot.sendMessage(chat_id, "Local IP: " + WiFi.localIP().toString(), "");  
    }

    if (text == "/externalipaddress") {
      bot.sendMessage(chat_id, "Public IP: " + GetExternalIP(), "");  
    }

    if (text == "/ssid") {
      bot.sendMessage(chat_id, "SSID: " + WiFi.SSID(), "");  
    }
  
    if (text == "/rssi") {
      long num = WiFi.RSSI();
      String rssi = String(num);
      bot.sendMessage(chat_id, "RSSI: " + rssi, "");  // Send Signal Strength (RSSI) of WiFi connection
    }

    if (text == "/quality") {
      long num = WiFi.RSSI();
      long qualit = 2 * (num + 100);
      String qual = String(qualit);
      bot.sendMessage(chat_id, qual + "%", "");  // Send Signal Strength (RSSI) of WiFi connection
    }
        
  //    if (text == "/restart") {
//     const int resetPin = 0;
//     digitalWrite(resetPin, HIGH);
 //    ESP.reset();  
  //    System.restart();
  //}

    

    if (text == "/start") {
      String welcome = "Welcome to alarm system, " + from_name + ".\n";
      welcome += "Configured by Christos Prassas.\n\n";
      welcome += "/ledon : to switch the Led ON\n\n";
      welcome += "/ledoff : to switch the Led OFF\n\n";
      welcome += "/buzzeron : to switch the Buzzer ON\n\n";
      welcome += "/buzzeroff : to switch the Buzzer OFF\n\n";
      welcome += "/status : Returns current status of LED\n\n";
      welcome += "/ssid : Returns connected SSID\n\n";
      welcome += "/rssi : Returns RSSI\n\n";
      welcome += "/quality : Returns Signal Strength in % \n\n";
      welcome += "/internalipaddress : Returns IP address\n\n";
      welcome += "/externalipaddress : Returns IP address\n\n";
      //welcome += "/restart : Reboots device\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}


void setup() {
  Serial.begin(115200);
  WiFiManager wifiManager; //Initialize library
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

    
  wifiManager.autoConnect("ESP8266-Setup", "makedonia");
  pinMode(2, OUTPUT);
  
  pinMode(ledPin, OUTPUT); // initialize digital ledPin as an output.
  delay(10);
  digitalWrite(ledPin, LOW); // initialize pin as off
  pinMode(buzzerPin, OUTPUT); // initialize digital buzzerPin as an output.
  delay(10);
  digitalWrite(buzzerPin, LOW); // initialize pin as off
  pinMode(switchPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(switchPin), telegramButtonPressed, RISING);
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());

  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void telegramButtonPressed() {
  Serial.println("telegramButtonPressed");
  int button = digitalRead(switchPin);
  Serial.println(digitalRead(switchPin)); //check
  if(button == HIGH)
  {
    telegramButtonPressedFlag = true;
  }
  return;
}


void sendTelegramMessage() {
  if(bot.sendMessage(CHAT_IDD, "Alarm ON", "")){
    Serial.println("TELEGRAM Successfully sent --> Alarm ON");
  }
  delay(10000); //Delay to send second message!!!
  if(bot.sendMessage(CHAT_IDD, "Alarm ON", "")){
    Serial.println("TELEGRAM Successfully sent --> Alarm ON");
  }
  delay(10000); //Delay to send third message!!!
  if(bot.sendMessage(CHAT_IDD, "Alarm ON", "")){
    Serial.println("TELEGRAM Successfully sent --> Alarm ON");
  }
  telegramButtonPressedFlag = false;
  delay(5000);
}

void sendBootFlagMessage() {
  if(bot.sendMessage(CHAT_IDD, "Device Rebooted", "")){
    Serial.println("TELEGRAM Successfully sent -- > Device Rebooted");
  }
 bootFlag = 'c';
}

void blinkOnboardBlueLed() {
  digitalWrite(2, LOW);   // Turn the LED on (Note that LOW is the voltage level
                                    // but actually the LED is on; this is because 
                                    // it is acive low on the ESP-01)
  delay(200);                      // Wait for a second
  digitalWrite(2, HIGH);  // Turn the LED off by making the voltage HIGH
  delay(500);                      // Wait for two seconds (to demonstrate the active low LED)
  }

// Get external IP and give it to Telegram client
String GetExternalIP() {
    HTTPClient http;  //Declare an object of class HTTPClient
    http.begin("http://api.ipify.org/");  //Specify request destination
    int httpCode = http.GET();                                                                  //Send the request
 
    if (httpCode > 0) { //Check the returning code
 
      String payload = http.getString();   //Get the request response payload
      //Serial.println(payload);             //Print the response payload
      return payload;
    }
     http.end();   //Close connection
    }
 
void loop() {
  if (millis() > Bot_lasttime + Bot_mtbs)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    Bot_lasttime = millis();
  }

  if ( telegramButtonPressedFlag ) {
    sendTelegramMessage();
    delay(1000);
  }
  
  WiFi.status();
  if (WiFi.status() == WL_CONNECTED) {
    blinkOnboardBlueLed();
    //delay(15000);
  }

  if(bootFlag == 'a') {
   bootFlag = 'b';
  }
  
  if (bootFlag == 'b') {
      sendBootFlagMessage();  
  }
  
}
