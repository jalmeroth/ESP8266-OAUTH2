#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>


/*
 * ============ Variables ============
 */

//#define DEBUG true
#define FILE_NAME "/config.json"    // config on SPIFFS
#define LED_PIN 2                   // built-in LED

// WiFi Setup
String wifi_ssid = "";
String wifi_pass = "";

// OAUTH2 credentials
String client_id = "";
String client_secret = "";

// Tokens
String access_token = "";
String refresh_token = "";

// Poll settings
const int POLL_INTERVAL = 10;
unsigned long POLL_MILLIS = 0;

/*
 * ============ OAUTH2 ============
 */

// SSL Setup
// http://askubuntu.com/questions/156620/how-to-verify-the-ssl-fingerprint-by-command-line-wget-curl/
// echo | openssl s_client -connect www.googleapis.com:443 | openssl x509 -fingerprint -noout
const int httpsPort = 443;
const char *host = "www.googleapis.com";
const char *fingerprint1 = "A6 7A 38 10 2C 29 27 9F F5 91 52 92 49 F2 2A E7 C0 B4 20 A8";
const char *fingerprint2 = "39 F2 B5 65 4F C9 E2 EF 46 F1 8E BC 66 15 E2 72 79 20 94 46";
const char *fingerprint3 = "73 56 DE 7F 17 31 42 9C E4 00 B6 5E E2 8F 59 6E 43 D5 1F 79";

// OAUTH2 Basics
String access_type = "offline";
String redirect_uri = "urn:ietf:wg:oauth:2.0:oob";
String response_type = "code";
String auth_uri = "https://accounts.google.com/o/oauth2/auth";
String info_uri = "/oauth2/v3/tokeninfo";
String token_uri = "/oauth2/v4/token";
String authorization_code = ""; // leave empty

// Send messages only. No read or modify privileges on mailbox.
String gmail_scope = "https://www.googleapis.com/auth/gmail.readonly";
// required to determine which user really authenticated
String scope = "email " + gmail_scope;

static const int ERROR_STATE = -1;
static const int INITIAL_STATE = 0;
static const int AWAIT_CHALLANGE = 1;
static const int EXCHANGING = 2;
static const int INFO = 3;
static const int REFRESHING = 4;
static const int DO_IT = 5;
static const int END_STATE = 6;

// Set global variable attributes.
static int CURRENT_STATE = INITIAL_STATE;

String parseResponse(String response, String key) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(response);
  
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return "";
  } else {
    return root[key];
    String output;
    root.printTo(output);
    Serial.print("JSON: "); Serial.println(output);
    return output;
  }
}

String getRequest(const char* server, String request) {
#ifdef DEBUG
  Serial.print("Function: "); Serial.println("getRequest()");
#endif

  String result = "";
  
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;

#ifdef DEBUG
  Serial.print("Connecting to: "); Serial.println(server);
#endif

  if (!client.connect(server, httpsPort)) {
    Serial.println("connection failed");
    return result;
  }

  if (client.verify(fingerprint1, server) || client.verify(fingerprint2, server) || client.verify(fingerprint3, server)) {
#ifdef DEBUG
    Serial.println("certificate matches");
    Serial.print("get: "); Serial.println(request);
#endif

    client.print(request);

#ifdef DEBUG
    Serial.println("request sent");
    Serial.println("Receiving response");
#endif

    while (client.connected()) {
      if(client.find("HTTP/1.1 ")) {
        String status_code = client.readStringUntil('\r');
      #ifdef DEBUG
        Serial.print("Status code: "); Serial.println(status_code);
      #endif
        if(status_code == "401 Unauthorized") {
          // lets see if access_token expired
          CURRENT_STATE = INFO;
        } else if(status_code != "200 OK") {
          Serial.println("There was an error");
          break;
        }
      }
      if(client.find("\r\n\r\n")) {
        String line = client.readStringUntil('\r');
      #ifdef DEBUG
        Serial.println("Data:");
        Serial.println(line);
      #endif
        result += line;
      }
    }
  #ifdef DEBUG
    Serial.println("closing connection");
  #endif
    return result;
  } else {
    Serial.println("certificate doesn't match");
  }
  return result;
}


String postRequest(const char* server, String header, String data) {
#ifdef DEBUG
  Serial.print("Function: "); Serial.println("postRequest()");
#endif

  String result = "";

  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
#ifdef DEBUG
  Serial.print("Connecting to: "); Serial.println(server);
#endif

  if (!client.connect(server, httpsPort)) {
    Serial.println("connection failed");
    return result;
  }

  if (client.verify(fingerprint1, server) || client.verify(fingerprint2, server) || client.verify(fingerprint3, server)) {
  #ifdef DEBUG
    Serial.println("certificate matches");
    Serial.print("post: "); Serial.println(header + data);
  #endif

    client.print(header + data);

  #ifdef DEBUG
    Serial.println("request sent");
    Serial.println("Receiving response");
  #endif

    while (client.connected()) {
      if(client.find("HTTP/1.1 ")) {
        String status_code = client.readStringUntil('\r');
      #ifdef DEBUG
        Serial.print("Status code: "); Serial.println(status_code);
      #endif
        if(status_code != "200 OK") {
          Serial.println("There was an error");
          break;
        }
      }
      if(client.find("\r\n\r\n")) {
        String line = client.readStringUntil('\r');
      #ifdef DEBUG
        Serial.println("Data:");
        Serial.println(line);
      #endif
        result += line;
      }
    }

  #ifdef DEBUG
    Serial.println("closing connection");
  #endif
    return result;
  } else {
    Serial.println("certificate doesn't match");
  }
  return result;
}

// create URL
void authorize() {
#ifdef DEBUG
  Serial.print("Function: "); Serial.println("authorize()");
#endif
  if (refresh_token == "") {
    String URL = auth_uri + "?";
    URL += "scope=" + urlencode(scope);
    URL += "&redirect_uri=" + urlencode(redirect_uri);
    URL += "&response_type=" + urlencode(response_type);
    URL += "&client_id=" + urlencode(client_id);
    URL += "&access_type=" + urlencode(access_type);
    Serial.println("Goto URL: ");
    Serial.println(URL); Serial.println();
    Serial.print("Enter code: ");
    CURRENT_STATE = AWAIT_CHALLANGE;
  } else {
    CURRENT_STATE = INFO;
  }
}

bool exchange() {
#ifdef DEBUG
  Serial.print("Function: "); Serial.println("exchange()");
#endif

  if (authorization_code != "") {

    String postData = "";
    postData += "code=" + authorization_code;
    postData += "&client_id=" + client_id;
    postData += "&client_secret=" + client_secret;
    postData += "&redirect_uri=" + redirect_uri;
    postData += "&grant_type=" + String("authorization_code");

    String postHeader = "";
    postHeader += ("POST " + token_uri + " HTTP/1.1\r\n");
    postHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    postHeader += ("Connection: close\r\n");
    postHeader += ("Content-Type: application/x-www-form-urlencoded\r\n");
    postHeader += ("Content-Length: ");
    postHeader += (postData.length());
    postHeader += ("\r\n\r\n");

    String result = postRequest(host, postHeader, postData);
    
    CURRENT_STATE = END_STATE;
    return true;
  } else {
    return false;
  }
}

void refresh() {
#ifdef DEBUG
  Serial.print("Function: "); Serial.println("refresh()");
#endif
  if (refresh_token != "") {

    String postData = "";
    postData += "refresh_token=" + refresh_token;
    postData += "&client_id=" + client_id;
    postData += "&client_secret=" + client_secret;
    postData += "&grant_type=" + String("refresh_token");

    String postHeader = "";
    postHeader += ("POST " + token_uri + " HTTP/1.1\r\n");
    postHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    postHeader += ("Connection: close\r\n");
    postHeader += ("Content-Type: application/x-www-form-urlencoded\r\n");
    postHeader += ("Content-Length: ");
    postHeader += (postData.length());
    postHeader += ("\r\n\r\n");

    String result = postRequest(host, postHeader, postData);
    access_token = parseResponse(result, String("access_token"));
  #ifdef DEBUG
    Serial.print("access_token: "); Serial.println(access_token);
  #endif
    if(access_token != "") {
      CURRENT_STATE = DO_IT;
    } else {
      CURRENT_STATE = END_STATE;
    }
  }
}

bool info() {
#ifdef DEBUG
  Serial.print("Function: "); Serial.println("info()");
#endif
  if (access_token != "") {

    String reqHeader = "";
    reqHeader += ("GET " + info_uri + "?access_token=" + access_token + " HTTP/1.1\r\n");
    reqHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    reqHeader += ("Connection: close\r\n");
    reqHeader += ("\r\n\r\n");
    String result = getRequest(host, reqHeader);
    // need to check for valid token here
    // Look for expires_in
    String expires_in = parseResponse(result, String("expires_in"));
    Serial.print("expires_in: "); Serial.println(expires_in); 
    if(expires_in.toInt() >= 300) {       // got 5 min.?
      CURRENT_STATE = DO_IT;
    } else {
      CURRENT_STATE = REFRESHING;
    }
  } else {
    CURRENT_STATE = REFRESHING;
    return false;
  }
  return true;
}

/*
 * ============ Filesystem ============
 */

void cleanFS() {
  //clean FS, for testing
  Serial.println("formatting file system");
  SPIFFS.format();
  Serial.println("done.");
}

bool writeConfig() {
  bool result = false;
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");

    Serial.println("open config file for writing");
    File configFile = SPIFFS.open(FILE_NAME, "w+");
    
    if (configFile) {
      Serial.println("opened config file");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& jsonConfig = jsonBuffer.createObject();
      jsonConfig["wifi_ssid"] = wifi_ssid;
      jsonConfig["wifi_pass"] = wifi_pass;
      jsonConfig["client_id"] = client_id;
      jsonConfig["client_secret"] = client_secret;
      jsonConfig["refresh_token"] = refresh_token;
      jsonConfig.prettyPrintTo(Serial); Serial.println();
      jsonConfig.printTo(configFile);
      result = true;
    }

    Serial.println("closing config file");
    configFile.close();

  } else {
    Serial.println("failed to mount FS");
  }
  Serial.println("unmounting file system");
  SPIFFS.end();
  return result;
}

bool readConfig() {
  
  bool result = false;

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists(FILE_NAME)) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open(FILE_NAME, "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& jsonConfig = jsonBuffer.parseObject(buf.get());

        if (jsonConfig.success()) {
          Serial.println("config parsed successfully");
          jsonConfig.prettyPrintTo(Serial); Serial.println();

          wifi_ssid = jsonConfig["wifi_ssid"].as<String>();
          wifi_pass = jsonConfig["wifi_pass"].as<String>();
          client_id = jsonConfig["client_id"].as<String>();
          client_secret = jsonConfig["client_secret"].as<String>();
          refresh_token = jsonConfig["refresh_token"].as<String>();
          
          result = true;
        } else {
          Serial.println("failed to load json config");
        }
      }
      Serial.println("closing config file");
      configFile.close();
    } else {
      Serial.println("No config file found.");
    }
  } else {
    Serial.println("failed to mount FS");
  }
  Serial.println("unmounting file system");
  SPIFFS.end();
  return result;
}

/*
 * ============ Gmail Noot ============
 */

bool getUnreadCount() {
//  GET https://www.googleapis.com/gmail/v1/users/userId/threads

  if(millis() - POLL_MILLIS >= POLL_INTERVAL * 1000UL || POLL_MILLIS == 0) {
    String reqHeader = "";
    reqHeader += ("GET /gmail/v1/users/me/threads?");
    reqHeader += ("access_token=" + access_token);
    reqHeader += ("&labelIds=INBOX");
    reqHeader += ("&q=is:unread");
    reqHeader += ("&fields=resultSizeEstimate");
    reqHeader += (" HTTP/1.1\r\n");
    reqHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    reqHeader += ("Connection: close\r\n");
    reqHeader += ("\r\n\r\n");
  
    String result = getRequest(host, reqHeader);
    String unread = parseResponse(result, String("resultSizeEstimate"));

  #ifdef DEBUG
    Serial.print("unread: "); Serial.println(unread);
  #endif

    if(unread.toInt() > 0) {      // you got mail.
      Serial.print("You got "); Serial.print(unread); Serial.println(" unread mail thread(s) in your inbox.");
      digitalWrite(LED_PIN, LOW); // noot noot
    } else {
      digitalWrite(LED_PIN, HIGH);
    }
    
    POLL_MILLIS = millis();
    return true;
  } else {
    return false;
  }
}

/*
 * ============ General ============
 */

String urlencode(String str)
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      code2 = '\0';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
      //encodedString+=code2;
    }
    yield();
  }
  return encodedString;
}

// enable Serial communication
String serialComm() {
  String result = "";
  while (Serial.available()) {
    String inputString = Serial.readString();
    inputString.trim();
    if(inputString != ""){
      if (inputString == "clean") {
        cleanFS();
      } else if (inputString == "read") {
        readConfig();
      } else if (inputString == "write") {
        writeConfig();
      } else {
        result = inputString;
      }
    }
  }
  return result;
}

void setup() {
  Serial.begin(115200); Serial.println();
  pinMode(LED_PIN, OUTPUT);
  // read config from file system  
  readConfig();
  if(wifi_ssid != "") {
    Serial.print("Connecting to: "); Serial.println(wifi_ssid);
    char ssidBuf[wifi_ssid.length()+1];
    wifi_ssid.toCharArray(ssidBuf, wifi_ssid.length()+1);
    char passBuf[wifi_pass.length()+1];
    wifi_pass.toCharArray(passBuf, wifi_pass.length()+1);
    WiFi.begin(ssidBuf, passBuf);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    Serial.print("WiFi connected. "); Serial.print("IP address: "); Serial.println(WiFi.localIP());
    Serial.println();
  }
}

void loop() {
  serialComm();
  switch (CURRENT_STATE) {
    case INITIAL_STATE:
      authorize();
      break;
    case AWAIT_CHALLANGE:
      authorization_code = serialComm();
      if (authorization_code != "") {
        Serial.println(authorization_code);
        CURRENT_STATE = EXCHANGING;
      }
      break;
    case EXCHANGING:
      exchange();
      break;
    case INFO:
      info();
      break;
    case REFRESHING:
      refresh();
      break;
    case DO_IT:
      getUnreadCount();
      break;
    case END_STATE:
      break;
    default:
      Serial.println("ERROR");
      break;
  }
}
