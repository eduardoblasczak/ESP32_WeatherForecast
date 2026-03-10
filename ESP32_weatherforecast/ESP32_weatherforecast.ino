#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>              
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include "secrets.h" 

// ==========================================
// CONFIGURAÇÕES DO USUÁRIO
// ==========================================
String city = "Curitiba,BR";
const int HORA_ALERTA = 7;
const int MINUTO_ALERTA = 0;

// ==========================================
// DEFINIÇÕES DE HARDWARE E PINOS
// ==========================================
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define I2C_SDA 21
#define I2C_SCL 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define LED_VERDE 25
#define LED_AMARELO 26
#define LED_VERMELHO 27
#define PINO_BUZZER 32

#define BOTAO_RELATORIO 18
#define BOTAO_LUZES 19

// ==========================================
// VARIÁVEIS GLOBAIS DE ESTADO
// ==========================================
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000); 

bool mensagemEnviadaHoje = false;
bool luzesAcesas = true; 

float tempQuartoAtual = 0.0;
float tempForaAtual = -99.0;

bool ultimoEstadoRelatorio = HIGH;
bool ultimoEstadoLuzes = HIGH;

// Temporizadores para o Bot
int tempoVerificacaoBot = 1000; 
unsigned long ultimaVerificacaoBot = 0;

// Variável para a "memória de conversa" do bot
bool esperandoHoraRetorno = false;

// ==========================================
// TECLADOS INLINE (BOTÕES DO TELEGRAM)
// ==========================================
// Teclado principal com 2 linhas
String tecladoPrincipal = "[[{\"text\":\"🌡️ Clima Agora\", \"callback_data\":\"/clima\"}, {\"text\":\"💡 Luzes\", \"callback_data\":\"/luzes\"}], [{\"text\":\"🚪 Vou Sair (Previsão)\", \"callback_data\":\"/sair\"}]]";

// Teclado menor para o final dos relatórios
String tecladoAcoes = "[[{\"text\":\"🔄 Atualizar Clima\", \"callback_data\":\"/clima\"}, {\"text\":\"💡 Luzes\", \"callback_data\":\"/luzes\"}]]";

// ==========================================
// FUNÇÕES DE HARDWARE (BUZZER E LUZES)
// ==========================================
void apitarBuzzer(int tempoMs) {
  digitalWrite(PINO_BUZZER, HIGH);
  delay(tempoMs);
  digitalWrite(PINO_BUZZER, LOW);
}

void atualizarSemaforo(float tempQuarto, float tempFora) {
  if (tempFora == -99.0 || isnan(tempQuarto)) {
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_AMARELO, LOW);
    digitalWrite(LED_VERMELHO, LOW);
    return;
  }
  float diferenca = abs(tempQuarto - tempFora);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARELO, LOW);
  digitalWrite(LED_VERMELHO, LOW);

  if (diferenca <= 2.0) {
    digitalWrite(LED_VERDE, HIGH);   
  } else if (diferenca > 2.0 && diferenca < 4.0) {
    digitalWrite(LED_AMARELO, HIGH); 
  } else if (diferenca >= 4.0) {
    digitalWrite(LED_VERMELHO, HIGH); 
  }
}

// ==========================================
// FUNÇÃO DE PREVISÃO DETALHADA
// ==========================================
void enviarPrevisaoDetalhada(String chat_id, int horaRetorno) {
  if (WiFi.status() == WL_CONNECTED) {
    bot.sendMessage(chat_id, "📡 Buscando dados meteorológicos detalhados...", "");
    apitarBuzzer(100);

    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/forecast?q=" + city + "&units=metric&lang=pt_br&appid=" + String(OPENWEATHER_API_KEY) + "&cnt=8";
    
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(4096); 
      deserializeJson(doc, payload);

      bool precisaGuardaChuva = false;
      float chuvaTotal = 0.0;
      int maxPop = 0; 

      String mensagem = "📅 **Previsão até a meia-noite:**\n\n";

      long fusoHorario = doc["city"]["timezone"];
      
      for (int i = 0; i < doc["list"].size(); i++) {
        long epoch = doc["list"][i]["dt"];
        long localTime = epoch + fusoHorario;
        int horaPrevisao = (localTime % 86400L) / 3600; 

        if (i > 0 && horaPrevisao < ((long)doc["list"][i-1]["dt"] + fusoHorario) % 86400L / 3600) {
           break; 
        }

        float temp = doc["list"][i]["main"]["temp"];
        int probabilidade = (float)doc["list"][i]["pop"] * 100; 
        float volumeChuva = 0.0;
        
        if (doc["list"][i]["rain"]["3h"]) {
          volumeChuva = doc["list"][i]["rain"]["3h"];
        }

        if (horaPrevisao <= horaRetorno || (horaRetorno < timeClient.getHours() && horaPrevisao > timeClient.getHours())) {
           if (probabilidade > 20 || volumeChuva > 0) {
              precisaGuardaChuva = true;
           }
           if (probabilidade > maxPop) maxPop = probabilidade;
           chuvaTotal += volumeChuva;
        }

        mensagem += "🕒 " + String(horaPrevisao) + "h: " + String(temp, 1) + "°C | ☔ " + String(probabilidade) + "%";
        if (volumeChuva > 0) {
           mensagem += " (" + String(volumeChuva, 1) + "mm)";
        }
        mensagem += "\n";
      }

      mensagem += "\n🎯 **Veredito do Guarda-Chuva:**\n";
      if (precisaGuardaChuva) {
        mensagem += "⚠️ SIM! Há " + String(maxPop) + "% de chance de chuva (" + String(chuvaTotal, 1) + "mm esperados) antes de você voltar. Leve o guarda-chuva!";
      } else {
        mensagem += "🕶️ NÃO. O caminho parece limpo até o seu retorno! Pode ir tranquilo.";
      }

      // Envia a previsão acompanhada dos botões de ações rápidas
      bot.sendMessageWithInlineKeyboard(chat_id, mensagem, "", tecladoAcoes);
      Serial.println("Relatório detalhado enviado com sucesso.");
      
    } else {
      bot.sendMessage(chat_id, "Desculpe, ocorreu um erro ao buscar a previsão.", "");
    }
    http.end();
  }
}

// ==========================================
// FUNÇÃO PARA INTERPRETAR AS MENSAGENS DO TELEGRAM
// ==========================================
void lidarComMensagensNovas(int numNovasMensagens) {
  for (int i = 0; i < numNovasMensagens; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Desculpe, você não tem autorização.", "");
      continue;
    }

    String texto = bot.messages[i].text;
    String nome_usuario = bot.messages[i].from_name;

    if (esperandoHoraRetorno) {
      int horaDigitada = texto.toInt();
      
      if (texto.startsWith("/")) {
        esperandoHoraRetorno = false; 
        bot.sendMessageWithInlineKeyboard(chat_id, "Pedido de previsão cancelado.", "", tecladoPrincipal);
      }
      else if (horaDigitada >= 0 && horaDigitada <= 23) {
        esperandoHoraRetorno = false;
        enviarPrevisaoDetalhada(chat_id, horaDigitada);
      } else {
        bot.sendMessage(chat_id, "Formato inválido. Por favor, digite um número de 0 a 23 (ex: 18).", "");
      }
      continue; 
    }

    if (texto == "/start") {
      String boasVindas = "Olá, " + nome_usuario + "!\n";
      boasVindas += "Eu sou o seu Assistente Matinal IoT.\n\n";
      boasVindas += "Selecione uma opção abaixo:";
      
      // Envia a mensagem com o Teclado Principal embutido!
      bot.sendMessageWithInlineKeyboard(chat_id, boasVindas, "", tecladoPrincipal);
    }
    else if (texto == "/clima") {
      enviarRelatorioMatinal(tempQuartoAtual);
    }
    else if (texto == "/sair") {
      esperandoHoraRetorno = true;
      bot.sendMessage(chat_id, "Vai sair? Que horas você pretende voltar? (Digite apenas a hora em formato 24h, ex: 18)", "");
    }
    else if (texto == "/luzes") {
      luzesAcesas = !luzesAcesas; 
      apitarBuzzer(100);
      if (!luzesAcesas) {
        display.clearDisplay();
        display.display();
        display.ssd1306_command(SSD1306_DISPLAYOFF); 
        
        digitalWrite(LED_VERDE, LOW);
        digitalWrite(LED_AMARELO, LOW);
        digitalWrite(LED_VERMELHO, LOW);
        
        bot.sendMessageWithInlineKeyboard(chat_id, "🌙 Modo Noturno ativado. Luzes apagadas.", "", tecladoAcoes);
      } else {
        display.ssd1306_command(SSD1306_DISPLAYON); 
        
        atualizarDisplay(tempQuartoAtual, tempForaAtual);
        atualizarSemaforo(tempQuartoAtual, tempForaAtual);
        
        bot.sendMessageWithInlineKeyboard(chat_id, "☀️ Luzes acesas novamente.", "", tecladoAcoes);
      }
    }
    else {
      bot.sendMessageWithInlineKeyboard(chat_id, "Comando não reconhecido. Use os botões abaixo:", "", tecladoPrincipal);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(PINO_BUZZER, OUTPUT);
  pinMode(BOTAO_RELATORIO, INPUT_PULLUP);
  pinMode(BOTAO_LUZES, INPUT_PULLUP);
  
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARELO, LOW);
  digitalWrite(LED_VERMELHO, LOW);
  digitalWrite(PINO_BUZZER, LOW);

  dht.begin();
  Wire.begin(I2C_SDA, I2C_SCL);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Falha ao iniciar o OLED"));
    for(;;); 
  }
  
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,20);
  display.print("Conectando Wi-Fi...");
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); 
  secured_client.setInsecure(); 
  
  timeClient.begin();
  Serial.println("\nSistema Iniciado!");
  apitarBuzzer(500); 
}

void loop() {
  timeClient.update();
  
  if (millis() - ultimaVerificacaoBot > tempoVerificacaoBot) {
    int numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);
    while (numNovasMensagens) {
      lidarComMensagensNovas(numNovasMensagens);
      numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);
    }
    ultimaVerificacaoBot = millis();
  }

  bool estadoRelatorio = digitalRead(BOTAO_RELATORIO);
  if (estadoRelatorio == LOW && ultimoEstadoRelatorio == HIGH) {
    apitarBuzzer(100); 
    enviarRelatorioMatinal(tempQuartoAtual);
  }
  ultimoEstadoRelatorio = estadoRelatorio;

  bool estadoLuzes = digitalRead(BOTAO_LUZES);
  if (estadoLuzes == LOW && ultimoEstadoLuzes == HIGH) {
    luzesAcesas = !luzesAcesas; 
    apitarBuzzer(100); 
    if (!luzesAcesas) {
      display.clearDisplay();
      display.display();
      display.ssd1306_command(SSD1306_DISPLAYOFF); 
      
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_AMARELO, LOW);
      digitalWrite(LED_VERMELHO, LOW);
    } else {
      display.ssd1306_command(SSD1306_DISPLAYON); 
      
      atualizarDisplay(tempQuartoAtual, tempForaAtual);
      atualizarSemaforo(tempQuartoAtual, tempForaAtual);
    }
  }
  ultimoEstadoLuzes = estadoLuzes;

  static unsigned long ultimoUpdateSensores = 0;
  if (millis() - ultimoUpdateSensores > 2000 || ultimoUpdateSensores == 0) {
    tempQuartoAtual = dht.readTemperature(); 
    static unsigned long lastUpdateClima = 0;
    if (millis() - lastUpdateClima > 600000 || lastUpdateClima == 0) { 
      tempForaAtual = buscarTemperaturaExterna();
      lastUpdateClima = millis();
    }
    if (luzesAcesas) {
      atualizarDisplay(tempQuartoAtual, tempForaAtual);
      atualizarSemaforo(tempQuartoAtual, tempForaAtual);
    }
    ultimoUpdateSensores = millis(); 
  }

  int horaAtual = timeClient.getHours();
  int minutoAtual = timeClient.getMinutes();

  if (horaAtual == 0 && minutoAtual == 0) {
    mensagemEnviadaHoje = false;
  }
  if (horaAtual == HORA_ALERTA && minutoAtual == MINUTO_ALERTA && !mensagemEnviadaHoje) {
    enviarRelatorioMatinal(tempQuartoAtual);
    mensagemEnviadaHoje = true;
  }
  
  delay(50); 
}

float buscarTemperaturaExterna() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&units=metric&lang=pt_br&appid=" + String(OPENWEATHER_API_KEY);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      float temp = doc["main"]["temp"];
      http.end();
      return temp;
    }
    http.end();
  }
  return -99.0;
}

void enviarRelatorioMatinal(float tempQuarto) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&units=metric&lang=pt_br&appid=" + String(OPENWEATHER_API_KEY);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      float tempFora = doc["main"]["temp"];
      String condicao = doc["weather"][0]["description"];
      String mainWeather = doc["weather"][0]["main"];
      float diferenca = tempQuarto - tempFora;

      String mensagem = "☀️ Relatório Sob Demanda:\n\n";
      mensagem += "🏠 Temp. no Quarto: " + String(tempQuarto, 1) + "°C\n";
      mensagem += "☁️ Temp. lá Fora: " + String(tempFora, 1) + "°C\n";
      mensagem += "📊 Diferença: " + String(abs(diferenca), 1) + "°C ";
      
      if (diferenca > 0) {
        mensagem += "mais quente no quarto.\n";
      } else {
        mensagem += "mais frio no quarto.\n";
      }
      mensagem += "\nCondição: " + condicao + "\n";

      // Aqui usamos a nova função para colocar os botões no fim do relatório!
      bool enviado = bot.sendMessageWithInlineKeyboard(CHAT_ID, mensagem, "", tecladoAcoes);
      
      if (enviado) {
        Serial.println("✅ Sucesso: Relatório enviado!");
        apitarBuzzer(200);
        delay(100);
        apitarBuzzer(200);
      }
    }
    http.end();
  }
}

void atualizarDisplay(float tempQuarto, float tempFora) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("  Assistente Matinal");
  display.drawLine(0, 10, 128, 10, WHITE);
  
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print("In: ");
  display.print(tempQuarto, 1);
  display.setTextSize(1);
  display.print("C");

  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print("Out:");
  
  if (tempFora == -99.0) {
    display.print("Erro");
  } else {
    display.print(tempFora, 1);
    display.setTextSize(1);
    display.print("C");
  }
  display.display();
}