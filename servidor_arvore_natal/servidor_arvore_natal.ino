#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "FS.h"
#include <SoftwareSerial.h>
#include <WiFiManager.h>
#include <FastLED.h>

#ifdef ESP8266
#define min _min
#define max _max
#endif

//#define power_good xx
#define power_on 14 // D5
#define DataPin 5 // D1
#define syncPin 4 // D2 // consome 56uA
#define rele 12 // D6

#define led_Wifi 2 // D4
#define led_geral 16 // D0

SoftwareSerial mp3Serial(13, 15); // TX = D8 e RX = D7

#define NL 250
#define NE 41
#define SampleRate 30

Adafruit_NeoPixel leds(NL + NE, DataPin, NEO_RGB + NEO_KHZ800);
const uint32_t red = leds.Color(255, 0, 0);
const uint32_t green = leds.Color(0, 255, 0);
const uint32_t blue = leds.Color(0, 0, 255);
const uint32_t orange = leds.Color(255, 165, 0);
const uint32_t black = leds.Color(0, 0, 0);
const uint32_t white = leds.Color(255, 255, 255);


#define sparkling_factor 17
#define mp3_initial_volume 15
#define n_segue_musica 4
#define tempo_alternar 1000
#define tempo_dissolve_2 2000
#define tempo_fonte_off 10
#define brilho_inicial 0x60
#define fator_rainbow_wave 0x11
#define N_shimmer 20
#define tempo_teste_fonte 3
#define tempo_altofalantes_off 20

#define SampleTime 1000/SampleRate

// Tabelas de relação entre efeito e string
const String tabela1[] = {"off", "uma", "duas", "tres", "arco", "onda", "retro", "alt", "degrade", "segue", "spark", "dissol", "fade", "shim"};
const String tabela2[] = {"off", "uma", "duas", "tres", "alt", "fade", "rgb", "arco", "circle", "rad", "onda"};

// Definição dos efeitos geral
enum Efeito : byte {
  nenhum = 0,
  uma,
  duas,
  tres,
  rainbow,
  rainbow_wave,
  retro,
  alternar,
  degrade,
  segue,
  sparkling,
  dissolve,
  fade,
  shimmer
};

// Definição dos efeitos estrela
enum Estrela : byte {
  e_nenhum = 0,
  e_uma,
  e_duas,
  e_tres,
  e_alternar,
  e_fade,
  e_rgb,
  e_rainbow,
  e_circle,
  e_radial,
  e_radial_rb
};

// MP3 commands
/************ Command byte **************************/
#define CMD_NEXT_SONG          0X01  // Play next song.
#define CMD_PREV_SONG          0X02  // Play previous song.
#define CMD_PLAY_W_INDEX       0X03
#define CMD_VOLUME_UP          0X04
#define CMD_VOLUME_DOWN        0X05
#define CMD_SET_VOLUME         0X06
#define CMD_SET_EUQALIZER      0X07  // Set Equalizer (0=normal, 1=pop, 2=rock, 3=jazz, 4=classic, 5=base)
#define CMD_SNG_CYCL_PLAY      0X08  // Single Cycle Play.
#define CMD_SEL_DEV            0X09  // (TF card is 0x02)
#define CMD_SLEEP_MODE         0X0A
#define CMD_WAKE_UP            0X0B
#define CMD_RESET              0X0C
#define CMD_PLAY               0X0D
#define CMD_PAUSE              0X0E
#define CMD_PLAY_FOLDER_FILE   0X0F
#define CMD_STOP               0X16  // Stop playing continuously. 
#define CMD_FOLDER_CYCLE       0X17  // (data is the folder index)
#define CMD_SHUFFLE_PLAY       0x18  // (data 0 to enable, 1 to disable)
#define CMD_SET_SNGL_CYCL      0X19  // Set single cycle. (repeat loop play of current file / data 0 to enable and 1 to disable)
#define CMD_SET_DAC            0X1A  // Equivalent to mute
#define DAC_ON                 0X00
#define DAC_OFF                0X01
#define CMD_PLAY_W_VOL         0X22  // (data hi byte is track index and low bute is volume)
#define CMD_SHUFFLE_FOLDER     0x28  // (data high byte is folder index)
// Querys
#define QUERY_STATUS           0x42
#define QUERY_VOLUME           0x43
#define QUERY_EQUALIZER        0x44
#define QUERY_TOT_TRACKS       0x48
#define QUERY_PLAYING          0x4c
#define QUERY_FLDR_TRACKS      0x4e
#define QUERY_FLDR_COUNT       0x4f
// Respostas
#define ANSWER_ERROR_NOT_FOUND 0x40
#define ANSWER_ACK_OK          0x41
#define ANSWER_STATUS          0x42  // Data high byte is file store (2 for tf). Low byte 0=stopped, 1=play, 2=paused.
#define ANSWER_VOLUME          0x43
#define ANSWER_EQUALIZER       0x44
#define ANSWER_TOT_FILES       0x48
#define ANSWER_PLAYING         0x4c
#define ANSWER_FOLDER_FILES    0x4e
#define ANSWER_TOT_FILES       0x4f
// Mensagens não solicitadas
#define TF_INSERTED            0x3a
#define TF_REMOVED             0x3b
#define FILE_END               0x3d  // (data is the file index)
#define INITIALIZED            0x3f
/************ Opitons **************************/
#define DEV_TF                 0X02

#define CMD_PLAYING_N         0x4C


// Erros
enum Erros : byte {
  Mp3,
  Fonte,
  Wifi,
  FS
};

uint32_t cores_retro[] = {      red, orange, green, blue};
unsigned int tempos_retro[] = {2000,   1887,  2576, 2211};
#define n_retro 4

uint32_t cores[2][3][2];
Efeito efeito;
Estrela estrela;
bool nef, nes, retorno_suave_dissolve, radial_in, aux_bool[2], musica_on;
volatile bool syncbool;
unsigned long tref, t_off, aux_tref[2], t_off_fte;
byte tempo_arvore, tempo_estrela, buf[10], aux_var, volume_mp3;
unsigned int i, shimmer_num[N_shimmer], aux_int, aux_int_temp, di[NL];
int temp_int;
float shimmer_percent[N_shimmer];
const byte NLE = (NE % 2) ? NE / 5 : (NE - 1) / 5;

void(* resetFunc) (void) = 0; // função de reset

ESP8266WebServer server(80);

String lerArquivo(String filename, String msgDefault) {
  String message = msgDefault;
  if (SPIFFS.exists("/" + filename)) {
    File file = SPIFFS.open("/" + filename, "r");
    message = file.readString();
    file.close();
  }
  return message;
}

String cor2string(uint32_t cor) {
  String r, g, b;
  r = String((cor >> 16) & 0xff, HEX);
  g = String((cor >> 8) & 0xff, HEX);
  b = String(cor & 0xff, HEX);
  if (r.length() == 1) r = "0" + r;
  if (g.length() == 1) g = "0" + g;
  if (b.length() == 1) b = "0" + b;
  return r + g + b;
}

uint32_t string2cor(String s) {
  uint32_t cc = strtol(&s[0], NULL, 16);
  return cc;
}

String statusTotal() {
  String ef[] = {tabela1[efeito], tabela2[estrela]};
  if (efeito == dissolve) if (retorno_suave_dissolve) ef[1] += "ve";
  if (estrela == e_radial) ef[1] += (radial_in) ? "in" : "out";
  String message = "{\"brilho\": ";
  message += leds.getBrightness();
  message += ",\"arvore\": {\"efeito\": \"";
  message += ef[0];
  message += "\",\"cores\": {\"cor1\": \"";
  message += cor2string(cores[0][0][1]);
  message += "\",\"cor2\": \"";
  message += cor2string(cores[0][1][1]);
  message += "\",\"cor3\": \"";
  message += cor2string(cores[0][2][1]);
  message += "\"},\"tempo\": ";
  message += tempo_arvore;
  message += "},\"estrela\": {\"efeito\": \"";
  message += ef[1];
  message += "\",\"cores\": {\"cor1\": \"";
  message += cor2string(cores[1][0][1]);
  message += "\",\"cor2\": \"";
  message += cor2string(cores[1][1][1]);
  message += "\"},\"tempo\": ";
  message += tempo_estrela;
  message += "},\"mp3\": {\"volume\": ";
  message += volume_mp3;
  message += ",\"playing\": ";
  message += mp3Query(QUERY_STATUS);
  message += "},\"ip\":\"";
  message += WiFi.localIP().toString();
  message += "\"}";
  return message;
}

String processarComandos() {
  bool tt[2];
  tt[0] = false;
  tt[1] = false;
  String message = "{";
  for (byte a = 0; a < server.args(); a++) {
    if (server.argName(a).startsWith("cor")) {
      switch (server.argName(a).charAt(3)) {
        case '1':
          cores[0][0][1] = string2cor(server.arg(a));
          cores[0][0][0] = cores[0][0][1];
          //message += "Cor1 alterada para #";
          //message += server.arg(a);
          //message += "\n";
          break;
        case '2':
          cores[0][1][1] = string2cor(server.arg(a));
          cores[0][1][0] = cores[0][1][1];
          //message += "Cor2 alterada para #";
          //message += server.arg(a);
          //message += "\n";
          break;
        case '3':
          cores[0][2][1] = string2cor(server.arg(a));
          cores[0][2][0] = cores[0][2][1];
          //message += "Cor3 alterada para #";
          //message += server.arg(a);
          //message += "\n";
          break;
        case '4':
          cores[1][0][1] = string2cor(server.arg(a));
          cores[1][0][0] = cores[1][0][1] & 0xffL;
          cores[1][0][0] += (cores[1][0][1] >> 8) & 0xff00L;
          cores[1][0][0] += (cores[1][0][1] << 8) & 0xff0000L;
          //message += "Cor4 alterada para #";
          //message += server.arg(a);
          //message += "\n";
          break;
        case '5':
          cores[1][1][1] = string2cor(server.arg(a));
          cores[1][1][0] = cores[1][1][1] & 0xffL;
          cores[1][1][0] += (cores[1][1][1] >> 8) & 0xff00L;
          cores[1][1][0] += (cores[1][1][1] << 8) & 0xff0000L;
          //message += "Cor5 alterada para #";
          //message += server.arg(a);
          //message += "\n";
          break;
      }
    } else if (server.argName(a).startsWith("tempo_")) {
      if (server.argName(a).substring(6) == "arvore") {
        tempo_arvore = server.arg(a).toInt();
        tempo_arvore = max(1, tempo_arvore);
        tempo_arvore = min(600, tempo_arvore);
        nef = true;
        //message += "Tempo da árvore alterado para: ";
        //message += tempo_arvore;
        //message += "\n";
      } else if (server.argName(a).substring(6) == "estrela") {
        tempo_estrela = server.arg(a).toInt();
        tempo_estrela = max(1, tempo_estrela);
        tempo_estrela = min(600, tempo_estrela);
        nes = true;
        //message += "Tempo da estrela alterado para: ";
        //message += tempo_estrela;
        //message += "\n";
      }
    } else if (server.argName(a) == "mp3") {
      byte cc = 0;
      if (server.arg(a) == "play") {
        cc = CMD_PLAY;
      } else if (server.arg(a) == "pause") {
        cc = CMD_PAUSE;
      } else if (server.arg(a) == "stop") {
        cc = CMD_STOP;
      } else if (server.arg(a) == "vup") {
        cc = CMD_VOLUME_UP;
      } else if (server.arg(a) == "vdown") {
        cc = CMD_VOLUME_DOWN;
      } else if (server.arg(a) == "next") {
        cc = CMD_NEXT_SONG;
      } else if (server.arg(a) == "prev") {
        cc = CMD_PREV_SONG;
      }
      if (cc) mp3(cc);
    } else if (server.argName(a) == "brilho") {
      temp_int = server.arg(a).toInt();
      temp_int = max(0, temp_int);
      temp_int = min(255, temp_int);
      leds.setBrightness(temp_int);
      message += "\"brilho\":";
      message += leds.getBrightness();
      message += ",";
    } else if (server.argName(a) == "volume") {
      temp_int = server.arg(a).toInt();
      temp_int = max(0, temp_int);
      temp_int = min(30, temp_int);
      mp3AjustaVolume(temp_int);
      if (volume_mp3 != temp_int) volume_mp3 = 0;
      message += "\"volume\":";
      message += volume_mp3;
      message += ",";
    } else if (server.argName(a) == "efeito_arvore") {
      tt[0] = true;
      if (server.arg(a).startsWith("dissol")) {
        efeito = dissolve;
        retorno_suave_dissolve = server.arg(a).startsWith("dissolve");
      } else {
        for (i = 0; i < (sizeof(tabela1) / sizeof(tabela1[0])); i++) {
          if (tabela1[i] == server.arg(a)) {
            efeito = (Efeito)i;
            break;
          }
        }
      }
      //message += "\"efeito_arvore\":\"";
      //message += server.arg(a);
      //message += "\",";
    } else if (server.argName(a) == "efeito_estrela") {
      tt[1] = true;
      if (server.arg(a).startsWith("rad")) {
        estrela = e_radial;
        radial_in = (server.arg(a).substring(3) == "in");
      } else {
        for (i = 0; i < (sizeof(tabela2) / sizeof(tabela2[0])); i++) {
          if (tabela2[i] == server.arg(a)) {
            estrela = (Estrela)i;
            break;
          }
        }
      }
      //message += "\"efeito_estrela\":\"";
      //message += server.arg(a);
      //message += "\",";
    } else if (server.argName(a) == "musica") {
      temp_int = server.arg(a).toInt();
      temp_int = max(0, temp_int);
      temp_int = min(255, temp_int);
      if (mp3Toca((byte)temp_int)) {
        message += "\"musica\":";
        message += temp_int;
        message += ",";
      }
    }
  }
  message += "}";
  if (tt[0]) recebe_arvore();
  if (tt[1]) recebe_estrela();
  return message;
}

void ICACHE_RAM_ATTR syncfunc() {
  syncbool = true;
}

bool mp3AjustaVolume(byte vol) {
  for (byte c = 0; c < 5; c++) {
    mp3(CMD_SET_VOLUME, vol, true);
    delay(10);
    if (mp3Query(QUERY_VOLUME) == vol) return true;
    delay(10);
  }
  return false;
}

void setup(void) {
#ifdef power_good
  pinMode(power_good, INPUT_PULLUP);
#endif
#ifdef power_on
  pinMode(power_on, OUTPUT);
  digitalWrite(power_on, LOW);
#endif
  pinMode(led_Wifi, OUTPUT);
  pinMode(led_geral, OUTPUT);
  digitalWrite(led_Wifi, 1);
  digitalWrite(led_geral, 0);
  pinMode(syncPin, INPUT_PULLUP);
  pinMode(rele, OUTPUT);
  digitalWrite(rele, 0);
  Serial.begin(115200);
  leds.begin();
  leds.clear();
  leds.show();
  leds.setBrightness(brilho_inicial);
  delay(1000);
  mp3Serial.begin(9600);
  mp3Serial.setTimeout(30);
#if defined(power_on) && defined(power_good)
  Serial.print("Testando a fonte... ");
  fonte(1);
  fonte(0);
  Serial.println("Fonte ok.");
#endif
  delay(100);
  Serial.print("Testando MP3... ");
  mp3(CMD_RESET);
  mp3Serial.readBytes(buf, 10);
  bool mp3_ok = mp3(CMD_SEL_DEV, DEV_TF, true);
  delay(200);
  mp3_ok = mp3_ok && mp3AjustaVolume(mp3_initial_volume);
  if (mp3_ok) {
    Serial.println("MP3 working ok.");
  } else {
    Serial.println("MP3 not working.");
    //erro(Mp3);
  }
  digitalWrite(led_Wifi, 0);
  WiFiManager wfm;
  wfm.autoConnect("Arvore de Natal");
  delay(300);
  if (MDNS.begin("arvoredenatal")) Serial.println("MDNS responder started");
  server.serveStatic("/favicon.png", SPIFFS, "/favicon.png");
  server.serveStatic("/help", SPIFFS, "/ajuda.html");
  server.serveStatic("/b.jpg", SPIFFS, "/b.jpg");
  server.on("/", []() {
    if (server.args()) {
      processarComandos();
      server.sendHeader("Location", "http://" + WiFi.localIP().toString() + "/");
      server.send(303);
    } else {
      server.send(200, "text/html", lerArquivo("principal.html", "Erro ao ler a página da interface gráfica."));
    }
  });
  server.on("/off", []() {
    Serial.println("Desligando tudo.");
    efeito = nenhum;
    estrela = e_nenhum;
    //nes = false;
    //nef = false;
    leds.clear();
    leds.show();
    digitalWrite(rele, 0);
    mp3(CMD_STOP);
    String html = "<html><head><meta charset=\"UTF-8\" http-equiv=\"refresh\" content=\"3;url=/\" /></head><body><h1>Desligando a árvore...</h1></body></html>";
    server.send(200, "text/html", html);
  });
  server.on("/status", []() {
    server.send(200, "text/json", statusTotal());
  });
  server.on("/comandos", []() {
    server.send(200, "text/json", processarComandos());
  });
  server.on("/musicas", []() {
    if (server.args()) {
      for (byte a = 0; a < server.args(); a++) {
        if (server.argName(a) == "json") {
          File file = SPIFFS.open("/musicas.json", "w");
          String m = server.arg(a);
          m.replace("%20", " ");
          file.print(m);
          file.close();
        }
      }
    } else {
      server.send(200, "text/json", lerArquivo("musicas.json", "Erro ao ler o arquivo de músicas."));
    }
  });
  server.onNotFound([]() {
    server.send(404, "text/html", "Essa página não existe.");
  });
  server.begin();
  Serial.println("HTTP server started");
  digitalWrite(led_Wifi, 1);
  if (SPIFFS.begin()) {
    Serial.println("File system working ok.");
  } else {
    erro(FS);
  }
  efeito = nenhum;
  estrela = e_nenhum;
  nef = false;
  nes = false;
  tempo_arvore = 1;
  tempo_estrela = 1;
  musica_on = false;
  mp3AjustaVolume(mp3_initial_volume);
  attachInterrupt(digitalPinToInterrupt(syncPin), syncfunc, FALLING);
  digitalWrite(led_geral, 1);
  t_off_fte = millis();
  tref = millis();
  t_off = millis();
}

void loop(void) {
  server.handleClient();
  MDNS.update();
  if (efeito == segue && syncbool) {
    nef = true;
    syncbool = false;
  }
#ifdef power_on
  if (!fonte() && (efeito != nenhum || estrela != e_nenhum)) fonte(1);
#endif

  if (millis() - tref >= SampleTime) {
    tref = millis();
    if ((efeito != nenhum || estrela != e_nenhum) && (nef || nes) && fonte()) {
      if (nef) {
        switch (efeito) {
          case uma:
            solido(1, false);
            nef = false;
            break;
          case duas:
            solido(2, false);
            nef = false;
            break;
          case tres:
            solido(3, false);
            nef = false;
            break;
          case degrade:
            arvore_degrade();
            break;
          case retro:
            arvore_retro();
            break;
          case rainbow:
            arvore_rainbow();
            break;
          case rainbow_wave:
            arvore_rainbow_wave();
            break;
          case sparkling:
            arvore_sparkling();
            break;
          case segue:
            arvore_segue();
            break;
          case alternar:
            arvore_alternar();
            break;
          case dissolve:
            arvore_dissolve();
            break;
          case fade:
            arvore_fade();
            break;
          case shimmer:
            arvore_shimmer();
            break;
        }
      }
      if (nes) {
        switch (estrela) {
          case e_uma:
            solido(1, true);
            nes = false;
            break;
          case e_rgb:
            solido(3, true);
            nes = false;
            break;
          case e_rainbow:
            estrela_rainbow();
            break;
          case e_alternar:
            estrela_alternar();
            break;
          case e_fade:
            estrela_fade();
            break;
          case e_circle:
            estrela_circle();
            break;
          case e_radial:
            estrela_radial();
            break;
          case e_radial_rb:
            estrela_radial_rb();
            break;
        }
      }
      leds.show();
    }
  } else if (millis() - tref > 1000000) tref = 0;

#ifdef power_on
  // Se a fonte estiver ligada por mais de 10s sem efeito, desliga a fonte
  if (!efeito && !estrela && digitalRead(power_on)) {
    if (millis() - t_off > (1000 * tempo_fonte_off)) digitalWrite(power_on, 0);
  } else t_off = millis();
#endif

  // Se a caixa de som estiver ligada por mais de 20s sem música (depois do comando STOP), desliga ela.
  if (!musica_on && digitalRead(rele)) {
    if (millis() - t_off_fte > (1000 * tempo_altofalantes_off)) digitalWrite(rele, 0);
  } else t_off_fte = millis();
}

bool fonte() {
#if defined (power_on) && defined (power_good)
  if (digitalRead(power_on)) return digitalRead(power_good);
  return false;
#elif defined (power_on)
  return digitalRead(power_on);
#else
  return true;
#endif
}

void fonte(bool estado) {
#if defined (power_on) && defined (power_good)
  bool ok = false;
  digitalWrite(power_on, estado);
  unsigned int contador = 0;
  while (contador < tempo_teste_fonte * 1000) {
    ok = (estado) ? digitalRead(power_good) : !digitalRead(power_good);
    if (ok) break;
    delay(1);
    contador++;
  }
  if (!ok) erro(Fonte);
#elif defined (power_on)
  digitalWrite(power_on, estado);
#endif
}

void erro(Erros caso) {
#ifdef power_on
  digitalWrite(power_on, LOW);
#endif
  mp3(CMD_SLEEP_MODE);
  mp3Serial.end();
  Serial.println();
  unsigned int ton, toff;
  switch (caso) {
    case Mp3:
      ton = 150;
      toff = 150;
      Serial.println("Mp3 player error.");
      break;
    case Fonte:
      ton = 900;
      toff = 100;
      Serial.println("Power Supply error.");
      break;
    case Wifi:
      ton = 50;
      toff = 950;
      Serial.println("Could not connect to wifi.");
      break;
    case FS:
      ton = 2000;
      toff = 2000;
      Serial.println("Could not initalize file system.");
      break;
  }
  leds.clear();
  leds.show();
  byte led_uso = (caso == Wifi) ? led_Wifi : led_geral;
  while (true) {
    digitalWrite(led_uso, 0);
    delay(ton);
    digitalWrite(led_uso, 1);
    delay(toff);
  }
}

bool mp3(byte comando) {
  return mp3(comando, 0, true);
}

bool mp3(byte comando, unsigned int info, bool confere) {
  while (mp3Serial.available()) {
    mp3Serial.read();
    if (!mp3Serial.available()) delay(2);
  }
  mp3Serial.write(0x7E);
  mp3Serial.write(0xFF);
  mp3Serial.write(0x06);
  mp3Serial.write(comando);
  mp3Serial.write(0x01);
  mp3Serial.write(info >> 8);
  mp3Serial.write(info & 0xFF);
  mp3Serial.write(0xEF);
  if (confere) {
    if (!mp3Serial.readBytes(buf, 10)) return false;
    if (buf[0] != 0x7E) return false;
    if (buf[1] != 0xFF) return false;
    if (buf[2] != 0x06) return false;
    if (buf[3] != 0x41) return false;
    if (buf[4]) return false;
    if (buf[5]) return false;
    if (buf[6]) return false;
    if (buf[7] != 0xFE) return false;
    if (buf[8] != 0xBA) return false;
    if (buf[9] != 0xEF) return false;
  }
  if (comando == CMD_NEXT_SONG || comando == CMD_PREV_SONG || comando == CMD_PLAY_W_INDEX || comando == CMD_PLAY || comando == CMD_PAUSE || comando == CMD_PLAY_FOLDER_FILE || comando == CMD_PLAY_W_VOL || comando == CMD_SHUFFLE_FOLDER || comando == CMD_SHUFFLE_PLAY || comando == CMD_FOLDER_CYCLE || comando == CMD_SET_SNGL_CYCL || comando == CMD_PLAYING_N) {
    musica_on = true;
    digitalWrite(rele, 1);

  } else if (comando == CMD_STOP) {
    musica_on = false;
  }
  return true;
}

byte mp3Query(byte Query) {
  byte resp = 0;
  mp3(Query, 0, false);
  if (mp3Serial.readBytes(buf, 10) == 10) {
    if (buf[3] == Query) resp = buf[6];
  }
  while (mp3Serial.readBytes(buf, 10) == 10) {
    ;
  }
  if (Query == QUERY_VOLUME) volume_mp3 = resp;
  return resp;
}

bool mp3Toca(byte musica) {
  unsigned int temporario = musica + 0x0100;
  return mp3(CMD_PLAY_FOLDER_FILE, temporario, true);
}

uint32_t colorSlope(uint32_t c1, uint32_t c2, float percentual) {
  uint32_t temp;
  byte temp_cor, x1, x2;
  x1 = (c1 >> 16) & 0xff;
  x2 = (c2 >> 16) & 0xff;
  temp_cor = byte(float(x1) + percentual * float(x2 - x1));
  temp = temp_cor;
  temp <<= 8;
  x1 = (c1 >> 8) & 0xff;
  x2 = (c2 >> 8) & 0xff;
  temp_cor = byte(float(x1) + percentual * float(x2 - x1));
  temp += temp_cor;
  temp <<= 8;
  x1 = c1 & 0xff;
  x2 = c2 & 0xff;
  temp_cor = byte(float(x1) + percentual * float(x2 - x1));
  temp += temp_cor;
  return temp;
}

void solido(byte n, bool estrela) {
  if (estrela) {
    for (i = NL; i < NL + NE; i++) leds.setPixelColor(i, cores[1][i % n][0]);
  } else {
    for (i = 0; i < NL; i++) leds.setPixelColor(i, cores[0][i % n][0]);
  }
  leds.show();
}

void swap_colors(byte linha) {
  cores[linha][2][0] = cores[linha][0][0];
  cores[linha][0][0] = cores[linha][1][0];
  cores[linha][1][0] = cores[linha][2][0];
}

uint32_t hue2rgb(byte h) {
  uint32_t cor = 0;
  CRGB corF;
  corF.setHue(h);
  cor = corF.r;
  cor <<= 8;
  cor += corF.g;
  cor <<= 8;
  cor += corF.b;
  return cor;
}

void arvore_sparkling() {
  for (i = 0; i < NL; i++) leds.setPixelColor(i, !random(sparkling_factor) ? cores[0][0][0] : black);
}

void arvore_degrade() {
  for (i = 0; i < NL; i++) {
    cores[0][2][0] = colorSlope(cores[0][1][0], cores[0][0][0], float(i) / float(NL));
    leds.setPixelColor(i, cores[0][2][0]);
  }
  nef = false;
}

void arvore_retro() {
  bool e[n_retro];
  byte j;
  for (i = 0; i < n_retro; i++) e[i] = (tref % tempos_retro[i] < tempos_retro[i] / 2);
  for (i = 0; i < NL; i++) {
    j = i % n_retro;
    leds.setPixelColor(i, (e[j]) ? black : cores_retro[j]);
  }
}

void arvore_rainbow() {
  float a = float(tref) / (float(tempo_arvore) * 3.9);
  uint32_t b = hue2rgb((byte)(a));
  for (i = 0; i < NL; i++) leds.setPixelColor(i, b);
}

void arvore_rainbow_wave() {
  float a = float(tref) / (float(tempo_arvore) * 3.9);
  byte b = (byte)(a);
  a = 256.0 / float(NL);
  if (fator_rainbow_wave & 0xF0) {
    a *= float(fator_rainbow_wave >> 4);
  } else {
    a /= float(fator_rainbow_wave & 0x0F);
  }
  for (i = 0; i < NL; i++) leds.setPixelColor(i, hue2rgb(b + (byte)(a * float(i))));
}

void arvore_segue() {
  uint32_t c1, c2;
  byte c;
  aux_var = ++aux_var % n_segue_musica;
  for (i = 0; i < NL; i++) {
    leds.setPixelColor(i, cores[0][0][0]);
    if ((i % n_segue_musica) == aux_var) {
      c1 = leds.getPixelColor(i);
      c2 = 0;
      c = (c1 >> 19) & 0x1f;
      c2 += c;
      c2 <<= 8;
      c = (c1 >> 11) & 0x1f;
      c2 += c;
      c2 <<= 8;
      c = (c1 >> 3) & 0x1f;
      c2 += c;
      leds.setPixelColor(i, c2);
    }
  }
  nef = false;
}

void arvore_alternar() {
  aux_bool[0] = (tref / tempo_alternar) & 1;
  for (i = 0; i < NL; i++) leds.setPixelColor(i, (aux_bool[0] ^ (i & 1)) ? black : cores[0][aux_bool[0]][0]);
}

void arvore_fade() {
  if (tref - aux_tref[0] <= (tempo_arvore * 1200)) {
    cores[0][2][0] = colorSlope(cores[0][0][0], cores[0][1][0], min(1.0, float(tref - aux_tref[0]) / float(tempo_arvore * 1000)));
    for (i = 0; i < NL; i++) leds.setPixelColor(i, cores[0][2][0]);
  } else {
    aux_tref[0] = tref;
    swap_colors(0);
  }
}

void arvore_shimmer() {
  for (i = 0; i < N_shimmer; i++) {
    if (!shimmer_num[i] && !random(10)) {
      shimmer_num[i] = random(NL + 1);
      shimmer_percent[i] = 0.0;
    }
    if (shimmer_num[i]) {
      shimmer_percent[i] += (i & 1) ? 0.04 : 0.02;
      if (shimmer_percent[i] > 2.0) {
        leds.setPixelColor(shimmer_num[i] - 1, cores[0][0][0]);
        shimmer_num[i] = 0;
      } else {
        leds.setPixelColor(shimmer_num[i] - 1, colorSlope(cores[0][1][0], cores[0][0][0], (shimmer_percent[i] > 1.0) ? (float(shimmer_percent[i]) - 1.0) : (1.0 - float(shimmer_percent[i]))));
      }
    }
  }
}

void montaVetor() {
  aux_int = 0;
  unsigned int r, ind;
  for (i = 0; i < NL; i++) di[i] = i;
  for (i = NL; i > 1; i--) {
    ind = random(i);
    r = di[i - 1];
    di[i - 1] = di[ind];
    di[ind] = r;
  }
}

void arvore_dissolve() {
  if (aux_int < NL) {
    while (tref - aux_tref[0] >= aux_int_temp) {
      leds.setPixelColor(di[aux_int++], cores[0][1][0]);
      aux_tref[0] += aux_int_temp;
      if (aux_int >= NL) break;
    }
  } else {
    if (tref - aux_tref[0] > tempo_dissolve_2) {
      montaVetor();
      if (retorno_suave_dissolve) {
        swap_colors(0);
      } else {
        solido(1, false);
      }
      aux_tref[0] = tref;
    }
  }
}

void estrela_rainbow() {
  uint32_t ct;
  if (efeito != rainbow) {
    float a = float(tref) / (float(tempo_estrela) * 3.9);
    ct = hue2rgb((byte)(a));
    for (i = NL; i < NL + NE; i++) leds.setPixelColor(i, ct);
  } else {
    uint32_t c1, c2;
    ct = leds.getPixelColor(0);
    c1 = (ct & 0xff00L) << 8;
    c2 = (ct >> 8) & 0xff00L;
    ct &= 0x0000ffL;
    ct += c1 + c2;
    for (i = NL; i < NL + NE; i++) leds.setPixelColor(i, ct);
  }
}

void estrela_alternar() {
  aux_bool[1] = (efeito == alternar) ? aux_bool[0] : (tref / tempo_alternar) & 1;
  for (i = NL; i < NL + NE; i++) leds.setPixelColor(i, cores[1][aux_bool[1]][0]);
}

void estrela_fade() {
  float a;
  byte t_fade = (efeito == fade) ? tempo_arvore : tempo_estrela;
  if (tref - aux_tref[1] <= (t_fade * 1200)) {
    a = float(tref - aux_tref[1]);
    a /= float(t_fade * 1000);
    if (a > 1.0) a = 1.0;
    cores[1][2][0] = colorSlope(cores[1][0][0], cores[1][1][0], a);
    for (i = NL; i < NL + NE; i++) leds.setPixelColor(i, cores[1][2][0]);
  } else {
    aux_tref[1] = tref;
    swap_colors(1);
  }
}

void estrela_circle() {
  leds.setPixelColor(NL, white);
  uint32_t ct;
  float a = float(tref) / (float(tempo_estrela) * 3.9);
  byte b = (byte)(a);
  for (byte j = 0; j < 5; j++) {
    ct = hue2rgb(b + (51 * j));
    for (i = 0; i < NLE; i++) leds.setPixelColor(NL + j * NLE + i + 1, ct);
  }
}

void estrela_radial() {
  float a =  float(NLE) * float(tref) / (1000.0 * float(tempo_estrela));
  unsigned long b = ((unsigned long)(a)) % NLE;
  for (i = NL; i < NL + NE; i++) leds.setPixelColor(i, cores[1][0][0]);
  unsigned int aux = radial_in ? b : NLE - b;
  if (aux) {
    for (i = 0; i < 5; i++) leds.setPixelColor(NL + aux + i * NLE, cores[1][1][0]);
  } else leds.setPixelColor(NL, cores[1][1][0]);
  if (!radial_in) {
    if (aux == 1) {
      leds.setPixelColor(NL, cores[1][2][0]);
    } else if (!aux) {
      for (i = 1; i < 6; i++) leds.setPixelColor(NL - 1 + i * NLE, cores[1][2][0]);
    } else {
      for (i = 0; i < 5; i++) leds.setPixelColor(NL + aux - 1 + i * NLE, cores[1][2][0]);
    }
  } else {
    if (aux == NLE) {
      leds.setPixelColor(NL, cores[1][2][0]);
    } else {
      for (i = 0; i < 5; i++) leds.setPixelColor(NL + aux + 1 + i * NLE, cores[1][2][0]);
    }
  }
}

void estrela_radial_rb() {
  float a;
  if (efeito == rainbow) {
    a = float(tref) / (float(tempo_arvore) * 3.9);
  } else {
    a = float(tref) / (float(tempo_estrela) * 3.9);
  }
  byte b = (byte)(a);
  byte nr = NLE + 1;
  byte j, h, h2;
  a = 256.0 / float(nr);
  for (j = 0; j < nr; j++) {
    h2 = (!radial_in) ? (nr - j) : j;
    if (h2) {
      h = b + (byte)(a * h2);
      uint32_t ch = hue2rgb(h);
      for (i = 0; i < 5; i++) leds.setPixelColor(NL + j + (i * NLE), ch);
    } else leds.setPixelColor(NL, hue2rgb(b));
  }
}

void recebe_arvore() {
  nef = true;
  switch (efeito) {
    case nenhum:
      cores[0][0][0] = black;
      solido(1, false);
      //nef = false;
      break;
    case uma:
    case duas:
    case tres:
      solido(efeito, false);
      //nef = false;
      break;
    case dissolve:
      solido(1, false);
      montaVetor();
      aux_int_temp = tempo_arvore * 1000 / NL;
      aux_tref[0] = millis();
      break;
    case fade:
      solido(1, false);
      aux_tref[0] = millis();
      break;
    case shimmer:
      solido(1, false);
      for (i = 0; i < N_shimmer; i++) shimmer_num[i] = 0;
      break;
  }
}

void recebe_estrela() {
  nes = true;
  switch (estrela) {
    case e_nenhum:
      cores[1][0][0] = black;
      solido(1, true);
      //nes = false;
      break;
    case e_uma:
    case e_duas:
    case e_tres:
      solido(estrela, true);
      //nes = false;
      break;
    case e_fade:
      solido(1, true);
      break;
    case e_rgb:
      cores[1][0][0] = green;
      cores[1][1][0] = red;
      cores[1][2][0] = blue;
      break;
    case e_radial:
      cores[1][2][0] = colorSlope(cores[1][0][0], cores[1][1][0], 0.5);
      break;
  }
}
