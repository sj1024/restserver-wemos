// FILE: rest.ino
// vi: rnu smartindent autoindent sts=8 fdm=indent expandtab
// Import required libraries
#include <stdarg.h>
#include <ESP8266WiFi.h>
#include <SimpleDHT.h>
#include <aREST.h>
#include <Scheduler.h>
#include "credentials.h"
// Symbols
#define ON              1
#define OFF             0
// Boards
#define WEMOS_PIN_D0    16
#define WEMOS_PIN_D1    5
#define WEMOS_PIN_D2    4
#define WEMOS_PIN_D3    0
#define WEMOS_PIN_D4_LED        2
#define WEMOS_PIN_D5    14
#define WEMOS_PIN_D6    12
#define WEMOS_PIN_D7    13
#define WEMOS_PIN_D8    15
// Controls
#define ARRAY_N 7 
#define REST_WDT_SEC    360UL   // 1H
// Create aREST instance
aREST rest = aREST();
// WiFi parameters
const char* ssid        = SSID;             // your ssid
const char* password    = SSID_PASSWORD;          // your password
// The port to listen for incoming TCP connections
#define LISTEN_PORT           80
// Create an instance of the server
WiFiServer server(LISTEN_PORT);
// Variables to be exposed to the API
int temperature;
int humidity;
// WDT for Rest Server
int restwdt = 0;
// Declare functions to be exposed to the API
int LedControl(String command);
int Operation(String command);
int ledtoggle;
int oper;
//
int dhtready;
const int tempLevel[]   = {15, 17, 19, 21};
//
#if 0
String GetTime() {
        WiFiClient client;
        while (!!!client.connect("google.com", 80)) {
                Serial.println("connection failed, retrying...");
        }

        client.print("HEAD / HTTP/1.1\r\n\r\n");
 
        while(!!!client.available()) {
                yield();
        }

        while(client.available()){
                if (client.read() == '\n') {   
                        if (client.read() == 'D') {   
                                if (client.read() == 'a') {   
                                        if (client.read() == 't') {   
                                                if (client.read() == 'e') {   
                                                        if (client.read() == ':') {   
                                                                client.read();
                                                                String theDate = client.readStringUntil('\r'); 
                                                                client.stop();
                                                                return theDate;
                                                        }
                                                }
                                        }
                                }
                        }
                }
        }
}
#endif
void Printf(char *fmt, ... ) {
        char buf[128]; // resulting string limited to 128 chars
        va_list args;
        va_start (args, fmt );
        vsnprintf(buf, 128, fmt, args);
        va_end (args);
        Serial.print(buf);
}
void Relay(int Pin, int onoff) {
    if(onoff == ON) {
            pinMode(Pin, OUTPUT);
            digitalWrite(Pin, LOW);
    } else {
            pinMode(Pin, INPUT); 
    }
}
class ThemoTask : public Task {
        protected:
        void setup() {
                oper = 1;
                do{
                    delay(10);
                }while(!dhtready);
        }
        void loop() {
                int PinLevel0   = WEMOS_PIN_D5;
                int PinLevel1   = WEMOS_PIN_D6;
                int PinHeat     = WEMOS_PIN_D7;
                int PinCool     = WEMOS_PIN_D1;
                int level;
                level = digitalRead(PinLevel0);
                level = level | digitalRead(PinLevel1) << 1; 
                Printf("Target Temp: %d\r\n", tempLevel[level]);
                delay(500);
                if(oper != 0) {
                        if(temperature < tempLevel[level]) {
                                Printf("^ Heater On\r\n");
                                Relay(PinHeat, ON);
                        } else { 
                                Printf("^ Heater off\r\n");
                                Relay(PinHeat, OFF);
                        }
        
                        if(temperature > tempLevel[level]) {
                                Printf("^ Aircon On\r\n");
                                Relay(PinCool, ON);
                        } else {
                                Printf("^ Aircon Off\r\n");
                                Relay(PinCool, OFF);
                        }
                } else {
                        Printf("! Operation not permitted\r\n");
                }

                delay(1000*60);
        }
        private:
} themo_task;
class Dht11Task : public Task {
        protected:
        void setup() {
        }

        void loop() {
                int PinPower    = WEMOS_PIN_D2;
                int DHT11Pin    = WEMOS_PIN_D3;
                int tempArray[ARRAY_N];
                int humiArray[ARRAY_N];

                SimpleDHT11 dht11;

                pinMode(PinPower,       OUTPUT);
                digitalWrite(PinPower,  HIGH);
                delay(500);

                // Pin Wiring
                byte btemperature = 0;
                byte bhumidity = 0;
                int tm, hm, err; 
                for(int i=0; i<ARRAY_N; i++) {
                        err = SimpleDHTErrSuccess;
                        if ((err = dht11.read(DHT11Pin, &btemperature, &bhumidity, NULL)) != SimpleDHTErrSuccess) {
                                int pin_reset = WEMOS_PIN_D0;
                                Printf("! Read DHT11 failed, err= %d\n",err); 
                                //ESP.deepSleep(1);       //reset
                                delay(50);
                                pinMode(pin_reset, OUTPUT);
                                digitalWrite(pin_reset, LOW);
                        }
                        tempArray[i] = (int)btemperature;
                        humiArray[i] = (int)bhumidity;
                        delay(500);
                }
                //sort
                temperature = get_middle(&tempArray[0]);
                humidity    = get_middle(&humiArray[0]);
                dhtready = 1;
                Printf("%d,", temperature);
                //Printf("Humi Middle: %d\n", humidity);
                if(temperature == 0) { // DEBUG CODE
                        for(int i=0; i<ARRAY_N; i++) {
                                Printf("tempArray[%d]=%d\r\n", i, tempArray[i]);
                        }
                }
                // DHT Power Off 
                digitalWrite(PinPower,  LOW);
                delay(1000);
        }
        private:
} dht11_task;
class RestTask: public Task {
        protected:
        void setup() {
        }

        void loop() {
                // Handle REST calls
                WiFiClient client = server.available();
                if (!client) {
                        return;
                }
                while(!client.available()){
                        delay(1);
                }
                rest.handle(client);
                digitalWrite(WEMOS_PIN_D4_LED, ledtoggle);
                restwdt = 1;
                ledtoggle ^= 1;
        }
        private:
} rest_task;
class RestWdtTask: public Task {
        protected:
        void setup() {
        }

        void loop() {

                delay(1000 * REST_WDT_SEC); 

                if(restwdt != 1){
                        int pin_reset = WEMOS_PIN_D0;
                        Printf("! No request for %d sec\n", REST_WDT_SEC);
                        Printf("^ Reboot the system\n");
                        //ESP.deepSleep(1); //reset
                        delay(50);
                        pinMode(pin_reset, OUTPUT);
                        digitalWrite(pin_reset, LOW);
                } else{
                        restwdt = 0;
                }

        }
        private:
} rest_wdt_task;
void insertion_sort (int *data, int n) {
        int i, j, remember;
        for ( i = 1; i < n; i++ ) {
                remember = data[(j=i)];
                while ( --j >= 0 && remember < data[j] ){
                        data[j+1] = data[j];
                        data[j] = remember;
                }
        }
}
int get_middle(int* array) {
        insertion_sort(array, ARRAY_N);
        return *(array + (int)(ARRAY_N/2));
}
// Custom function accessible by the API
// e.g.  /led?params=0
int LedControl(String command) {
        // Get state from command
        int state = command.toInt();
        digitalWrite(WEMOS_PIN_D4_LED, state);
        return 1;
}
int Operation(String command) {
        int PinHeat     = WEMOS_PIN_D7;
        int PinCool     = WEMOS_PIN_D1;
        oper = command.toInt();
        if(oper == 0) {
                Relay(PinHeat, OFF);
                Relay(PinCool, OFF);
        }
        return 1;
}
void loop() {}
void setup(void) {
        pinMode(WEMOS_PIN_D4_LED, OUTPUT);
        // Start Serial
        Serial.begin(115200);
        Serial.println("\r\n");
        // Init variables and expose them to REST API
        dhtready = 0;
        rest.variable("temperature", &temperature);
        rest.variable("humidity", &humidity);
        // Function to be exposed
        rest.function("led", LedControl);
        rest.function("oper", Operation);
        // Connect to WiFi
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
                delay(500);
                Serial.print(".");
        }
        Serial.println("");
        Serial.println("WiFi connected");
        // Start the server
        server.begin();
        Serial.println("Server started");
        // Print the IP address
        Serial.println(WiFi.localIP());
        // Give name & ID to the device (ID should be 6 characters long)
        rest.set_id(String(WiFi.localIP()[3]).c_str());
        rest.set_name("esp8266");
        // Scheduler start
        Scheduler.start(&rest_task);
        //Scheduler.start(&rest_wdt_task);
        Scheduler.start(&dht11_task);
        Scheduler.start(&themo_task);
        Scheduler.begin();
}
