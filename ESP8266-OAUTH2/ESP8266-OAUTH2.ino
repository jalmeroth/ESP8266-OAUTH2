#include <base64.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

/*
 * =====================================
 * Start editing your configuration here
 */

//#define DEBUG true

// WiFi Setup
const char* ssid = "";
const char* pass = "";

// OAUTH2 Client credentials
String client_id = "";
String client_secret = "";

// Tokens
String authorization_code = "";
String access_token = "";
String refresh_token = "";

// Email Setup
String email_from = "";
String email_to = "";

// Sheets Setup
String sheet_id = "";
String sheet_range = "Sheet1!A:B";

/* ====================================
 * Stop editing your configuration here
 */

// SSL Setup
// http://askubuntu.com/questions/156620/how-to-verify-the-ssl-fingerprint-by-command-line-wget-curl/
// echo | openssl s_client -connect www.googleapis.com:443 | openssl x509 -fingerprint -noout
const char* host = "www.googleapis.com";
const char* sheetsHost = "sheets.googleapis.com";
const int httpsPort = 443;
const char* fingerprint = "A6 7A 38 10 2C 29 27 9F F5 91 52 92 49 F2 2A E7 C0 B4 20 A8";

// OAUTH2 Basics
String access_type = "offline";
String redirect_uri = "urn:ietf:wg:oauth:2.0:oob";
String response_type = "code";
String auth_uri = "https://accounts.google.com/o/oauth2/auth";
String info_uri = "/oauth2/v3/tokeninfo";
String token_uri = "/oauth2/v4/token";

// Send messages only. No read or modify privileges on mailbox.
String gmail_scope = "https://www.googleapis.com/auth/gmail.send";
// Allows read/write access to the user's sheets and their properties.
String sheet_scope = "https://www.googleapis.com/auth/spreadsheets";
// required to determine which user really authenticated
String scope = "email " + gmail_scope + " " + sheet_scope;

static const int ERROR_STATE = -1;
static const int INITIAL_STATE = 0;
static const int AWAIT_CHALLANGE = 1;
static const int EXCHANGING = 2;
static const int INFO = 3;
static const int REFRESHING = 4;
static const int END_STATE = 5;

// Set global variable attributes.
static int CURRENT_STATE = INITIAL_STATE;

String postRequest(const char* server, String header, String data) {
#ifdef DEBUG
  Serial.print("Function: "); Serial.println("postRequest()");
#endif

  String result = "";

  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  Serial.print("Connecting to: "); Serial.println(server);

  if (!client.connect(server, httpsPort)) {
    Serial.println("connection failed");
    return result;
  }

  if (client.verify(fingerprint, server)) {
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
      String line = client.readStringUntil('\n');
      Serial.println(line);
      if (line == "") {
        Serial.println("headers received");
        break;
      }
      result += line;
    }

    Serial.println("closing connection");
    return result;
  } else {
    Serial.println("certificate doesn't match");
  }
  return result;
}

bool appendToSheet() {
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  Serial.print("Connecting to: "); Serial.println(sheetsHost);

  if (!client.connect(sheetsHost, httpsPort)) {
    Serial.println("connection failed");
    return false;
  }

  if (client.verify(fingerprint, sheetsHost)) {
    Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn't match");
  }

  String postData = "";
  postData += "{\n  \"values\": [[\"brasel\",\"fink\"]]\n}";

  String postHeader = "";
  postHeader += ("POST /v4/spreadsheets/" + sheet_id + "/values/" + sheet_range + ":append" + "?valueInputOption=raw" + " HTTP/1.1\r\n");
  postHeader += ("Host: " + String(sheetsHost) + ":" + String(httpsPort) + "\r\n");
  postHeader += ("Connection: close\r\n");
  postHeader += ("Authorization: Bearer " + access_token + "\r\n");
  postHeader += ("Content-Type: application/json; charset=UTF-8\r\n");
  postHeader += ("Content-Length: ");
  postHeader += (postData.length());
  postHeader += ("\r\n\r\n");
  Serial.print("post: "); Serial.println(postHeader + postData);
  client.print(postHeader + postData);
  Serial.println("request sent");
  Serial.println("Receiving response");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    //line.trim();
    if (line == "") {
      Serial.println("headers received");
      break;
    }
  }

  Serial.println("closing connection");
  return true;
}

bool getSheetContent() {
  // GET https://sheets.googleapis.com/v4/spreadsheets/spreadsheetId/values/Sheet1!A1:D5
  if (sheet_id != "" && sheet_range != "") {
    // Use WiFiClientSecure class to create TLS connection
    WiFiClientSecure client;
    Serial.print("Connecting to: "); Serial.println(sheetsHost);

    if (!client.connect(sheetsHost, httpsPort)) {
      Serial.println("connection failed");
      return false;
    }

    if (client.verify(fingerprint, sheetsHost)) {
      Serial.println("certificate matches");
    } else {
      Serial.println("certificate doesn't match");
    }

    String reqHeader = "";
    reqHeader += ("GET /v4/spreadsheets/" + sheet_id + "/values/" + sheet_range + "?access_token=" + access_token + " HTTP/1.1\r\n");
    reqHeader += ("Host: " + String(sheetsHost) + ":" + String(httpsPort) + "\r\n");
    reqHeader += ("Connection: close\r\n");
    reqHeader += ("\r\n\r\n");
    Serial.print("Req: "); Serial.println(reqHeader);
    client.print(reqHeader);

    Serial.println("request sent");
    Serial.println("Receiving response");

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
      //line.trim();
      if (line == "") {
        Serial.println("headers received");
        break;
      }
    }

    Serial.println("closing connection");
    return true;
  } else {
    return false;
  }
}

bool sendEmail(String body) {
  if (body != "") {
    String userId = "me";
    String headers = "";
    headers += "From: " + email_from + "\n";
    headers += "To: " + email_from + "\n";
    headers += "Subject: I love you!\n\n";

    String email = base64::encode(headers + body);
    email.replace("\n", "");
    //Serial.println(email);

    // Use WiFiClientSecure class to create TLS connection
    WiFiClientSecure client;
    Serial.print("Connecting to: "); Serial.println(host);

    if (!client.connect(host, httpsPort)) {
      Serial.println("connection failed");
      return false;
    }

    if (client.verify(fingerprint, host)) {
      Serial.println("certificate matches");
    } else {
      Serial.println("certificate doesn't match");
    }

    String postData = "";
    postData += "{\n  \"raw\": \"" + email + "\"\n}";

    String postHeader = "";
    postHeader += ("POST /gmail/v1/users/" + userId + "/messages/send" + " HTTP/1.1\r\n");
    postHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    postHeader += ("Connection: close\r\n");
    postHeader += ("Authorization: Bearer " + access_token + "\r\n");
    postHeader += ("Content-Type: application/json; charset=UTF-8\r\n");
    postHeader += ("Content-Length: ");
    postHeader += (postData.length());
    postHeader += ("\r\n\r\n");
    Serial.print("post: "); Serial.println(postHeader + postData);
    client.print(postHeader + postData);

    Serial.println("request sent");
    Serial.println("Receiving response");

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
      //line.trim();
      if (line == "") {
        Serial.println("headers received");
        break;
      }
    }

    Serial.println("closing connection");
    return true;
  } else {
    return false;
  }
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
  Serial.println("Trying to exchange...");
  if (authorization_code != "") {
    //    // Use WiFiClientSecure class to create TLS connection
    //    WiFiClientSecure client;
    //    Serial.print("Connecting to: "); Serial.println(host);
    //
    //    if (!client.connect(host, httpsPort)) {
    //      Serial.println("connection failed");
    //      return false;
    //    }
    //
    //    if (client.verify(fingerprint, host)) {
    //      Serial.println("certificate matches");
    //    } else {
    //      Serial.println("certificate doesn't match");
    //    }

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

    //    client.print(postHeader + postData);
    //
    //    Serial.println("request sent");
    //    Serial.println("Receiving response");
    //
    //    while (client.connected()) {
    //      String line = client.readStringUntil('\n');
    //      Serial.println(line);
    //      //line.trim();
    //      if (line == "") {
    //        Serial.println("headers received");
    //        break;
    //      }
    //    }
    //
    //    Serial.println("closing connection");

    CURRENT_STATE = END_STATE;
    return true;
  } else {
    return false;
  }
}

bool refresh() {
  if (refresh_token != "") {
    // Use WiFiClientSecure class to create TLS connection
    WiFiClientSecure client;
    Serial.print("Connecting to: "); Serial.println(host);

    if (!client.connect(host, httpsPort)) {
      Serial.println("connection failed");
      return false;
    }

    if (client.verify(fingerprint, host)) {
      Serial.println("certificate matches");
    } else {
      Serial.println("certificate doesn't match");
    }

    String postData = "";
    postData += "refresh_token=" + refresh_token;
    postData += "&client_id=" + client_id;
    postData += "&client_secret=" + client_secret;
    postData += "&grant_type=" + String("refresh_token");
    //Serial.print("postData: "); Serial.println(postData);

    String postHeader = "";
    postHeader += ("POST " + token_uri + " HTTP/1.1\r\n");
    postHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    postHeader += ("Connection: close\r\n");
    postHeader += ("Content-Type: application/x-www-form-urlencoded\r\n");
    postHeader += ("Content-Length: ");
    postHeader += (postData.length());
    postHeader += ("\r\n\r\n");
    Serial.print("post: "); Serial.println(postHeader + postData);
    client.print(postHeader + postData);

    Serial.println("request sent");
    Serial.println("Receiving response");

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
      //line.trim();
      if (line == "") {
        Serial.println("headers received");
        break;
      }
    }

    Serial.println("closing connection");

    CURRENT_STATE = END_STATE;
  } else {
    return false;
  }
  return true;
}

bool info() {
  if (access_token != "") {
    // Use WiFiClientSecure class to create TLS connection
    WiFiClientSecure client;
    Serial.print("Connecting to: "); Serial.println(host);

    if (!client.connect(host, httpsPort)) {
      Serial.println("connection failed");
      return false;
    }

    if (client.verify(fingerprint, host)) {
      Serial.println("certificate matches");
    } else {
      Serial.println("certificate doesn't match");
    }

    String reqHeader = "";
    reqHeader += ("GET " + info_uri + "?access_token=" + access_token + " HTTP/1.1\r\n");
    reqHeader += ("Host: " + String(host) + ":" + String(httpsPort) + "\r\n");
    reqHeader += ("Connection: close\r\n");
    reqHeader += ("\r\n\r\n");
    Serial.print("Req: "); Serial.println(reqHeader);
    client.print(reqHeader);

    Serial.println("request sent");
    Serial.println("Receiving response");

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
      //line.trim();
      if (line == "") {
        Serial.println("headers received");
        break;
      }
    }

    Serial.println("closing connection");

    CURRENT_STATE = END_STATE;
  } else {
    return false;
  }
  return true;
}

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
    if (inputString != "") {
      return inputString;
    }
  }
  return result;
}

void setup() {
  Serial.begin(115200); Serial.println();

  Serial.print("Connecting to: "); Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("WiFi connected. "); Serial.print("IP address: "); Serial.println(WiFi.localIP());
  Serial.println();
}

void loop() {
  if (access_token != "") {
    sendEmail("<3");
    appendToSheet();
    getSheetContent();
    CURRENT_STATE = INFO;
  }


  // put your main code here, to run repeatedly:
  switch (CURRENT_STATE) {

    case INITIAL_STATE:
      authorize();
      break;
    case AWAIT_CHALLANGE:
      authorization_code = serialComm();
      if (authorization_code != "") {
        Serial.println("********");
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
    case END_STATE:
      break;
    default:
      //executeErrorState();
      Serial.println("ERROR");
      break;
  }
}
