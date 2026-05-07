#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

#include <LiquidCrystal_I2C.h>
#include "WiFiManager.h"
#include "MqttManager.h"
#include "DebugManager.h"

/*Autor: Davi Nogueira, Fellipe Simon, Gabriel Bocchino, Gabriel Expindola e Heitor Barreto
Programa: Projeto MesaDK
Descrição: Projeto de automação inteligente de um maquinário
Data: 06/05/2026 - 08/05/2026
Versão: 1.0
*/

// --- PINOS ---
const int PINO_LAMPADA = 3;
const int PINO_LED_RGB = 48;
const int PINO_BOTAO_ESTADO = 0; // Botão BOOT
const int QUANTIDADE_LEDS = 1;

// --- CONSTANTES DE ESTADO (Substituindo Enum) ---
const int STATUS_DESLIGADA = 0;
const int STATUS_OPERANDO  = 1;
const int STATUS_ALERTA    = 2;
const int STATUS_FALHA     = 3;

// --- MQTT ---
const char TOPICO_COMANDO[] = "senai134/davinogueira/esp32/comando";
const char TOPICO_STATUS[]  = "senai134/davinogueira/esp32/status"; // Para reportar ao outro ESP

LiquidCrystal_I2C lcd(0x27, 20, 4);

// --- PROTÓTIPOS ---
void tratarMensagemRecebida(const char *topico, const String &mensagem);
void configurarLedRGB();
void tratarJsonComando(const String &mensagem);
void alterarCorLedRGB(int vermelho, int verde, int azul);
void configurarLCD();
void atualizarLCD();
String tempoFormatado();
void tratarBotoes();
void definirEstado(int novoEstado);

// --- VARIÁVEIS GLOBAIS ---
float temperatura = 25.0;
int indiceEstadoAtual = STATUS_DESLIGADA; 
String estadoMaquina = "DESLIGADA";
String modoOperacao = "LOCAL";

unsigned long tempoInicio = 0;
bool maquinaLigada = false;

// Variáveis para Debounce do Botão
unsigned long ultimoTempoDebounce = 0;
const int atrasoDebounce = 50;
int ultimoEstadoBotao = HIGH;
int estadoBotaoEstavel = HIGH;

Adafruit_NeoPixel ledRGB(QUANTIDADE_LEDS, PINO_LED_RGB, NEO_GRB + NEO_KHZ800);

void setup() {
  pinMode(PINO_LAMPADA, OUTPUT);
  pinMode(PINO_BOTAO_ESTADO, INPUT_PULLUP); // Botão com resistor interno

  configurarDebug();
  configurarLedRGB();
  
  lcd.init();
  lcd.backlight();
  configurarLCD();

  conectarWifi();
  configurarMQTT();
  registrarCallbackMensagem(tratarMensagemRecebida);
  conectarMQTT();

  definirEstado(STATUS_DESLIGADA); // Inicia em estado seguro
}

void loop() {
  garantirWiFiConectado();
  garantirMQTTConectado();
  loopMQTT();

  tratarBotoes(); // Verifica se o botão físico foi apertado

  // Atualiza LCD a cada 1 segundo
  static unsigned long ultimoLCD = 0;
  if (millis() - ultimoLCD > 1000) {
    atualizarLCD();
    ultimoLCD = millis();
  }
}

// --- LOGICA DE BOTÕES ---
  void tratarBotoes() {
  // O botão BOOT (GPIO 0) é LOW quando pressionado
  int leitura = digitalRead(PINO_BOTAO_ESTADO);

  // Debounce simples
  if (leitura != ultimoEstadoBotao) {
    ultimoTempoDebounce = millis();
  }

  if ((millis() - ultimoTempoDebounce) > atrasoDebounce) {
    // Se o estado do botão mudou de SOLTO (HIGH) para APERTADO (LOW)
    if (leitura == LOW && estadoBotaoEstavel == HIGH) {
      
      int proximo = indiceEstadoAtual + 1;
      if (proximo > 3) proximo = 0;
      
      Serial.println("Botão BOOT pressionado! Mudando estado...");
      definirEstado(proximo);
    }
    estadoBotaoEstavel = leitura;
  }
  ultimoEstadoBotao = leitura;
}

// --- MÁQUINA DE ESTADOS (Ação Central) ---
void definirEstado(int novoEstado) {
  indiceEstadoAtual = novoEstado;

  switch (indiceEstadoAtual) {
    case STATUS_DESLIGADA:
      estadoMaquina = "DESLIGADA";
      alterarCorLedRGB(0, 0, 255);       // Apagado
      digitalWrite(PINO_LAMPADA, LOW); // Lâmpada off
      maquinaLigada = false;
      break;

    case STATUS_OPERANDO:
      estadoMaquina = "OPERANDO";
      alterarCorLedRGB(0, 255, 0);      // Verde
      digitalWrite(PINO_LAMPADA, HIGH); // Lâmpada On
      if (!maquinaLigada) { tempoInicio = millis(); maquinaLigada = true; }
      break;

    case STATUS_ALERTA:
      estadoMaquina = "ALERTA";
      alterarCorLedRGB(255, 100, 0);    // Laranja/Amarelo
      digitalWrite(PINO_LAMPADA, HIGH); // Lâmpada On (Ainda opera)
      maquinaLigada = true;
      break;

    case STATUS_FALHA:
      estadoMaquina = "FALHA";
      alterarCorLedRGB(255, 0, 0);      // Vermelho
      digitalWrite(PINO_LAMPADA, LOW);  // Lâmpada Off (Segurança)
      maquinaLigada = false;
      break;
  }

  // Opcional: Avisar o outro ESP32 sobre a mudança via MQTT
  // publicarMensagem(TOPICO_STATUS, String(indiceEstadoAtual));
  
  debugInfo("Estado alterado para: " + estadoMaquina);
  atualizarLCD();
}

void tratarMensagemRecebida(const char *topico, const String &mensagem) {
  if (topico != nullptr && strcmp(topico, TOPICO_COMANDO) == 0) {
    tratarJsonComando(mensagem);
  }
}

void configurarLedRGB() {
  ledRGB.begin();
  ledRGB.setBrightness(80);
  ledRGB.clear();
  ledRGB.show();
}

void alterarCorLedRGB(int vermelho, int verde, int azul) {
  ledRGB.setPixelColor(0, ledRGB.Color(vermelho, verde, azul));
  ledRGB.show();
}

void tratarJsonComando(const String &mensagem) {
  JsonDocument doc;
  if (deserializeJson(doc, mensagem)) {
    debugErro("Erro ao interpretar JSON");
    return;
  }

  // Se o comando vier via JSON, ele sobrepõe o estado atual
  if (doc["estado"].is<int>()) {
    definirEstado(doc["estado"].as<int>());
  } else if (doc["estado"].is<String>()) {
    // Caso receba como String (LIGADA/DESLIGADA)
    String st = doc["estado"].as<String>();
    if(st == "DESLIGADA") definirEstado(STATUS_DESLIGADA);
    if(st == "OPERANDO")  definirEstado(STATUS_OPERANDO);
  }
  
  if (doc["modo"].is<String>()) {
    modoOperacao = doc["modo"].as<String>();
  }
}

void configurarLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MesaDK Iniciando...");
}

void atualizarLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: "); lcd.print(temperatura); lcd.print(" C");
  
  lcd.setCursor(0, 1);
  lcd.print("Status: "); lcd.print(estadoMaquina);

  lcd.setCursor(0, 2);
  lcd.print("Modo: "); lcd.print(modoOperacao);

  lcd.setCursor(0, 3);
  lcd.print("Tempo: "); lcd.print(tempoFormatado());
}

String tempoFormatado() {
  if (!maquinaLigada) return "00:00";
  unsigned long segundos = (millis() - tempoInicio) / 1000;
  int minutos = segundos / 60;
  int seg = segundos % 60;
  char buffer[10];
  sprintf(buffer, "%02d:%02d", minutos, seg);
  return String(buffer);
}