#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

const char* ssid = "Emergy.NET";
const char* password = "";

const int fan_pin = 5;
const int led_pin = 4;

const int pwr_ctl_pin = 14;
const int fan_ctl_pin = 13;
const int led_ctl_pin = 2;

int fan_status = 0;
int pwr_status = 0;
int led_status = 0;

ESP8266WebServer server(80);

void setup() {
    Serial.begin(115200);
    Serial.println("Booting");
    Serial.println("Connect to Wi-Fi");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("Connection Failed! Rebooting...");
        //ESP.restart();
    }

    // Port defaults to 8266
    // ArduinoOTA.setPort(8266);

    // Hostname defaults to esp8266-[ChipID]
    // ArduinoOTA.setHostname("myesp8266");

    // No authentication by default
    // ArduinoOTA.setPassword("admin");

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else // U_SPIFFS
            type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    pinMode(fan_pin, OUTPUT);
    pinMode(pwr_ctl_pin, OUTPUT);
    pinMode(fan_ctl_pin, INPUT);
    pinMode(led_pin, OUTPUT);
    pinMode(led_ctl_pin, INPUT);

    analogWriteFreq(14000);

    Serial.println("Init /update location");
    server.on("/update", handleUpdate);

    Serial.println("Init /status location");
    server.on("/status", handleStatus);

    Serial.println("Init NotFound location");
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");
}

void handleUpdate() {
    if (server.hasArg("fan")) {
        int fan_speed = server.arg("fan").toInt();
        if (fan_speed != fan_status) {
            fan_status = fan_speed;
            Serial.println("Update fan speed");
            updatePin(fan_pin, fan_speed);
        }
    }

    if (server.hasArg("led")) {
        int led_brightness = server.arg("led").toInt();
        if (led_brightness != led_status) {
            led_status = led_brightness;
            Serial.println("Update led brightness");
            updatePin(led_pin, led_brightness);
        }
    }

    if (fan_status == 0 && led_status == 0) {
        Serial.println("ATX Power off");
        digitalWrite(pwr_ctl_pin, 0);
        pwr_status = 0;
    } else {
        Serial.println("ATX Power on");
        digitalWrite(pwr_ctl_pin, 1);
        pwr_status = 1;
    }

    server.send(200, "text/plain", "OK");
}

void updatePin(int pin, int value) {
    char buf[30];
    sprintf(buf, "Update pin %d: %d", pin, value);
    Serial.println(buf);

    if (value >= 0 && value <= 1023) {
        analogWrite(pin, value);
        Serial.println("OK");
    } else {
        Serial.println("Error: pin value");
    }
}

void handleStatus() {
    char pwr_status_buf[4];
    char fan_status_buf[4];
    char led_status_buf[4];

    sprintf(pwr_status_buf, "%d", pwr_status);
    sprintf(fan_status_buf, "%d", fan_status);
    sprintf(led_status_buf, "%d", led_status);

    if (server.hasArg("json") && server.arg("json").toInt() == 1) {
        String message = "{\n";
        message += "  \"power_status\" : \"";
        message += pwr_status_buf;
        message += "\",\n";
        message += "  \"fan_status\" : \"";
        message += fan_status_buf;
        message += "\",\n";
        message += "  \"led_status\" : \"";
        message += led_status_buf;
        message += "\"\n";
        message += "}\n";
        server.send(200, "application/json", message);
    } else {
        String message = "Power status: ";
        message += (pwr_status == 1)?"on":"off";
        message += "\nFan status: ";
        message += fan_status_buf;
        message += "\nLed status: ";
        message += led_status_buf;
        message += "\n";
        server.send(200, "text/plain", message);
    }
}

void handleNotFound(){
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET)?"GET":"POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i=0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
}

void loop() {
    ArduinoOTA.handle();
    server.handleClient();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Reconnect to Wi-Fi");
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        delay(5000);
    }
}

