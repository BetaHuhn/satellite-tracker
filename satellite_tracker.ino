/* NodeMCU ESP8266 ISS Satellite tracker
 * Maximilian Schiller - hello@mxis.ch - 06.08.2019
 * 
 * NodeMCU ESP8266 Satellite tracker can locate the position of Satellites (currently ISS and HST)
 * and point to it. It uses a stepper motor and a servo for the postioning of the pointer
 * and a simple webserver for calibration and control. It gets the Azimuth and Elevation of
 * the satellite from the n2yo API.
 * Old version used a python server which calculated the Azimuth and Elevation
 *  
*/
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <Stepper.h>


//Set user variables used *change these*
//Get your API key by creating a account at https://www.n2yo.com/
#define WIFI_SSID "NETWORK_NAME"
#define  WIFI_PASSWORD "NETWORK_PASSWORD"
const String api_key = "API_KEY"; 
const String user_lat = "LATITUDE";
const String user_long = "LONGITUDE";
const String user_alt = "ALLTITUDE";

const int stepsPerRevolution = 2048;
Stepper myStepper(stepsPerRevolution, D1, D5, D2, D6); //Define pins for stepper motor D1/In3, D5/In1, D2/In4, D6/In2

int period = 10000;  //Interval of updating position
unsigned long time_now = 0; 
byte duration;

bool calibration = false;
String current_satellite = "iss"; //change this if you want to start with HST 
String satellite;
String url;

int steps;
int angle;
int value;
String header;

int current_servo = 90;
int STEPS = 2048; //change this if your stepper has more than 2048 steps per revolution 
int steps_taken = 0;
float steps_to_take;
float steps_per_deg = float(STEPS)/360.0;

Servo myservo;
WiFiServer server(80);

void setup() {  
  Serial.begin(115200);
  delay(2000);
  myservo.attach(D3); //Servo Pin
  Serial.println("Ready");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  myStepper.setSpeed(15);
  myservo.write(90);
  server.begin();
}

void loop() {
  WiFiClient client = server.available();   //Waiting for clients
  if (client) {                             // If client connects
    Serial.println("New Client.");          
    String currentLine = "";                //create string with incoming data from client
    while (client.connected()) {            
      if (client.available()) {             //If byte available,
        char c = client.read();             //read it                  
        header += c;
        if (c == '\n') {                    //If end of request send response
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Refresh: 10;URL='//192.168.2.123/'>"); //Tell client to refresh every 10 sec
            client.println("Connection: close");
            client.println();
            //defining what happens on button presses
            if (header.indexOf("GET /turn/left") >= 0) {
              Serial.println("Turn left");
              client.print("<HEAD>");
              client.print("<meta http-equiv=\"refresh\" content=\"0;url=/\">");
              client.print("</head>");
              myStepper.step(-80);
            } else if (header.indexOf("GET /turn/right") >= 0) {
              Serial.println("Turn right");
              client.print("<HEAD>");
              client.print("<meta http-equiv=\"refresh\" content=\"0;url=/\">");
              client.print("</head>");
              myStepper.step(80);
            } else if (header.indexOf("GET /turn/90") >= 0) {
              Serial.println("Turn 90");
              client.print("<HEAD>");
              client.print("<meta http-equiv=\"refresh\" content=\"0;url=/\">");
              client.print("</head>");
              myStepper.step(-512);
            } else if (header.indexOf("GET /turn/m90") >= 0) {
              Serial.println("Turn -90");
              client.print("<HEAD>");
              client.print("<meta http-equiv=\"refresh\" content=\"0;url=/\">");
              client.print("</head>");
              myStepper.step(512);
            } else if (header.indexOf("GET /change/iss") >= 0) {
              Serial.println("Change to ISS");
              current_satellite = "iss";
              getData(current_satellite);
              time_now = millis();
              client.print("<HEAD>");
              client.print("<meta http-equiv=\"refresh\" content=\"0;url=/\">");
              client.print("</head>");
            } else if (header.indexOf("GET /change/hst") >= 0) {
              Serial.println("Change to HST");
              current_satellite = "hst";
              getData(current_satellite);
              time_now = millis();
              client.print("<HEAD>");
              client.print("<meta http-equiv=\"refresh\" content=\"0;url=/\">");
              client.print("</head>");
            } else if (header.indexOf("GET /calibrated") >= 0) {
              Serial.println("End calibration");
              calibration = !calibration;
              steps_to_take = steps_taken * -1;
              Serial.print("Returning to north: ");
              Serial.println(steps_to_take);
              moveStepper(steps_to_take);
              client.print("<HEAD>");
              client.print("<meta http-equiv=\"refresh\" content=\"0;url=/\">");
              client.print("</head>");
            } else if (header.indexOf("GET /calibrate") >= 0) {
              Serial.println("Start calibration");
              myservo.write(90);
              calibration = !calibration;
              client.print("<HEAD>");
              client.print("<meta http-equiv=\"refresh\" content=\"0;url=/\">");
              client.print("</head>");
            }
            //Printing html site
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<title>Satellite pointer</title>");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            //CSS code
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #333344; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #888899;}</style></head>");
            client.println("<body><h1>Satellite tracker</h1>"); //change website title
            //Show buttons according to variables
            if(calibration == true){
              client.println("<h2>Current Satellite: " + current_satellite + "</h2>");     
              if (current_satellite == "iss") {
                client.println("<p><a href=\"/change/hst\"><button class=\"button\">Switch to HST</button></a></p>");
              } else {
                client.println("<p><a href=\"/change/iss\"><button class=\"button\">Switch to ISS</button></a></p>");
              }
              client.println("<p><a href=\"/calibrate\"><button class=\"button button2\">Calibrated</button></a></p>");
            }else if(calibration == false){
              myservo.write(90);
              client.println("<h2>Currently not calibrated</h2>");
              client.println("<p><a href=\"/turn/left\"><button class=\"button button2\">left</button></a></p>");
              client.println("<p><a href=\"/turn/right\"><button class=\"button button2\">right</button></a></p>");
              client.println("<p><a href=\"/turn/90\"><button class=\"button button2\">turn 90</button></a></p>");
              client.println("<p><a href=\"/turn/m90\"><button class=\"button button2\">turn -90</button></a></p>");
              client.println("<p><a href=\"/calibrated\"><button class=\"button\">Calibrate</button></a></p>");
            }
            client.println("</body></html>");
            client.println();
            break;
          } else { 
            currentLine = "";
          }
        } else if (c != '\r') {  
          currentLine += c;    
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Client disconected.");
    Serial.println("");
  }
  if(calibration == false){
     Serial.print(".");
     delay(1000);
  }else if(calibration == true){
    if(millis() > time_now + period){
      time_now = millis();
      Serial.println("Getting new Data");
      getData(current_satellite);
    }
  }
}

void getData(String satellite){
  if(satellite == "iss"){
    url = "http://www.n2yo.com/rest/v1/satellite/positions/25544/" + user_lat + "/" + user_long + "/" + user_alt + "/1?apiKey=" + api_key;
  }else if(satellite == "hst"){
    url = "http://www.n2yo.com/rest/v1/satellite/positions/20580/" + user_lat + "/" + user_long + "/" + user_alt + "/1?apiKey=" + api_key;
  }else{
    Serial.print("Function does not support the param: ");
    Serial.println(satellite);
  }
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode > 0) {
    String json_data = http.getString();
    http.end();
    const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(8) + 150;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, json_data);
    JsonObject info = doc["info"];
    const char* sat_name = info["satname"]; 
    int sat_id = info["satid"]; 
    int transactions_count = info["transactionscount"];
    if(transactions_count > 800){
        Serial.print("Warning, exceding max requests: "); 
        Serial.println(transactions_count);
        delay(10000);
    }
    JsonObject positions_0 = doc["positions"][0];
    float sat_azimuth = positions_0["azimuth"]; 
    float sat_elevation = positions_0["elevation"];
    Serial.print("Satellite: ");
    Serial.println(sat_name);
    Serial.print("Azimuth: ");
    Serial.println(sat_azimuth);
    Serial.print("Elevation: ");
    Serial.println(sat_elevation);
    Serial.println();
    int steps_to_take = sat_azimuth * steps_per_deg;
    steps_to_take = steps_to_take - steps_taken;
    steps_taken += steps_to_take;
    moveStepper(steps_to_take);
    moveServo(sat_elevation);
  }else {
    Serial.println(httpCode);
    Serial.printf("Request failed: %s\n", http.errorToString(httpCode).c_str());
    http.end();
  }
}

void moveServo(int angle){
  angle = map(angle, -90, 90, 180, 0);
  Serial.print("Current Angle: ");
  Serial.println(current_servo);
  if(current_servo == angle){
    Serial.print("Servo already at: ");
    Serial.println(angle);
  }else{
    Serial.print("Moving Servo to: ");
    Serial.println(angle);
    myservo.write(angle);
    Serial.print("Servo Angle: ");
    Serial.println(angle);
    current_servo = angle;
  }
}

void moveStepper(int steps){
  steps = steps * -1;
  Serial.print("Moving Stepper motor: ");
  Serial.println(steps);
  myStepper.step(steps);
  Serial.print("Stepper steps: ");
  Serial.println(steps);
}


