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

String mqttserver = "";
int mqttport = 0;
String mqttpubtopic = "";
String mqttsubtopic = "";
String lwtmessage = "";

String mdnsaddress = "";

long lastReconnectAttempt = 0;

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
</head>
<body onload="fetchnetworks()">
<h2>ESP8266 Settings Update</h2>
<p id="networks">Scanning available networks..</p>

<div id="net_list"></div>
<br>
<table style="width:40%">
  <tr>
    <td>WiFi Password</td>
    <td><input value="" type="password" id="password" placeholder="WiFi password"/></td>
  </tr>
  <tr>
    <td>Multicast DNS</td>
    <td><input value="esp8266" type="text" id="mdnsaddress" placeholder="MDNS address"/> .local</td>
  </tr>
</table>

<h4>MQTT server details</h4>
<table style="width:40%">
  <tr>
    <td>Broker</td>
    <td><input value="broker.hivemq.com" type="text" id="mqttserver" placeholder="MQTT server"/></td>
  </tr>
  <tr>
    <td>Port</td>
    <td><input value="1883" type="text" id="mqttport" placeholder="MQTT port"/></td>
  </tr>
  <tr>
    <td>Username</td>
    <td><input value="NULL" type="text" id="mqttuser" placeholder="MQTT user"/></td>
  </tr>
  <tr>
    <td>Password</td>
    <td><input value="NULL" type="password" id="mqttpassword" placeholder="MQTT password"/></td>
  </tr>
  <tr>
    <td>Subscribe</td>
    <td><input value="intopic" type="text" id="mqttsubtopic" placeholder="MQTT subscribe"/></td>
  </tr>
  <tr>
    <td>Publish</td>
    <td><input value="outtopic" type="text" id="mqttpubtopic" placeholder="MQTT publish"/></td>
  </tr>
  <tr>
    <td>Last will</td>
    <td><input value="ESP went offline" type="text" id="mqttlwt" placeholder="Last will message"/></td>
  </tr>
</table>
<br>
<div>
  <button onclick="uploadConfig()">Save and restart</button>
</div>
<h5 style="color:red;"><i>*sending nil SSID and password would delete saved network and restart the node.</i></h5>
<h4>Diagnostics</h4>
<table style="width:40%">
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
  var mdnsaddress = document.getElementById("mdnsaddress").value;
  var mqttserver = document.getElementById("mqttserver").value;
  var mqttport = document.getElementById("mqttport").value;
  var mqttuser = document.getElementById("mqttuser").value;
  var mqttpassword = document.getElementById("mqttpassword").value;
  var mqttsubtopic = document.getElementById("mqttsubtopic").value;
  var mqttpubtopic = document.getElementById("mqttpubtopic").value;
  var mqttlwt = document.getElementById("mqttlwt").value;

  var credentials = {ssid:ssid, password:password, mdnsaddress:mdnsaddress, mqttserver:mqttserver, mqttport:mqttport, mqttuser:mqttuser, mqttpassword:mqttpassword, mqttsubtopic:mqttsubtopic, mqttpubtopic:mqttpubtopic, mqttlwt:mqttlwt};
  console.log(JSON.stringify(credentials));
  
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
  
  Serial.print("Configuration written: ");
  Serial.println();
  jObject.prettyPrintTo(Serial);
  Serial.println();
        
  server.send(200, "application/json", "{\"status\":\"OK\"}");
  delay(1000);
  //initWiFi(); // Disconnect and then reconnect using updated credentials
  ESP.restart();
}

void fetchnetworks() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonArray& networks = root.createNestedArray("networks");
  Serial.print("Scan start ... ");
  int n = WiFi.scanNetworks();
  Serial.print(n);
  Serial.println(" network(s) found");
  for (int i = 0; i < n; i++) {
    networks.add(WiFi.SSID(i));
  }
  root.prettyPrintTo(Serial);
  String jsonStr;
  root.printTo(jsonStr);
  
  server.send(200, "application/json", jsonStr);  
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

        mqttserver = jObject["mqttserver"].as<String>();
        String mqttport_str = jObject["mqttport"];
        mqttport = mqttport_str.toInt();

        mqttpubtopic = jObject["mqttpubtopic"].as<String>();
        mqttsubtopic = jObject["mqttsubtopic"].as<String>();
        lwtmessage = jObject["mqttlwt"].as<String>();
        mdnsaddress = jObject["mdnsaddress"].as<String>();
        
        Serial.print("Reading stored config: ");
        Serial.println();
        jObject.prettyPrintTo(Serial);
        Serial.println();
        Serial.print("Connecting to home wifi using stored credentials..");
        WiFi.mode(WIFI_STA);
        WiFi.begin(_ssid, _pass);
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.print(".");
          digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
          if(millis()-startTime > 5000) {
            break;
          }
        }
        randomSeed(micros());
        
        if(WiFi.status() == WL_CONNECTED) {
          Serial.println("Connected to WiFi using stored credentials.");
          Serial.print("Station IP [");
          Serial.print(WiFi.localIP());
          Serial.print("] ");
          Serial.println("Please visit station IP in your browser to change WiFi network.");
          digitalWrite(LED_BUILTIN, HIGH); //Turn off LED to indicate success Wifi connection
          
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

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  Serial.print("Payload length [");
  Serial.print(length);
  Serial.print("] ");

  char message[length];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  Serial.print(message);
  Serial.println();

  handleCommand(message);
}

boolean reconnect() {
  // Create a random client ID
  digitalWrite(LED_BUILTIN, LOW); // indicate MQTT connection is in progress
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  String clientId = "ESP8266Client-";
  clientId += String(random(0xffff), HEX);
  if (client.connect(clientId.c_str(), NULL, NULL, strdup(mqttpubtopic.c_str()), 1, 0, strdup(lwtmessage.c_str()))) {
      Serial.println("MQTT connection re-established.");
      // Once connected, publish an announcement...
      client.publish(strdup(mqttpubtopic.c_str()), "MQTT connection re-established.");
      // ... and resubscribe
      client.subscribe(strdup(mqttsubtopic.c_str()));
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
    }
  return client.connected();
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  SPIFFS.begin(); //mount file system
  pinMode(LED_BUILTIN, OUTPUT);
  initWiFi();

  // Handle routes
  server.on("/", handleRoot);
  server.on("/fetchnetworks", fetchnetworks);
  server.on("/updateconfig", handleConfigUpdate);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  // Initialize MQTT broker
  client.setServer (strdup(mqttserver.c_str()), mqttport);
  client.setCallback(callback);

  // Initialize MDNS  
  if (!MDNS.begin(strdup(mdnsaddress.c_str()))) {
    Serial.println("Error setting up mDNS responder");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started!");
  MDNS.addService("http", "tcp", 80);
  
}

void loop() {
  MDNS.update(); // MDNS listener
  server.handleClient(); // HTTP server listener

  if (!client.connected() && WiFi.status() == WL_CONNECTED) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    client.loop();
  }
}

void handleCommand(char command[]) {
  //client.publish(strdup(mqttpubtopic.c_str()), command);
  DynamicJsonBuffer jBuffer;
  JsonObject& jObject = jBuffer.parseObject(command);
  
  String cmd = jObject["command"].as<String>();
  int gpio = jObject["gpio"].as<String>().toInt();
  bool state = jObject["state"].as<String>() == "HIGH" ? HIGH : LOW;
  
  Serial.println(cmd);
  Serial.println(gpio);
  Serial.println(state);

  if(cmd == "write") {
    digitalWrite(gpio, state);
    readGpioAndPublish(gpio);
  }
  else if(cmd == "read") {
    readGpioAndPublish(gpio);
  }
  
}

void readGpioAndPublish(int gpio) {
  int _st = digitalRead(gpio);
  Serial.println(_st);
  if(digitalRead(gpio))
    client.publish(strdup(mqttpubtopic.c_str()), "HIGH");
  else if(!digitalRead(gpio))
    client.publish(strdup(mqttpubtopic.c_str()), "LOW");
  else
    client.publish(strdup(mqttpubtopic.c_str()), "UNDEF");
}
