#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>
#include "esp_sleep.h"

// -------- WIFI --------
const char* ssid = "SUA_REDE_WIFI";
const char* password = "SUA_SENHA_WIFI";

// -------- TELEGRAM --------
const char* botToken = "SEU_TOKEN_DO_TELEGRAM";
const char* chatID = "SEU_CHAT_ID";

// -------- PINOS --------
const int pinoTouch = 2;
const int pinoInterruptor = 4;
const int pinoRele = 3;
const int pinoADC = 0;

// -------- TEMPOS --------
const unsigned long TRAVA_TOUCH = 2000;
const unsigned long DEBOUNCE = 150;
const unsigned long INTERVALO_SLEEP = 5000;
const unsigned long INTERVALO_TELEGRAM = 600000;
const unsigned long TEMPO_AUTO_OFF = 180000; // 3 minutos em milissegundos

// -------- ESTADO LUZ E SISTEMA --------
bool estadoLuz = false;
bool modoOTA = false;
bool luzAcesaViaTouch = false;
unsigned long timerTouch = 0;
unsigned long ultimaAtividade = 0;
unsigned long ultimoTelegram = 0;

// -------- TOUCH --------
bool ultimoTouch = LOW;
unsigned long ultimoToqueValido = 0;

// -------- INTERRUPTOR --------
bool estadoInterruptor = HIGH;
bool leituraInterruptorCrua = HIGH;
unsigned long ultimoDebounce = 0;

// -------- FILA TELEGRAM (Background Task) --------
volatile bool flagEnviarTelegram = false;
String msgFilaTelegram = "";

// ---------------- DEBUG ----------------
void debugln(String msg) {
  Serial.println("[DEBUG] " + msg);
}

// ---------------- BATERIA ----------------
float lerTensao() {
  long soma = 0;
  for (int i = 0; i < 5; i++) {
    soma += analogRead(pinoADC);
    delay(5);
  }
  float leitura = soma / 5.0;
  float tensao = (leitura * 3.3 / 4095.0) * 2.0;
  return tensao;
}

int calcularPercentual(float v) {
  int perc = map(v * 100, 300, 420, 0, 100);
  return constrain(perc, 0, 100);
}

// ---------------- BACKGROUND TASK (CORE 0) ----------------
// Esta função roda em paralelo e não trava o interruptor físico
void taskRede(void * parameter) {
  for(;;) {
    if (flagEnviarTelegram) {
      debugln("REDE | Iniciando conexão em background...");
      
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      
      unsigned long t0 = millis();
      bool conectado = false;
      
      while (millis() - t0 < 8000) {
        if (WiFi.status() == WL_CONNECTED) {
          conectado = true;
          break;
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
      }

      if (conectado) {
        debugln("REDE | Conectado. Enviando Telegram...");
        WiFiClientSecure client;
        client.setInsecure();

        if (client.connect("api.telegram.org", 443)) {
          String url = "/bot" + String(botToken) + "/sendMessage?chat_id=" + String(chatID) + "&text=" + msgFilaTelegram;
          client.print(String("GET ") + url + " HTTP/1.1\r\n"
                       "Host: api.telegram.org\r\n"
                       "Connection: close\r\n\r\n");
          vTaskDelay(300 / portTICK_PERIOD_MS);
          client.stop();
          debugln("REDE | Mensagem enviada.");
        }
      } else {
        debugln("REDE | Falha de conexão WiFi.");
      }

      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      flagEnviarTelegram = false; // Libera a flag após tentar enviar
    }
    
    // Pequeno delay para liberar a CPU para outras tarefas do FreeRTOS
    vTaskDelay(100 / portTICK_PERIOD_MS); 
  }
}

// ---------------- SOLICITAR TELEGRAM ----------------
// Apenas enfileira a mensagem para a Task do Core 0 resolver
void solicitarEnvioTelegram(String msg) {
  if (!flagEnviarTelegram) { // Só enfileira se não estiver ocupado enviando
    msgFilaTelegram = msg;
    flagEnviarTelegram = true;
  }
}

// ---------------- LUZ ----------------
void alternarLuz(bool viaTouch) {
  estadoLuz = !estadoLuz;
  digitalWrite(pinoRele, estadoLuz ? HIGH : LOW);
  ultimaAtividade = millis();

  debugln(String("LUZ | ") + (estadoLuz ? "ON" : "OFF"));

  int bat = calcularPercentual(lerTensao());

  if (viaTouch && estadoLuz) {
    luzAcesaViaTouch = true;
    timerTouch = millis();
    solicitarEnvioTelegram("💡 LUZ: ON (Touch) | 🔋 " + String(bat) + "%");
  } else {
    luzAcesaViaTouch = false;
    if (viaTouch) {
      solicitarEnvioTelegram("💡 LUZ: OFF (Touch) | 🔋 " + String(bat) + "%");
    }
  }
}

// ---------------- TOUCH ----------------
void handleTouch() {
  bool leitura = digitalRead(pinoTouch);
  
  if (leitura == HIGH && ultimoTouch == LOW) { // Detecta borda de subida
    if (millis() - ultimoToqueValido > TRAVA_TOUCH) {
      ultimoToqueValido = millis();
      debugln("TOUCH | Acionado");
      alternarLuz(true);
    } else {
      debugln("TOUCH | Bloqueado (Anti-spam)");
    }
  }
  ultimoTouch = leitura;
}

// ---------------- INTERRUPTOR ----------------
void handleInterruptor() {
  bool leituraReal = digitalRead(pinoInterruptor);
  
  // Se houver qualquer variação física, reseta o tempo de debounce
  if (leituraReal != leituraInterruptorCrua) {
    ultimoDebounce = millis();
  }
  
  // Se o sinal estabilizou pelo tempo definido no DEBOUNCE
  if ((millis() - ultimoDebounce) > DEBOUNCE) {
    // E se o estado estabilizado for diferente do estado lógico atual do sistema
    if (leituraReal != estadoInterruptor) {
      estadoInterruptor = leituraReal;
      debugln("INTERRUPTOR | Toggle");
      alternarLuz(false);
    }
  }
  
  leituraInterruptorCrua = leituraReal;
}

// ---------------- TIMER AUTO-OFF ----------------
void checkTimerLuz() {
  if (estadoLuz && luzAcesaViaTouch) {
    if (millis() - timerTouch > TEMPO_AUTO_OFF) {
      debugln("TIMER | 4 minutos atingidos. Desligando a luz.");
      alternarLuz(false); // O 'false' indica que não foi acionamento manual
    }
  }
}

// ---------------- STATUS PERIÓDICO ----------------
void checkTelegramMensal() {
  if (millis() - ultimoTelegram > INTERVALO_TELEGRAM) {
    int bat = calcularPercentual(lerTensao());
    String msg = "STATUS\n";
    msg += estadoLuz ? "Luz: ON\n" : "Luz: OFF\n";
    msg += "Bateria: " + String(bat) + "%";
    
    solicitarEnvioTelegram(msg);
    ultimoTelegram = millis();
  }
}

// ---------------- SLEEP ----------------

bool podeDormir() {
  if (modoOTA) return false;
  // Não deve dormir enquanto o Core 0 estiver processando o envio do Telegram
  if (flagEnviarTelegram) return false; 
  // Impede que o ESP32 durma e desarme o pino 3 (Relé)
  if (estadoLuz) return false; 
  
  return true;
}

void entrarLightSleep() {
  debugln("SLEEP | Entrando");

  // Configura despertar pelo Touch (Sempre que for para HIGH)
  gpio_wakeup_enable((gpio_num_t)pinoTouch, GPIO_INTR_HIGH_LEVEL);

  // Configura despertar dinâmico pelo Interruptor
  // Se estiver HIGH, precisa acordar no LOW, e vice-versa, para evitar Loop Infinito
  if (digitalRead(pinoInterruptor) == HIGH) {
    gpio_wakeup_enable((gpio_num_t)pinoInterruptor, GPIO_INTR_LOW_LEVEL);
  } else {
    gpio_wakeup_enable((gpio_num_t)pinoInterruptor, GPIO_INTR_HIGH_LEVEL);
  }

  esp_sleep_enable_gpio_wakeup();

  // O Light Sleep por padrão mantém o estado dos pinos lógicos (a luz não vai apagar)
  esp_light_sleep_start();

  debugln("SLEEP | Acordou");
  ultimaAtividade = millis();
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(pinoTouch, INPUT);
  pinMode(pinoInterruptor, INPUT_PULLUP);
  pinMode(pinoRele, OUTPUT);
  digitalWrite(pinoRele, LOW);

  estadoInterruptor = digitalRead(pinoInterruptor);
  leituraInterruptorCrua = estadoInterruptor;

  // Inicia a tarefa de rede no Core 0
  xTaskCreatePinnedToCore(
    taskRede,     // Função
    "TaskRede",   // Nome
    8192,         // Tamanho da Stack
    NULL,         // Parâmetros
    1,            // Prioridade
    NULL,         // Handle
    0             // Rodar no Core 0 (O void loop roda no Core 1)
  );

  ultimaAtividade = millis();
  debugln("SYSTEM | Iniciado");
}

// ---------------- LOOP ----------------
void loop() {
  handleTouch();
  handleInterruptor();
  checkTimerLuz();
  checkTelegramMensal();

  if (podeDormir() && (millis() - ultimaAtividade > INTERVALO_SLEEP)) {
    entrarLightSleep();
  }
}