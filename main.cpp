#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <SoftwareSerial.h>
#include <thermalprinter.h>
#include <TJpg_Decoder.h>
#include <SPI.H>


#define PRINTER_RX_PIN              16
#define PRINTER_TX_PIN              17

#define BUTTON_PIN 22

#define LED_OK_PIN 25
#define LED_NOK_PIN 32

int printStatus = 0;
int timesince = 0;

Epson TM88 = Epson(PRINTER_RX_PIN, PRINTER_TX_PIN); // init the Printer with Output-Pin


int buttonState = 0;
bool pushed = false;

const char *ssid =  "-> Untere Aufstiegsstation";     // replace with your wifi ssid and wpa2 key
const char *password =  "Schnabeltier";
WiFiClientSecure client;

const char *host = "thecocktaildb.com";

JsonObject drink;

byte bw_image[8192] = { 0 }; //max 8 kb
int img_w = 0;
int img_h = 0;

void status(bool status){
  if(status)
  {     
    digitalWrite(LED_OK_PIN,HIGH);
    digitalWrite(LED_NOK_PIN,LOW);
  }else{
    digitalWrite(LED_OK_PIN,LOW);
    digitalWrite(LED_NOK_PIN,HIGH);
  }
}

void changeFont(uint8_t font){
TM88.write(0x1B); TM88.write(0x4D);TM88.write(font);
}

void printHeader(String text){
  TM88.doubleHeightOn();
  TM88.println(text);
  TM88.doubleHeightOff();
}

void printUnderline(String text){
  TM88.underlineOn();
  TM88.println(text);
  TM88.underlineOff();
}

void printBold(String text){
  TM88.boldOn();
  TM88.println(text);
  TM88.boldOff();
}

void printSubheader(String text){
  TM88.reverseOn();
  TM88.print(" ");
  TM88.print(text);
  TM88.print(" ");
  TM88.println("");
  TM88.reverseOff();
}

void printGerman(String input) { 
  input.replace("ä","\x7B");
  input.replace("ö","\x7C");
  input.replace("ü","\x7D");
  input.replace("Ä","\x5B");
  input.replace("Ö","\x5C");
  input.replace("Ü","\x5D");
  input.replace("ß","\x7E");
  TM88.characterSet(2); // German
  TM88.println(input);
  TM88.characterSet(0);
}



bool tm88_object(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  // Stop further decoding as image is running off bottom of screen
  if ( y >= 256) return 0;
  //Serial.printf("X: %i, Y: %i, W: %i, H: %i\n",x,y,w,h);

  for(int yP = 0; yP < h; yP++){
    for(int xP = 0; xP < w; xP++){
      //int p = x + xP + y*img_w + yP*img_w; //horizontal
        int p = y + yP + x*img_w + xP*img_w; //vertical

      unsigned r = (bitmap[xP+yP*w] & 0xF800) >> 8;       // Red
      unsigned g = (bitmap[xP+yP*w] & 0x07E0) >> 3;       // Green
      unsigned b = (bitmap[xP+yP*w] & 0x1F) << 3;         // Blue
      //printf("Pixel has color %i,%i,%i\n", r,g,b);
      //Serial.printf("\tPixel %i,%i local(%i) global(%i)-> %i,%i,%i\n",xP,yP,xP+yP*w,x+xP+y*img_w+yP*img_w,r,g,b);
      //Serial.printf("Set %i to value %i (%#X)\n",x+xP+y*img_w+yP*img_w,g,g);
      if(p/8 > sizeof(bw_image)){
        printf("Error. Could not write in image array at position %i\n",p/8);
        return 0;
      }
      
      if((0.2126*r + 0.7152*g + 0.0722*b) < 125){
        //printf("Set black pixel in array position %i pixel %i and add %i for byte %i \n",p/8,p,(int)pow(2.0,7.0-p%8),p%8);
        bw_image[p/8] += (int)pow(2.0,7.0-p%8);
      }else{
        //printf("Leave white pixel in array position %i pixel %i and add nothing for byte %i \n",p/8,p,p%8);
      } 
    }
  }

  // Return 1 to decode next block
  return 1;
}

//Goal https://en.wikipedia.org/wiki/Dither
//https://www.cyotek.com/blog/dithering-an-image-using-the-floyd-steinberg-algorithm-in-csharp
void dither(){
  for(int i=0;i<ceil((img_w*img_h)/8.0);i++) { //top to down
  
  }
  /*
  for each y from top to bottom do
      for each x from left to right do
          oldpixel := pixel[x][y]
          newpixel := round(oldpixel / 256);
          pixel[x][y] := newpixel
          quant_error := oldpixel - newpixel
          pixel[x + 1][y    ] := pixel[x + 1][y    ] + quant_error × 7 / 16
          pixel[x - 1][y + 1] := pixel[x - 1][y + 1] + quant_error × 3 / 16
          pixel[x    ][y + 1] := pixel[x    ][y + 1] + quant_error × 5 / 16
          pixel[x + 1][y + 1] := pixel[x + 1][y + 1] + quant_error × 1 / 16
*/
}

void decodeJPEG(String filepath){
  memset(bw_image, 0, sizeof bw_image);
  Serial.println("Convert JPEG to Array");
  uint16_t w = 0, h = 0, scale;
  TJpgDec.getFsJpgSize(&w, &h, filepath);
  img_w = w;
  img_h = h;
  int bpp = 1; //bytes per pixel
  int stride = w;
  TJpgDec.setJpgScale(1);
  Serial.printf("Image has size %ix%i Pixel\n",img_w,img_h);
  TJpgDec.setCallback(tm88_object);
  TJpgDec.drawFsJpg(0, 0, SPIFFS.open(filepath, "r"));
}


void printBitImage(){
  TM88.write(0x1D); TM88.write('/');TM88.write(3); 
}

void defineBitImage(){  
  uint8_t x = img_w/8;
  uint8_t y = img_h/8; 
  Serial.printf("Buffer image with %.1f bytes.\n",(img_w*img_h)/8.0);
  TM88.write(0x1D);TM88.write('*');TM88.write(x);TM88.write(y); //PRINT #1, CHR$(&H1D);"*";CHR$(30);CHR$(5);

  //d1...dk
  for(int i=0;i<ceil((img_w*img_h)/8.0);i++) {
    //Serial.printf("0x%X, ",bw_image[i]);
    TM88.write(bw_image[i]);
  }
}

bool downloadImage(String url){
  url = url + "/preview";
  //url = "https://raw.githubusercontent.com/s1effen/TM88/main/demo4.jpg";
  
  Serial.println("Download image with URL:");
  Serial.println(url);
  String filepath = "/cocktail.jpg";
  String host = url.substring(8,url.indexOf('/',8));
  String path = url.substring(url.indexOf('/',8));
  Serial.println("Connect to ");
  Serial.print(host);
  Serial.print(path);
  Serial.println();

  //Prepare file and Memory:
  size_t  total = 0;
  uint8_t buffer[1024] = { 0 };

  File file = SPIFFS.open(String(filepath), "w+");
  if (!file)
  {
    Serial.print(filepath); Serial.println(" open failed");
    return false;
  }
  
  // Connect to HTTPS server
  client.setTimeout(10000);
  if (!client.connect(host.c_str(), 443)) {
    Serial.println(F("Connection failed"));
    return false;
  }

  Serial.println(F("Connected!"));
  // Send HTTP request
  client.println("GET " + path + " HTTP/1.0");
  client.println("Host: " + host);
  client.println("Connection: close");
  if (client.println() == 0) {
    Serial.println(F("Failed to send request"));
    return false;
  }

  // Check HTTP status
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  // It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK"
  if (strcmp(status + 9, "200 OK") != 0) {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    return false;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    return false;
  }

  Serial.println("Downloading");

  while (client.connected())
  {
    int32_t start = millis();
    /*
    // this doesn't work as expected, but it helps for long downloads
    for (int16_t t = 0, dly = 50; t < 20; t++, dly += 50)
    {
      if (!client.connected()) break;
      if (client.available()) break; // read would not recover after having returned 0
      delay(dly);
    }
    */
    if (!client.connected()) break;
    int32_t elapsed = millis() - start;
    if (elapsed > 250)
    {
      Serial.print("waited for available "); Serial.print(millis() - start); Serial.print(" ms @ "); Serial.println(total);
    }
    size_t available = client.available();
    if (0 == available)
    {
      Serial.print("download error: timeout on available() after "); Serial.print(total); Serial.println(" bytes");
      break; // don't hang forever
    }
    size_t fetch = available <= sizeof(buffer) ? available : sizeof(buffer);
    if (fetch > 0)
    {
      size_t got = client.read(buffer, fetch);
      file.write(buffer, got);
      total += got;
      if(total % 100 == 0) Serial.printf("Downloaded bytes: %i\n",total);
    }
    delay(1); // yield();
    if (total > 30000) delay(250); // helps for long downloads
  }
  Serial.print("done, "); Serial.print(total); Serial.println(" bytes transferred");
  file.close();
  Serial.println("Written data to file: " + filepath);

  decodeJPEG(filepath);

// Disconnect
client.stop();
return true;
}

bool getCocktail(){
Serial.printf("Connecting %s...\n",host);

  // Connect to HTTPS server
  client.setTimeout(10000);
  if (!client.connect("thecocktaildb.com", 443)) {
    Serial.println(F("Connection failed"));
    return false;
  }

  Serial.println(F("Connected to cocktail server!"));

  // Send HTTP request
  client.println(F("GET /api/json/v1/1/random.php HTTP/1.0"));
  //client.println(F("GET /api/json/v1/1/lookup.php?i=178340 HTTP/1.0"));
  client.println(F("Host: thecocktaildb.com"));
  client.println(F("Connection: close"));
  if (client.println() == 0) {
    Serial.println(F("Failed to send request"));
    return false;
  }

  // Check HTTP status
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  // It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK"
  if (strcmp(status + 9, "200 OK") != 0) {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    return false;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    return false;
  }

  // Allocate the JSON document
  // Use arduinojson.org/v6/assistant to compute the capacity.
  const size_t capacity = 3072;
  DynamicJsonDocument doc(capacity);

  // Parse JSON object
  DeserializationError error = deserializeJson(doc, client);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }

  // Disconnect
  client.stop();
  drink = doc["drinks"][0];
  return true;
}

String splitInLines(String source, int width = 56)
{
    int currIndex = 0;
    int lastIndex = 0;
    int linebreak = 0;
    for (int i=0;i < source.length();i++){
      if(source[i]=='\n'){
        linebreak = i;
      }
      else if(source[i]==' '){
          currIndex = i;
          if(currIndex > linebreak + width){
            source[lastIndex] = '\n';
            linebreak = lastIndex;
          }
      }
      lastIndex = currIndex;
    }
    return source;
}

  void printCocktail(){
  if(!getCocktail())
  {
    TM88.println("Error. Could not get Cocktail.");
    return;
  }
  const char* name = drink["strDrink"]; // "Champagne Cocktail"
  const char* category = drink["strCategory"]; // "Ordinary Drink"
  const char* IBA = drink["strIBA"]; // "Contemporary Classics"
  const char* alcoholic = drink["strAlcoholic"]; // "Alcoholic"
  const char* glass = drink["strGlass"]; // "Champagne flute"
  const char* instructionEN = drink["strInstructions"]; // "Add dash of Angostura bitter ..
  const char* instructionDE = drink["strInstructionsDE"]; // "Einen Schuss Angostura-Bitter ..

  const char* Ingredient1 = drink["strIngredient1"]; // "Champagne"
  const char* Ingredient2 = drink["strIngredient2"]; // "Sugar"
  const char* Ingredient3 = drink["strIngredient3"]; // "Bitters"
  const char* Ingredient4 = drink["strIngredient4"]; // "Lemon peel"

  const char* Ingredient5 = drink["strIngredient5"]; // "Cognac"
  const char* Ingredient6 = drink["strIngredient6"]; // "Cognac"
  const char* Ingredient7 = drink["strIngredient7"]; // "Cognac"
  const char* Ingredient8 = drink["strIngredient8"]; // "Cognac"
  const char* Ingredient9 = drink["strIngredient9"]; // "Cognac"
  const char* Ingredient10 = drink["strIngredient10"]; // "Cognac"

  const char* Measure1 = drink["strMeasure1"]; // "Chilled "
  const char* Measure2 = drink["strMeasure2"]; // "1 piece "
  const char* Measure3 = drink["strMeasure3"]; // "2 dashes "
  const char* Measure4 = drink["strMeasure4"]; // "1 twist of "
  const char* Measure5 = drink["strMeasure5"]; // "1 dash"
  const char* Measure6 = drink["strMeasure6"]; // "1 dash"
  const char* Measure7 = drink["strMeasure7"]; // "1 dash"
  const char* Measure8 = drink["strMeasure8"]; // "1 dash"
  const char* Measure9 = drink["strMeasure9"]; // "1 dash"
  const char* Measure10 = drink["strMeasure10"]; // "1 dash"


  const char* ImageURL = drink["strDrinkThumb"]; // "https://pixabay.com/de/photos/champagner-cocktail-frenc
  
  String drinkName = name;
  String drinkCat = category;
  String ingedients = "";
  
  String instructions = splitInLines(instructionEN);

  if(Ingredient1 && Ingredient1[0]){
    ingedients += Ingredient1;
    if(Measure1){
      ingedients += " (";
      ingedients += Measure1;
      ingedients.trim();
      ingedients += ")";
    }
  }
  if(Ingredient2 && Ingredient2[0]){
    ingedients += "\n";
    ingedients += Ingredient2;
    if(Measure2){
      ingedients += " (";
      ingedients += Measure2;
      ingedients.trim();
      ingedients += ")";
    }
  }
  if(Ingredient3 && Ingredient3[0]){
    ingedients += "\n";
    ingedients += Ingredient3;
    if(Measure3){
      ingedients += " (";
      ingedients += Measure3;
      ingedients.trim();
      ingedients += ")";
    }
  }
  if(Ingredient4 && Ingredient4[0]){
    ingedients += "\n";
    ingedients += Ingredient4;
    if(Measure4){
      ingedients += " (";
      ingedients += Measure4;
      ingedients.trim();
      ingedients += ")";
    }
  }
  if(Ingredient5 && Ingredient5[0]){
    Serial.print("'");
    Serial.print(Ingredient5);
    Serial.print("'");
    Serial.println();
    ingedients += "\n";
    ingedients += Ingredient5;
    if(Measure5){
      ingedients += " (";
      ingedients += Measure5;
      ingedients.trim();
      ingedients += ")";
    }
  }
  if(Ingredient6 && Ingredient6[0]){
    ingedients += "\n";
    ingedients += Ingredient6;
    if(Measure6){
      ingedients += " (";
      ingedients += Measure6;
      ingedients.trim();
      ingedients += ")";
    }
  }
  if(Ingredient7 && Ingredient7[0]){
    ingedients += "\n";
    ingedients += Ingredient7;
    if(Measure7){
      ingedients += " (";
      ingedients += Measure7;
      ingedients.trim();
      ingedients += ")";
    }
  }
  if(Ingredient8 && Ingredient8[0]){
    ingedients += "\n";
    ingedients += Ingredient8;
    if(Measure8){
      ingedients += " (";
      ingedients += Measure8;
      ingedients.trim();
      ingedients += ")";
    }
  }
  if(Ingredient9 && Ingredient9[0]){
    ingedients += "\n";
    ingedients += Ingredient9;
    if(Measure9){
      ingedients += " (";
      ingedients += Measure9;
      ingedients.trim();
      ingedients += ")";
    }
  }
  if(Ingredient10 && Ingredient10[0]){
    ingedients += "\n";
    ingedients += Ingredient10;
    if(Measure10){
      ingedients += " (";
      ingedients += Measure10;
      ingedients.trim();
      ingedients += ")";
    }
  }
  TM88.justifyCenter();
  ;
  if(!downloadImage(ImageURL))
  {
    TM88.println("Error. Could not get Iamge.");
    return;
  }

  delay(100);
  defineBitImage();
  printBitImage();
  TM88.justifyLeft();

  TM88.feed(2);
  Serial.println("Print drink " + drinkName);
  changeFont(0);
  printHeader(drinkName);
  changeFont(1);
  printSubheader(drinkCat);
  
  TM88.feed(2);
  printUnderline("Ingredients:");
  changeFont(1);
  TM88.println(ingedients);

  TM88.feed(2);
  printUnderline("Instruction:");
  changeFont(1);
  TM88.print(instructions);
  changeFont(0);
  TM88.feed(5);
  TM88.cut();

  Serial.printf("%s ",drink);
}

void demo(){
  TM88.doubleHeightOn();
  TM88.println("double height on");  
  
  TM88.doubleHeightOff();
  TM88.println("double height off");  
  
  TM88.boldOn();
  TM88.println("bold on");  
  
  TM88.boldOff();  
  TM88.println("bold off");  
  
  TM88.underlineOn();
  TM88.println("underline on");  
  
  TM88.underlineOff();   
  TM88.println("underline off");  

  TM88.reverseOn();
  TM88.println("reverse on");  
  TM88.println(" Hamburgefonts ");  
  TM88.reverseOff();   
  TM88.println("reverse off"); 

  changeFont(0);
  TM88.println("Font 0");  

  changeFont(1);
  TM88.println("Font 1"); 

  TM88.feed(5);
  TM88.cut(); 

}

boolean InitalizeFileSystem() {
  bool initok = false;
  initok = SPIFFS.begin();
  
  if (!(initok)) {
       Serial.println("SPIFFS file system formatted.");
       SPIFFS.format();
       initok = SPIFFS.begin();   
  }
  if (!(initok)){
    SPIFFS.format();
    initok = SPIFFS.begin();
  }
  if (initok){
    Serial.println("SPIFFS is OK");
  }else{
    Serial.println("SPIFFS is not OK");
  }
  return initok;
  }

void setup() {
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_OK_PIN, OUTPUT);
  pinMode(LED_NOK_PIN, OUTPUT);
  digitalWrite(LED_NOK_PIN,HIGH);

  Serial.begin(9600);
  TM88.start();

  SPI.begin();
  InitalizeFileSystem();
  
  delay(10);
  Serial.println('\n');
  
  WiFi.begin(ssid, password);             // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid); Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i); Serial.print(' ');
  }

  Serial.println('\n');
  Serial.println("Connection established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
}


void loop() { 
buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == HIGH && !pushed) {
    pushed = true;
    Serial.println("Button Pushed");
    printCocktail();
  } else if (buttonState == LOW && pushed){
    pushed = false;
    delay(10);
    Serial.println("Button Released");
  }

  if(timesince % 1000 == 0){
    timesince = 0;
    printStatus = TM88.getStatus();     // get the current status of the TM88 printer
    if (printStatus == 22) {            // if status is 22 then we are good
      status(true);
    } else {
      status(false);
      //Serial.printf("Printer Status: is %i \n",printStatus); // debug the returned status code  
    } 
    
    if (Serial.available() > 0) {
      TM88.print(Serial.read());
    }
  }
  delay(50);
  timesince += 50;
}