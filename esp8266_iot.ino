#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <FS.h>

#ifndef APSSID
#define APSSID "ESP-arduino"
#define APPSK  "12345678"
#endif

//AP crdentials
const char *ssid = APSSID;
const char *password = APPSK;

//MQTT details
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_user = NULL;
const char* mqtt_pass = NULL;
const char* intopic = "intopic"; //subscription topic
const char* outtopic = "outtopic"; //publishing topic
const char* lwtMessage = "ESP8266 went offline";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

ESP8266WebServer server(80);

//Config page
const char PROGMEM root[] = R"=====(
<html>
<head>
<style>
  table, th, td {
  border: 1px solid black;
  }
</style>
</head>
<body onload="fetchnetworks()">
<h2>ESP8266 Settings Update</h2>
<p id="networks">Scanning available networks..</p>

<div id="net_list"></div>
<br>
<table>
  <tr>
    <td>Password</td>
    <td><input value="" type="password" id="password" placeholder="WiFi password"/></td>
  </tr>
</table>
<br>
<div>
  <button onclick="uploadConfig()">Save and restart</button>
</div>
<h5 style="color:red;"><i>*sending nil SSID and password would delete saved network and restart the node.</i></h5>
<h4>Server Settings</h4>
<ul id="server_settings"></ul>
<h4>Diagnostics</h4>
<table style="width:30%">
  <tr>
    <th>LED Status</th>
    <th>Interpretation</th> 
  </tr>
  <tr>
    <td>SOLID OFF</td>
    <td>Successfully connected to the Internet</td>
  </tr>
  <tr>
    <td>SOLID ON</td>
    <td>WiFi connection failed, entered AP config mode</td>
  </tr>
  <tr>
    <td>NORMAL BLINK (~ 1 sec)</td>
    <td>Waiting for WiFi connection in station mode</td>
  </tr>
  <tr>
    <td>SHORT BURST (~ 3 sec)</td>
    <td>WiFi connected, but no Internet connection.</td>
  </tr>
</table>

<p id="status"></p>

<script>
function fetchnetworks() { // called after home page loads
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      var respObj = JSON.parse(this.responseText);
     var data=respObj.networks;
     for(i=0;i<data.length;i++) {
       var radiobox = document.createElement('input');
        radiobox.type = 'radio';
        radiobox.value = data[i];
        radiobox.name = 'networks';
        var label = document.createElement('label')
        label.htmlFor = 'contact'; 
        var description = document.createTextNode(data[i]);
        label.appendChild(description);
        var newline = document.createElement('br');
        var container = document.getElementById('net_list');
        container.appendChild(radiobox);
        container.appendChild(label);
        container.appendChild(newline);
     }
     document.getElementById("networks").innerHTML = "Available networks: " + data.length;

     console.log(JSON.stringify(respObj.mqtt_data));
     var str = "<li>MQTT server: " + respObj.mqtt_data["server"] + "</li>" +
               "<li>MQTT port: " + respObj.mqtt_data["port"] + "</li>" +
               "<li>MQTT user: " + respObj.mqtt_data["username"] + "</li>" +
               "<li>MQTT pass: " + respObj.mqtt_data["password"] + "</li>" +
               "<li>Publish topic: " + respObj.mqtt_data["pub_topic"] + "</li>" +
               "<li>Subscribed topic: " + respObj.mqtt_data["sub_topic"] + "</li>" +
               "<li>Last will: " + respObj.mqtt_data["lwt"] + "</li>";
     document.getElementById("server_settings").innerHTML = str;
    }
  };
  xhttp.open("GET", "/fetchnetworks", true);
  xhttp.send();  
}

function uploadConfig() {
  var xhttp = new XMLHttpRequest();
  var elements = document.getElementsByName('networks');
  var ssid; 
  elements.forEach(e => {
        if (e.checked) {
            //if radio button is checked, set sort style
            ssid = e.value;
        }
  });
  var password = document.getElementById("password").value;
  console.log("SSID: " + ssid + ", Password: " + password);
  var credentials = {ssid:ssid, password:password};
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
      var respObj = JSON.parse(this.responseText);
      console.log(respObj.status);
      document.getElementById("status").innerHTML = "Network saved: " + respObj.status + 
      ". ESP8266 will restart and auto connect to the saved network."
    }
  };
  xhttp.open("POST", "/updateconfig", true);
  xhttp.send(JSON.stringify(credentials));
}
</script>

</body>
</html>
)=====";

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void handleRoot() {
  server.send_P(200, "text/html", root);
}

void handleConfigUpdate() {
  String data = server.arg("plain");
  DynamicJsonBuffer jBuffer;
  JsonObject& jObject = jBuffer.parseObject(data);
  
  File configFile = SPIFFS.open("/config.json", "w");
  jObject.printTo(configFile);  
  configFile.close();
  Serial.println(data); //Received from browser
  server.send(200, "application/json", "{\"status\":\"OK\"}");
  delay(500);
  initWiFi(); // Disconnect and then reconnect using updated credentials
}

void fetchnetworks() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& mqtt_data = root.createNestedObject("mqtt_data");
  mqtt_data["server"] = mqtt_server;
  mqtt_data["port"] = mqtt_port;
  mqtt_data["username"] = mqtt_user;
  mqtt_data["password"] = mqtt_pass;
  mqtt_data["sub_topic"] = intopic;
  mqtt_data["pub_topic"] = outtopic;
  mqtt_data["lwt"] = lwtMessage;
  JsonArray& networks = root.createNestedArray("networks");
  Serial.print("Scan start ... ");
  int n = WiFi.scanNetworks();
  Serial.print(n);
  Serial.println(" network(s) found");
  for (int i = 0; i < n; i++)
  {
    Serial.println(WiFi.SSID(i));
    networks.add(WiFi.SSID(i));
  }
  root.prettyPrintTo(Serial);
  String jsonStr;
  root.printTo(jsonStr);
  
  server.send(200, "application/json", jsonStr);  
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  SPIFFS.begin(); //mount file system
  pinMode(LED_BUILTIN, OUTPUT);
  initWiFi();

  // Start MDNS responder: This is a shakey bit. Sometimes node needs restart to work
  if (!MDNS.begin("esp8266")) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  
  server.on("/", handleRoot);
  server.on("/fetchnetworks", fetchnetworks);
  server.on("/updateconfig", handleConfigUpdate);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
}

void loop() {
  MDNS.update(); // MDNS listener
  server.handleClient(); // HTTP server listener
  if (!client.connected() && WiFi.status() == WL_CONNECTED) {
     reconnect(); //MQTT connection listener
  }
  client.loop();
}

void initWiFi() {
  WiFi.softAPdisconnect(true);
  WiFi.disconnect();
  delay(1000);

  if(SPIFFS.exists("/config.json")) {
    const char * _ssid = "", *_pass = "";
    File configFile = SPIFFS.open("/config.json", "r");
    if(configFile) {
      size_t size = configFile.size();
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);
      configFile.close();
      DynamicJsonBuffer jsonBuffer;
      JsonObject& jObject = jsonBuffer.parseObject(buf.get());
      if(jObject.success()) {
        _ssid = jObject["ssid"];
        _pass = jObject["password"];
        Serial.println("Read SSID from JSON: " + String(_ssid));
        Serial.println("Read PASS from JSON: " + String(_pass));
        WiFi.mode(WIFI_STA);
        WiFi.begin(_ssid, _pass);
        Serial.print("Connecting to home wifi using stored credentials..");
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.print(".");
          digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
          if(millis()-startTime > 5000) {
            break;
          }
        }
        if(WiFi.status() == WL_CONNECTED) {
          Serial.println("Connected to WiFi using stored credentials.");
          delay(1000);
          Serial.print("Station IP [");
          Serial.print(WiFi.localIP());
          Serial.print("] ");
          Serial.println("Please visit station IP in your browser to change WiFi network.");
          digitalWrite(LED_BUILTIN, HIGH); //Turn off LED to indicate success Wifi connection
  
          //Now connect to MQTT after connecting to internet
          initMQTT();
        
        } else {
          Serial.println("Could not connect to WiFi using stored credentials.");
          Serial.println("Connect to below AP and set up ESP8266 by visiting 192.168.4.1 in browser.");
          WiFi.mode(WIFI_AP);
          WiFi.softAP(ssid, password);
          IPAddress myIP = WiFi.softAPIP();
          Serial.print("AP IP address: ");
          Serial.println(myIP);
          digitalWrite(LED_BUILTIN, LOW); // Turn on LED to indicate AP mode connection available 
        } 
      }
    }
  }
}

void initMQTT() {
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  Serial.print("Attempting MQTT connection...");
  // Create a random client ID
  String clientId = "ESP8266Client-";
  clientId += String(random(0xffff), HEX);
  // Attempt to connect
  if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass, outtopic, 1, 1, lwtMessage)) {
    Serial.println("connected");
    delay(500);
    client.subscribe(intopic);
    // Once connected, publish an announcement...
    client.publish(outtopic, "ESP8266 came online!");   
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char message[10]="";
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  Serial.print(message);
  //Test
  //digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  handleCommand(message, length);
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    digitalWrite(LED_BUILTIN, LOW); // indicate MQTT connection is in progress
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass, outtopic, 1, 1, lwtMessage)) {
      Serial.println("MQTT connection re-established.");
      // Once connected, publish an announcement...
      client.publish(outtopic, "MQTT connection re-established.");
      // ... and resubscribe
      client.subscribe(intopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 3 seconds");
      // Wait 3 seconds before retrying
      delay(3000);
    }
  }
  digitalWrite(LED_BUILTIN, HIGH); // indicate MQTT connection established
}

void handleCommand(char buf[], unsigned int length) { //Command format: opcode|gpio_num|state
  int i = 0;
  char *p = strtok (buf, "|");
  char *array[3];
  while (p != NULL) {
    array[i++] = p;
    p = strtok (NULL, "|");
  }
  String op = (String)array[0];
  int gpio = ((String)array[1]).toInt();
  int state = ((String)array[2]).equals("HIGH") ? HIGH : LOW;
  Serial.println();
  Serial.println(op);
  Serial.println(gpio);
  Serial.println(state);

  if(op.equals("wr")) {   ////////////////////////////// Ex. "wr|2|HIGH"
    digitalWrite(gpio, state);
    readGpioAndPublish(gpio);
  }
  else if(op.equals("rd")) {  ////////////////////////////// Ex. "rd|2"
    readGpioAndPublish(gpio);    
  }
  else {
    client.publish(outtopic, "invalid command");
  }
 
}

void readGpioAndPublish(int gpio) {
  int _st = digitalRead(gpio);
  Serial.println(_st);
  if(digitalRead(gpio))
    client.publish(outtopic, "HIGH");
  else if(!digitalRead(gpio))
    client.publish(outtopic, "LOW");
  else
    client.publish(outtopic, "UNDEF");
}
