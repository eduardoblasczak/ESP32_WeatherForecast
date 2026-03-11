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
#include <WebServer.h>

#include "secrets.h" 

// ==========================================
// CONFIGURAÇÕES DO USUÁRIO
// ==========================================
String city = "Curitiba,PR,BR"; 
String nomeCidade = "Curitiba"; 

int HORA_ALERTA = 7;   
int MINUTO_ALERTA = 0; 

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

WebServer server(80);

bool mensagemEnviadaHoje = false;
bool luzesAcesas = true; 
bool modoSilencioso = false; 

float tempQuartoAtual = 0.0;
float umidadeQuartoAtual = 0.0; 
float tempForaAtual = -99.0;
String condicaoFora = "Buscando...";

String statusTelegram = "Aguardando envio...";
String ultimaAtualizacaoClima = "Aguardando...";

bool ultimoEstadoRelatorio = HIGH;
bool ultimoEstadoLuzes = HIGH;

int tempoVerificacaoBot = 1000; 
unsigned long ultimaVerificacaoBot = 0;

// Variáveis de conversação do Bot
bool esperandoHoraRetorno = false;
bool esperandoNomeCidade = false; 

// Teclados do Telegram
String tecladoPrincipal = "[[{\"text\":\"🌡️ Clima\", \"callback_data\":\"/clima\"}, {\"text\":\"💡 Luzes\", \"callback_data\":\"/luzes\"}], [{\"text\":\"🚪 Vou Sair\", \"callback_data\":\"/sair\"}, {\"text\":\"📍 Cidade\", \"callback_data\":\"/cidade\"}], [{\"text\":\"🔇 Som\", \"callback_data\":\"/silencioso\"}]]";
String tecladoAcoes = "[[{\"text\":\"🔄 Atualizar\", \"callback_data\":\"/clima\"}, {\"text\":\"💡 Luzes\", \"callback_data\":\"/luzes\"}], [{\"text\":\"📍 Cidade\", \"callback_data\":\"/cidade\"}, {\"text\":\"🔇 Som\", \"callback_data\":\"/silencioso\"}]]";

// ==========================================
// PÁGINA HTML COMPLETA (Atmos)
// ==========================================
const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Atmos IoT</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;600&display=swap');
    :root {
      --bg: #0f172a; --glass: rgba(255, 255, 255, 0.05); --border: rgba(255, 255, 255, 0.1);
      --text: #f8fafc; --accent: #3b82f6; --accent-hover: #2563eb;
    }
    body {
      font-family: 'Poppins', sans-serif; background-color: var(--bg); color: var(--text);
      margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center;
      background-image: radial-gradient(circle at top left, #1e293b, #0f172a); min-height: 100vh;
    }
    h1 { font-weight: 600; margin-bottom: 5px; text-align: center; animation: fadeIn 1s ease-in; letter-spacing: 1px;}
    p.subtitle { color: #94a3b8; font-size: 0.9em; margin-bottom: 20px; text-align: center;}
    
    .tabs { display: flex; gap: 10px; margin-bottom: 20px; width: 100%; max-width: 600px; justify-content: center;}
    .tab-btn {
      background: var(--glass); border: 1px solid var(--border); color: #cbd5e1;
      padding: 8px 20px; border-radius: 20px; cursor: pointer; transition: all 0.3s; font-family: inherit;
    }
    .tab-btn.active { background: var(--accent); color: white; border-color: var(--accent); box-shadow: 0 0 10px rgba(59,130,246,0.5); }
    .tab-content { display: none; width: 100%; max-width: 600px; flex-direction: column; gap: 20px; animation: fadeIn 0.4s ease; }
    .tab-content.active { display: flex; }

    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 15px; }
    .card {
      background: var(--glass); border: 1px solid var(--border); border-radius: 16px; padding: 20px;
      text-align: center; backdrop-filter: blur(10px); box-shadow: 0 4px 6px rgba(0,0,0,0.1);
      transition: transform 0.3s ease;
    }
    .card:hover { transform: translateY(-5px); box-shadow: 0 10px 15px rgba(0,0,0,0.2); }
    .card h3 { margin: 0 0 10px 0; font-size: 1em; color: #cbd5e1; font-weight: 400; }
    .card .value { font-size: 2em; font-weight: 600; margin: 0; }
    .card .sub { font-size: 0.8em; color: #64748b; }

    .sys-card { padding: 15px; text-align: left; }
    .sys-card h3 { font-size: 0.9em; color: #94a3b8; margin-bottom: 5px;}
    .sys-card .value { font-size: 1.2em; }

    .btn {
      background: linear-gradient(135deg, var(--accent), #6366f1); border: none; padding: 12px 20px;
      color: white; font-family: inherit; font-size: 1em; border-radius: 8px; cursor: pointer;
      font-weight: 600; transition: all 0.3s ease; width: 100%; box-shadow: 0 4px 15px rgba(59,130,246,0.4);
    }
    .btn:hover { transform: scale(1.02); filter: brightness(1.1); }
    .btn:active { transform: scale(0.98); }
    .btn-dark { background: #334155; box-shadow: none; }
    .btn-dark:hover { background: #475569; }

    .settings {
      background: var(--glass); border: 1px solid var(--border); border-radius: 16px; padding: 20px;
      display: flex; justify-content: space-between; align-items: center; gap: 10px; flex-wrap: wrap;
    }
    
    select, input[type="time"] {
      background: rgba(0,0,0,0.2); border: 1px solid var(--border); color: white;
      padding: 10px; border-radius: 8px; font-family: inherit; font-size: 0.9em; outline: none; width: 100%;
    }
    select option { background: #1e293b; color: white; }

    .status { text-align: center; font-size: 0.8em; color: #64748b; margin-top: 20px; }
    @keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
  </style>
</head>
<body>
  <h1>Atmos</h1>
  <p class="subtitle" id="top-status">Conectado</p>

  <div class="tabs">
    <button class="tab-btn active" onclick="showTab('dash', this)">🏠 Painel</button>
    <button class="tab-btn" onclick="showTab('sys', this)">⚙️ Sistema</button>
  </div>

  <div id="dash" class="tab-content active">
    <div class="grid">
      <div class="card">
        <h3>🏠 Sensor Interno</h3>
        <p class="value" id="t-quarto">--°C</p>
        <p class="sub" id="h-quarto">Umidade: --%</p>
      </div>
      <div class="card">
        <h3 id="nome-cidade-web">☁️ Lá Fora</h3>
        <p class="value" id="t-fora">--°C</p>
        <p class="sub" id="c-fora">--</p>
      </div>
    </div>

    <div class="grid">
      <button class="btn btn-dark" id="btn-luz" onclick="sendCommand('luzes')">💡 Modo Noturno</button>
      <button class="btn" id="btn-som" onclick="sendCommand('silencioso')">🔊 Som: Ligado</button>
      <button class="btn" onclick="sendCommand('relatorio')" style="grid-column: 1 / -1;">📩 Enviar Relatório ao Telegram</button>
    </div>

    <div class="settings">
      <div style="flex: 1; min-width: 150px;">
        <h3 style="margin: 0; font-size: 1em;">⏰ Alarme</h3>
        <p style="margin: 0; font-size: 0.8em; color: #94a3b8;">Notificação Matinal</p>
      </div>
      <div style="display: flex; gap: 10px;">
        <input type="time" id="alarm-time" value="07:00">
        <button class="btn" style="width: auto; padding: 10px;" onclick="saveAlarm()">Salvar</button>
      </div>
    </div>

    <div class="settings" style="flex-direction: column; align-items: flex-start;">
      <div style="width: 100%;">
        <h3 style="margin: 0; font-size: 1em;">📍 Localização (Brasil)</h3>
        <p style="margin: 0 0 10px 0; font-size: 0.8em; color: #94a3b8;">Atual: <span id="current-city" style="color:#fff;">Carregando...</span></p>
      </div>
      <div style="display: flex; gap: 10px; width: 100%; flex-wrap: wrap;">
        <div style="flex: 1; min-width: 100px;">
          <select id="estado-select" onchange="loadCities()">
            <option value="">Estado...</option>
          </select>
        </div>
        <div style="flex: 2; min-width: 150px;">
          <select id="cidade-select">
            <option value="">Selecione o Estado primeiro...</option>
          </select>
        </div>
        <button class="btn" style="width: 100%; padding: 10px;" onclick="saveCity()">Salvar Cidade</button>
      </div>
    </div>
  </div>

  <div id="sys" class="tab-content">
    <div class="grid">
      <div class="card sys-card">
        <h3>📡 Sinal Wi-Fi</h3>
        <p class="value" id="sys-wifi">-- dBm</p>
      </div>
      <div class="card sys-card">
        <h3>🧠 Memória RAM Livre</h3>
        <p class="value" id="sys-heap">-- KB</p>
      </div>
      <div class="card sys-card">
        <h3>⏱️ Tempo Ligado (Uptime)</h3>
        <p class="value" id="sys-uptime">--</p>
      </div>
      <div class="card sys-card">
        <h3>💬 Telegram Status</h3>
        <p class="value" style="font-size: 1em; color: #3b82f6;" id="sys-tg">--</p>
      </div>
      <div class="card sys-card" style="grid-column: 1 / -1;">
        <h3>🌤️ Última Atualização do Clima</h3>
        <p class="value" style="font-size: 1em;" id="sys-weather">--</p>
      </div>
    </div>
  </div>

  <div class="status">Sincronizado: <span id="last-update">Agora</span></div>

  <script>
    function showTab(tabId, btn) {
      document.querySelectorAll('.tab-content').forEach(tab => tab.classList.remove('active'));
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.getElementById(tabId).classList.add('active');
      btn.classList.add('active');
    }

    function formatUptime(seconds) {
      let h = Math.floor(seconds / 3600); let m = Math.floor((seconds % 3600) / 60); let s = seconds % 60;
      return `${h}h ${m}m ${s}s`;
    }

    function loadStates() {
      fetch('https://servicodados.ibge.gov.br/api/v1/localidades/estados?orderBy=nome')
        .then(res => res.json())
        .then(estados => {
          let select = document.getElementById('estado-select');
          estados.forEach(est => {
            let opt = document.createElement('option');
            opt.value = est.sigla; 
            opt.innerText = est.nome;
            select.appendChild(opt);
          });
        });
    }

    function loadCities() {
      let uf = document.getElementById('estado-select').value;
      let citySelect = document.getElementById('cidade-select');
      citySelect.innerHTML = '<option value="">Carregando...</option>';
      
      if(uf) {
        fetch(`https://servicodados.ibge.gov.br/api/v1/localidades/estados/${uf}/municipios?orderBy=nome`)
          .then(res => res.json())
          .then(cidades => {
            citySelect.innerHTML = '<option value="">Selecione a Cidade...</option>';
            cidades.forEach(cid => {
              let opt = document.createElement('option');
              opt.value = cid.nome; 
              opt.innerText = cid.nome;
              citySelect.appendChild(opt);
            });
          });
      } else {
        citySelect.innerHTML = '<option value="">Selecione o Estado primeiro...</option>';
      }
    }

    function fetchData() {
      fetch('/api/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('t-quarto').innerText = data.t_quarto + '°C';
          document.getElementById('h-quarto').innerText = 'Umidade: ' + data.h_quarto + '%';
          document.getElementById('nome-cidade-web').innerText = '☁️ ' + data.cidade;
          document.getElementById('current-city').innerText = data.cidade; 
          document.getElementById('t-fora').innerText = data.t_fora + '°C';
          document.getElementById('c-fora').innerText = data.c_fora;
          document.getElementById('top-status').innerText = '🟢 Wi-Fi: ' + data.wifi_rssi + ' dBm';
          
          let btnLuz = document.getElementById('btn-luz');
          if(data.luzes) {
            btnLuz.innerText = "💡 Desligar Luzes"; btnLuz.className = "btn btn-dark";
          } else {
            btnLuz.innerText = "🌙 Ligar Luzes"; btnLuz.className = "btn";
          }

          let btnSom = document.getElementById('btn-som');
          if(data.silencioso) {
            btnSom.innerText = "🔇 Som: Mudo"; btnSom.className = "btn btn-dark";
          } else {
            btnSom.innerText = "🔊 Som: Ligado"; btnSom.className = "btn";
          }

          if(document.activeElement !== document.getElementById('alarm-time')) {
            let h = data.hora.toString().padStart(2, '0'); let m = data.minuto.toString().padStart(2, '0');
            document.getElementById('alarm-time').value = `${h}:${m}`;
          }

          document.getElementById('sys-wifi').innerText = data.wifi_rssi + ' dBm';
          document.getElementById('sys-heap').innerText = (data.heap / 1024).toFixed(1) + ' KB';
          document.getElementById('sys-uptime').innerText = formatUptime(data.uptime);
          document.getElementById('sys-tg').innerText = data.status_tg;
          document.getElementById('sys-weather').innerText = data.status_weather;
          
          let now = new Date(); document.getElementById('last-update').innerText = now.toLocaleTimeString();
        });
    }

    function sendCommand(cmd) { fetch('/api/command?action=' + cmd).then(() => fetchData()); }
    
    function saveAlarm() {
      let timeVal = document.getElementById('alarm-time').value;
      if(timeVal) {
        let parts = timeVal.split(':');
        fetch(`/api/config?h=${parts[0]}&m=${parts[1]}`).then(() => alert('⏰ Alarme atualizado!'));
      }
    }

    function saveCity() {
      let uf = document.getElementById('estado-select').value;
      let city = document.getElementById('cidade-select').value;
      
      if(uf && city) {
        let apiSearch = city + "," + uf + ",BR";
        fetch(`/api/config?citySearch=${encodeURIComponent(apiSearch)}&cityName=${encodeURIComponent(city)}`).then(() => {
          alert('📍 Localização atualizada com sucesso! Buscando clima...');
          fetchData(); 
        });
      } else {
        alert('Por favor, selecione o Estado e a Cidade.');
      }
    }

    loadStates();
    fetchData();
    setInterval(fetchData, 3000);
  </script>
</body>
</html>
)rawliteral";

// ==========================================
// FUNÇÕES AUXILIARES
// ==========================================
String formatarURL(String texto) {
  String original = texto;
  original.replace(" ", "%20");
  return original;
}

void apitarBuzzer(int tempoMs) {
  if (!modoSilencioso) {
    digitalWrite(PINO_BUZZER, HIGH); delay(tempoMs); digitalWrite(PINO_BUZZER, LOW);
  }
}

void atualizarSemaforo(float tempQuarto, float tempFora) {
  if (tempFora == -99.0 || isnan(tempQuarto)) {
    digitalWrite(LED_VERDE, LOW); digitalWrite(LED_AMARELO, LOW); digitalWrite(LED_VERMELHO, LOW);
    return;
  }
  float diferenca = abs(tempQuarto - tempFora);
  digitalWrite(LED_VERDE, LOW); digitalWrite(LED_AMARELO, LOW); digitalWrite(LED_VERMELHO, LOW);

  if (diferenca <= 2.0) digitalWrite(LED_VERDE, HIGH);   
  else if (diferenca > 2.0 && diferenca < 4.0) digitalWrite(LED_AMARELO, HIGH); 
  else if (diferenca >= 4.0) digitalWrite(LED_VERMELHO, HIGH); 
}

// ==========================================
// FUNÇÕES DO SERVIDOR WEB E API
// ==========================================
void handleRoot() { server.send(200, "text/html", PAGE_HTML); }

void handleData() {
  String json = "{";
  json += "\"t_quarto\":" + String(tempQuartoAtual, 1) + ",";
  json += "\"h_quarto\":" + String(umidadeQuartoAtual, 1) + ",";
  json += "\"t_fora\":" + String(tempForaAtual, 1) + ",";
  json += "\"c_fora\":\"" + condicaoFora + "\",";
  json += "\"cidade\":\"" + nomeCidade + "\",";
  json += "\"luzes\":" + String(luzesAcesas ? "true" : "false") + ",";
  json += "\"silencioso\":" + String(modoSilencioso ? "true" : "false") + ",";
  json += "\"hora\":" + String(HORA_ALERTA) + ",";
  json += "\"minuto\":" + String(MINUTO_ALERTA) + ",";
  json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"status_tg\":\"" + statusTelegram + "\",";
  json += "\"status_weather\":\"" + ultimaAtualizacaoClima + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleCommand() {
  if (server.hasArg("action")) {
    String action = server.arg("action");
    if (action == "luzes") {
      luzesAcesas = !luzesAcesas; apitarBuzzer(100);
      if (!luzesAcesas) {
        display.clearDisplay(); display.display(); delay(50); 
        display.ssd1306_command(SSD1306_DISPLAYOFF); 
        digitalWrite(LED_VERDE, LOW); digitalWrite(LED_AMARELO, LOW); digitalWrite(LED_VERMELHO, LOW);
      } else {
        display.ssd1306_command(SSD1306_DISPLAYON); 
        atualizarDisplay(tempQuartoAtual, tempForaAtual); atualizarSemaforo(tempQuartoAtual, tempForaAtual);
      }
    } 
    else if (action == "silencioso") {
      modoSilencioso = !modoSilencioso; 
      if (!modoSilencioso) apitarBuzzer(100);
    }
    else if (action == "relatorio") { apitarBuzzer(100); enviarRelatorioMatinal(tempQuartoAtual); }
  }
  server.send(200, "text/plain", "OK");
}

void handleConfig() {
  if (server.hasArg("h") && server.hasArg("m")) {
    HORA_ALERTA = server.arg("h").toInt(); MINUTO_ALERTA = server.arg("m").toInt();
  }
  
  if (server.hasArg("citySearch") && server.hasArg("cityName")) {
    city = server.arg("citySearch");   
    nomeCidade = server.arg("cityName"); 
    apitarBuzzer(100);
    buscarTemperaturaExterna(); 
    atualizarDisplay(tempQuartoAtual, tempForaAtual); 
  } 
  else if (server.hasArg("city")) {
    city = server.arg("city");
    nomeCidade = city;
    apitarBuzzer(100);
    buscarTemperaturaExterna(); 
    atualizarDisplay(tempQuartoAtual, tempForaAtual); 
  }
  
  server.send(200, "text/plain", "OK");
}

// ==========================================
// FUNÇÕES DE CLIMA E TELEGRAM
// ==========================================
void enviarPrevisaoDetalhada(String chat_id, int horaRetorno) {
  if (WiFi.status() == WL_CONNECTED) {
    bot.sendMessage(chat_id, "📡 Buscando dados meteorológicos...", "");
    apitarBuzzer(100);
    HTTPClient http;
    http.begin("http://api.openweathermap.org/data/2.5/forecast?q=" + formatarURL(city) + "&units=metric&lang=pt_br&appid=" + String(OPENWEATHER_API_KEY) + "&cnt=8");
    if (http.GET() > 0) {
      DynamicJsonDocument doc(4096); deserializeJson(doc, http.getString());
      bool precisaGuardaChuva = false; float chuvaTotal = 0.0; int maxPop = 0; 
      String mensagem = "📅 **Previsão em " + nomeCidade + " até a meia-noite:**\n\n";
      long fusoHorario = doc["city"]["timezone"];
      
      for (int i = 0; i < doc["list"].size(); i++) {
        long localTime = (long)doc["list"][i]["dt"] + fusoHorario;
        int horaPrevisao = (localTime % 86400L) / 3600; 
        if (i > 0 && horaPrevisao < ((long)doc["list"][i-1]["dt"] + fusoHorario) % 86400L / 3600) break; 
        
        float temp = doc["list"][i]["main"]["temp"];
        int probabilidade = (float)doc["list"][i]["pop"] * 100; 
        float volumeChuva = doc["list"][i]["rain"]["3h"] ? (float)doc["list"][i]["rain"]["3h"] : 0.0;

        if (horaPrevisao <= horaRetorno || (horaRetorno < timeClient.getHours() && horaPrevisao > timeClient.getHours())) {
           if (probabilidade > 20 || volumeChuva > 0) precisaGuardaChuva = true;
           if (probabilidade > maxPop) maxPop = probabilidade;
           chuvaTotal += volumeChuva;
        }

        mensagem += "🕒 " + String(horaPrevisao) + "h: " + String(temp, 1) + "°C | ☔ " + String(probabilidade) + "%";
        if (volumeChuva > 0) mensagem += " (" + String(volumeChuva, 1) + "mm)";
        mensagem += "\n";
      }
      mensagem += "\n🎯 **Veredito do Guarda-Chuva:**\n";
      if (precisaGuardaChuva) mensagem += "⚠️ SIM! Há " + String(maxPop) + "% de chance de chuva (" + String(chuvaTotal, 1) + "mm) antes do seu retorno!";
      else mensagem += "🕶️ NÃO. O caminho parece limpo! Pode ir tranquilo.";

      if(bot.sendMessageWithInlineKeyboard(chat_id, mensagem, "", tecladoAcoes)) {
        statusTelegram = "Previsão enviada às " + timeClient.getFormattedTime();
      }
    } 
    http.end();
  }
}

void lidarComMensagensNovas(int numNovasMensagens) {
  for (int i = 0; i < numNovasMensagens; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) continue;

    String texto = bot.messages[i].text; String nome_usuario = bot.messages[i].from_name;

    if (esperandoHoraRetorno) {
      if (texto.startsWith("/")) { esperandoHoraRetorno = false; bot.sendMessageWithInlineKeyboard(chat_id, "Cancelado.", "", tecladoPrincipal); }
      else if (texto.toInt() >= 0 && texto.toInt() <= 23) { esperandoHoraRetorno = false; enviarPrevisaoDetalhada(chat_id, texto.toInt()); }
      else bot.sendMessage(chat_id, "Formato inválido. Digite 0 a 23.", "");
      continue; 
    }
    
    if (esperandoNomeCidade) {
      if (texto.startsWith("/")) { 
        esperandoNomeCidade = false; 
        bot.sendMessageWithInlineKeyboard(chat_id, "Configuração de cidade cancelada.", "", tecladoPrincipal); 
      } else { 
        esperandoNomeCidade = false; 
        city = texto; 
        nomeCidade = texto; 
        
        bot.sendMessage(chat_id, "📍 Cidade atualizada para: " + city + "\nBuscando novos dados meteorológicos...", "");
        apitarBuzzer(100);
        
        buscarTemperaturaExterna(); 
        atualizarDisplay(tempQuartoAtual, tempForaAtual);
        enviarRelatorioMatinal(tempQuartoAtual); 
      }
      continue; 
    }

    if (texto == "/start") { bot.sendMessageWithInlineKeyboard(chat_id, "Olá, " + nome_usuario + "!\nEu sou o Atmos, seu assistente IoT.\nSelecione uma opção:", "", tecladoPrincipal); }
    else if (texto == "/clima") enviarRelatorioMatinal(tempQuartoAtual);
    else if (texto == "/sair") { esperandoHoraRetorno = true; bot.sendMessage(chat_id, "A que horas você pretende voltar? (formato 24h, ex: 18)", ""); }
    else if (texto == "/cidade") {
      esperandoNomeCidade = true;
      bot.sendMessage(chat_id, "📍 Qual cidade você quer monitorar agora?\nDigite o nome (ex: São Paulo,BR ou Lisboa,PT):", "");
    }
    else if (texto == "/luzes") {
      luzesAcesas = !luzesAcesas; apitarBuzzer(100);
      if (!luzesAcesas) {
        display.clearDisplay(); display.display(); delay(50); 
        display.ssd1306_command(SSD1306_DISPLAYOFF); 
        digitalWrite(LED_VERDE, LOW); digitalWrite(LED_AMARELO, LOW); digitalWrite(LED_VERMELHO, LOW);
        bot.sendMessageWithInlineKeyboard(chat_id, "🌙 Modo Noturno ativado.", "", tecladoAcoes);
      } else {
        display.ssd1306_command(SSD1306_DISPLAYON); 
        atualizarDisplay(tempQuartoAtual, tempForaAtual); atualizarSemaforo(tempQuartoAtual, tempForaAtual);
        bot.sendMessageWithInlineKeyboard(chat_id, "☀️ Luzes acesas.", "", tecladoAcoes);
      }
    }
    else if (texto == "/silencioso") {
      modoSilencioso = !modoSilencioso; 
      if (!modoSilencioso) apitarBuzzer(100);
      String msg = modoSilencioso ? "🔇 Modo Silencioso ativado. O dispositivo não emitirá sons." : "🔊 Som ativado. O buzzer voltou a funcionar.";
      bot.sendMessageWithInlineKeyboard(chat_id, msg, "", tecladoAcoes);
    }
  }
}

float buscarTemperaturaExterna() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://api.openweathermap.org/data/2.5/weather?q=" + formatarURL(city) + "&units=metric&lang=pt_br&appid=" + String(OPENWEATHER_API_KEY));
    if (http.GET() > 0) {
      DynamicJsonDocument doc(1024); deserializeJson(doc, http.getString());
      condicaoFora = doc["weather"][0]["description"].as<String>(); 
      float temp = doc["main"]["temp"];
      ultimaAtualizacaoClima = "Sucesso às " + timeClient.getFormattedTime(); 
      http.end();
      return temp;
    }
    http.end();
  }
  ultimaAtualizacaoClima = "Erro de conexão";
  return -99.0;
}

void enviarRelatorioMatinal(float tempQuarto) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://api.openweathermap.org/data/2.5/weather?q=" + formatarURL(city) + "&units=metric&lang=pt_br&appid=" + String(OPENWEATHER_API_KEY));
    if (http.GET() > 0) {
      DynamicJsonDocument doc(1024); deserializeJson(doc, http.getString());
      float tempFora = doc["main"]["temp"]; float diferenca = tempQuarto - tempFora;
      
      String mensagem = "☀️ Relatório Atmos:\n\n🏠 Sensor Interno: " + String(tempQuarto, 1) + "°C\n💧 Umidade: " + String(umidadeQuartoAtual, 1) + "%\n☁️ Temp. em " + nomeCidade + ": " + String(tempFora, 1) + "°C\n\nCondição: " + doc["weather"][0]["description"].as<String>();
      
      if (bot.sendMessageWithInlineKeyboard(CHAT_ID, mensagem, "", tecladoAcoes)) {
        apitarBuzzer(200); delay(100); apitarBuzzer(200);
        statusTelegram = "Enviado às " + timeClient.getFormattedTime(); 
      } else {
        statusTelegram = "Erro no envio";
      }
    }
    http.end();
  }
}

void atualizarDisplay(float tempQuarto, float tempFora) {
  display.clearDisplay(); display.setTextSize(1); display.setCursor(0, 0);
  display.print("IP: " + WiFi.localIP().toString()); 
  display.drawLine(0, 10, 128, 10, WHITE);
  
  display.setTextSize(2); display.setCursor(0, 20);
  display.print("In: "); display.print(tempQuarto, 1); display.setTextSize(1); display.print("C");
  display.setTextSize(2); display.setCursor(0, 45);
  display.print("Out:"); 
  if (tempFora == -99.0) display.print("Erro"); else { display.print(tempFora, 1); display.setTextSize(1); display.print("C"); }
  display.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_VERDE, OUTPUT); pinMode(LED_AMARELO, OUTPUT); pinMode(LED_VERMELHO, OUTPUT); pinMode(PINO_BUZZER, OUTPUT);
  pinMode(BOTAO_RELATORIO, INPUT_PULLUP); pinMode(BOTAO_LUZES, INPUT_PULLUP);
  
  digitalWrite(LED_VERDE, LOW); digitalWrite(LED_AMARELO, LOW); digitalWrite(LED_VERMELHO, LOW); digitalWrite(PINO_BUZZER, LOW);

  dht.begin(); Wire.begin(I2C_SDA, I2C_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for(;;); 
  
  display.clearDisplay(); display.setTextColor(WHITE); display.setCursor(0,20);
  display.print("Conectando Wi-Fi..."); display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  server.on("/", handleRoot);
  server.on("/api/data", handleData);
  server.on("/api/command", handleCommand);
  server.on("/api/config", handleConfig);
  server.begin(); 

  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); secured_client.setInsecure(); 
  timeClient.begin(); timeClient.update();
  
  Serial.println("\nSistema Iniciado!");
  Serial.print("🌍 Acesse a interface Web em: http://"); Serial.println(WiFi.localIP());
  apitarBuzzer(500); 
}

void loop() {
  timeClient.update();
  server.handleClient(); 
  
  if (millis() - ultimaVerificacaoBot > tempoVerificacaoBot) {
    int numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);
    while (numNovasMensagens) { lidarComMensagensNovas(numNovasMensagens); numNovasMensagens = bot.getUpdates(bot.last_message_received + 1); }
    ultimaVerificacaoBot = millis();
  }

  if (digitalRead(BOTAO_RELATORIO) == LOW && ultimoEstadoRelatorio == HIGH) { apitarBuzzer(100); enviarRelatorioMatinal(tempQuartoAtual); }
  ultimoEstadoRelatorio = digitalRead(BOTAO_RELATORIO);

  if (digitalRead(BOTAO_LUZES) == LOW && ultimoEstadoLuzes == HIGH) {
    luzesAcesas = !luzesAcesas; apitarBuzzer(100); 
    if (!luzesAcesas) {
      display.clearDisplay(); display.display(); delay(50); 
      display.ssd1306_command(SSD1306_DISPLAYOFF); 
      digitalWrite(LED_VERDE, LOW); digitalWrite(LED_AMARELO, LOW); digitalWrite(LED_VERMELHO, LOW);
    } else { 
      display.ssd1306_command(SSD1306_DISPLAYON); 
      atualizarDisplay(tempQuartoAtual, tempForaAtual); atualizarSemaforo(tempQuartoAtual, tempForaAtual); 
    }
  }
  ultimoEstadoLuzes = digitalRead(BOTAO_LUZES);

  static unsigned long ultimoUpdateSensores = 0;
  if (millis() - ultimoUpdateSensores > 2000 || ultimoUpdateSensores == 0) {
    tempQuartoAtual = dht.readTemperature(); 
    umidadeQuartoAtual = dht.readHumidity();

    static unsigned long lastUpdateClima = 0;
    if (millis() - lastUpdateClima > 600000 || lastUpdateClima == 0) { 
      tempForaAtual = buscarTemperaturaExterna(); lastUpdateClima = millis();
    }
    if (luzesAcesas) { atualizarDisplay(tempQuartoAtual, tempForaAtual); atualizarSemaforo(tempQuartoAtual, tempForaAtual); }
    ultimoUpdateSensores = millis(); 
  }

  int horaAtual = timeClient.getHours(); int minutoAtual = timeClient.getMinutes();
  if (horaAtual == 0 && minutoAtual == 0) mensagemEnviadaHoje = false;
  if (horaAtual == HORA_ALERTA && minutoAtual == MINUTO_ALERTA && !mensagemEnviadaHoje) { enviarRelatorioMatinal(tempQuartoAtual); mensagemEnviadaHoje = true; }
  delay(10); 
}