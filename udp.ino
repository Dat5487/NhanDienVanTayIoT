#include <SPI.h>
#include <FPM.h>
#include <HardwareSerial.h>
#include <WiFiClientSecure.h>
#include <LiquidCrystal_I2C.h>
#define BTN_PIN1 15
#define BTN_PIN2 4
#define LED_PIN 32
#define BUZZER_PIN 33
#define TRANSFER_SZ 128
#define HEADER_BUF_SZ   256
#include <HTTPClient.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
LiquidCrystal_I2C LCD = LiquidCrystal_I2C(0x27, 20, 4);
WiFiClientSecure client;

HardwareSerial fserial(1);
FPM finger(&fserial);
FPM_System_Params params;

uint8_t buffer[TRANSFER_SZ];
char headerBuf[HEADER_BUF_SZ];
char controlName[] = "fingerprint";
String fileName = "temp";
int status = WL_IDLE_STATUS;
int selectedMode;
const char* PARAM_INPUT_1 = "input1";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
  <meta charset="utf-8">
    <title>Input Form</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body {
        font-family: Arial, sans-serif;
        background-color: #f2f2f2;
        margin: 0;
        padding: 20px;
      }
      h1 {
        text-align: center;
        color: #333;
      }
      form {
        max-width: 400px;
        margin: 0 auto;
        background-color: #fff;
        padding: 20px;
        border-radius: 5px;
        box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
      }
      label {
        display: block;
        margin-bottom: 10px;
        color: #666;
      }
      .alert {
        padding: 20px;
        background-color: #f44336;
        color: white;
        opacity: 1;
        transition: opacity 0.6s;
        margin-bottom: 15px;
      }
      .alert.success {background-color: #04AA6D;}
      .closebtn {
        margin-left: 15px;
        color: white;
        font-weight: bold;
        float: right;
        font-size: 22px;
        line-height: 20px;
        cursor: pointer;
        transition: 0.3s;
      }
      .closebtn:hover {
        color: black;
      }
      
      input[type="text"],
      input[type="email"],
      textarea {
        width: 100%;
        padding: 10px;
        border: 1px solid #ccc;
        border-radius: 4px;
        box-sizing: border-box;
        font-size: 16px;
        margin-bottom: 15px;
      }
      
      input[type="submit"] {
        background-color: #4CAF50;
        text-decoration: none;
        color: white;
        padding: 12px 20px;
        border: none;
        border-radius: 4px;
        cursor: pointer;
        font-size: 16px;
      }
      input[type="submit"]:hover {
        background-color: #45a049;
      }
    </style>
  </head>
  <body>
    <! -- Alert -->
    <h1>Cập nhật vân tay cho sinh viên</h1>
    <form action="/get">
      <label for="name">Mã sinh viên:</label>
      <input type="text" id="name" name="input1" maxlength="9" required>
      <input type="submit" value="Xác nhận">
    </form>
  </body>
  <script>
    var close = document.getElementsByClassName("closebtn");
    var i;
    
    for (i = 0; i < close.length; i++) {
      close[i].onclick = function(){
        var div = this.parentElement;
        div.style.opacity = "0";
        setTimeout(function(){ div.style.display = "none"; }, 600);
      }
    }
    </script>
</html>)rawliteral";



void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void setup(void)
{
  
    LCD.init();
    LCD.backlight();
    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_PIN1, INPUT_PULLUP);
    pinMode(BTN_PIN2, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);
    Serial.begin(57600);
    fserial.begin(57600, SERIAL_8N1, 16, 17);
    if (finger.begin()) {
        finger.readParams(&params);
        params.capacity;
        FPM::packet_lengths[params.packet_len];
    }
    else {
        Serial.println("Khong tim thay may quet van tay :(");
        LCD.setCursor(0, 1);
        LCD.print("Khong tim thay");
        LCD.setCursor(0, 2);
        LCD.print("May quet van tay");
        while (1) yield();
    }

    LCD.setCursor(0, 1);
    LCD.print("Dang ket noi den");
    LCD.setCursor(0, 2);
    LCD.print("WiFi");
    while (WiFi.status() != WL_CONNECTED) { //Check for the connection
      WiFi.begin("Live 4_3875","12345aaa"); 
      delay(3000);
    }
    client.setInsecure();
    client.setNoDelay(1);
    
    LCD.clear();
    LCD.setCursor(0, 0);
    LCD.print("--- Chon che do ---");
    LCD.setCursor(0, 1);
    LCD.print("Nhan 1 de Cap nhat");
    LCD.setCursor(0, 2);
    LCD.print("Nhan 2 de Nhan dien");
    while (digitalRead(BTN_PIN1) == HIGH && digitalRead(BTN_PIN2) == HIGH) {
    // Do nothing
    }

    if (digitalRead(BTN_PIN1) == LOW){
      selectedMode=1;
    }
    if(digitalRead(BTN_PIN2) == LOW){
      selectedMode=2;
    }
    LCD.clear();
}

void loop() {
    fileName = "temp";
    serverSet();
    post_image();
    delay(1000);
}

String alertHtml = "<div class=\"alert success\"><span class=\"closebtn\">&times;</span>Nhập mã cho sinh viên : {{studentId}} thành công</div>";
String noAlert = "<! -- Alert -->";

void serverSet(void){

    IPAddress ip = WiFi.localIP();
    if(selectedMode==1){
      LCD.setCursor(0, 0);
      LCD.print("Vao dia chi:");
      LCD.setCursor(0, 1);
      LCD.print(ip);
      LCD.setCursor(0, 2);
      LCD.print("De nhap ma sinh vien");
        // Send web page with input fields to client
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
      });
    
      // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
      server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String inputMessage;
        String inputParam;
        // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
        if (request->hasParam(PARAM_INPUT_1)) {
          inputMessage = request->getParam(PARAM_INPUT_1)->value();
          inputParam = PARAM_INPUT_1;
        }
        else {
          inputMessage = "No message sent";
          inputParam = "none";
        }
        setFileName(inputMessage);
        String index_Html = String(index_html);
        index_Html.replace(noAlert, alertHtml);
        index_Html.replace("{{studentId}}", inputMessage);
        request->send(200, "text/html", index_Html);
      });
      server.onNotFound(notFound);
      server.begin();
      while(fileName=="temp"){
        
      }
  }
}
void setFileName(String newFileName){
  fileName = newFileName;
}
void spinner(void) {
  static int8_t counter = 0;
  const char* glyphs = "\xa1\xa5\xdb";
  LCD.setCursor(19, 3);
  LCD.print(glyphs[counter++]);
  if (counter == strlen(glyphs)) {
    counter = 0;
  }
}

void responseLCD(String response){
    if (response.length() > 20) {
    // Split the text into multiple lines
    int numLines = response.length() / 20 + 1;
    for (int i = 0; i < numLines; i++) {
      String line = response.substring(i * 20, (i + 1) * 20);
      // Display the line on the LCD
      LCD.setCursor(0, i);
      LCD.print(line);
    }
  } else {
    LCD.setCursor(0, 0);
    LCD.print(response);
  }
}

bool set_packet_len_128(void) {
  uint8_t param = FPM_SETPARAM_PACKET_LEN; // Example
  uint8_t value = FPM_PLEN_128;
  int16_t p = finger.setParam(param, value);
  switch (p) {
    case FPM_OK:
      break;
    case FPM_PACKETRECIEVEERR:
      Serial.println("Comms error");
      break;
    case FPM_INVALIDREG:
      Serial.println("Invalid settings!");
      break;
    default:
      Serial.println("Unknown error");
  }

  return (p == FPM_OK);
}

uint16_t connectAndAssembleMultipartHeaders(void)
{
    uint16_t bodyLen = 0;
    IPAddress server(192,168,194,1);
    const char *server2 = "goodsageboard53.conveyor.cloud";
    if (client.connect(server,5001)) 
    {
      Serial.print("Đã kết nối được tới server");
      client.println("POST /SinhViens/fingerprint HTTP/1.1");
      client.println("Host: 192.168.194.1");
      client.println("Content-Type: multipart/form-data; boundary=X-ARDUINO_MULTIPART");
      client.println("Connection: close");
      
      /* Copy boundary and content disposition for the part */
      uint16_t availSpace = HEADER_BUF_SZ;
      int wouldCopy = 0;
      wouldCopy = snprintf(headerBuf + wouldCopy, availSpace, "--X-ARDUINO_MULTIPART\r\nContent-Disposition: form-data;"
                                                              " name=\"%s\"; filename=\"%s\"\r\n", controlName, fileName);
      
      if (wouldCopy < 0 || wouldCopy >= availSpace)
      {
        Serial.println("Header buffer too small. Stopping.");
        return 0;
      }
      
      bodyLen += wouldCopy;
      availSpace -= wouldCopy;

      /* Copy content type for the part */
      wouldCopy = snprintf(headerBuf + wouldCopy, availSpace, "Content-Type: application/octet-stream\r\n\r\n");
      
      if (wouldCopy < 0 || wouldCopy >= availSpace)
      {
        Serial.println("Header buffer too small (2). Stopping.");
        return 0;
      }
      
      bodyLen += wouldCopy;
      availSpace -= wouldCopy;
      
      /* Add the image size itself */
      uint16_t IMAGE_SZ = 36864;
      bodyLen += IMAGE_SZ;

      /* Add the length of the final boundary */
      bodyLen += strlen("\r\n--X-ARDUINO_MULTIPART--\r\n\r\n");
      
      /* Send content length finally -- a sum of all bytes sent from the beginning of first boundary
        * till the last byte of the last boundary */
      client.print("Content-Length: "); client.print(bodyLen); client.print("\r\n\r\n");
      
      /* Then send the header for the first (and only) part of this multipart request */
      client.print(headerBuf);
      return bodyLen;
    }
    else{
      Serial.println("Connection error");
    }
    
    return 0;
}


void post_image(void) {
  if (!set_packet_len_128()) {
    return;
  }
  delay(100);
  if(selectedMode == 1){
    LCD.setCursor(0, 0);
    LCD.print("Nhap ma sinh vien");
    LCD.setCursor(0, 1);
    Serial.println("Đã nhập sinh viên: " + fileName);
  }
  
  LCD.clear();
  int16_t p = -1;
  LCD.setCursor(0, 0);
  LCD.print("--Diem danh van tay-");
  LCD.setCursor(0, 1);
  LCD.print("   Dua van tay len  ");
  LCD.setCursor(0, 2);
  LCD.print("      may quet");
  while (p != FPM_OK) {
    p = finger.getImage();
    delay(500);
    switch (p) {
      case FPM_OK:
        p = finger.getImage();
        break;
      case FPM_NOFINGER:
        break;
      case FPM_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FPM_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
    yield();
  }
  
  if (connectAndAssembleMultipartHeaders())
  {
    /* The client is ready to send the data. Now request the image from the sensor */
    p = finger.downImage();
    switch (p) {
    case FPM_OK:
      LCD.clear();
      LCD.setCursor(0, 1);
      LCD.print(" Dang xu ly van tay");
      break;
    case FPM_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return;
    case FPM_UPLOADFAIL:
      Serial.println("Cannot transfer the image");
      return;
    }

    bool read_finished;
    uint16_t readlen = TRANSFER_SZ;
    uint16_t count = 0;
    
    while(true)
    {
      bool ret = finger.readRaw(FPM_OUTPUT_TO_BUFFER, buffer, &read_finished, &readlen);
      if (ret) {
        count++;
        client.write(buffer, readlen);
        /* indicate the length to be read next time like before */
        readlen = TRANSFER_SZ;
        if (read_finished)
        {
          client.print("\r\n--X-ARDUINO_MULTIPART--\r\n\r\n");
          break;
        }
      }
      else {
        Serial.print("\r\nError receiving packet ");
        Serial.println(count);
        return;
      }
    }
  }
  String response;
  while(client.connected()) {
    while(client.available()){
      char c = client.read();
      response += c;
    }
  }
   
  int startIndex;
  String extractedLine = "Chuong trinh gap loi";
  // Split the string into lines
  
  
    if(response.indexOf("Sinh vien")!= -1){
      startIndex = response.indexOf("Sinh vien");
    }
    else if(response.indexOf("Khong tim")!= -1){
      startIndex = response.indexOf("Khong tim");
    }
    else if(response.indexOf("Lop chua")!= -1){
      startIndex = response.indexOf("Lop chua");
    }
    else if(response.indexOf("Tim thay")!= -1){
      startIndex = response.indexOf("Tim thay");
    }
  int endIndex = response.indexOf('\n', startIndex);
  // Check if the line is found
  if (startIndex != -1 && endIndex != -1) {
    extractedLine = response.substring(startIndex, endIndex);
  }
  Serial.println(extractedLine);
  digitalWrite(LED_PIN, HIGH); 
  LCD.clear();
  responseLCD(extractedLine);
  LCD.setCursor(0, 2);
  LCD.print("An nut de tiep tuc");
  digitalWrite(BUZZER_PIN,HIGH);
  delay(300);
  digitalWrite(BUZZER_PIN,LOW);
  while (digitalRead(BTN_PIN1) == HIGH && digitalRead(BTN_PIN2) == HIGH) {
  // Do nothing
  }
  LCD.clear();
  digitalWrite(LED_PIN, LOW); 
  Serial.println("-------------------------------");
}
