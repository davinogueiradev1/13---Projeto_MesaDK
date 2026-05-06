#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <LED.h>

#include "WiFiManager.h"
#include "MqttManager.h"
#include "DebugManager.h"

/*Autor: Davi Nogueira, Fellipe Simon, Gabriel Bocchino, Gabriel Expindola e Heitor Barreto
Programa: Projeto MesaDK
Descrição: Projeto de automação inteligente de um maquinário
Data: 06/05/2026 - 08/05/2026
Versão: 1.0
*/

const int PINO_LAMPADA = 3;
const int PINO_LED_RGB = 48;
const int QUANTIDADE_LEDS = 1;

const char TOPICO_COMANDO[] = "senai134/davinogueira/esp32/comando";

void tratarMensagemRecebida(const char *topico, const String &mensagem); // Funcao de callback da aplicacao
void configurarLedRGB();
void tratarJsonComando(const String &mensagem);
void alterarCorLedRGB(int vermelho, int verde, int azul);

Adafruit_NeoPixel ledRGB(
    QUANTIDADE_LEDS,
    PINO_LED_RGB,
    NEO_GRB + NEO_KHZ800 // NAO EXPLICADO
);

void setup()
{
  pinMode(PINO_LAMPADA, OUTPUT);
  configurarDebug(); // Inicia Serial e manda uma mensagem
  configurarLedRGB();
  conectarWifi();
  configurarMQTT();
  registrarCallbackMensagem(tratarMensagemRecebida);
  conectarMQTT();
}

void loop()
{
  garantirWiFiConectado();
  garantirMQTTConectado();
  loopMQTT();
}

void tratarMensagemRecebida(const char *topico, const String &mensagem)
{
  debugInfo("==================");
  debugInfo("Mensagem recebida na aplicação");
  debugInfo("==================");

  if (topico == nullptr)
  {
    debugErro("Tópico MQTT inválido");
    return;
  }

  debugInfo("Tópico: " + String(topico));
  debugInfo("Mensagem: " + mensagem);

  if (strcmp(topico, TOPICO_COMANDO) == 0)
  {
    tratarJsonComando(mensagem);
    return;
  }

  debugErro("Tópico não tratado: " + String(topico));
}

void configurarLedRGB()
{
  ledRGB.begin();
  ledRGB.setBrightness(80); // Se define a quantidade de luminosidade de 0 a 255
  ledRGB.clear();
  ledRGB.show(); // Envia as cores para todos os leds/atualiza estado do led

  debugInfo("LED RGB configurado no GPIO " + String(PINO_LED_RGB));
}

void alterarEstadoLampada(bool estadoLampada)
{
  if(estadoLampada)
  {
  digitalWrite(PINO_LAMPADA, HIGH);
  }
  else
  {
    digitalWrite(PINO_LAMPADA,LOW);
  }
}

void alterarCorLedRGB(int vermelho, int verde, int azul)
{
  vermelho = constrain(vermelho, 0, 255); // limitante
  verde = constrain(verde, 0, 255);
  azul = constrain(azul, 0, 255);

  ledRGB.setPixelColor(0, ledRGB.Color(vermelho, verde, azul));
  ledRGB.show();

  debugInfo("Cor aplicada no LED RGB: ");
  debugInfo("R: " + String(vermelho));
  debugInfo("G: " + String(verde));
  debugInfo("B: " + String(azul));
}

void tratarJsonComando(const String &mensagem)
{
  JsonDocument doc;

  DeserializationError erro = deserializeJson(doc, mensagem);

  if (erro)
  {
    debugErro("Erro ao interpretar o JSON");
    debugErro(erro.c_str());
    return;
  }

  if (doc["led"].is<JsonObject>())
  {
    if (!doc["led"]["r"].is<int>() || !doc["led"]["g"].is<int>() || !doc["led"]["b"].is<int>())
    {
      debugErro("JSON inválido. Use led.r, led.g e led.b");
      return;
    }
    else
    {
      int vermelho = doc["led"]["r"].as<int>();
      int verde = doc["led"]["g"].as<int>();
      int azul = doc["led"]["b"].as<int>();

      alterarCorLedRGB(vermelho, verde, azul);
    }
  }

  if (doc["lampada"].is<bool >())
  {
    if (!doc["lampada"].is<bool>())
    {
      debugErro("JSON inválido. Use true ou false");
      return;
    }
    else
    {
      bool estadoLampada = doc["lampada"].as<bool >();
      alterarEstadoLampada(estadoLampada);
    }
  }
}