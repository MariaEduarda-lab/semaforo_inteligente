#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "UbidotsEsp32Mqtt.h"

// Configurações de Rede
const char* WIFI_SSID = "Inteli.Iot";
const char* WIFI_PASSWORD = "%(Yk(sxGMtvFEs.3";

// HiveMQ Broker
const char* MQTT_BROKER_HIVEMQ = "ed2825f9cf3d414093d7e43c94381f68.s1.eu.hivemq.cloud";
const int MQTT_PORT_HIVEMQ = 8883;
const char* MQTT_CLIENT_ID_HIVEMQ = "ESP32_Semaforo_01";
const char* MQTT_USER_HIVEMQ = "hivemq.webclient.1762795823524";
const char* MQTT_PASSWORD_HIVEMQ = "6894PDFAJbIclaki;<?>";

// Tópicos MQTT (HiveMQ)
const char* TOPIC_STATUS = "semaforo/status";
const char* TOPIC_VEICULOS = "semaforo/veiculos";
const char* TOPIC_SENSORES = "semaforo/sensores";
const char* TOPIC_COMANDO = "semaforo/comando";

// Ubidots
const char* UBIDOTS_TOKEN = "BBUS-hRRfiBCNVYEtObCygTMr0NnD8KCObp";
const char* UBIDOTS_DEVICE_LABEL = "semaforo_inteligente";

// Pinos dos LEDs
const int SEM1_VERMELHO = 18;
const int SEM1_AMARELO = 19;
const int SEM1_VERDE = 5;
const int SEM2_VERMELHO = 15;
const int SEM2_AMARELO = 4;
const int SEM2_VERDE = 16;

// Pinos dos Sensores
const int LDR_PIN = 34;
const int ULTRASONIC_TRIGGER = 21;
const int ULTRASONIC_ECHO = 22;

// Configurações
const int SENSOR_SEMAFORO = 2;

// Thresholds e Tempos
const int LDR_THRESHOLD_DIA = 1200;
const int LDR_THRESHOLD_NOITE = 500;
const int DISTANCIA_DETECCAO = 5;

const unsigned long TEMPO_VERMELHO = 6000;
const unsigned long TEMPO_AMARELO = 2000;
const unsigned long TEMPO_VERDE = 4000;
const unsigned long TEMPO_PISCA_NOTURNO = 500;

// Intervalo de publicação
const unsigned long INTERVALO_MQTT_HIVEMQ = 5000;
const unsigned long INTERVALO_UBIDOTS = 30000;

// Variáveis Globais
enum EstadoSemaforo { VERMELHO, AMARELO, VERDE };
EstadoSemaforo estadoSem1 = VERMELHO;
EstadoSemaforo estadoSem2 = VERDE;

enum Modo { DIURNO, NOTURNO };
Modo modoAtual = DIURNO;

unsigned long tempoAnterior = 0;
unsigned long tempoAnteriorMQTT = 0;
unsigned long tempoAnteriorUbidots = 0;
bool pisca = false;

int veiculosDetectados = 0;
unsigned long tempoUltimaDeteccao = 0;
const unsigned long DEBOUNCE_DETECCAO = 2000;

int valorLDR = 0;
float distanciaUltrasonico = 0;

// Cliente MQTT para HiveMQ
WiFiClientSecure wifiClientSecureHiveMQ;
PubSubClient mqttClientHiveMQ(wifiClientSecureHiveMQ);

// Cliente Ubidots
Ubidots ubidots(UBIDOTS_TOKEN);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("SEMÁFORO INTELIGENTE");

  // Configurar pinos dos LEDs
  pinMode(SEM1_VERMELHO, OUTPUT);
  pinMode(SEM1_AMARELO, OUTPUT);
  pinMode(SEM1_VERDE, OUTPUT);
  pinMode(SEM2_VERMELHO, OUTPUT);
  pinMode(SEM2_AMARELO, OUTPUT);
  pinMode(SEM2_VERDE, OUTPUT);

  // Configurar sensor ultrassônico
  pinMode(ULTRASONIC_TRIGGER, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);

  // Configurar LDR
  pinMode(LDR_PIN, INPUT);

  // Desligar todos LEDs
  apagarTodosLEDs();

  // Conectar WiFi
  conectarWiFi();

  // Configurar HiveMQ com TLS
  wifiClientSecureHiveMQ.setInsecure();
  mqttClientHiveMQ.setServer(MQTT_BROKER_HIVEMQ, MQTT_PORT_HIVEMQ);
  mqttClientHiveMQ.setCallback(mqttCallbackHiveMQ);
  mqttClientHiveMQ.setKeepAlive(60);
  mqttClientHiveMQ.setSocketTimeout(30);
  mqttClientHiveMQ.setBufferSize(512);

  Serial.println("HiveMQ configurado\n");

  // Configurar Ubidots usando a biblioteca oficial
  Serial.println("Configurando Ubidots...");
  ubidots.setDebug(true); // Ativar debug
  ubidots.connectToWifi(WIFI_SSID, WIFI_PASSWORD);
  ubidots.setup();
  ubidots.reconnect();
  
  Serial.println("Ubidots configurado");
  Serial.println("  Device: " + String(UBIDOTS_DEVICE_LABEL));
  Serial.println("  Token: " + String(UBIDOTS_TOKEN).substring(0, 10) + "...");
  Serial.println("  Intervalo: 30 segundos\n");

  // Inicializar estado
  tempoAnterior = millis();
  tempoAnteriorMQTT = millis();
  tempoAnteriorUbidots = millis();
  estadoSem1 = VERMELHO;
  estadoSem2 = VERDE;
  atualizarLEDs();

  Serial.println("Sistema pronto!\n");
}

void loop() {
  // Manter conexões ativas
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  // HiveMQ
  if (!mqttClientHiveMQ.connected()) {
    reconectarHiveMQ();
  }
  mqttClientHiveMQ.loop();

  // Ubidots - manter conexão ativa
  if (!ubidots.connected()) {
    Serial.println("[Ubidots] Reconectando...");
    ubidots.reconnect();
  }
  ubidots.loop();

  // Ler sensores
  valorLDR = analogRead(LDR_PIN);
  distanciaUltrasonico = lerUltrasonico();

  // Verificar modo dia/noite
  verificarModo();

  // Executar lógica apropriada
  if (modoAtual == DIURNO) {
    verificarVeiculo();
    logicaSemaforoDiurno();
  } else {
    logicaSemaforoNoturno();
  }

  // Atualizar LEDs
  atualizarLEDs();

  // Publicar dados via MQTT (HiveMQ)
  if (millis() - tempoAnteriorMQTT >= INTERVALO_MQTT_HIVEMQ) {
    publicarDadosHiveMQ();
    tempoAnteriorMQTT = millis();
  }

  // Enviar dados para Ubidots
  if (millis() - tempoAnteriorUbidots >= INTERVALO_UBIDOTS) {
    enviarParaUbidots();
    tempoAnteriorUbidots = millis();
  }

  // Debug a cada 2 segundos
  static unsigned long ultimoDebug = 0;
  if (millis() - ultimoDebug > 2000) {
    imprimirStatus();
    ultimoDebug = millis();
  }

  delay(50);
}

// Funções WiFi
void conectarWiFi() {
  Serial.print("Conectando ao WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado!");
    Serial.print("  IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("  RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm\n");
  } else {
    Serial.println("\nFalha ao conectar WiFi!");
  }
}

// Funções HiveMQ
void reconectarHiveMQ() {
  int tentativas = 0;
  while (!mqttClientHiveMQ.connected() && tentativas < 3) {
    Serial.print("Conectando ao HiveMQ...");

    if (mqttClientHiveMQ.connect(MQTT_CLIENT_ID_HIVEMQ, MQTT_USER_HIVEMQ, MQTT_PASSWORD_HIVEMQ)) {
      Serial.println(" Conectado!");
      
      if (mqttClientHiveMQ.subscribe(TOPIC_COMANDO)) {
        Serial.println("  Subscrito em: " + String(TOPIC_COMANDO));
      }
      
      String msgInicio = "{\"status\":\"online\"}";
      mqttClientHiveMQ.publish(TOPIC_STATUS, msgInicio.c_str());
      
    } else {
      Serial.print(" Falha, rc=");
      Serial.println(mqttClientHiveMQ.state());
      tentativas++;
      if (tentativas < 3) {
        delay(3000);
      }
    }
  }
}

void mqttCallbackHiveMQ(char* topic, byte* payload, unsigned int length) {
  Serial.print("\n[HiveMQ] Mensagem em [");
  Serial.print(topic);
  Serial.print("]: ");

  String mensagem = "";
  for (int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }
  Serial.println(mensagem);

  if (String(topic) == TOPIC_COMANDO) {
    if (mensagem == "reset") {
      veiculosDetectados = 0;
      Serial.println("  → Contador resetado!");
      mqttClientHiveMQ.publish(TOPIC_STATUS, "{\"acao\":\"contador_resetado\"}");
    }
    else if (mensagem == "status") {
      publicarDadosHiveMQ();
    }
    else if (mensagem == "modo_diurno") {
      modoAtual = DIURNO;
      tempoAnterior = millis();
      estadoSem1 = VERMELHO;
      estadoSem2 = VERDE;
      Serial.println("  → Modo diurno forçado");
    }
    else if (mensagem == "modo_noturno") {
      modoAtual = NOTURNO;
      tempoAnterior = millis();
      pisca = false;
      Serial.println("  → Modo noturno forçado");
    }
  }
}

void publicarDadosHiveMQ() {
  if (!mqttClientHiveMQ.connected()) return;

  String statusMsg = "{";
  statusMsg += "\"modo\":\"" + String(modoAtual == DIURNO ? "diurno" : "noturno") + "\",";
  statusMsg += "\"sem1\":\"" + estadoParaString(estadoSem1) + "\",";
  statusMsg += "\"sem2\":\"" + estadoParaString(estadoSem2) + "\",";
  statusMsg += "\"veiculos\":" + String(veiculosDetectados);
  statusMsg += "}";
  
  mqttClientHiveMQ.publish(TOPIC_STATUS, statusMsg.c_str());

  String sensoresMsg = "{";
  sensoresMsg += "\"ldr\":" + String(valorLDR) + ",";
  sensoresMsg += "\"distancia\":" + String(distanciaUltrasonico, 1);
  sensoresMsg += "}";
  
  mqttClientHiveMQ.publish(TOPIC_SENSORES, sensoresMsg.c_str());
  
  Serial.println("[HiveMQ] Dados publicados");
}

// Funções Ubidots
void enviarParaUbidots() {
  if (!ubidots.connected()) {
    Serial.println("[Ubidots] Não conectado, pulando publicação");
    return;
  }

  Serial.println("\n[Ubidots] Enviando dados...");
  
  // Adicionar variáveis usando a biblioteca oficial
  ubidots.add("ldr", valorLDR);
  ubidots.add("distancia", distanciaUltrasonico);
  ubidots.add("veiculos", veiculosDetectados);
  ubidots.add("modo", modoAtual == DIURNO ? 1 : 0);
  ubidots.add("sem1_estado", estadoSem1);
  ubidots.add("sem2_estado", estadoSem2);

  // Publicar no device
  bool publicado = ubidots.publish(UBIDOTS_DEVICE_LABEL);
  
  if (publicado) {
    Serial.println("  Dados enviados com sucesso!");
    Serial.println("  → LDR=" + String(valorLDR) + 
                   ", Dist=" + String(distanciaUltrasonico, 1) + 
                   ", Veiculos=" + String(veiculosDetectados));
  } else {
    Serial.println("  Falha ao publicar");
  }
}

// Funções dos Sensores
float lerUltrasonico() {
  digitalWrite(ULTRASONIC_TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIGGER, LOW);

  long duracao = pulseIn(ULTRASONIC_ECHO, HIGH, 30000);

  if (duracao == 0) {
    return 999.0;
  }

  float distancia = duracao * 0.034 / 2;

  if (distancia > 400) {
    distancia = 999.0;
  }

  return distancia;
}

void verificarModo() {
  if (valorLDR > LDR_THRESHOLD_DIA && modoAtual == NOTURNO) {
    modoAtual = DIURNO;
    tempoAnterior = millis();
    estadoSem1 = VERMELHO;
    estadoSem2 = VERDE;
    Serial.println("\nMODO DIURNO ATIVADO\n");
    
    if (mqttClientHiveMQ.connected()) {
      mqttClientHiveMQ.publish(TOPIC_STATUS, "{\"evento\":\"modo_diurno\"}");
    }
  } 
  else if (valorLDR < LDR_THRESHOLD_NOITE && modoAtual == DIURNO) {
    modoAtual = NOTURNO;
    tempoAnterior = millis();
    pisca = false;
    Serial.println("\nMODO NOTURNO ATIVADO\n");
    
    if (mqttClientHiveMQ.connected()) {
      mqttClientHiveMQ.publish(TOPIC_STATUS, "{\"evento\":\"modo_noturno\"}");
    }
  }
}

void verificarVeiculo() {
  if (distanciaUltrasonico <= DISTANCIA_DETECCAO) {
    if (millis() - tempoUltimaDeteccao > DEBOUNCE_DETECCAO) {
      veiculosDetectados++;
      tempoUltimaDeteccao = millis();

      Serial.println("\nVEÍCULO DETECTADO!");
      Serial.print("   Total de veículos: ");
      Serial.println(veiculosDetectados);

      if (mqttClientHiveMQ.connected()) {
        String msg = "{\"veiculo_detectado\":true,\"total\":" + String(veiculosDetectados) + "}";
        mqttClientHiveMQ.publish(TOPIC_VEICULOS, msg.c_str());
      }

      forcaSemaforoVerde();
    }
  }
}

void forcaSemaforoVerde() {
  if (SENSOR_SEMAFORO == 1) {
    estadoSem1 = VERDE;
    estadoSem2 = VERMELHO;
    Serial.println("   Semáforo 1 FORÇADO para VERDE");
  } else {
    estadoSem1 = VERMELHO;
    estadoSem2 = VERDE;
    Serial.println("   Semáforo 2 FORÇADO para VERDE");
  }
  tempoAnterior = millis();
}

// Lógicas dos Semáforos
void logicaSemaforoDiurno() {
  unsigned long tempoAtual = millis();
  unsigned long tempoDecorrido = tempoAtual - tempoAnterior;

  if (estadoSem1 == VERMELHO && estadoSem2 == VERDE) {
    if (tempoDecorrido >= TEMPO_VERDE) {
      estadoSem2 = AMARELO;
      tempoAnterior = tempoAtual;
      Serial.println("-> Sem2: VERDE -> AMARELO");
    }
  } 
  else if (estadoSem1 == VERMELHO && estadoSem2 == AMARELO) {
    if (tempoDecorrido >= TEMPO_AMARELO) {
      estadoSem1 = VERDE;
      estadoSem2 = VERMELHO;
      tempoAnterior = tempoAtual;
      Serial.println("-> Sem1: VERMELHO -> VERDE | Sem2: AMARELO -> VERMELHO");
    }
  } 
  else if (estadoSem1 == VERDE && estadoSem2 == VERMELHO) {
    if (tempoDecorrido >= TEMPO_VERDE) {
      estadoSem1 = AMARELO;
      tempoAnterior = tempoAtual;
      Serial.println("-> Sem1: VERDE -> AMARELO");
    }
  } 
  else if (estadoSem1 == AMARELO && estadoSem2 == VERMELHO) {
    if (tempoDecorrido >= TEMPO_AMARELO) {
      estadoSem1 = VERMELHO;
      estadoSem2 = VERDE;
      tempoAnterior = tempoAtual;
      Serial.println("-> Sem1: AMARELO -> VERMELHO | Sem2: VERMELHO →\-> VERDE");
    }
  }
}

void logicaSemaforoNoturno() {
  unsigned long tempoAtual = millis();

  if (tempoAtual - tempoAnterior >= TEMPO_PISCA_NOTURNO) {
    pisca = !pisca;
    tempoAnterior = tempoAtual;
  }

  estadoSem1 = AMARELO;
  estadoSem2 = AMARELO;
}

// Controle dos LEDs
void atualizarLEDs() {
  if (modoAtual == DIURNO) {
    digitalWrite(SEM1_VERMELHO, estadoSem1 == VERMELHO ? HIGH : LOW);
    digitalWrite(SEM1_AMARELO, estadoSem1 == AMARELO ? HIGH : LOW);
    digitalWrite(SEM1_VERDE, estadoSem1 == VERDE ? HIGH : LOW);

    digitalWrite(SEM2_VERMELHO, estadoSem2 == VERMELHO ? HIGH : LOW);
    digitalWrite(SEM2_AMARELO, estadoSem2 == AMARELO ? HIGH : LOW);
    digitalWrite(SEM2_VERDE, estadoSem2 == VERDE ? HIGH : LOW);
  } else {
    apagarTodosLEDs();
    if (pisca) {
      digitalWrite(SEM1_AMARELO, HIGH);
      digitalWrite(SEM2_AMARELO, HIGH);
    }
  }
}

void apagarTodosLEDs() {
  digitalWrite(SEM1_VERMELHO, LOW);
  digitalWrite(SEM1_AMARELO, LOW);
  digitalWrite(SEM1_VERDE, LOW);
  digitalWrite(SEM2_VERMELHO, LOW);
  digitalWrite(SEM2_AMARELO, LOW);
  digitalWrite(SEM2_VERDE, LOW);
}

// Funções de Debug
void imprimirStatus() {
  Serial.println("\n");
  
  Serial.print("WiFi: ");
  Serial.print(WiFi.status() == WL_CONNECTED ? "Sim" : "Não");
  Serial.print("  HiveMQ: ");
  Serial.print(mqttClientHiveMQ.connected() ? "Sim" : "Não");
  Serial.print("  Ubidots: ");
  Serial.print(ubidots.connected() ? "Sim" : "Não");
  Serial.print("  |  ");

  Serial.print("Modo: ");
  Serial.print(modoAtual == DIURNO ? "DIURNO" : "NOTURNO");
  Serial.print("  |  ");

  Serial.print("LDR: ");
  Serial.print(valorLDR);
  Serial.print("  Dist: ");
  if (distanciaUltrasonico > 100) {
    Serial.print("---");
  } else {
    Serial.print(distanciaUltrasonico, 1);
  }
  Serial.println(" cm");

  Serial.print("Sem1: ");
  Serial.print(estadoParaString(estadoSem1));
  Serial.print("  |  Sem2: ");
  Serial.print(estadoParaString(estadoSem2));
  Serial.print("  |  Veículos: ");
  Serial.println(veiculosDetectados);
}

String estadoParaString(EstadoSemaforo estado) {
  switch (estado) {
    case VERMELHO: return "VERMELHO";
    case AMARELO:  return "AMARELO ";
    case VERDE:    return "VERDE   ";
    default:       return "??? ";
  }
}