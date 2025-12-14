#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

const char* WIFI_SSID = "Casa_Da_Vo";
const char* WIFI_PASS = "";

const char* MQTT_SERVER_IP = "192.168.1.17";
const char* MQTT_CLIENT_ID = "esp32-irrigacao-01";

// Tópicos MQTT
const char* TOPICO_UMIDADE_SOLO_PUB = "irrigacao/umidade_solo";
const char* TOPICO_TEMP_AR_PUB = "irrigacao/temp_ar";
const char* TOPICO_UMIDADE_AR_PUB = "irrigacao/umidade_ar";
const char* TOPICO_STATUS_BOMBA_PUB = "irrigacao/status_bomba";
const char* TOPICO_COMANDO_SUB = "irrigacao/comando";

// Pinos
const int SENSOR_UMIDADE_PIN = 34;
const int RELE_PIN = 22;           
const int DHT_PIN = 4;             

const int VALOR_SENSOR_SECO = 4095;
const int VALOR_SENSOR_MOLHADO = 1650;

#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

bool estadoBomba = false;
long lastMsgTime = 0;
const int INTERVALO_LEITURA = 10000;

// Variáveis para guardar os valores dos sensores
float umidadeSoloPct = 0;
float tempAr = 0;
float umidadeAr = 0;

// Função de callback
void callback(char* topic, byte* payload, unsigned int length) {
  String mensagem = "";
  for (int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }
  Serial.print("Comando recebido [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(mensagem);

  if (strcmp(topic, TOPICO_COMANDO_SUB) == 0) {
    if (mensagem == "ON") {
      digitalWrite(RELE_PIN, HIGH); // Active HIGH: Liga o relé
      estadoBomba = true;
      Serial.println("Bomba LIGADA");
    } else if (mensagem == "OFF") {
      digitalWrite(RELE_PIN, LOW); // Active HIGH: Desliga o relé
      estadoBomba = false;
      Serial.println("Bomba DESLIGADA");
    }
    // Publica o novo status imediatamente
    publicarStatusBomba();
  }
}

// Função de conexão wifi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// Função de reconexão MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    if (client.connect(MQTT_CLIENT_ID)) {
      Serial.println("Conectado!");
      // Se inscreve no tópico de comando
      client.subscribe(TOPICO_COMANDO_SUB);
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

// Função para publicar o status da bomba
void publicarStatusBomba() {
  if (estadoBomba) {
    client.publish(TOPICO_STATUS_BOMBA_PUB, "LIGADA");
  } else {
    client.publish(TOPICO_STATUS_BOMBA_PUB, "DESLIGADA");
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(RELE_PIN, OUTPUT);
  digitalWrite(RELE_PIN, LOW); // Garante que a bomba começa desligada

  dht.begin(); // Inicializa o sensor DHT

  setup_wifi();
  client.setServer(MQTT_SERVER_IP, 1883);
  client.setCallback(callback);
  
  delay(1500);
}

// Loop principal
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsgTime > INTERVALO_LEITURA) {
    lastMsgTime = now;

    //1. Ler sensor de umidade do solo
    int valorLido = analogRead(SENSOR_UMIDADE_PIN);
    // Mapeia usando os seus valores (com lógica invertida)
    umidadeSoloPct = map(valorLido, VALOR_SENSOR_SECO, VALOR_SENSOR_MOLHADO, 0, 100);
    
    // Garante que o valor fique entre 0 e 100
    if (umidadeSoloPct < 0) umidadeSoloPct = 0;
    if (umidadeSoloPct > 100) umidadeSoloPct = 100;

    Serial.print("Umidade Solo: "); Serial.print(umidadeSoloPct); Serial.println("%");

    //2. Ler sensor DHT22
    tempAr = dht.readTemperature();
    umidadeAr = dht.readHumidity();

    // Checa se a leitura falhou
    if (isnan(tempAr) || isnan(umidadeAr)) {
      Serial.println("Falha ao ler o sensor DHT22!");
    } else {
      Serial.print("Temp. Ar: "); Serial.print(tempAr); Serial.println(" *C");
      Serial.print("Umidade Ar: "); Serial.print(umidadeAr); Serial.println(" %");
    }

    //3. Publica dados via MQTT
    client.publish(TOPICO_UMIDADE_SOLO_PUB, String(umidadeSoloPct).c_str());
    client.publish(TOPICO_TEMP_AR_PUB, String(tempAr).c_str());
    client.publish(TOPICO_UMIDADE_AR_PUB, String(umidadeAr).c_str());
    publicarStatusBomba(); // Publica o status atual da bomba

  }
}
