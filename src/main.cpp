#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <LiquidCrystal_I2C.h>
#include "WiFiManager.h"
#include "MqttManager.h"
#include "DebugManager.h"

/*Autores: Davi Nogueira, Gabriel Expindola, Gabriel Bocchino, Heitor Barreto
Programa: Projeto Mesa DK 
Descrição: Projeto com LED, LCD, Lampada. 
Data: 06/05/2026
Versão:1.0
*/

const int PINO_LAMPADA = 3;
const int PINO_LED_RGB = 48;
const int QUANTIDADE_LEDS = 1;

const char TOPICO_COMANDO[] = "senai134/davinogueira/esp32/comando";

LiquidCrystal_I2C lcd(0x27, 20, 4);

void tratarMensagemRecebida(const char *topico, const String &mensagem);
void configurarLedRGB();
void tratarJsonComando(const String &mensagem);
void alterarCorLedRGB(int vermelho, int verde, int azul);

// LCD
void configurarLCD();
void atualizarLCD();
String tempoFormatado();

// Variáveis
float temperatura = 25.0;
String estadoMaquina = "DESLIGADA";
String modoOperacao = "LOCAL";

unsigned long tempoInicio = 0;
bool maquinaLigada = false;

Adafruit_NeoPixel ledRGB(
    QUANTIDADE_LEDS,
    PINO_LED_RGB,
    NEO_GRB + NEO_KHZ800
);

void setup()
{
  lcd.init();
  lcd.backlight();

  pinMode(PINO_LAMPADA, OUTPUT);

  configurarDebug();
  configurarLedRGB();
  conectarWifi();
  configurarMQTT();
  registrarCallbackMensagem(tratarMensagemRecebida);
  conectarMQTT();

  configurarLCD();
  atualizarLCD();
}

void loop()
{
  garantirWiFiConectado();
  garantirMQTTConectado();
  loopMQTT();

  // Atualiza LCD a cada 1 segundo
  static unsigned long ultimoLCD = 0;

  if (millis() - ultimoLCD > 1000)
  {
    atualizarLCD();
    ultimoLCD = millis();
  }
}

void tratarMensagemRecebida(const char *topico, const String &mensagem)
{
  if (topico == nullptr)
  {
    debugErro("Tópico MQTT inválido");
    return;
  }

  if (strcmp(topico, TOPICO_COMANDO) == 0)
  {
    tratarJsonComando(mensagem);
    return;
  }
}

void configurarLedRGB()
{
  ledRGB.begin();
  ledRGB.setBrightness(80);
  ledRGB.clear();
  ledRGB.show();
}

void alterarEstadoLampada(bool estadoLampada)
{
  digitalWrite(PINO_LAMPADA, estadoLampada ? HIGH : LOW);
}

void alterarCorLedRGB(int vermelho, int verde, int azul)
{
  vermelho = constrain(vermelho, 0, 255);
  verde = constrain(verde, 0, 255);
  azul = constrain(azul, 0, 255);

  ledRGB.setPixelColor(0, ledRGB.Color(vermelho, verde, azul));
  ledRGB.show();
}

void tratarJsonComando(const String &mensagem)
{
  JsonDocument doc;

  if (deserializeJson(doc, mensagem))
  {
    debugErro("Erro ao interpretar JSON");
    return;
  }

  if (doc["led"].is<JsonObject>())
  {
    int r = doc["led"]["r"];
    int g = doc["led"]["g"];
    int b = doc["led"]["b"];
    alterarCorLedRGB(r, g, b);
  }

  if (doc["lampada"].is<bool>())
  {
    alterarEstadoLampada(doc["lampada"]);
  }

  // LCD
  if (doc["estado"].is<String>())
  {
    estadoMaquina = doc["estado"].as<String>();

    if (estadoMaquina == "LIGADA")
    {
      if (!maquinaLigada)
      {
        tempoInicio = millis();
        maquinaLigada = true;
      }
    }
    else
    {
      maquinaLigada = false;
      tempoInicio = millis();
    }
  }

  if (doc["modo"].is<String>())
  {
    modoOperacao = doc["modo"].as<String>();
  }
}

// LCD
void configurarLCD()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");
}

void atualizarLCD()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temperatura);
  lcd.print(" C");

  lcd.setCursor(0, 1);
  lcd.print("Estado: ");
  lcd.print(estadoMaquina);

  lcd.setCursor(0, 2);
  lcd.print("Modo: ");
  lcd.print(modoOperacao);

  lcd.setCursor(0, 3);
  lcd.print("Tempo: ");
  lcd.print(tempoFormatado());
}

String tempoFormatado()
{
  if (!maquinaLigada)
  {
    return "00:00";
  }

  unsigned long segundos = (millis() - tempoInicio) / 1000;

  int minutos = segundos / 60;
  int seg = segundos - (minutos * 60);

  String tempo = "";

  if (minutos < 10) tempo = tempo + "0";
  tempo = tempo + String(minutos);
  tempo = tempo + ":";

  if (seg < 10) tempo = tempo + "0";
  tempo = tempo + String(seg);

  return tempo;
}