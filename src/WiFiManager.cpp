//! WiFiManager.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "WiFiManager.h"
#include "secrets.h"
#include <WiFiClient.h>
#include "DebugManager.h"

bool wifiEstaConectado()
{
    return WiFi.status() == WL_CONNECTED;
}

void conectarWifi()
{
  debugInfo("============================");
  debugInfo("Iniciando conexão WiFi...");
  debugInfo("============================");

  //Configura o ESP32 como station
  //Ele vai se conecrar a um roteador existente.
  WiFi.mode(WIFI_STA); 

  //Inicia a conexão com SSID e Senha
  WiFi.begin(WIFI_SSID, WIFI_SENHA);

  debugInfo("Conectando");

  int tentativasWiFi = 0;
  int maxTentativasWiFi = 30;

  //Aguarda a conexão por até 30 tentativas
  while (WiFi.status() != WL_CONNECTED && tentativasWiFi < maxTentativasWiFi)
  {
    delay(500);
    Serial.print(".");
    tentativasWiFi++;
  }

  debugInfo("");
  if (WiFi.status() == WL_CONNECTED)
  {
    debugInfo("WiFi conectado com sucesso!");
    debugInfoSemLinha("[INFO] Endereço IP: ");
    debugInfoSemLinha(WiFi.localIP().toString());
    debugInfoSemLinha("\n\r");
  }
  
  else
  {
    debugInfo("Falha ao conectar ao WiFi");
    debugInfo("Verifique SSID, senha e sinal de rede.");
  }
  
}

void garantirWiFiConectado()
{
  if(WiFi.status() != WL_CONNECTED)
  {
    debugInfo("WiFi desconectado. Tentando reconectar...");
    conectarWifi();
  }

  if(WiFi.status() != WL_CONNECTED)
  {
    debugInfo("Não foi possivel reconectar ao WiFi.");
  }
}