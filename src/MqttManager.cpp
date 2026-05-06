//! MqttManager.cpp

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <Arduino.h>
#include <PubSubClient.h>

//TODO: Talvez precise do .cpp do secrets
#include "secrets.h"
#include "WiFiManager.h"
#include "MqttManager.h"
#include "DebugManager.h"

//========Instâncias========
WiFiClient wifiCliente;
WiFiClientSecure wifiClienteSecure;
PubSubClient mqttClient;

CallbackMensagemMQTT callbackDaAplicacao = nullptr;

void registrarCallbackMensagem(CallbackMensagemMQTT callback)
{
    callbackDaAplicacao = callback;

    if (callbackDaAplicacao != nullptr)
    {
        debugInfo("Callback da aplicação registrada com sucesso.");
    }
    else
    {
        debugErro("Callback da aplicação não foi registrada.");
    }
}

const char *obterTopicoPublicacao(int indiceTopico)
{
    if (indiceTopico < 0 || indiceTopico >= TOTAL_TOPICOS_PUBLICAR)
    {
        debugErro("Índice inválido para tópico de publicação: " + String(indiceTopico));
        return "";
    }

    return TOPICOS_PUBLICAR[indiceTopico];
}

const char *obterTopicoRecebimento(int indiceTopico)
{
    if (indiceTopico < 0 || indiceTopico >= TOTAL_TOPICOS_RECEBER)
    {
        debugErro("Índice inválido para tópico de recebimento: " + String(indiceTopico));
        return "";
    }

    return TOPICOS_RECEBER[indiceTopico];
}

void callbackInternoMQTT(char *topico, byte *payload, unsigned int tamanho) // funcao de so receber a mensagem
{
    String mensagem = "";

    for (unsigned int i = 0; i < tamanho; i++) // para juntar mensagens que vierem em byte
    {
        mensagem += (char)payload[i]; // atribuicao por soma, mesma coisa que mensagem = mensagem + ...
    }

    debugInfo("=================");
    debugInfo("Mensagem MQTT Recebida!");
    debugInfo("=================");
    debugInfo(" Tópico: " + String(topico));
    debugInfo(" Mensagem: " + mensagem);

    if (callbackDaAplicacao != nullptr)
    {
        callbackDaAplicacao(topico, mensagem); // Para tratar a String
    }

    else
    {
        debugErro("Mensagem recebida, mas nenhuma callback da aplicação foi registrado");
    }
}

void configurarMQTT()
{
    debugInfo("=================");
    debugInfo(" Configurando MQTT...");
    debugInfo("=================");

    if (USAR_AWS_IOT)
    {
        // TODO: IMPLEMENTAR CONEXÃO COM A AWS
    }

    else if (MQTT_USAR_TLS)
    {
        debugInfo("Modo selecionado: MQTT com TLS.");
        if (strlen(MQTT_CERTIFICADO_CA)> 100)
        {
            debugInfo("Certificado CA do broker MQTT configurado.");
            wifiClienteSecure.setCACert(MQTT_CERTIFICADO_CA);
        }

        else
        {
            debugErro("Certificado CA do MQTT não configurado. Usando setInsecure apenas para teste.");
            wifiClienteSecure.setInsecure();
        }

        mqttClient.setClient(wifiClienteSecure); // como conectar na internet
        mqttClient.setServer(MQTT_BROKER, MQTT_PORTA);

        debugInfo("Broker MQTT: " + String(MQTT_BROKER));
        debugInfo("Porta MQTT " + String(MQTT_PORTA));
    }

    else // conectar no broker sem certificado
    {
        debugInfo("Modo Selecionado: MQTT sem TLS.");

        mqttClient.setClient(wifiCliente); // como conectar na internet
        mqttClient.setServer(MQTT_BROKER, MQTT_PORTA);

        debugInfo("Broker MQTT: " + String(MQTT_BROKER));
        debugInfo("Porta MQTT " + String(MQTT_PORTA));
    }

    mqttClient.setCallback(callbackInternoMQTT);
    debugInfo("Callback interno do MQTT configurado.");
}

void conectarMQTT()
{
    if (!wifiEstaConectado())   //Vai verificar esta conectado no WIFI
    {
        debugErro("MQTT não pode conectar porque o WiFi está desconectado."); 
        return;
    }

    debugInfo("=================");
    debugInfo("Iniciando conexão MQTT...");
    debugInfo("=================");

    int tentativasMQTT = 0;
    const int maxTentativasMQTT = 5; //define como maximo de tentativas de conexao 5vzs

    while (!mqttClient.connected() && tentativasMQTT < maxTentativasMQTT)   //Enquanto nao estiver conectado, Tenta realizar conexão  até as tentativas maximas (5 vezes)
    {
        debugInfo("Tentando conectar ao broker MQTT. Tentativa: " + String(tentativasMQTT));

        bool conectado = false;

        if (USAR_AWS_IOT)
        {
            // TODO: Implementar futuramente
        }

        else
        {
            if (strlen(MQTT_USUARIO) > 0)
            {
                debugInfo("Conectando MQTT com usuário e senha");

                conectado = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USUARIO, MQTT_SENHA); 
            }
            
            else //Sem usuário
            {
                debugInfo("Conectando MQTT sem usuário e senha.");

                conectado = mqttClient.connect(MQTT_CLIENT_ID);
            }
        }
        if(conectado) 
        {
            debugInfo("MQTT conectado com sucesso.");

            int totalTopicos = obterTotalTopicosRecebimento();
            
            debugInfo("Total de tópicos para inscrição: " + String(totalTopicos));

            for(int i = 0; i < totalTopicos; i++)
            {
                const char* topico = obterTopicoRecebimento(i);

                bool inscrito = mqttClient.subscribe(topico);

                if(inscrito)
                {
                    debugInfo("Inscrito no tópico: " + String(topico));
                }

                else
                {
                    debugErro("Falha ao se inscrever no tópico: " + String (topico));
                }
            }
            //TODO: publicar uma mensagem em um topico informando que o esp foi conectado.
        }
        else
        {
            debugErro("Falha ao conectar no MQTT. Código de erro: " + String(mqttClient.state() ) );

            tentativasMQTT++;
            delay(2000);
        }
    } //Fim do WHILE

    if(!mqttClient.connected())
    {
        debugErro("Não foi possível conectar ao broker MQTT após " + String(maxTentativasMQTT) + "tentativas");
    }
}

void garantirMQTTConectado()
{
    if(!wifiEstaConectado)
    {
        debugErro("MQTT não será reconectado porque o WiFi está desconectado.");
        return;
    }

    if(!mqttClient.connected())
    {
        debugErro("MQTT desconectado. Tentando reconectar...");
        conectarMQTT();
    }
}

void loopMQTT()
{
    mqttClient.loop();
}

void publicarMensagem(const char* topico, const char* mensagem)
{
    if(!mqttClient.connected())
    {
        debugErro("Não foi possível publicar. MQTT desconectado.");
    }

    bool publicado = mqttClient.publish(topico, mensagem);

    if(publicado)
    {
        debugInfo("Mensagem publicada via MQTT. ");
        debugInfo("Tópico: " + String(topico));
        debugInfo("Mensagem: " + String(mensagem));
    }

    else
    {
        debugErro("Falha ao publicar mensagem no tópico: " + String(topico));
    }
}

void publicarMensagemNoTopico(int indiceTopico, const char* mensagem)
{
    const char* topico = obterTopicoPublicacao(indiceTopico);

    if(strlen(topico) == 0)
    {
        debugErro("Não foi possível publicar. Índice de tópico inválido: " + String(indiceTopico));
        return;
    }
    publicarMensagem(topico, mensagem);
}

bool mqttEstaConectado()
{
    return mqttClient.connected();
}

int obterTotalTopicosRecebimento()
{
    return TOTAL_TOPICOS_RECEBER;
}