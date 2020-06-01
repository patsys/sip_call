
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "SPIFFS.h"
#include "ArduinoJson.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include <Ticker.h>
#include "time.h"
#include "SIP.h"

#define login() if(!checkUser()){return server.requestAuthentication(BASIC_AUTH, "door", "login failed");}


const char* host = "door";
const char* ssid = "door";
const char* password = "eurxdqi!it>yiuj";
const char* bootUser = "admin";
const char* bootPassword = "fe590f7d2a79441576969a187e62054b62ca0ee6728e9fe0c3581c089bb4c977";
const char* ntpServer = "europe.pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
int doNotDistBeginHour=-1;
int doNotDistEndHour=25;

struct Button {
  const uint8_t PIN;
  uint32_t numberKeyPresses;
  unsigned long PressesTime;
  int max;
  bool trigger;
  int resetCount;
};

Button ring{18,0,0,0,0,false};

WebServer server(80);
SIP sipRinger;
Ticker closeTheValve;


/*
 * Login page
 */

 const char* loginIndex =
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value==\admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";
 
/*
 * Server Index Page
 */
 
const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

const char* routerSettings = 
 "<form name='loginForm' action='/routerSettings' method='post'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>ssid:</td>"
        "<td><input type='text' size=25 name='ssid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='password'><br></td>"
              "<br>"
        "<br>"
        "<tr>"
            "<td>Host:</td>"
            "<td><input type='text' size=25 name='host'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' value='Apply'></td>"
        "</tr>"
    "</table>"
"</form>";

const char* sipSettings = 
 "<form name='loginForm' action='/sipSettings' method='post'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>SipSettings</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
        "<td>url:</td>"
        "<td><input type='text' size=25 name='url'><br></td>"
        "</tr>"
        "<tr>"
        "<tr>"
        "<td>port:</td>"
        "<td><input type='text' size=25 name='port'><br></td>"
        "</tr>"
        "<td>user:</td>"
        "<td><input type='text' size=25 name='user'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='password'><br></td>"
              "<br>"
        "<br>"
        "<tr>"
            "<td>dail number:</td>"
            "<td><input type='text' size=25 name='dail'><br></td>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td>Ring Time(seconds):</td>"
            "<td><input type='text' size=25 name='dailtime'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' value='Apply'></td>"
        "</tr>"
    "</table>"
"</form>";

const char* ntpSettings = 
 "<form name='loginForm' action='/ntpSettings' method='post'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ntpSettings</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
        "<td>ntpServer:</td>"
        "<td><input type='text' size=25 name='ntpServer'><br></td>"
        "</tr>"
        "<tr>"
        "<tr>"
        "<td>gmtOffeset</td>"
        "<td><input type='text' size=25 name='gmtOffset'><br></td>"
        "</tr>"
        "<td>daylightOffset:</td>"
        "<td><input type='text' size=25 name='daylightOffset'><br></td>"
        "</tr>"
        "<td>doNotDistBeginHour:</td>"
        "<td><input type='text' size=25 name='doNotDistBeginHour'><br></td>"
        "</tr>"
        "<td>doNotDistEndHour:</td>"
        "<td><input type='text' size=25 name='doNotDistEndHour'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td><input type='submit' value='Apply'></td>"
        "</tr>"
    "</table>"
"</form>";

void stopSnapper() {
  digitalWrite(19,LOW);
}

void setTime(){
  File file;
  if(!SPIFFS.exists("/ntp.json")){
    DynamicJsonDocument doc(512);
    doc["ntpServer"]=ntpServer;
    doc["gmtOffset"]=gmtOffset_sec;
    doc["daylightOffset"]=daylightOffset_sec;
    doc["doNotDistBeginHour"]=doNotDistBeginHour;
    doc["doNotDistEndHour"]=doNotDistEndHour;
    file = SPIFFS.open("/ntp.json", FILE_WRITE);
    serializeJson(doc, file);
    file.close();
  }
  file = SPIFFS.open("/ntp.json", FILE_READ);
  StaticJsonDocument<512> doc;
  deserializeJson(doc, file);
  file.close();
  JsonObject parsed =  doc.as<JsonObject>();
  configTime(parsed["gmtOffset"], parsed["daylightOffset"], parsed["ntpServer"]);
  doNotDistBeginHour=doc["doNotDistBeginHour"];
  doNotDistEndHour=doc["doNotDistEndHour"];
}

char* createSHA(char *str){
  byte res[32];
  char* ret=(char*)(malloc(64));
  mbedtls_md_context_t ctx;
mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
 
 
mbedtls_md_init(&ctx);
mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
mbedtls_md_starts(&ctx);
mbedtls_md_update(&ctx, (const unsigned char *) str, strlen(str));
mbedtls_md_finish(&ctx, res);
mbedtls_md_free(&ctx);

for(int i= 0; i< sizeof(res); i++)
{
sprintf((ret+i*2), "%02x", (int)res[i]);
}
return ret;
}

bool checkUser(){
  bool ret;
  if(!server.hasHeader("Authorization")) {
    return false;
  }
  const char* auth=server.header("Authorization").c_str();
  char* b64=(char*)(malloc(strlen(auth)+1));
  sscanf(auth,"Basic %s",b64);
  unsigned int db64len;
  int b64len=strlen(b64);
  char* db64=(char*)(malloc(b64len+1));
  mbedtls_base64_decode((unsigned char*)db64,b64len,&db64len,(unsigned char*)b64,b64len);
  db64[db64len]=(unsigned char)0;
  free(b64);
  char* hUsername=(char*)(malloc(db64len));
  char* hPassword=(char*)(malloc(db64len));
  sscanf((char*)db64,"%[^:]:%s",hUsername,hPassword);
  free(db64);
  File file = SPIFFS.open("/users.json", FILE_READ);
  StaticJsonDocument<4096> doc;
  deserializeJson(doc, file);
  JsonObject parsed =  doc.as<JsonObject>();
  file.close();
  if(!parsed.containsKey(hUsername)){
    ret=false;
  }else if(strcmp((const char*)parsed[hUsername]["password"],createSHA(hPassword))){
    ret=false;
    Serial.println((const char*)parsed[hUsername]);
  }else{
    ret=true;
  }
  free(hUsername);
  free(hPassword);
  return ret;
}

bool initUser(){
  if(!SPIFFS.exists("/users.json")){
    File file;
    DynamicJsonDocument doc(512);
    doc[bootUser]["password"]=bootPassword;
    file = SPIFFS.open("/users.json", FILE_WRITE);
    serializeJson(doc, file);
    file.close();
  }
}

bool createWlan(){
   // Connect to WiFi network
  File file;
  if(!SPIFFS.exists("/boot.json")){
    DynamicJsonDocument doc(512);
    doc["ssid"]=ssid;
    doc["password"]=password;
    file = SPIFFS.open("/boot.json", FILE_WRITE);
    serializeJson(doc, file);
    file.close();
  }
  file = SPIFFS.open("/boot.json", FILE_READ);
  StaticJsonDocument<512> doc;
  deserializeJson(doc, file);
  file.close();
  JsonObject parsed =  doc.as<JsonObject>();

  WiFi.softAP((const char*)(parsed["ssid"]), (const char*)(parsed["password"]));
  return 0;
}

bool connectWlan(){
   // Connect to WiFi network
  if(SPIFFS.exists("/connect.json")){
  File file = SPIFFS.open("/connect.json", FILE_READ);
  StaticJsonDocument<512> doc;
  deserializeJson(doc, file);
  file.close();
  JsonObject parsed =  doc.as<JsonObject>();
  for(int j=0;j<2;j++){
  WiFi.begin((const char*)(parsed["ssid"]),(const char*)(parsed["password"]));
  WiFi.setHostname(parsed["host"]);
  
  // Wait for connection
  int i=0;
  while (WiFi.status() != WL_CONNECTED && i<15 ) {
        delay(1000);
        i++;
  }
  if(WiFi.status() != WL_CONNECTED){
    WiFi.disconnect(true);
    continue;
  }else{
    break;
  }
  }
  if(WiFi.status() != WL_CONNECTED){
    return 1;
  }
  setTime();
  

   /*use mdns for host name resolution*/
    Serial.println("Register.");
    if(SPIFFS.exists("/sip.json")){
      File file = SPIFFS.open("/sip.json", FILE_READ);
      StaticJsonDocument<512> doc;
      deserializeJson(doc, file);
      file.close();
      JsonObject parsed =  doc.as<JsonObject>();
      int port = parsed["port"];
      int dailtime = parsed["dailtime"];
      char *url=(char*)malloc(strlen((const char*)(parsed["url"]))+1);
      char *user=(char*)malloc(strlen((const char*)(parsed["user"]))+1);
      char *password=(char*)malloc(strlen((const char*)(parsed["password"]))+1);
      char *dail=(char*)malloc(strlen((const char*)(parsed["dail"]))+1);
      strcpy(url,(const char*)(parsed["url"]));
      strcpy(user,(const char*)(parsed["user"]));
      strcpy(password,(const char*)(parsed["password"]));
      strcpy(dail,(const char*)(parsed["dail"]));
      sipRinger.init(url, port, true, user, password, dail,dailtime );
    }
    esp_err_t err = mdns_init();
    if (err) {
        printf("MDNS Init failed: %d\n", err);
        return 0;
    }
    //set hostname
    mdns_hostname_set((const char*)(parsed["host"]));
    //set default instance
    mdns_instance_name_set((const char*)(parsed["host"]));
  }else{
    return 2;
  }
  return 0;
}


void IRAM_ATTR ringHigh() {
  int i;
  for(i=0;i<5&&digitalRead(18);i++){
    delay(6);
  }
  ring.resetCount=i;
  if(i==5){
    sipRinger.ring();
  }
/*  if(mil-ring.PressesTime>500){
     ring.PressesTime=mil;
     ring.resetCount=ring.numberKeyPresses;
     ring.numberKeyPresses=1;
  }else if(ring.numberKeyPresses>=24){
    struct tm timeinfo;
    if(getLocalTime(&timeinfo)){
      if(timeinfo.tm_hour<doNotDistBeginHour && timeinfo.tm_hour>=doNotDistEndHour){
        sipRinger.ring();
      }
    }else{
      sipRinger.ring();
    }
     ring.numberKeyPresses=1;
  }else{
    ring.numberKeyPresses++;
  }
*/
}

/*
 * setup function
 */
void setup(void) {
  Serial.begin(115200);
  bool initok = false;
  initok = SPIFFS.begin();
   if (!(initok)) // Format SPIFS, of not formatted. - Try 1
  {
    Serial.println("SPIFFS Dateisystem formatiert.");
    SPIFFS.format();
    initok = SPIFFS.begin();
  }
  if (!(initok)) // Format SPIFS. - Try 2
  {
    SPIFFS.format();
    initok = SPIFFS.begin();
  }
  initUser();
  if(connectWlan()){
    createWlan();
  }
 
  /*return index page which is stored in serverIndex */
     // Connect to WiFi network
  server.on("/ring", HTTP_GET, []() {
  if(SPIFFS.exists("/sip.json")){
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", "Ring");
    sipRinger.ring();
  }
  });
  server.on("/open", HTTP_GET, []() {
     server.sendHeader("Connection", "close");
     server.send(200, "text/html", "Open");
     digitalWrite(19, HIGH);
     closeTheValve.once_ms(3000,stopSnapper);
  });
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
    server.on("/getTime", HTTP_GET, []() {
    login();
    server.sendHeader("Connection", "close");
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      server.send(200, "text/html", "Failed to obtain time");
    }else{
      char timeStr[35];
      strftime(timeStr, 34, "%A, %B %d %Y %H:%M:%S",&timeinfo);
      server.send(200, "text/html", timeStr);
    }
  });
  server.on("/getTmp", HTTP_GET, []() {
    login();
    server.sendHeader("Connection", "close");
    char timeStr[10];
    sprintf(timeStr, "%d %d",doNotDistBeginHour,doNotDistEndHour);
    server.send(200, "text/html", timeStr);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    login();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  server.on("/routerSettings", HTTP_GET, []() {
    login();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", routerSettings);
  });
    server.on("/routerSettings", HTTP_POST, []() {
    login();
    if (server.hasArg("ssid")&&server.hasArg("password")&&server.hasArg("host")) {
    File file;
    DynamicJsonDocument doc(512);
    doc["ssid"]=server.arg("ssid");
    doc["password"]=server.arg("password");
    doc["host"]=server.arg("host");

    file = SPIFFS.open("/connect.json", FILE_WRITE);
    serializeJson(doc, file);
    file.close();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", routerSettings);
    delay(500);
    ESP.restart();
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", routerSettings);

  });
  server.on("/reboot", HTTP_GET, []() {
    login();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", "reboot");
    delay(500);
    ESP.restart();
  });
  server.on("/sipSettings", HTTP_GET, []() {
    login();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", sipSettings);
  });
  server.on("/maxCount", HTTP_GET, []() {
    char msg[40];
    login();
    server.sendHeader("Connection", "close");
    sprintf(msg,"max count: %d</br>reset Count %d", ring.max,ring.resetCount);
    server.send(200, "text/html", msg);
  });
  server.on("/restCount", HTTP_GET, []() {
    login();
    server.sendHeader("Connection", "close");
    ring.resetCount=0;
    server.send(200, "text/html", "resetCount");
  });
  server.on("/sipSettings", HTTP_POST, []() {
    login();
    if (server.hasArg("user")&&server.hasArg("password")&&server.hasArg("dail")&&server.hasArg("dailtime")&&server.hasArg("url")&&server.hasArg("port")) {
    File file;
    DynamicJsonDocument doc(512);
    int port;
    sscanf(server.arg("port").c_str(),"%d",&port);
    doc["url"]=server.arg("url");
    doc["port"]=port;
    doc["user"]=server.arg("user");
    doc["password"]=server.arg("password");
    doc["dail"]=server.arg("dail");
    doc["dailtime"]=server.arg("dailtime");
    file = SPIFFS.open("/sip.json", FILE_WRITE);
    serializeJson(doc, file);
    file.close();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", sipSettings);
    delay(500);
    ESP.restart();
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", "not all parameter");

  });
    server.on("/ntpSettings", HTTP_GET, []() {
    login();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", ntpSettings);
  });
  server.on("/ntpSettings", HTTP_POST, []() {
    login();
    if (server.hasArg("ntpServer")&&server.hasArg("gmtOffset")&&server.hasArg("daylightOffset")&&server.hasArg("doNotDistEndHour")&&server.hasArg("doNotDistBeginHour")) {
    File file;
    DynamicJsonDocument doc(512);
    int gmt;
    sscanf(server.arg("gmtOffset").c_str(),"%d",&gmt);
    doc["gmtOffset"]=gmt;
    sscanf(server.arg("daylightOffset").c_str(),"%d",&gmt);
    doc["daylightOffset"]=gmt;
    doc["ntpServer"]=server.arg("ntpServer");
    sscanf(server.arg("doNotDistBeginHour").c_str(),"%d",&gmt);
    doc["doNotDistBeginHour"]=gmt;
    sscanf(server.arg("doNotDistEndHour").c_str(),"%d",&gmt);
    doc["doNotDistEndHour"]=gmt;
    file = SPIFFS.open("/ntp.json", FILE_WRITE);
    serializeJson(doc, file);
    file.close();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", ntpSettings);
    delay(500);
    ESP.restart();
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", "not all parameter");

  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    login();
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
  pinMode(18, INPUT_PULLDOWN);
  pinMode(19, OUTPUT);
//  attachInterrupt(18, ringHigh, RISING);
}

void loop(void) {
  int i;
  for(i=0;i<20&&digitalRead(18);i++){
    delay(6);
    ring.resetCount=i;
  }
  if(i==20){
    sipRinger.ring();
  }
  server.handleClient();
  delay(1);
  sipRinger.update();
}
