// * INCLUDEs
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <LiquidCrystal_I2C.h>
#include "WiFiManager.h"
#include "MqttManager.h"
#include "DebugManager.h"

/*
Autores: Davi Nogueira, Fellipe Simon, Gabriel Bocchino,
Gabriel Expindola e Heitor Barreto
Projeto: MesaDK
Descrição: Projeto de automação inteligente de um maquinário
Data: 06/05/2026 - 08/05/2026
Versão: 
*/

// * PINOS 
const int PINO_LAMPADA = 3;
const int PINO_LED_RGB = 48;
const int PINO_BOTAO_BOOT = 0;
const int QUANTIDADE_LEDS = 1;

// *  ESTADOS / Maquina
const int STATUS_DESLIGADA = 0;
const int STATUS_OPERANDO  = 1;
const int STATUS_ALERTA    = 2;
const int STATUS_FALHA     = 3;

// * MQTT / Endereços
const char TOPICO_COMANDO[] = "senai134/davinogueira/esp32/comando";
const char TOPICO_STATUS[]  = "senai134/davinogueira/esp32/status";

// * OBJETOS
LiquidCrystal_I2C lcd(0x27, 20, 4);
Adafruit_NeoPixel ledRGB(QUANTIDADE_LEDS, PINO_LED_RGB, NEO_GRB + NEO_KHZ800);

// * VARIÁVEIS
float temperatura = 25.0;
String estadoMaquina = "DESLIGADA";
String modoOperacao = "LOCAL";
bool modoSupervisao = false;
bool maquinaLigada = false;
int estadoAtual = STATUS_DESLIGADA;
unsigned long tempoInicio = 0;
unsigned long ultimoLCD = 0;
unsigned long ultimoAumentoTemperatura = 0;
unsigned long tempoInicioFalha = 0;

// * BOTÃO
unsigned long esperaBotao = 0;
const int atrasoBotao = 50;
int ultimoEstadoBotao = HIGH;
int estadoBotaoEstavel = HIGH;

// * PROTÓTIPOS 
void tratarMensagemRecebida(const char *topico, const String &mensagem);
void tratarJsonComando(const String &mensagem);
void configurarLCD();
void atualizarLCD();
void configurarLedRGB();
void alterarCorLedRGB(int vermelho, int verde, int azul);
void definirEstado(int novoEstado);
void atualizarTemperatura();
void tratarBotaoBoot();
String tempoFormatado();

// ======================================================
// * void SETUP
// ======================================================

void setup() 
{
  pinMode(PINO_LAMPADA, OUTPUT);
  pinMode(PINO_BOTAO_BOOT, INPUT_PULLUP);
  configurarDebug();
  configurarLedRGB();
  lcd.init();
  lcd.backlight();
  configurarLCD();
  conectarWifi();
  configurarMQTT();
  registrarCallbackMensagem(tratarMensagemRecebida);
  conectarMQTT();
  definirEstado(STATUS_DESLIGADA);
}

// =====================================================
// * LOOP
// =====================================================

void loop() 
{
  garantirWiFiConectado();
  garantirMQTTConectado();
  loopMQTT();
  tratarBotaoBoot();
  atualizarTemperatura();
  if (millis() - ultimoLCD >= 1000) 
  {
    atualizarLCD();
    ultimoLCD = millis();
  }
}

// =========================================================
// * BOTÃO BOOT
// =========================================================

void tratarBotaoBoot() 
{
  int leitura = digitalRead(PINO_BOTAO_BOOT);
  if (leitura != ultimoEstadoBotao) 
  {
    esperaBotao = millis();
  }

  if ((millis() - esperaBotao) > atrasoBotao) 
  {
    if (leitura == LOW && estadoBotaoEstavel == HIGH) 
    {
      modoSupervisao = !modoSupervisao;

      if (modoSupervisao) 
      {
        modoOperacao = "SUPERVISAO";
        debugInfo("Modo supervisao ativado");
      }
      else 
      {
        modoOperacao = "LOCAL";
        debugInfo("Modo supervisao desativado");
      }

      atualizarLCD();
    }
    estadoBotaoEstavel = leitura;
  }
  ultimoEstadoBotao = leitura;
}

// =========================================================
// * ESTADOS DA MÁQUINA
// =========================================================

void definirEstado(int novoEstado) 
{
  estadoAtual = novoEstado;
  switch (estadoAtual) 
  {
    case STATUS_DESLIGADA:
      estadoMaquina = "DESLIGADA";
      alterarCorLedRGB(0, 0, 0);
      digitalWrite(PINO_LAMPADA, LOW);
      maquinaLigada = false;
      temperatura = 25.0;
      break;
    case STATUS_OPERANDO:
      estadoMaquina = "OPERANDO";
      alterarCorLedRGB(0, 255, 0);
      digitalWrite(PINO_LAMPADA, LOW);
      if (!maquinaLigada)
      {
        tempoInicio = millis();
      }
      maquinaLigada = true;
      break;
    case STATUS_ALERTA:
      estadoMaquina = "ALERTA";
      alterarCorLedRGB(255, 255, 0);
      digitalWrite(PINO_LAMPADA, HIGH);
      maquinaLigada = true;
      break;
    case STATUS_FALHA:
      estadoMaquina = "FALHA";
      alterarCorLedRGB(255, 0, 0);
      digitalWrite(PINO_LAMPADA, HIGH);
      maquinaLigada = false;
      tempoInicioFalha = millis();
      break;
  }

  atualizarLCD();
  debugInfo("Estado alterado para: " + estadoMaquina);
}

// =========================================================
// * TEMPERATURA
// =========================================================

void atualizarTemperatura() 
{
  if (estadoAtual == STATUS_OPERANDO || estadoAtual == STATUS_ALERTA) 
  {
    if (millis() - ultimoAumentoTemperatura >= 5000) 
    {
      temperatura++;
      ultimoAumentoTemperatura = millis();
      debugInfo("Temperatura: " + String(temperatura));
      if (temperatura >= 40 && estadoAtual == STATUS_OPERANDO)
      {
        definirEstado(STATUS_ALERTA);
      }
      if (temperatura >= 55 && estadoAtual != STATUS_FALHA)
      {
        definirEstado(STATUS_FALHA);
      }
    }
  }
  // DESLIGAMENTO AUTOMÁTICO
  if (estadoAtual == STATUS_FALHA) 
  {
    if (temperatura < 60) 
    {
      if (millis() - ultimoAumentoTemperatura >= 5000) 
      {
        temperatura++;
        ultimoAumentoTemperatura = millis();
      }
    }
    if (temperatura >= 60) {
      definirEstado(STATUS_DESLIGADA);
    }
  }
}
// =========================================================
// * MQTT
// =========================================================

void tratarMensagemRecebida(const char *topico, const String &mensagem) 
{
  if (topico != nullptr && strcmp(topico, TOPICO_COMANDO) == 0) 
  {
    tratarJsonComando(mensagem);
  }
}

void tratarJsonComando(const String &mensagem) 
{
  JsonDocument doc;
  if (deserializeJson(doc, mensagem)) 
  {
    debugErro("Erro ao interpretar JSON");
    return;
  }
  if (doc["estado"].is<String>()) 
  {
    String estadoRecebido = doc["estado"].as<String>();
    if (estadoRecebido == "DESLIGADA") 
    {
      definirEstado(STATUS_DESLIGADA);
    }
    else if (estadoRecebido == "OPERANDO") 
    {
      definirEstado(STATUS_OPERANDO);
    }
    else if (estadoRecebido == "ALERTA") 
    {
      definirEstado(STATUS_ALERTA);
    }
    else if (estadoRecebido == "FALHA") 
    {
      definirEstado(STATUS_FALHA);
    }
  }
  if (doc["modo"].is<String>()) 
  {
    modoOperacao = doc["modo"].as<String>();
    if (modoOperacao == "SUPERVISAO") 
    {
      modoSupervisao = true;
    }
    else 
    {
      modoSupervisao = false;
    }
  }

  atualizarLCD();
}

// =========================================================
// * LED
// =========================================================

void configurarLedRGB() 
{
  ledRGB.begin();
  ledRGB.setBrightness(80);
  ledRGB.clear();
  ledRGB.show();
}

void alterarCorLedRGB(int vermelho, int verde, int azul) 
{
  ledRGB.setPixelColor(0, ledRGB.Color(vermelho, verde, azul));
  ledRGB.show();
}

// =========================================================
// * LCD
// =========================================================

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
  lcd.print("Estado:");
  lcd.print(estadoMaquina);
  lcd.setCursor(0, 1);
  lcd.print("Temp:");
  lcd.print(temperatura);
  lcd.print("C");
  lcd.setCursor(0, 2);
  lcd.print("Modo:");
  lcd.print(modoOperacao);
  lcd.setCursor(0, 3);

  if (estadoAtual == STATUS_OPERANDO || estadoAtual == STATUS_ALERTA) 
  {
    lcd.print("Tempo:");
    lcd.print(tempoFormatado());
  }
  else 
  {
    lcd.print("Tempo: 00:00");
  }
}

// =========================================================
// * TEMPO FORMATADO
// =========================================================

String tempoFormatado() 
{
  if (!maquinaLigada) 
  {
    return "00:00";
  }
  unsigned long segundos = (millis() - tempoInicio) / 1000;
  int minutos = segundos / 60;
  int seg = segundos % 60;
  String tempo = "";
  if (minutos < 10) 
  {
    tempo = tempo + "0";
  }
  tempo = tempo + String(minutos);
  tempo = tempo + ":";

  if (seg < 10) 
  {
    tempo = tempo + "0";
  }
  tempo = tempo + String(seg);
  return tempo;
}