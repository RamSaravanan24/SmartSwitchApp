#include "ESP8266WiFi.h"
#include "ESPAsyncWebServer.h"
#include <EEPROM.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define RELAY_NO true
#define NUM_RELAYS  2
#define NAME_SIZE 20
#define TIME_SIZE 6
#define EEPROM_SIZE (NUM_RELAYS * NAME_SIZE)

bool relayStatus[NUM_RELAYS] = {false, false}; // true for ON, false for OFF

String relayOnTimes[NUM_RELAYS];
String relayOffTimes[NUM_RELAYS];

char relayNames[NUM_RELAYS][NAME_SIZE] = {"Relay 1","Relay 2"}; // Array to store names
int relayGPIOs[NUM_RELAYS] = {5, 4};  // Example GPIOs

const int relayNamesStartAddr = 0;              // Starting address for relay names
const int relaySchedulesStartAddr = relayNamesStartAddr + (NUM_RELAYS * NAME_SIZE); 

// Wi-Fi credentials
const char* ssid = "YOUR_WIFI_ID";
const char* password = "YOUR_WIFI_PASSWORD";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);  // Time sync

const char* PARAM_INPUT_1 = "relay";  
const char* PARAM_INPUT_2 = "state";
const char* PARAM_INPUT_NAME = "name";  // Parameter for relay name

AsyncWebServer server(80);

// HTML web page with input fields for relay names
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 3.0rem;}
    p {font-size: 3.0rem;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 34px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; transition: .4s; border-radius: 68px}
    input:checked+.slider {background-color: #2196F3}
    input:checked+.slider:before {transform: translateX(52px)}
  </style>
</head>
<body>
  <h2>Smart Switch Testing 65 Percent Completion</h2>
  %BUTTONPLACEHOLDER%

  <h3>Set Relay Schedules</h3>
  <div id="schedulePlaceHolder"></div>

  <script>
  
    function toggleCheckbox(element) {
    let relayId = element.id;  // Relay ID extracted from the element's ID
    let relayState = element.checked ? 1 : 0;  // Get relay state (1 = ON, 0 = OFF)

    // Log to the console to ensure the correct data is being captured
    console.log("Toggling Relay " + relayId + " to state " + relayState);

    // Send the request to the server to update the relay
    let xhr = new XMLHttpRequest();
    let url = "/update?input1=" + relayId + "&input2=" + relayState;
    console.log("Sending request to: " + url);  // Log the URL for debugging
    xhr.open("GET", url, true);
    xhr.send();
  }


  function updateRelayName(relayId) {
    var relayName = document.getElementById("name_" + relayId).value;
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/updateName?relay=" + relayId + "&name=" + encodeURIComponent(relayName), true);
    xhr.send();
  }

  function updateSchedule(relayId) {
  // Get the on and off times from the input fields
  var onTime = document.getElementById('onTime_' + relayId).value;
  var offTime = document.getElementById('offTime_' + relayId).value;

  // Check if the times are valid
  if (onTime && offTime) {
    // Send the on and off times to the backend via an HTTP request
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "/updateSchedule", true); // Assuming a POST request endpoint /updateSchedule
    xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
    
    // Sending relayId, onTime, and offTime as URL-encoded parameters
    xhr.send("relayId=" + relayId + "&onTime=" + onTime + "&offTime=" + offTime);

    // Handle the server's response (optional)
    xhr.onreadystatechange = function () {
      if (xhr.readyState == 4 && xhr.status == 200) {
        console.log("Schedule updated successfully for Relay " + relayId);
      } else if (xhr.readyState == 4) {
        console.error("Failed to update schedule for Relay " + relayId);
      }
    };
  } else {
    console.error("Invalid on/off times for Relay " + relayId);
    }
  }


  function generateSchedules(numRelays, relayNames) {
    var schedulePlaceholder = document.getElementById('schedulePlaceHolder');
    for (var i = 0; i < numRelays; i++) {
      var relayId = i + 1;
      var relayName = relayNames[i] || 'Relay ' + relayId; // Fallback to default if no name is set
      var scheduleHtml = `
        <div>
          <h4>Schedule for ${relayName}</h4>
          <label>On Time:</label>
          <input type="time" id="onTime_${relayId}" onchange="updateSchedule(${relayId})">
          <label>Off Time:</label>
          <input type="time" id="offTime_${relayId}" onchange="updateSchedule(${relayId})">
        </div>
      `;
      schedulePlaceholder.innerHTML += scheduleHtml;
    }
  }

  function loadSchedules() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/getSchedules", true); // Fetch the schedule data from the backend
  xhr.onload = function() {
    if (xhr.status == 200) {
      var schedules = JSON.parse(xhr.responseText);
      schedules.forEach(function(schedule) {
        var relayId = schedule.relayId;
        var onTime = schedule.onTime;
        var offTime = schedule.offTime;

        // Update the input fields with the fetched data
        document.getElementById('onTime_' + relayId).value = onTime;
        document.getElementById('offTime_' + relayId).value = offTime;
      });
    }
  };
  xhr.send();
}
  function updateRelayStatus() {
    // Fetch relay status from the Arduino server
    fetch('/relayStatus')
      .then(response => response.json())
      .then(data => {
        for (let i = 1; i <= NUM_RELAYS; i++) {
          let relaySlider = document.getElementById("relaySlider" + i);
          if (data["relay" + i] === "ON") {
            relaySlider.checked = true;  // Update slider to ON
          } else {
            relaySlider.checked = false; // Update slider to OFF
          }
        }
      })
      .catch(error => console.error('Error:', error));
  }
  setInterval(updateRelayStatus, 1000);

  function fetchRelayNames() {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/fetchRelayNames", true);  // Use /fetchRelayNames as defined in the server
    xhr.onload = function() {
      if (xhr.status === 200) {
        var relayNames = JSON.parse(xhr.responseText);
        generateSchedules(relayNames.length, relayNames);  // Use fetched relay names
      }
    };
    xhr.send();
  }
  window.onload = function() {
    fetchRelayNames();
    loadSchedules();
  };
  </script>
</body>
</html>
)rawliteral";

void saveSchedule(int relayId, String onTime, String offTime) {
    Serial.println("Saving schedule for Relay " + String(relayId));
    Serial.println("On Time: " + onTime);
    Serial.println("Off Time: " + offTime);

    int addr = relaySchedulesStartAddr + (relayId - 1) * (TIME_SIZE * 2); // Calculate the starting address for this relay

    // Save onTime (10 bytes max)
    for (int i = 0; i < 10; i++) {
        if (i < onTime.length()) {
            EEPROM.write(addr + i, onTime[i]);
        } else {
            EEPROM.write(addr + i, 0); // Null-terminate the string
        }
    }

    // Save offTime (10 bytes max)
    for (int i = 0; i < 10; i++) {
        if (i < offTime.length()) {
            EEPROM.write(addr + 10 + i, offTime[i]);
        } else {
            EEPROM.write(addr + 10 + i, 0); // Null-terminate the string
        }
    }
    
    EEPROM.commit(); // Commit changes to EEPROM
}

void loadRelaySchedules() {
    for (int relayId = 1; relayId <= NUM_RELAYS; relayId++) {
        relayOnTimes[relayId - 1] = getOnTimeFromEEPROM(relayId);
        relayOffTimes[relayId - 1] = getOffTimeFromEEPROM(relayId);
    }
}

void updateRelaySchedule(int relayId, String newOnTime, String newOffTime) {
    relayOnTimes[relayId - 1] = newOnTime; // Update the array
    relayOffTimes[relayId - 1] = newOffTime; // Update the array
    saveSchedule(relayId, newOnTime, newOffTime); // Save to EEPROM
    EEPROM.commit();
}

String getOnTimeFromEEPROM(int relayId) {
    String onTime = "";
    int addr = (relayId - 1) * 20; // Calculate the starting address
    for (int i = 0; i < 10; i++) {
        char ch = EEPROM.read(addr + i);
        if (ch == 0) break; // Stop reading if we hit the null terminator
        onTime += ch;
    }
    return onTime;
}

String getOffTimeFromEEPROM(int relayId) {
    String offTime = "";
    int addr = (relayId - 1) * 20 + 10; // Calculate the starting address for offTime
    for (int i = 0; i < 10; i++) {
        char ch = EEPROM.read(addr + i);
        if (ch == 0) break; // Stop reading if we hit the null terminator
        offTime += ch;
    }
    return offTime;
}

void handleRelayStatus(AsyncWebServerRequest *request) {
  String statusJSON = "{";
  for (int i = 0; i < NUM_RELAYS; i++) {
    statusJSON += "\"relay" + String(i + 1) + "\":" + (relayStatus[i] ? "\"ON\"" : "\"OFF\"");
    if (i < NUM_RELAYS - 1) {
      statusJSON += ",";
    }
  }
  statusJSON += "}";

  // Send the JSON response to the client
  request->send(200, "application/json", statusJSON);
}


void saveRelayNames() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    for (int j = 0; j < NAME_SIZE; j++) {
      EEPROM.write(relayNamesStartAddr + i * NAME_SIZE + j, relayNames[i][j]);
    }
  }
  EEPROM.commit();
}

// Load relay names from EEPROM
void loadRelayNames() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    for (int j = 0; j < NAME_SIZE; j++) {
      relayNames[i][j] = EEPROM.read(i * NAME_SIZE + j);
    }
  }
}

// Function to get relay names
String getRelayName(int index) {
  return String(relayNames[index - 1]); // Return the name for the relay at the specified index
}

void updateRelayName(int relayId, const char* newName) {
  strncpy(relayNames[relayId - 1], newName, NAME_SIZE - 1);
  relayNames[relayId - 1][NAME_SIZE - 1] = '\0';
  saveRelayNames();
  Serial.println("Updated Relay " + String(relayId) + " to name: " + String(newName));
  EEPROM.commit();
}

String relayState(int numRelay){
  if(RELAY_NO){
    if(digitalRead(relayGPIOs[numRelay-1])){
      return "";
    }
    else {
      return "checked";
    }
  }
  else {
    if(digitalRead(relayGPIOs[numRelay-1])){
      return "checked";
    }
    else {
      return "";
    }
  }
  return "";
}

// Process HTML template
String processor(const String& var) {
  if (var == "BUTTONPLACEHOLDER") {
    String buttons = "";
    for (int i = 1; i <= NUM_RELAYS; i++) { 
      String relayStateValue = relayState(i);
      String relayName = getRelayName(i); // Get stored name for each relay
      buttons += "<h4><input type=\"text\" value=\"" + relayName + "\" id=\"name_" + String(i) + "\" onchange=\"updateRelayName(" + String(i) + ")\"/></h4>";
      buttons += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"" + String(i) + "\" " + relayStateValue + ">";
      buttons += "<span class=\"slider\"></span></label>";
    }
    return buttons;
  }
  return String();
}

void blinkcon(){
  pinMode(LED_BUILTIN, OUTPUT);
   digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on (Note that LOW is the voltage level
  delay(1000);                      // Wait for a second
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  delay(1000);                      // Wait for two seconds (to demonstrate the active low LED)
}


void setup() {
  Serial.begin(115200);
  EEPROM.begin(512); // Initialize EEPROM with a size of 512 bytes (adjust if needed)
  loadRelayNames();
  for (int i = 1; i <= NUM_RELAYS; i++) {
    pinMode(relayGPIOs[i-1], OUTPUT);
    if (RELAY_NO) {
      digitalWrite(relayGPIOs[i-1], HIGH);
      relayStatus[i-1] = true;
    } else {
      digitalWrite(relayGPIOs[i-1], LOW);
      relayStatus[i-1] = false;
    }
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    blinkcon();
  }  
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.update();

  loadRelaySchedules();
  loadRelayNames();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/relayStatus", HTTP_GET, handleRelayStatus);

  server.on("/updateSchedule", HTTP_POST, [](AsyncWebServerRequest *request) {
  if (request->hasParam("relayId", true) && request->hasParam("onTime", true) && request->hasParam("offTime", true)) {
    int relayId = request->getParam("relayId", true)->value().toInt();
    String onTime = request->getParam("onTime", true)->value();
    String offTime = request->getParam("offTime", true)->value();

    // Save onTime and offTime for the relayId
    updateRelaySchedule(relayId, onTime, offTime);

    // Send success response
    request->send(200, "text/plain", "Schedule updated");
  } else {
    request->send(400, "text/plain", "Invalid request");
  }
});



  server.on("/fetchRelayNames", HTTP_GET, [](AsyncWebServerRequest *request) {
    String namesJson = "[";
    for (int i = 0; i < NUM_RELAYS; i++) {
        namesJson += "\"" + String(relayNames[i]) + "\"";
        if (i < NUM_RELAYS - 1) namesJson += ",";
    }
    namesJson += "]";
    request->send(200, "application/json", namesJson);
  });

  server.on("/getSchedules", HTTP_GET, [](AsyncWebServerRequest *request) {
  String jsonResponse = "[";

  for (int relayId = 1; relayId <= NUM_RELAYS; relayId++) {
    String onTime = getOnTimeFromEEPROM(relayId); // Fetch saved onTime from EEPROM
    String offTime = getOffTimeFromEEPROM(relayId); // Fetch saved offTime from EEPROM

    jsonResponse += "{";
    jsonResponse += "\"relayId\": " + String(relayId) + ",";
    jsonResponse += "\"onTime\": \"" + onTime + "\",";
    jsonResponse += "\"offTime\": \"" + offTime + "\"";
    jsonResponse += "}";

    if (relayId < NUM_RELAYS) {
      jsonResponse += ",";
    }
  }

  jsonResponse += "]";
  
  request->send(200, "application/json", jsonResponse);
  });

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    String inputMessage;
    String inputMessage2;

    // Use && for logical AND
    if (request->hasParam("input1") && request->hasParam("input2")) {
        inputMessage = request->getParam("input1")->value();   // Relay number
        inputMessage2 = request->getParam("input2")->value();  // ON/OFF state

        int relayId = inputMessage.toInt();  // Relay ID from the request
        Serial.println("Received relayId: " + String(relayId) + " | Relay state: " + inputMessage2);

        // Check if the relayId is valid
        if (relayId >= 1 && relayId <= 2){  // Assuming 2 relays: 1 for Relay 1, 2 for Relay 2
            if (RELAY_NO) {
                Serial.println("Toggling NO Relay " + String(relayId));
                digitalWrite(relayGPIOs[relayId - 1], !inputMessage2.toInt()); // Toggle state
                relayStatus[relayId - 1] = !inputMessage2.toInt();

            } else {
                Serial.println("Toggling NC Relay " + String(relayId));
                digitalWrite(relayGPIOs[relayId - 1], inputMessage2.toInt()); // Set state
                relayStatus[relayId - 1] = inputMessage2.toInt();
            }
            request->send(200, "text/plain", "Relay " + String(relayId) + " toggled");
        } else {
            Serial.println("Invalid Relay ID received");
            request->send(400, "text/plain", "Invalid Relay ID");
        }
    } else {
        Serial.println("Parameters missing");
        request->send(400, "text/plain", "Parameters missing");
    }
});

  // Route for updating relay names
server.on("/updateName", HTTP_GET, [](AsyncWebServerRequest *request) {
  char tempName[NAME_SIZE];
  if (request->hasParam("relay") && request->hasParam("name")) {
    int relayId = request->getParam("relay")->value().toInt();
    request->getParam("name")->value().toCharArray(tempName, NAME_SIZE);

    Serial.println("Received request to update Relay " + String(relayId) + " with name: " + String(tempName));

    strncpy(relayNames[relayId - 1], tempName, NAME_SIZE - 1);
    relayNames[relayId - 1][NAME_SIZE - 1] = '\0';

    saveRelayNames(); // Save updated names to EEPROM

    Serial.println("Updated Relay " + String(relayId) + " to name: " + String(tempName));
    request->send(200, "text/plain", "Name updated");
  } else {
    request->send(400, "text/plain", "Invalid request");
  }
});
server.begin();
}

void loop() {
  timeClient.update();
  String currentTime = timeClient.getFormattedTime().substring(0, 5); // Get current time in HH:MM format
  
  // Debugging: Print current time to check
  Serial.println("Current Time: " + currentTime);

  for (int relayId = 0; relayId < NUM_RELAYS; relayId++) {
    String onTime = relayOnTimes[relayId];
    String offTime = relayOffTimes[relayId];

    // Debugging: Print onTime and offTime to check
    Serial.println("Relay " + String(relayId + 1) + " On Time: " + onTime + " Off Time: " + offTime);

    // Check if current time matches onTime
    if ( (currentTime >= onTime && currentTime < offTime) && (onTime != "") && (offTime != "") ) {
      // Ensure relayId is within bounds
      if (relayId >= 0 && relayId < NUM_RELAYS) {
        digitalWrite(relayGPIOs[relayId], LOW); // Adjusted index
        relayStatus[relayId] = false; // Relay is ON
        Serial.println("Relay " + String(relayId + 1) + " Turned on at: " + onTime);
      }
    } else if ( (currentTime >= offTime) && (onTime != "") && (offTime != "") )  {
      // Ensure relayId is within bounds
      if (relayId >= 0 && relayId < NUM_RELAYS) {
        digitalWrite(relayGPIOs[relayId], HIGH); // Adjusted index
        relayStatus[relayId] = true; // Relay is OFF
        Serial.println("Relay " + String(relayId + 1) + " Turned on at: " + offTime);
      }
    }
  }
  
  delay(1000); // Delay for 1 second to check more frequently
}
