# Interruptor Inteligente Infantil com ESP32 💡👶

Este projeto foi desenvolvido para resolver um problema comum em casa: **dar autonomia para uma criança pequena acender e apagar a luz do quarto**, já que ela ainda não alcança o interruptor de parede tradicional. 

O sistema utiliza um ESP32 e integra um botão touch posicionado mais abaixo, mantendo também o funcionamento do interruptor mecânico original. **Tudo isso sem precisar quebrar paredes ou fazer reformas, reaproveitando a instalação e o espelho do interruptor que já estão no local.**

O sistema utiliza um ESP32 e integra um botão touch posicionado mais abaixo, mantendo também o funcionamento do interruptor mecânico original.

## ✨ Funcionalidades

* **Sem Quebra-Quebra:** O hardware pode ser acomodado na caixa 4x2 da parede. O sistema aproveita o interruptor tradicional já existente, mantendo a estética do cômodo e evitando obras.
* **Acionamento Duplo:** A luz pode ser controlada tanto pelo botão touch (acessível à criança) quanto pelo interruptor tradicional na parede.
* **Timer Auto-Off:** Se a luz for acesa via touch (pelos pequenos), ela desliga automaticamente após 3 minutos, evitando desperdício de energia elétrica e da bateria do dispositivo.
* **Anti-Spam (Debounce Avançado):** Trava de 2 segundos no botão touch para evitar que a criança fique piscando a luz repetidamente.
* **Multitarefa (FreeRTOS):** O sistema utiliza os dois núcleos do ESP32. O acionamento físico roda no Core 1 (resposta instantânea), enquanto as requisições de rede rodam no Core 0 (background).
* **Notificações via Telegram:** Envia o status da luz e a porcentagem da bateria diretamente para o seu celular.
* **Ultra Low Power:** Utiliza o *Light Sleep* inteligente do ESP32 para preservar a bateria, acordando instantaneamente apenas quando há interação física.

## 🛠️ Hardware Utilizado

* Placa ESP32
* Módulo Relé (1 Canal)
* Sensor Touch Capacitivo
* Interruptor Mecânico de Parede Tradicional
* Bateria e circuito divisor de tensão (para leitura do ADC)

## 📌 Pinagem (Pinout)

| Componente | Pino ESP32 | Observação |
| :--- | :--- | :--- |
| **Botão Touch** | `GPIO 2` | Configurado para despertar o ESP32 em borda de subida (`HIGH`). *Atenção: Strapping pin.* |
| **Interruptor Físico** | `GPIO 4` | Despertar dinâmico (`HIGH`/`LOW`) para evitar loop infinito de *wake-up*. |
| **Relé** | `GPIO 3` | Acionamento da lâmpada. |
| **Leitura Bateria (ADC)**| `GPIO 0` | Leitura de tensão. *Atenção: Strapping pin.* |

> ⚠️ **Nota Importante de Hardware:** Os pinos `0` e `2` do ESP32 são *Strapping Pins*. Certifique-se de desconectar fisicamente os sensores desses pinos durante o upload do código via USB, ou a placa não entrará em modo de gravação. Reconecte-os após o upload.

## 🚀 Como instalar e usar

1. Clone este repositório.
2. Abra o arquivo `.ino` na IDE do Arduino.
3. Instale as bibliotecas necessárias para ESP32 e WiFiClientSecure.
4. **Altere as credenciais no código:**
   ```cpp
   const char* ssid = "SUA_REDE_WIFI";
   const char* password = "SUA_SENHA_WIFI";
   const char* botToken = "SEU_TOKEN_DO_TELEGRAM";
   const char* chatID = "SEU_CHAT_ID";

5. Selecione a sua placa ESP32 e faça o upload.

🔋 Gerenciamento de Energia
O projeto foi pensado para rodar em bateria. Quando a lâmpada está apagada, o ESP32 entra em Light Sleep, desativando o rádio WiFi e pausando o processamento principal. O despertar ocorre em milissegundos mediante interrupção de hardware, garantindo a mesma responsividade de um interruptor puramente mecânico.
