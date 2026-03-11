#  Atmos: Estação Meteorológica IoT & Smart Assistant

![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![ESP32](https://img.shields.io/badge/ESP32-E7352C?style=for-the-badge&logo=espressif&logoColor=white)
![Telegram](https://img.shields.io/badge/Telegram-2CA5E0?style=for-the-badge&logo=telegram&logoColor=white)
![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white)

O **Atmos** é um sistema de monitoramento ambiental e assistente pessoal de alta performance baseado no microcontrolador ESP32. Desenvolvido em C++, o projeto integra hardware de sensoriamento físico com requisições assíncronas em nuvem, oferecendo controle em tempo real via **Dashboard Web Local** e **Bot interativo no Telegram**.



##  Arquitetura e Funcionalidades Principais

O sistema foi arquitetado com foco em **baixa latência e processamento não-bloqueante (Non-blocking I/O)**, substituindo rotinas tradicionais de `delay()` por controle de concorrência via `millis()`. Isso permite que o ESP32 sirva a página web, monitore interações físicas e processe mensagens do Telegram simultaneamente.

*  **Embedded Web Server (Full-Stack):** Interface UI/UX moderna (Glassmorphism/Dark Mode) servida nativamente pelo ESP32. Inclui requisições AJAX para atualização de dados em tempo real sem recarregamento da página.
*  **Integração Avançada com Telegram:** Bot bidirecional com *Inline Keyboards* para comandos rápidos (Clima, Previsão, Controle de Luzes, Modo Silencioso).
*  **Consumo de Múltiplas APIs:**
    * **OpenWeatherMap API:** Coleta de dados climáticos atuais e previsão de 24h para cálculo de probabilidade de chuva.
    * **IBGE API:** Integração client-side (JavaScript) para listagem dinâmica de Estados e Municípios Brasileiros direto no Dashboard Web.
*  **Eficiência de Hardware:** Sistema de prevenção de *Burn-in* para o display OLED (corte elétrico via comando SSD1306) e modo noturno automatizado.
*  **Painel de Diagnóstico (Dev):** Monitoramento em tempo real do *Heap Memory* (RAM livre), sinal Wi-Fi (RSSI) e *Uptime* do sistema.

##  Hardware Utilizado



* Placa de Desenvolvimento **ESP32 (NodeMCU)**
* Display **OLED 0.96" (I2C / SSD1306)**
* Sensor de Temperatura e Umidade **DHT11**
* **Buzzer Ativo** (Feedback sonoro e alarmes)
* **3x LEDs** (Feedback visual de variação térmica: Verde, Amarelo, Vermelho)
* **2x Push Buttons** (Pull-up interno para controle de estado)

##  Dependências e Bibliotecas

Para compilar este projeto na Arduino IDE, instale as seguintes bibliotecas através do *Library Manager*:

* `WiFi.h`, `WebServer.h`, `HTTPClient.h` (Nativas do ESP32)
* `ArduinoJson` (Manipulação de payloads REST)
* `UniversalTelegramBot` (Integração com a API do Telegram)
* `Adafruit GFX Library` & `Adafruit SSD1306` (Controle do OLED)
* `DHT sensor library` by Adafruit
* `NTPClient` (Sincronização global de tempo)

##  Configuração de Segurança (Importante)

Por razões de segurança, credenciais de rede e tokens de API **não devem ser versionados**. 
1. Crie um arquivo chamado `secrets.h` na mesma pasta do arquivo `.ino`.
2. Adicione as seguintes constantes com os seus dados:

```cpp
// secrets.h
#pragma once

// Credenciais Wi-Fi
#define WIFI_SSID "NOME_DA_SUA_REDE"
#define WIFI_PASSWORD "SUA_SENHA_WIFI"

// OpenWeatherMap
#define OPENWEATHER_API_KEY "SEU_TOKEN_OPENWEATHER"

// Telegram Bot
#define BOT_TOKEN "SEU_TOKEN_DO_BOTFATHER"
#define CHAT_ID "SEU_CHAT_ID_PESSOAL" // Restringe o uso do bot apenas a você
```

 ## Obtendo as Chaves de API
Para que o Atmos funcione, você precisará gerar credenciais gratuitas nos serviços utilizados:

OpenWeatherMap API (Dados de Clima):
Acesse o portal openweathermap.org e crie uma conta gratuita. No seu painel de usuário, vá em "My API Keys" e gere uma nova chave. (Atenção: Novas chaves podem levar até 2 horas para serem validadas pelos servidores).

Telegram Bot Token:
No aplicativo do Telegram, busque pelo bot oficial @BotFather. Envie o comando /newbot e siga as instruções para batizar o seu robô. Ao final, ele fornecerá o seu Token HTTP API.

Telegram Chat ID (Privacidade):
Para garantir que apenas você tenha acesso aos controles da sua casa, busque no Telegram por bots de identificação, como o @UserInfo3Bot. Envie o comando /start e copie a sequência numérica (Your ID).

 ## Como Usar
1. Compilação e Upload:

Com o arquivo secrets.h devidamente preenchido, conecte o ESP32 ao computador via USB, selecione a placa e a porta correspondentes na Arduino IDE e clique em Upload.

2. Inicialização:

Assim que a placa ligar, ela emitirá um bipe de inicialização. O display OLED mostrará o progresso de conexão Wi-Fi e, em seguida, exibirá o endereço IP Local atribuído pelo seu roteador.

3. Acesso ao Dashboard Web:

Em qualquer dispositivo (smartphone, tablet ou PC) conectado à mesma rede Wi-Fi, abra o navegador e digite o endereço IP mostrado no OLED (ex: http://192.168.1.X). Você terá acesso completo aos controles e diagnósticos de performance.

4. Interação com o Bot:

No Telegram, abra o chat do bot que você criou via BotFather. Digite /start para acionar o painel interativo (Inline Keyboard) e comece a enviar comandos para atualizar a cidade, solicitar relatórios de chuva, silenciar o alarme ou apagar a tela fisicamente.

 ## Autor
Desenvolvido por Eduardo Blasczak.
Transformando componentes de eletrônica e protocolos web em produtos inteligentes.
