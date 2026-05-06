//!secrets.h
#ifndef SECRETS_H
#define SECRETS_H

//Configuracoes do WIFI
extern const char* WIFI_SSID;
extern const char* WIFI_SENHA;


//TODO: Transformar todas as co e defines em extern

//====================
//MQTT
//====================
extern const bool USAR_AWS_IOT;

extern const char* MQTT_BROKER;
extern const int MQTT_PORTA;

extern const char* MQTT_CLIENT_ID;

extern const char* MQTT_USUARIO;
extern const char* MQTT_SENHA;

extern const bool MQTT_USAR_TLS; //modo de conexão sem segurança

extern const char MQTT_CERTIFICADO_CA[];

extern const char* TOPICOS_PUBLICAR[];

extern const int TOTAL_TOPICOS_PUBLICAR;

extern const char* TOPICOS_RECEBER[];

extern const int TOTAL_TOPICOS_RECEBER;


//====================
//DEBUG
//====================

// 0 = sem mensagens
// 1 = apenas erros
// 2 = todas mensagens
extern const int DEBUG_NIVEL_INICIAL;

//Pino usado para forçar todas as mensagens na inicialização
extern const int PINO_HABILITA_DEBUG_COMPLETO;


#endif 