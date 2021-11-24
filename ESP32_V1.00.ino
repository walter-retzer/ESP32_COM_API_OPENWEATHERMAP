/***************************************************************************************
  PROJETO IoT - DISPOSITIVO DE PREVISÃO DO TEMPO

  Objetivo: Requisitar dados de Previsão do Tempo ao Server do OpenWeatherMap utilizando
  o microcontrolador ESP32. Os dados são exibidos no display TFT, na exibição das informações
  são integradas algumas imagens de icones de previsão do tempo.

  Microcontrolador: ESP32 (ESP-WROOM-32) com Display TFT ILI9341 2.4"
  Versão: 1.00
  Data: 23/11/2021
  Autor: Walter Dawid Retzer



/***************************************************************************************
                          LIGAÇÃO DAS CONEXÕES

// ESP32 pinos utilizados na Interface Paralela do TFT:

 TFT    ESP-32
TFT_CS   33  // Chip select control pin
TFT_DC   15  // Data Command control pin - must use a pin in the range 0-31
TFT_RST  32  // Reset pin

TFT_WR    4  // Write strobe control pin - must use a pin in the range 0-31
TFT_RD    2

TFT_D0   12  
TFT_D1   13 
TFT_D2   26
TFT_D3   25
TFT_D4   17
TFT_D5   16
TFT_D6   27
TFT_D7   14

TFT_SD    ESP-32
SD_SS    GIOP05 (SS)
SD_DI    GIOP23 (MOSI)
SD_DO    GIOP19 (MISO)
SD_SCK   GIOP18 (SCK)







                          
***************************************************************************************/

/***************************************************************************************
                          Carregando as Bibliotecas
***************************************************************************************/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "time.h"
#include <moonPhase.h>
#include <time.h>

// usado para exibir uma barra de progresso no Setup
#include "GfxUi.h"
void drawProgress(uint8_t percentage, String text);

// API OpenWeather One Call API.
#include <OpenWeatherOneCall.h>              //<---Biblioteca da OpenWeather One Call API.
#define ONECALLKEY "ColoqueSuaChaveKeyAqui"  //<---Parâmetros para a API OpenWeather One Call API.
void forecast4Days();                        //<---Função para requisitar os dados dos próximos 4 dias
OpenWeatherOneCall OWOC;                     //<---Parâmetros para a API OpenWeather One Call API.


int city_id = 3447399;            //<-- SOROCABA, BR
float myLatitude = -23.501671;    //<---GPS coordenadas para a cidade de SOROCABA, BR
float myLongitude = -47.458061;   //<---GPS coordenadas para a cidade de SOROCABA, BR
bool metric = true;               //<---TRUE é METRIC, FALSE é IMPERIAL, BLANK é KELVIN


//Função que calcula a Fase da Lua
moonPhase moonPhase;
void moon_Phase();

// Variáveis para a conexão com o Servidor NTPServer (protocolo para sincronização dos relógios dos computadores baseado no protocolo UDP)
struct tm timeinfo = {0};
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -3600;
const int   daylightOffset_sec = 3600;
void printLocalTime();

//Display TFT ILI9341
#include <TFT_eSPI.h>
#include <SPI.h>
TFT_eSPI tft = TFT_eSPI();
GfxUi ui = GfxUi(&tft);     // Jpeg and bmpDraw functions TODO: pull outside of a class


//Fontes
#define GFXFF 1
#define FSS9 &FreeSans9pt7b
#define FSS12 &FreeSans12pt7b
#define FSS18 &FreeSans18pt7b
#define FSS24 &FreeSans24pt7b
#define FSSB9 &FreeSansBold9pt7b
#define FSSB12 &FreeSansBold12pt7b
#define FSSB18 &FreeSansBold18pt7b
#define FSSB24 &FreeSansBold24pt7b

// Cores: mais cores para serem utilizadas no Display TFT: (https://gist.github.com/Kongduino/36d152c81bbb1214a2128a2712ecdd18)
#define TFT_GRAY2 0x734D
#define TFT_BLUE2 0x04D5
#define TFT_AQUA 0x04FF
#define TFT_DARKCYAN 0x03EF
#define TFT_DARKCYAN2 0x0451
#define TFT_SKYBLUE 0x867D
#define TFT_ROYALBLUE 0x435C
#define TFT_STEELBLUE 0x4416
#define TFT_SLATEBLUE 0x6AD9
#define TFT_SEAGREEN 0x2C4A
#define TFT_SEASHELL 0xFFBD
#define TFT_MEDIUMVIOLETRED 0xC0B0
#define TFT_MIDNIGHTBLUE 0x18CE
#define TFT_MINTCREAM 0xF7FF
#define TFT_DARKORANGE 0xFC60
#define TFT_PLUM 0xDD1B
#define TFT_POWDERBLUE 0xB71C
#define TFT_PURPLE2 0x8010
#define TFT_DARKTURQUOISE 0x067A
#define TFT_DARKVIOLET 0x901A
#define TFT_DEEPPINK 0xF8B2
#define TFT_DEEPSKYBLUE 0x05FF
#define TFT_BLUEVIOLET 0x895C
#define TFT_LIGHTCYAN 0xE7FF

// Wifi Parâmetros
void setupWifi();
const char* ssid = "NomeDoSuaRede";
const char* password = "SuaSenha";
int intensidadeSinal = -9999;

// Setup do SDCard
void setupSdCard();

//Parâmetros para o clima atual(currentWeather) da OPENWEATHER API:
void getCurrentWeather();                  //Função responsável pelos dados do clima atual
String city = "sorocaba,br";               //Nome da Cidade: Sorocaba,Br
String key = "ColoqueSuaChaveKeyAqui";     //Chave Key do OPENWEATHERAPI
String payload;                            //Msg payload do Server OpenWeather
int timezone = -3;                         //Time zone Brasil: UTC -3
byte preset_unit = 0;                      //Temperatura unidade (0 = Celsius, 1 = Fahrenheit, 2 = Kelvin);
byte time_format = 0;                      //24H/12H time format (0 = 24H, 1 = 12H)

//Pisca LED da Placa do ESP32
int LED_BUILTIN = 22;
byte estadoLED = true;
unsigned long tempoLED = 0;          //tempo auxiliar LED
unsigned long intervalolLED = 250;   //tempo 250mseg entre on e OFF do LED

//Loop para receber os dados do OpenWeather
unsigned long tempoServer = 0;          //tempo auxiliar Server OpenWeather
unsigned long intervaloServer = 60000;  //tempo 60seg para atualizar os dados metereológicos

//Loop para receber os dados de time do NTPServer
unsigned long tempoNTPServer = 0;               //tempo auxiliar NTPServer
unsigned long intervaloNTPServer = 10000;       //tempo 10seg para atualizar o horário



/***************************************************************************************
                                       SETUP
***************************************************************************************/

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);      //ativa LED_BUILT_IN como saída
  digitalWrite(LED_BUILTIN, HIGH);   //ativa LED

  Serial.begin(115200);     //inicia Serial

  //inicia TFT Display
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  delay(1000);

  //inicia SD Card:
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }

  listDir(SD, "/", 0); // lista as pasta do SD Card e exibe na Serial
  setupSdCard();       // Setup do SD Card

  tft.fillRoundRect(10, 5, 220, 40, 15, TFT_BLUE);
  tft.drawRoundRect(10, 5, 220, 40, 15, TFT_WHITE);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setFreeFont(FSS9);                 // Select the font
  tft.setTextSize(2);
  tft.drawString("SETUP", 120, 44, GFXFF);// Print the string name of the font

  // desenha alguns ícones na Tela:
  drawBmp("/icons_telaInicial/mapamundi.bmp",  10, 55);
  drawBmp("/icons_telaInicial/logo_Open80x60.bmp",  10, 145);
  drawBmp("/icons_telaInicial/logo_wifi80x60.bmp",  10, 215);
  drawBmp("/icons_telaInicial/logo_ESP32.bmp", 100, 145);

  delay(5000);

  tft.setTextSize(0);
  tft.setTextFont(1);

  drawProgress(20, "Atualizando icones...");
  delay(1500);
  drawProgress(50, "Iniciando SD Card...");
  delay(1500);
  drawProgress(75, "Iniciando Wifi...");
  delay(1500);
  drawProgress(100, "Setup realizado!");
  delay(1500);

  setupWifi();

  tft.drawFastHLine(0, 258, 240, TFT_ORANGE);
  delay(2000);

  //Testando animação das Fases da Lua:
  drawBmp("/moon/moonphase_L0.bmp",  90, 260);
  drawBmp("/moon/moonphase_L1.bmp",  90, 260);
  drawBmp("/moon/moonphase_L2.bmp",  90, 260);
  drawBmp("/moon/moonphase_L3.bmp",  90, 260);
  drawBmp("/moon/moonphase_L4.bmp",  90, 260);
  drawBmp("/moon/moonphase_L5.bmp",  90, 260);
  drawBmp("/moon/moonphase_L6.bmp",  90, 260);
  drawBmp("/moon/moonphase_L7.bmp",  90, 260);
  drawBmp("/moon/moonphase_L8.bmp",  90, 260);
  drawBmp("/moon/moonphase_L9.bmp",  90, 260);
  drawBmp("/moon/moonphase_L10.bmp", 90, 260);
  drawBmp("/moon/moonphase_L11.bmp", 90, 260);
  drawBmp("/moon/moonphase_L12.bmp", 90, 260);
  drawBmp("/moon/moonphase_L12.bmp", 90, 260);
  drawBmp("/moon/moonphase_L13.bmp", 90, 260);
  drawBmp("/moon/moonphase_L14.bmp", 90, 260);
  drawBmp("/moon/moonphase_L15.bmp", 90, 260);
  drawBmp("/moon/moonphase_L16.bmp", 90, 260);
  drawBmp("/moon/moonphase_L17.bmp", 90, 260);
  drawBmp("/moon/moonphase_L18.bmp", 90, 260);
  drawBmp("/moon/moonphase_L19.bmp", 90, 260);
  drawBmp("/moon/moonphase_L20.bmp", 90, 260);
  drawBmp("/moon/moonphase_L21.bmp", 90, 260);
  drawBmp("/moon/moonphase_L22.bmp", 90, 260);
  drawBmp("/moon/moonphase_L23.bmp", 90, 260);
  drawBmp("/moon/moonphase_L0.bmp",  90, 260);
  drawBmp("/moon/moonphase_L1.bmp",  90, 260);
  drawBmp("/moon/moonphase_L2.bmp",  90, 260);
  drawBmp("/moon/moonphase_L3.bmp",  90, 260);
  drawBmp("/moon/moonphase_L4.bmp",  90, 260);
  drawBmp("/moon/moonphase_L5.bmp",  90, 260);
  drawBmp("/moon/moonphase_L6.bmp",  90, 260);
  drawBmp("/moon/moonphase_L7.bmp",  90, 260);
  drawBmp("/moon/moonphase_L8.bmp",  90, 260);
  drawBmp("/moon/moonphase_L9.bmp",  90, 260);
  drawBmp("/moon/moonphase_L10.bmp", 90, 260);
  drawBmp("/moon/moonphase_L11.bmp", 90, 260);
  drawBmp("/moon/moonphase_L12.bmp", 90, 260);

  delay(5000);

  Serial.println( "Connected. Getting time..." );
  //configTzTime("UTC+3", "0.pool.ntp.org"); // Timezone: TTC+3 (Argentina / Brasil)
  configTzTime("UTC+3", "a.st1.ntp.br"); // Timezone: TTC+3 (Argentina / Brasil)


  while ( !getLocalTime( &timeinfo, 0 ) )
    vTaskDelay( 10 / portTICK_PERIOD_MS );


  //Desenhando o DashBoard do Protótipo
  drawBmp("/dashboard/wifi_mini.bmp", 0, 0);
  drawBmp("/dashboard/sdCard_mini.bmp", 212, 0);
  tft.drawFastHLine(0, 30, 240, TFT_ORANGE);
  tft.drawFastHLine(0, 179, 240, TFT_ORANGE);
  tft.drawFastHLine(0, 258, 240, TFT_ORANGE);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFreeFont(FSSB12);                 // Select the font
  tft.drawString("SOROCABA", 120, 45, GFXFF);// Print the string name of the font

  printLocalTime();
  delay(1000);
  getCurrentWeather();
  delay(1000);
  forecast4Days();
  delay(1000);
  moon_Phase();
  delay(1000);

  tempoLED = millis();             //Variavel auxiliar para realizar os pulsos no LED
  tempoServer = millis();          //Variavel auxiliar para realizar a requisição da API ao SERVER
  tempoNTPServer = millis();          //Variavel auxiliar para realizar a requisição da NTPServer

}


/***************************************************************************************
                                        LOOP
***************************************************************************************/

void loop() {


  //Verifica se o intervalo do Led já foi atingido
  if (millis() - tempoLED >= intervalolLED) {

    estadoLED = !estadoLED;                 // Inverte o Estado do LED
    digitalWrite(LED_BUILTIN, estadoLED);   // Seta o LED DA PLACA ARDUINO
    tempoLED = millis();
  }

  //Verifica se o intervalo do tempoNTPServer já foi atingido
  if (millis() - tempoNTPServer >= intervaloNTPServer) {

    tempoNTPServer = millis();
    printLocalTime();

  }


  //Verifica se o intervalo do tempoServer já foi atingido
  if (millis() - tempoServer >= intervaloServer) {

    moon_Phase();
    getCurrentWeather();
    forecast4Days();

    tempoServer = millis();

  }
}


//========================================================================================================
// Dados Metereológicos do clima atual em formato JSON de OpenWeatherMap API

void getCurrentWeather() {

  // Setup da unidade das variáveis:
  String temperature_unit;
  char* temperature_deg;

  switch (preset_unit) {
    case 0: {
        temperature_unit = "metric";
        temperature_deg = "C";
      }
      break;
    case 1: {
        temperature_unit = "imperial";
        temperature_deg = "F";
      }
      break;
    case 2: {
        temperature_unit = "default";
        temperature_deg = "K";
      }
      break;
  }

  // endereço para realizar a requisição dos dados:
  String current = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&units=" + temperature_unit + "&APPID=" + key;

  //Checando o status da conexão Wi-Fi
  if ((WiFi.status() == WL_CONNECTED)) {
    //Fazendo a requisição ao Server
    HTTPClient http;
    http.begin(current);
    int httpCode = http.GET();
    String payload;

    //Checando o retorno do Server
    if (httpCode > 0) {
      const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 600;
      DynamicJsonDocument doc(capacity);

      String payload = http.getString();
      //Serial.println(httpCode);
      //Serial.println(payload);

      // Seriando os valores do JSON
      DeserializationError error = deserializeJson(doc, payload);

      // testa se há erro
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
      }

      //Variáveis do Clima Atual:
      float temperature = doc["main"]["temp"];
      int humidity = doc["main"]["humidity"];
      int pressure = doc["main"]["pressure"];
      float visibility = doc["visibility"];
      int id = doc["weather"][0]["id"];
      String description = doc["weather"][0]["description"];
      String statusWeather = doc["weather"][0]["main"];
      float wind_speed = doc["wind"]["speed"];
      int wind_deg = doc["wind"]["deg"];
      int rain_volume = doc["rain"]["1h"];
      int snow_volume = doc["snow"]["1h"];
      String sunrise = doc["sys"]["sunrise"];
      String sunset = doc["sys"]["sunset"];

      //Selecionando a imagem a ser mostrada na tela:
      if (statusWeather.equals("Clear"))  drawBmp("/big/sunny.bmp",  0, 30);
      if (statusWeather.equals("Thunderstorm"))  drawBmp("/big/tstorms.bmp",  0, 30);
      if (statusWeather.equals("Drizzle"))  drawBmp("/big/flurries.bmp",  0, 30);
      if (statusWeather.equals("Snow"))  drawBmp("/big/snow.bmp",  0, 30);

      if (description.equals("light rain"))  drawBmp("/big/chanceflurries.bmp",  0, 30);
      if (description.equals("moderate rain"))  drawBmp("/big/chancerain.bmp",  0, 30);
      if (description.equals("heavy intensity rain"))  drawBmp("/big/rain.bmp",  0, 30);
      if (description.equals("very heavy rain"))  drawBmp("/big/rain.bmp",  0, 30);
      if (description.equals("extreme rain"))  drawBmp("/big/rain.bmp",  0, 30);
      if (description.equals("freezing rain"))  drawBmp("/big/sleet.bmp",  0, 30);
      if (description.equals("light intensity shower rain"))  drawBmp("/big/rain.bmp",  0, 30);
      if (description.equals("shower rain"))  drawBmp("/big/rain.bmp",  0, 30);
      if (description.equals("heavy intensity shower rain"))  drawBmp("/big/rain.bmp",  0, 30);
      if (description.equals("ragged shower rain"))  drawBmp("/big/rain.bmp",  0, 30);

      if (description.equals("few clouds"))  drawBmp("/big/partlycloudy.bmp",  0, 30);
      if (description.equals("scattered clouds"))  drawBmp("/big/mostlysunny.bmp",  0, 30);
      if (description.equals("broken clouds"))  drawBmp("/big/mostlycloudy.bmp",  0, 30);
      if (description.equals("overcast clouds"))  drawBmp("/big/cloudy.bmp",  0, 30);

      //Desenhando o Dashboard com os Valores e os bitmaps dos Icones:

      tft.drawFastHLine(0, 30, 240, TFT_ORANGE);
      tft.setTextDatum(BC_DATUM);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setFreeFont(FSSB12);
      tft.drawString("SOROCABA", 120, 45, GFXFF);

      tft.setFreeFont(FSSB18);
      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.setCursor(1, 140);
      tft.print(temperature, 1);

      tft.setFreeFont(FSSB9);
      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.setCursor(67, 125);
      tft.print(F("o"));

      tft.setTextSize(1);
      tft.setFreeFont(FSSB18);
      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.setCursor(76, 140);
      tft.print(F("C"));

      tft.setTextSize(1);
      tft.setFreeFont(FSS9);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(5, 162);
      tft.print(statusWeather);

      tft.setTextSize(0);
      tft.setTextFont(1);
      tft.setCursor(5, 168);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.print(description);

      tft.drawRoundRect(104, 46, 60, 60, 5, TFT_WHITE);
      drawBmp("/dashboard/wi-raindrops2.bmp", 114, 48);
      tft.setTextSize(1);
      tft.setFreeFont(FSS9);                 // Select the font
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(114, 102);
      tft.print(rain_volume);
      tft.print(F("mm"));

      tft.drawRoundRect(172, 46, 60, 60, 5, TFT_WHITE);
      drawBmp("/dashboard/wi-humidity.bmp", 182, 48);
      tft.setTextSize(1);
      tft.setFreeFont(FSS9);                 // Select the font
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(185, 102);
      tft.print(humidity, 0);
      tft.print(F("%"));

      tft.drawRoundRect(104, 114, 60, 60, 5, TFT_WHITE);
      drawBmp("/dashboard/wi-strong-wind.bmp", 123, 125);
      tft.setTextSize(1);
      tft.setFreeFont(FSS9);                 // Select the font
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(106, 170);
      tft.print(wind_speed);
      tft.setTextSize(0);
      tft.setTextFont(1);
      tft.setCursor(143, 162);
      tft.print(F("m/s"));

      tft.setTextSize(1);
      tft.setFreeFont(FSS9);                 // Select the font
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(125, 131);

      //Desenha o bitmap da Direção do Vento de acordo com a variável: wind_deg
      if (wind_deg >= 0 && wind_deg <= 23) {
        drawBmp("/wind/N.bmp", 109, 133);
        tft.print(F("N "));
      }
      if (wind_deg > 23 && wind_deg <= 68) {
        drawBmp("/wind/NE.bmp", 109, 133);
        tft.print(F("NE"));
      }
      if (wind_deg > 68 && wind_deg <= 113) {
        drawBmp("/wind/E.bmp", 109, 133);
        tft.print(F("E "));
      }
      if (wind_deg > 113 && wind_deg <= 158) {
        drawBmp("/wind/SE.bmp", 109, 133);
        tft.print(F("SE"));
      }
      if (wind_deg > 158 && wind_deg <= 203) {
        drawBmp("/wind/S.bmp", 109, 133);
        tft.print(F("S "));
      }
      if (wind_deg > 203 && wind_deg <= 248) {
        drawBmp("/wind/SW.bmp", 109, 133);
        tft.print(F("SW"));
      }
      if (wind_deg > 248 && wind_deg <= 293) {
        drawBmp("/wind/W.bmp", 109, 133);
        tft.print(F("W "));
      }
      if (wind_deg > 293 && wind_deg <= 336) {
        drawBmp("/wind/NW.bmp", 109, 133);
        tft.print(F("NW"));
      }
      if (wind_deg > 336 && wind_deg <= 360) {
        drawBmp("/wind/N.bmp", 109, 133);
        tft.print(F("N "));
      }

      tft.drawRoundRect(172, 114, 60, 60, 5, TFT_WHITE);
      drawBmp("/dashboard/wi-barometer.bmp", 182, 116);
      tft.setTextSize(1);
      tft.setFreeFont(FSS9);                 // Select the font
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(182, 170);
      tft.print(pressure, 0);
      tft.setTextSize(0);
      tft.setTextFont(1);
      tft.setCursor(210, 143);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.print(F("hPa"));

      //Desenha os Icones de Por do Sol e Nascer do Sol e exibe o horário de cada um:
      tft.setTextSize(0);
      tft.setTextFont(1);

      drawBmp("/sun/sunrise.bmp", 0, 258);
      tft.drawFastHLine(0, 258, 240, TFT_ORANGE);

      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(45, 270);
      tft.print(convertTimestamp(doc["sys"]["sunrise"], time_format)); //converte em horas:min
      tft.println(F("h"));

      Serial.print("Sunrise: ");
      Serial.println(convertTimestamp(doc["sys"]["sunrise"], time_format)); //converte em horas:min

      drawBmp("/sun/sunset.bmp", 0, 286);

      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setCursor(45, 300);
      tft.print(convertTimestamp(doc["sys"]["sunset"], time_format)); //converte em horas:min
      tft.println(F("h"));

      Serial.print("Sunset: ");
      Serial.println(convertTimestamp(doc["sys"]["sunset"], time_format)); //converte em horas:min


    } else {
      Serial.println("HTTP request error");
    }
    http.end();
  }
}


//========================================================================================================
//Lista Diretórios do SD Card:

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {

  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

//========================================================================================================
//Desenha icones no formato .bmp 16colors no TFT Display:

void drawBmp(const char *filename, int16_t x, int16_t y) {

  if ((x >= tft.width()) || (y >= tft.height())) return;

  File bmpFS;

  //Abre arquivo no SD card
  bmpFS = SD.open(filename);

  if (!bmpFS)
  {
    Serial.print("File not found");
    return;
  }

  uint32_t seekOffset;
  uint16_t w, h, row, col;
  uint8_t  r, g, b;

  uint32_t startTime = millis();

  if (read16(bmpFS) == 0x4D42)
  {
    read32(bmpFS);
    read32(bmpFS);
    seekOffset = read32(bmpFS);
    read32(bmpFS);
    w = read32(bmpFS);
    h = read32(bmpFS);

    if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0))
    {
      y += h - 1;

      tft.setSwapBytes(true);
      bmpFS.seek(seekOffset);

      uint16_t padding = (4 - ((w * 3) & 3)) & 3;
      uint8_t lineBuffer[w * 3];

      for (row = 0; row < h; row++) {
        bmpFS.read(lineBuffer, sizeof(lineBuffer));
        uint8_t*  bptr = lineBuffer;
        uint16_t* tptr = (uint16_t*)lineBuffer;
        // Convert 24 to 16 bit colours
        for (uint16_t col = 0; col < w; col++)
        {
          b = *bptr++;
          g = *bptr++;
          r = *bptr++;
          *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
        // Read any line padding
        if (padding) bmpFS.read((uint8_t*)tptr, padding);
        // Push the pixel row to screen, pushImage will crop the line if needed
        tft.pushImage(x, y--, w, 1, (uint16_t*)lineBuffer);
      }
      Serial.print("Loaded in "); Serial.print(millis() - startTime);
      Serial.println(" ms");
    }
    else Serial.println("BMP format not recognized.");
  }
  bmpFS.close();
}


//========================================================================================================
// Função auxiliar para desenhar os icones bitmaps

uint16_t read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

//========================================================================================================
// Função auxiliar para desenhar os icones bitmaps

uint32_t read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

//========================================================================================================
//Convert tempo em formato: HH:MM

String convertTimestamp(String timestamp, byte time_format) {
  long int time_int = timestamp.toInt();
  time_int = time_int + timezone * 3600;
  struct tm *info;

  time_t t = time_int;
  info = gmtime(&t);
  String result;
  byte hours = ((info->tm_hour) > 12 && time_format == 1) ? info->tm_hour - 12 : info->tm_hour;

  String noon;
  if (time_format == 1) {
    noon = ((info->tm_hour) > 12) ? " PM" : " AM";
  } else {
    noon = "";
  }


  String o = (info->tm_min < 10) ? "0" : "";
  result = String(hours) + ":" + o + String(info->tm_min) + noon;

  return result;
}

//========================================================================================================
//Função para comparar Dia e Noite:

int intTime(String timestamp) {
  long int time_int = timestamp.toInt();
  struct tm *info;
  time_t t = time_int;
  info = gmtime(&t);
  int result = info->tm_hour * 100 + info->tm_min;
  return result;
}


//========================================================================================================
//Print Local Time do NPTServer

void printLocalTime()
{

  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    tft.setTextSize(1);
    tft.setFreeFont(FSS9);                 // Select the font
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(45, 15);
    tft.print("Failed to obtain time");
    return;
  }

  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  tft.fillRect(22, 0, 191, 20, TFT_BLACK);
  tft.setTextSize(1);
  tft.setFreeFont(FSS9);                 // Select the font
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(22, 15);
  tft.print(&timeinfo, "%a. %d/%m/%Y %Hh%M");

}

//========================================================================================================
//Calcula as Fases da Lua

void moon_Phase() {

  getLocalTime( &timeinfo );
  Serial.print( asctime( &timeinfo ) );
  Serial.println(&timeinfo, "%a, %B %d %Y %H:%M:%S");

  moonData_t moon = moonPhase.getPhase();

  //Desenha o icone de acordo com a posição (angulo) que a Lua esta:
  if (moon.angle >= 0 && moon.angle <= 8)  drawBmp("/moon/moonphase_L0.bmp",  90, 260);
  if (moon.angle >  8 && moon.angle <= 23) drawBmp("/moon/moonphase_L1.bmp",  90, 260);
  if (moon.angle > 23 && moon.angle <= 38) drawBmp("/moon/moonphase_L2.bmp",  90, 260);
  if (moon.angle > 38 && moon.angle <= 53) drawBmp("/moon/moonphase_L3.bmp",  90, 260);
  if (moon.angle > 53 && moon.angle <= 68) drawBmp("/moon/moonphase_L4.bmp",  90, 260);
  if (moon.angle > 68 && moon.angle <= 83) drawBmp("/moon/moonphase_L5.bmp",  90, 260);
  if (moon.angle > 83 && moon.angle <= 98) drawBmp("/moon/moonphase_L6.bmp",  90, 260);
  if (moon.angle > 98 && moon.angle <= 113)drawBmp("/moon/moonphase_L7.bmp",  90, 260);
  if (moon.angle > 113 && moon.angle <= 128)drawBmp("/moon/moonphase_L8.bmp",  90, 260);
  if (moon.angle > 128 && moon.angle <= 143)drawBmp("/moon/moonphase_L9.bmp",  90, 260);
  if (moon.angle > 143 && moon.angle <= 158)drawBmp("/moon/moonphase_L10.bmp", 90, 260);
  if (moon.angle > 158 && moon.angle <= 173)drawBmp("/moon/moonphase_L11.bmp", 90, 260);
  if (moon.angle > 173 && moon.angle <= 188)drawBmp("/moon/moonphase_L12.bmp", 90, 260);
  if (moon.angle > 188 && moon.angle <= 203)drawBmp("/moon/moonphase_L12.bmp", 90, 260);
  if (moon.angle > 203 && moon.angle <= 218)drawBmp("/moon/moonphase_L13.bmp", 90, 260);
  if (moon.angle > 218 && moon.angle <= 233)drawBmp("/moon/moonphase_L14.bmp", 90, 260);
  if (moon.angle > 233 && moon.angle <= 248)drawBmp("/moon/moonphase_L15.bmp", 90, 260);
  if (moon.angle > 248 && moon.angle <= 263)drawBmp("/moon/moonphase_L16.bmp", 90, 260);
  if (moon.angle > 263 && moon.angle <= 278)drawBmp("/moon/moonphase_L17.bmp", 90, 260);
  if (moon.angle > 278 && moon.angle <= 293)drawBmp("/moon/moonphase_L18.bmp", 90, 260);
  if (moon.angle > 293 && moon.angle <= 308)drawBmp("/moon/moonphase_L19.bmp", 90, 260);
  if (moon.angle > 308 && moon.angle <= 323)drawBmp("/moon/moonphase_L20.bmp", 90, 260);
  if (moon.angle > 323 && moon.angle <= 338)drawBmp("/moon/moonphase_L21.bmp", 90, 260);
  if (moon.angle > 338 && moon.angle <= 353)drawBmp("/moon/moonphase_L22.bmp", 90, 260);
  if (moon.angle > 353 && moon.angle <= 360)drawBmp("/moon/moonphase_L23.bmp", 90, 260);

  tft.drawFastHLine(0, 258, 240, TFT_ORANGE);

  tft.setTextSize(0);
  tft.setTextFont(1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(142, 264);

  if (moon.angle >= 0  && moon.angle < 45) tft.print(F("Lua Nova"));
  if (moon.angle >= 45 && moon.angle < 90) tft.print(F("Lua Crescente"));
  if (moon.angle >= 90 && moon.angle < 135) tft.print(F("Quarto Crescente"));
  if (moon.angle >= 135 && moon.angle < 180) tft.print(F("Crescente Gibosa"));
  if (moon.angle >= 180 && moon.angle < 225) tft.print(F("Lua Cheia"));
  if (moon.angle >= 225 && moon.angle < 270) tft.print(F("Minguante Gibosa"));
  if (moon.angle >= 270 && moon.angle < 315) tft.print(F("Quarto Minguante"));
  if (moon.angle >= 315 && moon.angle <= 360) tft.print(F("Lua Minguante"));

  tft.setTextSize(0);
  tft.setTextFont(1);
  tft.setCursor(155, 280);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.print(F("Angulo: "));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print(moon.angle);
  tft.print((char)247);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(155, 296);
  tft.print(F("Face: "));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print(moon.percentLit * 100);
  tft.print(F("%"));

}


//========================================================================================================
//Desenha a Barra de Progresso:

void drawProgress(uint8_t percentage, String text) {
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextPadding(275);
  tft.fillRect(0, 285, 240, 12, TFT_BLACK);
  delay(2000);
  tft.drawString(text, 120, 295);
  ui.drawProgressBar(10, 305, 240 - 20, 15, percentage, TFT_WHITE, TFT_GREEN);
  tft.setTextPadding(0);
}

//========================================================================================================
//Setup Wifi

void setupWifi() {

  tft.fillScreen(TFT_BLACK);
  tft.fillRoundRect(10, 5, 220, 40, 15, TFT_BLUE);
  tft.drawRoundRect(10, 5, 220, 40, 15, TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setFreeFont(FSS9);                 // Select the font
  tft.drawString("WIFI SETUP", 70, 18, GFXFF);// Print the string name of the font

  drawBmp("/wifi/wifi_connecting.bmp", 40, 60);
  delay(2000);

  Serial.println();
  Serial.print("Conectando-se a ");
  Serial.println(ssid);

  tft.setCursor(10, 230);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.print(F("Conectando-se: "));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    tft.print(F("."));
  }
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print(F("Ok"));

  delay(1000);
  drawBmp("/wifi/wifi_yellow.bmp", 40, 60);
  delay(2000);

  Serial.println("");
  Serial.println("WiFi conectada.");
  Serial.println("Endereço de IP: ");
  Serial.println(WiFi.localIP());

  tft.setCursor(10, 250);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.print(F("Status: "));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print(F("Conectada"));

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(10, 270);
  tft.print(F("Rede WiFi: "));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print(ssid);

  tft.setCursor(10, 290);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.print(F("IP: "));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println(WiFi.localIP());

  intensidadeSinal = WiFi.RSSI();

  tft.setCursor(10, 310);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.print(F("Sinal: "));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print(intensidadeSinal);
  tft.print(F("dB"));

  delay(5000);
  tft.fillScreen(TFT_BLACK);
}

//========================================================================================================
//Sd Card setup:

void setupSdCard() {

  tft.fillScreen(TFT_BLACK);
  tft.fillRoundRect(10, 5, 220, 40, 15, TFT_BLUE);
  tft.drawRoundRect(10, 5, 220, 40, 15, TFT_WHITE);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setFreeFont(FSS9);                 // Select the font
  tft.drawString("SD CARD SETUP", 120, 18, GFXFF);// Print the string name of the font

  drawBmp("/sdcard/sd-card-vector-icon.bmp", 48, 50);
  delay(2000);

  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    tft.setCursor(10, 255);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.print(F("SD Card: "));
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print(F("Falha"));
    return;
  }

  Serial.print("SD Card Type: ");
  tft.setCursor(10, 255);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.print(F("Tipo: "));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
    tft.print(F("MMC"));
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
    tft.print(F("SDSC"));
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
    tft.print(F("SDHC"));
  } else {
    Serial.println("UNKNOWN");
    tft.print(F("UNKNOWN"));
  }

  delay(1000);
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);

  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  tft.setCursor(10, 275);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.print(F("Tamanho: "));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print(cardSize);
  tft.print(F("Mb"));

  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  tft.setCursor(10, 295);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.print(F("Total: "));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print(SD.totalBytes() / (1024 * 1024));
  tft.print(F("Mb"));

  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(10, 315);
  tft.print(F("Usado: "));
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print(SD.usedBytes() / (1024 * 1024));
  tft.print(F("Mb"));

  delay(5000);
  tft.fillScreen(TFT_BLACK);
}



//========================================================================================================
//Previsão do Tempo para 4 Dias com o OpenWeatherOneCall:

void forecast4Days() {

  /*
     EXCLUDES ARE:
     EXCL_C(urrent) EXCL_D(aily) EXCL_H(ourly) EXCL_M(inutely) EXCL_A(lerts)
     In the form EXCL_C+EXCL_D+EXCL_H etc
     NULL is EXCLUDE NONE (Send ALL Info)
  */

  /*
     Historical Data is for the last 1-5 days
     NULL returns NO HISTORICAL DATA
     HISTORICAL Data overrides Current and is the ONLY data returned
     Change the below argument to number of days prior desired - 24 hours returned
     So, if today is Friday and you want Wednesday use 2
  */

  int  HISTORICAL = NULL; //<--------------------- set to # days for Historical weather, NULL for current

  OWOC.parseWeather(ONECALLKEY, NULL, NULL, NULL, metric, city_id, EXCL_H + EXCL_M, NULL); //<---------excludes hourly, minutely, historical data 1 day

  if (!HISTORICAL)
  {

    if (OWOC.current) {
      Serial.println(OWOC.current[0].temperature);

      Serial.println("");
      Serial.println("7 Day High Temperature Forecast:");
    }

    //Loop para pegar os dados da previsão do Tempo dos próximos 4 dias
    for (int y = 0; y < 5; y++) {

      //Dia da Semana da previsão dos dados metereológicos
      long DoW = OWOC.forecast[y].dayTime / 86400L;
      int day_of_week = (DoW + 4) % 7;

      Serial.print(OWOC.short_names[day_of_week]);
      Serial.print(": ");
      Serial.println(OWOC.forecast[y].temperatureHigh);
      Serial.print(" Sunset: ");
      Serial.println(OWOC.forecast[y].readableSunset);
      Serial.print(" Description: ");
      Serial.println(OWOC.forecast[y].summary);

      //    y == 0 corresponde ao dia atual, nesse programa optamos por não utilizar esses dados
      //      if (y == 0) {
      //
      //        String m1 = OWOC.forecast[y].main;
      //        String m2 = OWOC.forecast[y].summary;
      //
      //        if (m1.equals("Clear")) drawBmp("/small/sunny.bmp", 8, 192);
      //        if (m1.equals("Thunderstorm")) drawBmp("/small/tstorms.bmp", 8, 192);
      //        if (m1.equals("Drizzle")) drawBmp("/small/flurries.bmp", 8, 192);
      //        if (m1.equals("Snow")) drawBmp("/small/snow.bmp", 8, 192);
      //
      //        if (m2.equals("light rain"))  drawBmp("/small/chanceflurries.bmp",  8, 192);
      //        if (m2.equals("moderate rain"))  drawBmp("/small/chancerain.bmp",  8, 192);
      //        if (m2.equals("heavy intensity rain"))  drawBmp("/small/rain.bmp",  8, 192);
      //        if (m2.equals("very heavy rain"))  drawBmp("/small/rain.bmp",  8, 192);
      //        if (m2.equals("extreme rain"))  drawBmp("/small/rain.bmp",  8, 192);
      //        if (m2.equals("freezing rain"))  drawBmp("/small/sleet.bmp",  8, 192);
      //        if (m2.equals("light intensity shower rain"))  drawBmp("/small/rain.bmp",  8, 192);
      //        if (m2.equals("shower rain"))  drawBmp("/small/rain.bmp",  8, 192);
      //        if (m2.equals("heavy intensity shower rain"))  drawBmp("/small/rain.bmp",  8, 192);
      //        if (m2.equals("ragged shower rain"))  drawBmp("/small/rain.bmp",  8, 192);
      //
      //        if (m2.equals("few clouds"))  drawBmp("/small/partlycloudy.bmp",  8, 192);
      //        if (m2.equals("scattered clouds"))  drawBmp("/small/mostlysunny.bmp",  8, 192);
      //        if (m2.equals("broken clouds"))  drawBmp("/small/mostlycloudy.bmp",  8, 192);
      //        if (m2.equals("overcast clouds"))  drawBmp("/small/cloudy.bmp",  8, 192);
      //
      //        tft.setTextSize(1);
      //        tft.setFreeFont(FSS9);                 // Select the font
      //        tft.setTextColor(TFT_CYAN, TFT_BLACK);
      //        tft.setCursor(8, 195);
      //        tft.setTextColor(TFT_WHITE, TFT_BLACK);
      //        tft.print(OWOC.short_names[day_of_week]);
      //
      //        tft.setCursor(8, 250);
      //        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      //        tft.print(OWOC.forecast[y].temperatureHigh, 0);
      //        tft.print((char)247);
      //        tft.print(F("C"));
      //
      //      }

      //Dados do próximo dia, contando da data atual
      if (y == 1) {

        String m1 = OWOC.forecast[y].main;
        String m2 = OWOC.forecast[y].summary;

        //Desenhando os icones de acordo com a previsão do Tempo:
        if (m1.equals("Clear")) drawBmp("/small/sunny.bmp", 8, 192);
        if (m1.equals("Thunderstorm")) drawBmp("/small/tstorms.bmp", 8, 192);
        if (m1.equals("Drizzle")) drawBmp("/small/flurries.bmp", 8, 192);
        if (m1.equals("Snow")) drawBmp("/small/snow.bmp", 8, 192);

        if (m2.equals("light rain"))  drawBmp("/small/chanceflurries.bmp",  8, 192);
        if (m2.equals("moderate rain"))  drawBmp("/small/chancerain.bmp",  8, 192);
        if (m2.equals("heavy intensity rain"))  drawBmp("/small/rain.bmp",  8, 192);
        if (m2.equals("very heavy rain"))  drawBmp("/small/rain.bmp",  8, 192);
        if (m2.equals("extreme rain"))  drawBmp("/small/rain.bmp",  8, 192);
        if (m2.equals("freezing rain"))  drawBmp("/small/sleet.bmp",  8, 192);
        if (m2.equals("light intensity shower rain"))  drawBmp("/small/rain.bmp",  8, 192);
        if (m2.equals("shower rain"))  drawBmp("/small/rain.bmp",  8, 192);
        if (m2.equals("heavy intensity shower rain"))  drawBmp("/small/rain.bmp",  8, 192);
        if (m2.equals("ragged shower rain"))  drawBmp("/small/rain.bmp",  8, 192);

        if (m2.equals("few clouds"))  drawBmp("/small/partlycloudy.bmp",  8, 192);
        if (m2.equals("scattered clouds"))  drawBmp("/small/mostlysunny.bmp",  8, 192);
        if (m2.equals("broken clouds"))  drawBmp("/small/mostlycloudy.bmp",  8, 192);
        if (m2.equals("overcast clouds"))  drawBmp("/small/cloudy.bmp",  8, 192);


        tft.setTextSize(1);
        tft.setFreeFont(FSS9);                 // Select the font
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(8, 195);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print(OWOC.short_names[day_of_week]);

        tft.setCursor(8, 250);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.print(OWOC.forecast[y].temperatureHigh, 0);
        tft.print((char)247);
        tft.print(F("C"));

      }

      //Dados do 2° dia, contando da data atual
      if (y == 2) {

        String m1 = OWOC.forecast[y].main;
        String m2 = OWOC.forecast[y].summary;

        //Desenhando os icones de acordo com a previsão do Tempo:
        if (m1.equals("Clear")) drawBmp("/small/sunny.bmp", 66, 192);
        if (m1.equals("Thunderstorm")) drawBmp("/small/tstorms.bmp", 66, 192);
        if (m1.equals("Drizzle")) drawBmp("/small/flurries.bmp", 66, 192);
        if (m1.equals("Snow")) drawBmp("/small/snow.bmp", 66, 192);

        if (m2.equals("light rain"))  drawBmp("/small/chanceflurries.bmp",  66, 192);
        if (m2.equals("moderate rain"))  drawBmp("/small/chancerain.bmp",  66, 192);
        if (m2.equals("heavy intensity rain"))  drawBmp("/small/rain.bmp",  66, 192);
        if (m2.equals("very heavy rain"))  drawBmp("/small/rain.bmp",  66, 192);
        if (m2.equals("extreme rain"))  drawBmp("/small/rain.bmp",  66, 192);
        if (m2.equals("freezing rain"))  drawBmp("/small/sleet.bmp",  66, 192);
        if (m2.equals("light intensity shower rain"))  drawBmp("/small/rain.bmp",  66, 192);
        if (m2.equals("shower rain"))  drawBmp("/small/rain.bmp",  66, 192);
        if (m2.equals("heavy intensity shower rain"))  drawBmp("/small/rain.bmp",  66, 192);
        if (m2.equals("ragged shower rain"))  drawBmp("/small/rain.bmp",  66, 192);

        if (m2.equals("few clouds"))  drawBmp("/small/partlycloudy.bmp",  66, 192);
        if (m2.equals("scattered clouds"))  drawBmp("/small/mostlysunny.bmp",  66, 192);
        if (m2.equals("broken clouds"))  drawBmp("/small/mostlycloudy.bmp",  66, 192);
        if (m2.equals("overcast clouds"))  drawBmp("/small/cloudy.bmp",  66, 192);

        tft.setTextSize(1);
        tft.setFreeFont(FSS9);                 // Select the font
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(71, 195);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print(OWOC.short_names[day_of_week]);

        tft.setCursor(71, 250);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.print(OWOC.forecast[y].temperatureHigh, 0);
        tft.print((char)247);
        tft.print(F("C"));

      }

      //Dados do 3° dia, contando da data atual
      if (y == 3) {

        String m1 = OWOC.forecast[y].main;
        String m2 = OWOC.forecast[y].summary;

        //Desenhando os icones de acordo com a previsão do Tempo:
        if (m1.equals("Clear")) drawBmp("/small/sunny.bmp", 124, 192);
        if (m1.equals("Thunderstorm")) drawBmp("/small/tstorms.bmp", 124, 192);
        if (m1.equals("Drizzle")) drawBmp("/small/flurries.bmp", 124, 192);
        if (m1.equals("Snow")) drawBmp("/small/snow.bmp", 124, 192);

        if (m2.equals("light rain"))  drawBmp("/small/chanceflurries.bmp",  124, 192);
        if (m2.equals("moderate rain"))  drawBmp("/small/chancerain.bmp",  124, 192);
        if (m2.equals("heavy intensity rain"))  drawBmp("/small/rain.bmp",  124, 192);
        if (m2.equals("very heavy rain"))  drawBmp("/small/rain.bmp",  124, 192);
        if (m2.equals("extreme rain"))  drawBmp("/small/rain.bmp",  124, 192);
        if (m2.equals("freezing rain"))  drawBmp("/small/sleet.bmp",  124, 192);
        if (m2.equals("light intensity shower rain"))  drawBmp("/small/rain.bmp",  124, 192);
        if (m2.equals("shower rain"))  drawBmp("/small/rain.bmp",  124, 192);
        if (m2.equals("heavy intensity shower rain"))  drawBmp("/small/rain.bmp",  124, 192);
        if (m2.equals("ragged shower rain"))  drawBmp("/small/rain.bmp",  124, 192);

        if (m2.equals("few clouds"))  drawBmp("/small/partlycloudy.bmp",  124, 192);
        if (m2.equals("scattered clouds"))  drawBmp("/small/mostlysunny.bmp",  124, 192);
        if (m2.equals("broken clouds"))  drawBmp("/small/mostlycloudy.bmp",  124, 192);
        if (m2.equals("overcast clouds"))  drawBmp("/small/cloudy.bmp",  124, 192);

        tft.setTextSize(1);
        tft.setFreeFont(FSS9);                 // Select the font
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(129, 195);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print(OWOC.short_names[day_of_week]);

        tft.setCursor(129, 250);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.print(OWOC.forecast[y].temperatureHigh, 0);
        tft.print((char)247);
        tft.print(F("C"));

      }

      //Dados do 4° dia, contando da data atual
      if (y == 4) {

        String m1 = OWOC.forecast[y].main;
        String m2 = OWOC.forecast[y].summary;

        //Desenhando os icones de acordo com a previsão do Tempo:
        if (m1.equals("Clear")) drawBmp("/small/sunny.bmp", 182, 192);
        if (m1.equals("Thunderstorm")) drawBmp("/small/tstorms.bmp", 182, 192);
        if (m1.equals("Drizzle")) drawBmp("/small/flurries.bmp", 182, 192);
        if (m1.equals("Snow")) drawBmp("/small/snow.bmp", 182, 192);

        if (m2.equals("light rain"))  drawBmp("/small/chanceflurries.bmp",  182, 192);
        if (m2.equals("moderate rain"))  drawBmp("/small/chancerain.bmp",  182, 192);
        if (m2.equals("heavy intensity rain"))  drawBmp("/small/rain.bmp",  182, 192);
        if (m2.equals("very heavy rain"))  drawBmp("/small/rain.bmp",  182, 192);
        if (m2.equals("extreme rain"))  drawBmp("/small/rain.bmp",  182, 192);
        if (m2.equals("freezing rain"))  drawBmp("/small/sleet.bmp",  182, 192);
        if (m2.equals("light intensity shower rain"))  drawBmp("/small/rain.bmp",  182, 192);
        if (m2.equals("shower rain"))  drawBmp("/small/rain.bmp",  182, 192);
        if (m2.equals("heavy intensity shower rain"))  drawBmp("/small/rain.bmp",  182, 192);
        if (m2.equals("ragged shower rain"))  drawBmp("/small/rain.bmp",  182, 192);

        if (m2.equals("few clouds"))  drawBmp("/small/partlycloudy.bmp",  182, 192);
        if (m2.equals("scattered clouds"))  drawBmp("/small/mostlysunny.bmp",  182, 192);
        if (m2.equals("broken clouds"))  drawBmp("/small/mostlycloudy.bmp",  182, 192);
        if (m2.equals("overcast clouds"))  drawBmp("/small/cloudy.bmp",  182, 192);

        tft.setTextSize(1);
        tft.setFreeFont(FSS9);                 // Select the font
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(187, 195);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print(OWOC.short_names[day_of_week]);

        tft.setCursor(187, 250);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.print(OWOC.forecast[y].temperatureHigh, 0);
        tft.print((char)247);
        tft.print(F("C"));

      }

      delay(100);
    }


  } else {
    Serial.print("Falha!");

  }

}
