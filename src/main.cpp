#include <Arduino.h>
#include <FS.h>
#include <WiFiManager.h>

#ifdef ESP32
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#endif
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif
char username[20];
char password[200];
char service[10];
#include <ArduinoJson.h>
#define FORMAT_SPIFFS_IF_FAILED true
WiFiClient wifi;
HTTPClient http;
void setup()
{
  Serial.begin(9600);
  Serial.println();
  // put your setup code here, to run once:

  Serial.println("mounting FS...");
  if (SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json"))
    {
      // file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if (!deserializeError)
        {
          Serial.println("\nparsed json");
          strcpy(username, json["username"]);
          strcpy(password, json["password"]);
          strcpy(service, json["service"]);
          Serial.println(username);
          Serial.println(service);
        }
        configFile.close();
      }
      else
      {
        Serial.println("failed to load json config");
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }
  WiFiManagerParameter usernameField("username", "学号", "", 18, "placeholder=\"填入学号\"");
  WiFiManagerParameter passwordField("password", "密码", "", 198, "placeholder=\"输入UPC Net登录密码\"");
  const char *custom_radio_str = "<br/><label for='service'>服务类型</label><select  name='service'  id='day' onchange='document.getElementById(\"key_custom\").value = this.value'><option value='cmcc'>中国移动</option>  <option value='ctcc'>中国电信</option>  <option value='unicom'>中国联通</option>  <option value='default'>校园网</option><option value='local'>校园内网</option></select>  <script>    document.getElementById('day').value = '';    document.querySelector('[for=\"key_custom\"]').hidden = true;    document.getElementById('key_custom').hidden = true;  </script>";
  WiFiManagerParameter serviceField(custom_radio_str);
  WiFiManager wifiManager;
  WiFiManagerParameter custom_hidden("key_custom", "Will be hidden", "cmcc", 12);

  wifiManager.addParameter(&usernameField);
  wifiManager.addParameter(&passwordField);
  wifiManager.addParameter(&custom_hidden);

  wifiManager.addParameter(&serviceField);

  if (!wifiManager.autoConnect("AutoConnectAP", "12345678"))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    // reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }
  Serial.println("connected...yeey :)");
  const char *usernameVal = usernameField.getValue();
  const char *passwordVal = passwordField.getValue();
  const char *serviceVal = custom_hidden.getValue();
  Serial.println(usernameVal);
  if (usernameVal[0] != '\0')
  {
    Serial.println("username is not empty");
    strcpy(username, usernameVal);
    strcpy(password, passwordVal);
    strcpy(service, serviceVal);

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile)
    {
      Serial.println("failed to open config file for writing");
    }

    DynamicJsonDocument json(1024);
    json["username"] = username;
    json["password"] = password;
    json["service"] = service;
    serializeJson(json, Serial);
    serializeJson(json, configFile);
    configFile.close();
  }
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());
}
//Come from ArduinoHTTPClient
String encode(const String str)
{

  String encoded;
  int length = str.length();
  encoded.reserve(length);

  for (int i = 0; i < length; i++)
  {
    char c = str[i];
    const char HEX_DIGIT_MAPPER[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    if (isAlphaNumeric(c) || (c == '-') || (c == '.') || (c == '_') || (c == '~'))
    {
      encoded += c;
    }
    else
    {
      char s[4];
      s[0] = '%';
      s[1] = HEX_DIGIT_MAPPER[(c >> 4) & 0xf];
      s[2] = HEX_DIGIT_MAPPER[(c & 0x0f)];
      s[3] = 0;
      encoded += s;
    }
  }

  return encoded;
}
void loop()
{
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi..");
    Serial.print(".");
    WiFi.disconnect();
    WiFi.reconnect();
  }
  http.begin(wifi, "http://connect.rom.miui.com/generate_204");
  http.setReuse(false);
  int httpCode = http.GET();
  Serial.println(httpCode);

  if ((httpCode > 0) && (httpCode != 204))
  {
    Serial.println(httpCode);
    delay(500);
    String url = http.getLocation();
    http.getString();
    http.end();
    Serial.println(url);
    int hostIndex = url.indexOf("//");
    int hostEndIndex = url.indexOf("/", hostIndex + 2);
    String host = url.substring(hostIndex + 2, hostEndIndex);
    Serial.println(host);
    int paramIndex = url.indexOf("/eportal/index.jsp?");
    if (paramIndex > 0)
    {
      url = url.substring(paramIndex + 1);
    }
    url = encode(url);
    Serial.println(url);
    char params[400];
    sprintf(params, "userId=%s&password=%s&service=%s&queryString=%s&operatorPwd=&operatorUserId=&validcode=&passwordEncrypt=false", username, password, service, url.c_str());
    Serial.println(params);
    http.begin(wifi, "http://" + host + "/eportal/InterFace.do?method=login");
    http.setReuse(false);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");

    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/80.0.3987.122 Safari/537.36");
    Serial.println(22);
    httpCode = http.POST(params);
    Serial.println(12);
    Serial.println(httpCode);
    Serial.println(http.getString());
    http.end();
  }
  delay(30000);
}