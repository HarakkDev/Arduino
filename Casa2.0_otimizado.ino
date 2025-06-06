#include <SPI.h>
#include <Ethernet.h>
#include <Servo.h>

// Configuração de rede
const byte mac[] PROGMEM = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
const IPAddress ip(192, 168, 2, 177);
EthernetServer server(80);

// Definição dos pinos
const byte janelaBanheiro = 5;
const byte janelaVaranda = 6;
const byte pinoPortaoCasa1 = 8;
const byte pinoPortaoCasa2 = 9;
const byte pinoPortaoCasa3 = 11;
const byte pinoPortaoCasa4 = 12;
const byte pinoLaser = 4;

const byte pinoQuarto = 22;
const byte pinoVaranda = 23;
const byte pinoSala = 24;
const byte pinoCozinha = 25;
const byte pinoBanheiro = 26;
const byte pinoGaragem = 27;
const byte pinoPoste1 = 28;
const byte pinoPoste2 = 29;

const byte sensorChuva = 30;
const byte sensorFogo = 31;
const byte pinoVentilador = 32;
const byte pinoVelocidadeVentilador = 3;
const byte pinoBuzzer = 33;
const byte pinoLdrLaser = A0;
const byte pinoLdrPostes = A1;

Servo meuServo1;
Servo meuServo2;
Servo meuServo3;
Servo meuServo4;

// Variáveis globais
uint16_t valorLdrLaser = 0;
uint16_t valorLdrPostes = 0;
uint16_t valorSensorAgua = 0;
uint16_t valorSensorFogo = 0;

// Timers para millis()
unsigned long tempoUltimaLeituraChuva = 0;
unsigned long tempoUltimaLeituraFogo = 0;
unsigned long tempoUltimaLeituraLDR = 0;
unsigned long tempoUltimoLoop = 0;
const unsigned long intervaloLeituraChuva = 1000;
const uint16_t intervaloLeituraFogo = 300;
const uint16_t intervaloLeituraLDR = 500;

byte estadoChuvaAtual = HIGH, estadoChuvaAnterior = HIGH;
byte estadoFogoAtual = HIGH, estadoFogoAnterior = HIGH;
byte velocidadePortao = 2;
const uint16_t portaoAbrirFechar = 12750;

const uint16_t limiteLaser = 450;
const uint16_t limitePoste = 930;

bool estadoDoAlarme = false;
bool alertaEnviado = false;
bool laserInterrompido = false;
bool estadoDoAlarmeAnterior = false;
unsigned long tempoAtivacaoAlarme = 0;
unsigned long tempoUltimoApito = 0;
unsigned long tempoUltimaMensagem = 0;
const uint16_t intervaloApito = 50;
const uint16_t intervaloMensagem = 1000;

bool portaoCasaFechado = true;
bool portaoGaragemFechado = true;
bool janelaBanheiroFechado = true;
bool janelaVarandaFechado = true;
bool varalRecolhido = true;
bool alertaAtivo = false;
bool janelaAutomatica = false;

// Estados das lâmpadas
bool luzQuarto = false;
bool luzVaranda = false;
bool luzSala = false;
bool luzCozinha = false;
bool luzBanheiro = false;
bool luzGaragem = false;

void setup() {
  Serial.begin(9600);
  Ethernet.begin(mac, ip);
  server.begin();

  // Configura pinos das lâmpadas
  for (byte pino = 22; pino <= 29; pino++) {
    pinMode(pino, OUTPUT);
    digitalWrite(pino, LOW);
  }
  pinMode(pinoBuzzer, OUTPUT);
  digitalWrite(pinoBuzzer, LOW);

  // Configura outros pinos
  pinMode(pinoLdrLaser, INPUT);
  pinMode(pinoLdrPostes, INPUT);
  pinMode(pinoLaser, OUTPUT);
  pinMode(sensorChuva, INPUT);
  pinMode(sensorFogo, INPUT);
  pinMode(pinoVentilador, OUTPUT);
  pinMode(pinoVelocidadeVentilador, OUTPUT);

  desligarPinos();

  digitalWrite(pinoVentilador, HIGH);

  // Posição inicial do portão da garagem, varandas e varal
  meuServo1.attach(2);
  delay(100);
  meuServo1.write(165);
  delay(1000);
  meuServo1.detach();

  meuServo2.attach(5);
  delay(100);
  meuServo2.write(110);
  delay(1000);
  meuServo2.detach();

  meuServo3.attach(6);
  delay(100);
  meuServo3.write(0);
  delay(1000);
  meuServo3.detach();

  meuServo4.attach(7);
  delay(100);
  meuServo4.write(180);
  delay(1000);
  meuServo4.detach();

  Serial.print(F("Servidor web iniciado. Acesse: "));
  Serial.println(Ethernet.localIP());
}

void loop() {
  unsigned long tempoAtual = millis();
  processarServidorWeb();
  gerenciarAlarme(tempoAtual);

  if (tempoAtual - tempoUltimaLeituraLDR > intervaloLeituraLDR) {
    tempoUltimaLeituraLDR = tempoAtual;
    controlarPostes();
  }

  if (tempoAtual - tempoUltimaLeituraFogo > intervaloLeituraFogo) {
    tempoUltimaLeituraFogo = tempoAtual;
    detectarFogo();
  }

  if (tempoAtual - tempoUltimaLeituraChuva > intervaloLeituraChuva) {
    tempoUltimaLeituraChuva = tempoAtual;
    detectarAgua();
  }
}


void processarServidorWeb() {
  EthernetClient client = server.available();
  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.startsWith("GET /sensors")) {
            handleSensorsRequest(client);
            break;
          } else if (currentLine.length() == 0) {
            handleRootRequest(client);
            break;
          }
          currentLine = "";
        } else if (c != '\r') {
          currentLine += c;
          handleClientCommands(currentLine, client);
        }
      }
    }
    client.stop();
  }
}

void handleSensorsRequest(EthernetClient &client) {
  valorSensorAgua = digitalRead(sensorChuva);
  valorSensorFogo = digitalRead(sensorFogo);
  
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Connection: close"));
  client.println();
  
  client.print(F("{"));
  client.print(F("\"fogo\":"));
  client.print(valorSensorFogo == HIGH ? 0 : 1);
  client.print(F(",\"chuva\":"));
  client.print(valorSensorAgua == HIGH ? 0 : 1);
  client.print(F(",\"automatico\":"));
  client.print(janelaAutomatica ? 1 : 0); // Novo campo
  client.print(F(",\"alarme\":"));
  client.print(estadoDoAlarme ? 1 : 0); // Nova linha
  client.print(F(",\"alerta\":\""));
  
  if (estadoDoAlarme && valorLdrLaser > limiteLaser && !alertaAtivo) {
    client.print(F("LASER INTERROMPIDO!! CASA INVADIDA!!!!"));
    alertaAtivo = true;
  } else if (valorLdrLaser <= limiteLaser) {
    alertaAtivo = false;
    client.print(F(""));
  } else {
    client.print(F(""));
  }
  client.println(F("\"}"));
}


void handleRootRequest(EthernetClient &client) {
  // Cabeçalho HTTP
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close"));
  client.println();

  // Início do HTML
  client.println(F("<!DOCTYPE html><html>"));
  client.println(F("<head>"));
  client.println(F("<meta charset='UTF-8'>"));
  client.println(F("<meta name='viewport' content='width=device-width, initial-scale=1.0'>"));
  client.println(F("<title>Casa automatizada</title>"));

  // CSS
  client.println(F("<style>"));
  client.println(F("body {"));
  client.println(F("  font-family: Arial, sans-serif;"));
  client.println(F("  background-color: #343541;"));
  client.println(F("  color: #ffffff;"));
  client.println(F("  text-align: center;"));
  client.println(F("  padding: 20px;"));
  client.println(F("  margin: 0;"));
  client.println(F("}"));
  client.println(F(".container {"));
  client.println(F("  background-color: #444654;"));
  client.println(F("  padding: 20px;"));
  client.println(F("  border-radius: 10px;"));
  client.println(F("  box-shadow: 0px 0px 10px rgba(0,0,0,0.3);"));
  client.println(F("  max-width: 1000px;")); // aumentado para dar mais espaço
  client.println(F("  margin: 0 auto;"));
  client.println(F("  display: flex;"));
  client.println(F("  flex-wrap: wrap;"));  // importante para evitar transbordo
  client.println(F("  gap: 65px;"));
  client.println(F("  color: #ffffff;"));
  client.println(F("}"));
  client.println(F(".column {"));
  client.println(F("  flex: 1 1 400px;"));  // garante colunas responsivas
  client.println(F("  padding: 0 10px;"));
  client.println(F("}"));
  client.println(F("h1, h2 { color: #ffffff; margin-bottom: 10px; }"));
  client.println(F(".row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 5px; padding: 8px; }"));
  client.println(F(".row:nth-child(even) { background-color: #555666; }"));
  client.println(F(".label { font-weight: bold; width: 100px; text-align: left; white-space: nowrap; }"));
  client.println(F("button { padding: 8px 12px; font-size: 14px; cursor: pointer; border: none; border-radius: 10px; white-space: nowrap; }"));
  client.println(F(".ligar { background-color: #218838; color: white; }"));
  client.println(F(".desligar { background-color: #c82333; color: white; }"));
  client.println(F(".ligarVent { display: inline-block; width: 32%; background-color: #218838; color: white; margin: 1%; border-radius: 8px; }"));
  client.println(F(".desligarVent { display: block; background-color: #c82333; color: white; width: 100%; margin: 5px 0; border-radius: 10px; }"));
  client.println(F(".button-row { display: flex; flex-wrap: wrap; justify-content: space-between; margin-bottom: 10px; gap: 10px; }"));
  client.println(F(".ligarAlarme, .desligarAlarme, .abrir, .fechar { flex: 1; min-width: 120px; margin: 5px; padding: 10px 20px; font-size: 16px; cursor: pointer; border: none; border-radius: 10px; color: white; }"));
  client.println(F(".ligarAlarme, .abrir { background-color: #218838; }"));
  client.println(F(".desligarAlarme, .fechar { background-color: #c82333; }"));
  client.println(F(".sensores-container { border: 2px solid #999; border-radius: 10px; padding: 15px; margin-top: 20px; background-color: #2e2e3a; }"));
  client.println(F(".sensor-fogo::before { content: '🔥 '; font-size: 18px; }"));
  client.println(F(".sensor-chuva::before { content: '💧 '; font-size: 18px; }"));
  client.println(F(".row button.ligar, .row button.desligar { width: 30%; font-size: 14px; }"));
  client.println(F("</style>"));

  // JavaScript
  client.println(F("<script>"));
  client.println(F("function sendCommand(command) {"));
  client.println(F("  var xhttp = new XMLHttpRequest();"));
  client.println(F("  xhttp.onreadystatechange = function() { if (this.readyState==4 && this.status==200) { console.log('Sent '+command); }};"));
  client.println(F("  xhttp.open('GET','/?'+command,true); xhttp.send();"));
  client.println(F("}"));
  client.println(F("function updateSensors() {"));
  client.println(F("  var xhttp = new XMLHttpRequest(); xhttp.onreadystatechange=function(){ if(this.readyState==4&&this.status==200){var data=JSON.parse(this.responseText); document.getElementById('statusFogo').innerText=data.fogo ? 'FOGO DETECTADO!!':'Sem fogo.'; document.getElementById('statusChuva').innerText=data.chuva ? 'CHUVA DETECTADA!!':'Sem chuva.'; document.getElementById('statusAutomatico').innerText=data.automatico?'Ligada':'Desligada'; document.getElementById('statusAlarme').innerText=data.alarme?'Ligado':'Desligado'; if(data.alerta) alert(data.alerta);} }; xhttp.open('GET','/sensors',true); xhttp.send(); }"));
  client.println(F("setInterval(updateSensors,1000);"));
  client.println(F("</script>"));
  client.println(F("</head><body>"));

  // Container principal com duas colunas
  client.println(F("<div class='container'>"));

  // Coluna 1: Lâmpadas, Alarme, Ventilador
  client.println(F("<div class='column'>"));

  // Lâmpadas
  client.println(F("<h1>Lâmpadas</h1>"));
  const char* lampadas[] = {"Quarto","Varanda","Sala","Cozinha","Banheiro","Garagem","Luz Geral"};
  const char* ids[]     = {"luzQuarto","luzVaranda","luzSala","luzCozinha","luzBanheiro","luzGaragem","luzGeral"};
  for (int i=0;i<7;i++) {
    client.print(F("<div class='row'><span class='label'>"));
    client.print(lampadas[i]);
    client.print(F("</span><button onclick=\"sendCommand('"));
    client.print(ids[i]); client.print(F("=ligar')\" class='ligar'>Ligar</button>"));
    client.print(F("<button onclick=\"sendCommand('"));
    client.print(ids[i]); client.print(F("=desligar')\" class='desligar'>Desligar</button></div>"));
  }

  // Alarme
  client.println(F("<h2>Alarme</h2><div class='button-row'>"));
  client.println(F("<button onclick=\"sendCommand('alarm=Ligar Alarme')\" class='ligarAlarme'>Ligar Alarme</button>"));
  client.println(F("<button onclick=\"sendCommand('alarm=Desligar Alarme')\" class='desligarAlarme'>Desligar Alarme</button>"));
  client.println(F("</div>"));
  client.print(F("<p>Status do Alarme: <strong id='statusAlarme'>"));
  client.print(estadoDoAlarme? F("Ligado"):F("Desligado"));
  client.println(F("</strong></p>"));

  // Ventilador
  client.println(F("<h2>Ventilador</h2><div class='row' style='margin-bottom:0;'>"));
  client.println(F("<button onclick=\"sendCommand('ventilador=ligarMin')\" class='ligarVent'>Velocidade mínima</button>"));
  client.println(F("<button onclick=\"sendCommand('ventilador=ligarMed')\" class='ligarVent'>Velocidade média</button>"));
  client.println(F("<button onclick=\"sendCommand('ventilador=ligarMax')\" class='ligarVent'>Velocidade máxima</button></div>"));
  client.println(F("<button onclick=\"sendCommand('ventilador=desligar')\" class='desligarVent'>Desligar Ventilador</button>"));
  client.println(F("</div>")); // fim coluna 1

  // Coluna 2: Portões, Janelas, Sensores
  client.println(F("<div class='column'>"));

  // Portão da casa
  client.println(F("<h2>Portão da casa</h2><div class='button-row'>"));
  client.println(F("<button onclick=\"sendCommand('portaoCasa=abrir')\" class='abrir'>Abrir portão</button>"));
  client.println(F("<button onclick=\"sendCommand('portaoCasa=fechar')\" class='fechar'>Fechar portão</button>"));
  client.println(F("</div>"));

  // Portão da garagem
  client.println(F("<h2>Portão da Garagem</h2><div class='button-row'>"));
  client.println(F("<button onclick=\"sendCommand('portaoGaragem=abrir')\" class='abrir'>Abrir garagem</button>"));
  client.println(F("<button onclick=\"sendCommand('portaoGaragem=fechar')\" class='fechar'>Fechar garagem</button>"));
  client.println(F("</div>"));

  // Varal
  client.println(F("<h2>Varal de Roupas</h2><div class='button-row'>"));
  client.println(F("<button onclick=\"sendCommand('varal=abrir')\" class='abrir'>Estender Varal</button>"));
  client.println(F("<button onclick=\"sendCommand('varal=fechar')\" class='fechar'>Recolher Varal</button>"));
  client.println(F("</div>"));

  // Janelas
  client.println(F("<h2>Janelas da Casa</h2>"));
  const char* jnames[] = {"Banheiro","Varanda","Ambas"};
  const char* jids[]   = {"janelaBanheiro","janelaVaranda","janelasAmbas"};
  for(int i=0;i<3;i++){
    client.println(F("<div class='button-row'>"));
    client.print(F("<button onclick=\"sendCommand('"));
    client.print(jids[i]); client.print(F("=abrir')\" class='abrir'>Abrir ")); client.print(jnames[i]); client.println(F("</button>"));
    client.print(F("<button onclick=\"sendCommand('"));
    client.print(jids[i]); client.print(F("=fechar')\" class='fechar'>Fechar ")); client.print(jnames[i]); client.println(F("</button></div>"));
  }

  // Janelas e varal automatico
  client.println(F("<h2>Janelas e Varal automatizado</h2><div class='button-row'>"));
  client.println(F("<button onclick=\"sendCommand('janelaAutomatica=abrir')\" class='abrir'>Ativar automação</button>"));
  client.println(F("<button onclick=\"sendCommand('janelaAutomatica=fechar')\" class='fechar'>Desativar automação</button>"));
  client.println(F("</div>"));
  client.print(F("<p>Status: <strong id='statusAutomatico'>"));
  client.print(janelaAutomatica? F("Ligada"):F("Desligada"));
  client.println(F("</strong></p>"));

  client.println(F("<div class='sensores-container'>"));
  client.println(F("<h2>Sensores</h2>"));

  client.println(F("<p class='sensor-fogo'><strong id='statusFogo'>"));
  client.print(valorSensorFogo ? F("Sem fogo.") : F("FOGO DETECTADO!!"));
  client.println(F("</strong></p>"));

  client.println(F("<p class='sensor-chuva'><strong id='statusChuva'>"));
  client.print(valorSensorAgua ? F("Sem chuva.") : F("CHUVA DETECTADA!!"));
  client.println(F("</strong></p>"));

  client.println(F("</div>"));

  client.println(F("</div>")); // fim coluna 2
  client.println(F("</div>")); // fim container

  // Fecha HTML
  client.println(F("</body></html>"));
  client.println();
}


void handleClientCommands(String &currentLine, EthernetClient &client) {
  if (currentLine.endsWith("GET /?luzQuarto=ligar")) {
            luzQuarto = true;
            digitalWrite(pinoQuarto, HIGH);
          } else if (currentLine.endsWith("GET /?luzQuarto=desligar")) {
            luzQuarto = false;
            digitalWrite(pinoQuarto, LOW);
          } else if (currentLine.endsWith("GET /?luzVaranda=ligar")) {
            luzVaranda = true;
            digitalWrite(pinoVaranda, HIGH);
          } else if (currentLine.endsWith("GET /?luzVaranda=desligar")) {
            luzVaranda = false;
            digitalWrite(pinoVaranda, LOW);
          } else if (currentLine.endsWith("GET /?luzSala=ligar")) {
            luzSala = true;
            digitalWrite(pinoSala, HIGH);
          } else if (currentLine.endsWith("GET /?luzSala=desligar")) {
            luzSala = false;
            digitalWrite(pinoSala, LOW);
          } else if (currentLine.endsWith("GET /?luzCozinha=ligar")) {
            luzCozinha = true;
            digitalWrite(pinoCozinha, HIGH);
          } else if (currentLine.endsWith("GET /?luzCozinha=desligar")) {
            luzCozinha = false;
            digitalWrite(pinoCozinha, LOW);
          } else if (currentLine.endsWith("GET /?luzBanheiro=ligar")) {
            luzBanheiro = true;
            digitalWrite(pinoBanheiro, HIGH);
          } else if (currentLine.endsWith("GET /?luzBanheiro=desligar")) {
            luzBanheiro = false;
            digitalWrite(pinoBanheiro, LOW);
          } else if (currentLine.endsWith("GET /?luzGaragem=ligar")) {
            luzGaragem = true;
            digitalWrite(pinoGaragem, HIGH);
          } else if (currentLine.endsWith("GET /?luzGaragem=desligar")) {
            luzGaragem = false;
            digitalWrite(pinoGaragem, LOW);
          } else if (currentLine.endsWith("GET /?luzGeral=ligar")) {
            ligarTodasLuzes();
          } else if (currentLine.endsWith("GET /?luzGeral=desligar")) {
            desligarTodasLuzes();
  } else if (currentLine.endsWith("GET /?alarm=Ligar%20Alarme")) {
    if (!estadoDoAlarme) {
      estadoDoAlarme = true;
      digitalWrite(pinoLaser, HIGH);
      tempoAtivacaoAlarme = millis();
    }
  } else if (currentLine.endsWith("GET /?alarm=Desligar%20Alarme")) {
  if (estadoDoAlarme) {
    estadoDoAlarme = false;
    alertaAtivo = false;
    digitalWrite(pinoLaser, LOW);
    noTone(pinoBuzzer);
    Serial.println(F("Alarme desligado."));
}
  } else if (currentLine.endsWith("GET /?ventilador=desligar")) {  
  digitalWrite(pinoVentilador, LOW);
  Serial.println(F("Ventilador desligado"));
  } else if (currentLine.endsWith("GET /?ventilador=ligarMin")) {
  digitalWrite(pinoVentilador, HIGH);
  analogWrite(pinoVelocidadeVentilador, 100);
  Serial.println(F("Ventilador ligado na velocidade mínima"));
  } else if (currentLine.endsWith("GET /?ventilador=ligarMed")) {
  digitalWrite(pinoVentilador, HIGH);
  analogWrite(pinoVelocidadeVentilador, 140);
  Serial.println(F("Ventilador ligado na velocidade média"));
  } else if (currentLine.endsWith("GET /?ventilador=ligarMax")) {
  digitalWrite(pinoVentilador, HIGH);
  analogWrite(pinoVelocidadeVentilador, 255);
  Serial.println(F("Ventilador ligado na velocidade máxima")); 
  } else if (currentLine.endsWith("GET /?portaoCasa=abrir")) {
    abrirPortao(); 
  } else if (currentLine.endsWith("GET /?portaoCasa=fechar")) {
    fecharPortao();
  } else if (currentLine.endsWith("GET /?portaoGaragem=abrir")) {
    abrirPortaoGaragem();
  } else if (currentLine.endsWith("GET /?portaoGaragem=fechar")) {
    fecharPortaoGaragem();
  } else if (currentLine.endsWith("GET /?janelaBanheiro=abrir")) {
    abrirJanelaBanheiro();
  } else if (currentLine.endsWith("GET /?janelaBanheiro=fechar")) {
    fecharJanelaBanheiro();     
  } else if (currentLine.endsWith("GET /?janelaVaranda=abrir")) {
    abrirJanelaVaranda();
  } else if (currentLine.endsWith("GET /?janelaVaranda=fechar")) {
    fecharJanelaVaranda();  
  } else if (currentLine.endsWith("GET /?janelasAmbas=abrir")) {
    abrirJanelaVaranda();
    delay(500);
    abrirJanelaBanheiro();
  } else if (currentLine.endsWith("GET /?janelasAmbas=fechar")) {
    fecharJanelaVaranda();
    delay(500);
    fecharJanelaBanheiro();
  } else if (currentLine.endsWith("GET /?janelaAutomatica=abrir")) {
    if(!janelaAutomatica){
    Serial.print(F("Janela automatizada ativada!"));
    janelaAutomatica = true;
    } 
  } else if (currentLine.endsWith("GET /?janelaAutomatica=fechar")) {
    if(janelaAutomatica){
    Serial.print(F("Janela automatizada desativada!"));
    janelaAutomatica = false;
    }  
  } else if (currentLine.endsWith("GET /?varal=abrir")){
    estenderVaral();
  } else if (currentLine.endsWith("GET /?varal=fechar")){
    recolherVaral();
  }
}

void gerenciarAlarme(unsigned long tempoAtual) {
  if (estadoDoAlarme != estadoDoAlarmeAnterior) {
    if (estadoDoAlarme) {
      laserInterrompido = false;
      digitalWrite(pinoLaser, HIGH);
      tempoAtivacaoAlarme = tempoAtual;
      Serial.println(F("Alarme ligado."));
    } else {
      digitalWrite(pinoLaser, LOW);
      noTone(pinoBuzzer);
      Serial.println(F("Alarme desligado."));
    }
    estadoDoAlarmeAnterior = estadoDoAlarme;
  }

  if (estadoDoAlarme && (tempoAtual - tempoAtivacaoAlarme > 200)) {
    valorLdrLaser = analogRead(pinoLdrLaser);

    if (valorLdrLaser <= limiteLaser) {
      if (laserInterrompido) {
        laserInterrompido = false;
        Serial.print(F("Laser detectado! LDR: "));
        Serial.println(valorLdrLaser);
        noTone(pinoBuzzer);
      }
    } else {
      if (!laserInterrompido) {
        laserInterrompido = true;
        Serial.print(F("Laser interrompido!!! Casa invadida!! LDR: "));
        Serial.println(valorLdrLaser);
      }

      // Controle do buzzer
      if (tempoAtual - tempoUltimoApito > intervaloApito) {
        tempoUltimoApito = tempoAtual;
        bool buzzerLigado = (digitalRead(pinoBuzzer) == HIGH);
        if (buzzerLigado) {
          noTone(pinoBuzzer);
        } else {
          tone(pinoBuzzer, 2000);
          desligarTodasLuzes();
          delay(200);
          ligarTodasLuzes();
          delay(200);
        }
      }
    }

    if (tempoAtual - tempoUltimaMensagem > intervaloMensagem) {
      tempoUltimaMensagem = tempoAtual;
      Serial.print(laserInterrompido ? "Laser interrompido!!! " : "Laser detectado. ");
      Serial.print(F("LDR: "));
      Serial.println(valorLdrLaser);
    }
  }
}

void controlarPostes() {
  valorLdrPostes = analogRead(pinoLdrPostes);
  bool postesLigados = (valorLdrPostes > limitePoste);
  digitalWrite(pinoPoste1, postesLigados ? HIGH : LOW);
  digitalWrite(pinoPoste2, postesLigados ? HIGH : LOW);
  
  static unsigned long ultimoLogPostes = 0;
  if (millis() - ultimoLogPostes > 5000) {
    ultimoLogPostes = millis();
    Serial.print(postesLigados ? "Noite, postes ligados. " : "Dia, postes desligados. ");
    Serial.print(F("Valor do LDR: "));
    Serial.println(valorLdrPostes);
  }
}

void detectarFogo() {
  int leituraFogo = digitalRead(sensorFogo);
  
  if (leituraFogo == estadoFogoAnterior) {
    estadoFogoAtual = leituraFogo;
  }
  estadoFogoAnterior = leituraFogo;
  
  if (estadoFogoAtual == LOW) {
    tone(pinoBuzzer, 2000);
    digitalWrite(pinoCozinha,HIGH);
    delay(200);
    digitalWrite(pinoCozinha,LOW);
    delay(200);
  } else {
    noTone(pinoBuzzer);
  }
}

void detectarAgua(){
    int leituraChuva = digitalRead(sensorChuva);

    if (leituraChuva == estadoChuvaAnterior) {
        estadoChuvaAtual = leituraChuva;
    }
    estadoChuvaAnterior = leituraChuva;

    if (estadoChuvaAtual == LOW && janelaAutomatica == true) {
      recolherVaral();
    } else if (estadoChuvaAtual == HIGH && janelaAutomatica == true) {
      estenderVaral();
    }

    if (estadoChuvaAtual == LOW && janelaAutomatica == true) {
        fecharJanelaBanheiro();  
    }
    else if (estadoChuvaAtual == HIGH && janelaAutomatica == true) {
        abrirJanelaBanheiro();  
    }

    if (estadoChuvaAtual == LOW && janelaAutomatica == true) {
        fecharJanelaVaranda();  
    }
    else if (estadoChuvaAtual == HIGH && janelaAutomatica == true) {
        abrirJanelaVaranda();  
    }
}

// -------------------------------------------------------------------------
// -------------- FUNÇÃO DE ABRIR E FECHAR PORTÃO DA CASA ------------------
// -------------------------------------------------------------------------
void abrirPortao() {
    if(portaoCasaFechado){
    Serial.print(F("Portão abrindo..."));
    unsigned long tempoDeAbrir = millis();
    ligarPinos();
    while (millis() - tempoDeAbrir < portaoAbrirFechar) {
    digitalWrite(pinoPortaoCasa1, LOW);
    digitalWrite(pinoPortaoCasa2, LOW);
    digitalWrite(pinoPortaoCasa3, HIGH);
    digitalWrite(pinoPortaoCasa4, HIGH);
    delay(velocidadePortao);

    digitalWrite(pinoPortaoCasa1, LOW);
    digitalWrite(pinoPortaoCasa2, HIGH);
    digitalWrite(pinoPortaoCasa3, HIGH);
    digitalWrite(pinoPortaoCasa4, LOW);
    delay(velocidadePortao);

    digitalWrite(pinoPortaoCasa1, HIGH);
    digitalWrite(pinoPortaoCasa2, HIGH);
    digitalWrite(pinoPortaoCasa3, LOW);
    digitalWrite(pinoPortaoCasa4, LOW);
    delay(velocidadePortao);

    digitalWrite(pinoPortaoCasa1, HIGH);
    digitalWrite(pinoPortaoCasa2, LOW);
    digitalWrite(pinoPortaoCasa3, LOW);
    digitalWrite(pinoPortaoCasa4, HIGH);
    delay(velocidadePortao);
    }
  desligarPinos();
  Serial.print(F("Portão aberto!"));
  portaoCasaFechado = false;
  } else
  Serial.print(F("Portão ja aberto!"));
}

void fecharPortao() {
    if(!portaoCasaFechado){
    Serial.print(F("Portão fechando..."));
    unsigned long tempoDeAbrir = millis();
    ligarPinos();
    while (millis() - tempoDeAbrir < portaoAbrirFechar) {
    digitalWrite(pinoPortaoCasa1, HIGH);
    digitalWrite(pinoPortaoCasa2, LOW);
    digitalWrite(pinoPortaoCasa3, LOW);
    digitalWrite(pinoPortaoCasa4, HIGH);
    delay(velocidadePortao);

    digitalWrite(pinoPortaoCasa1, HIGH);
    digitalWrite(pinoPortaoCasa2, HIGH);
    digitalWrite(pinoPortaoCasa3, LOW);
    digitalWrite(pinoPortaoCasa4, LOW);
    delay(velocidadePortao);

    digitalWrite(pinoPortaoCasa1, LOW);
    digitalWrite(pinoPortaoCasa2, HIGH);
    digitalWrite(pinoPortaoCasa3, HIGH);
    digitalWrite(pinoPortaoCasa4, LOW);
    delay(velocidadePortao);

    digitalWrite(pinoPortaoCasa1, LOW);
    digitalWrite(pinoPortaoCasa2, LOW);
    digitalWrite(pinoPortaoCasa3, HIGH);
    digitalWrite(pinoPortaoCasa4, HIGH);
    delay(velocidadePortao);
    }
  desligarPinos();
  portaoCasaFechado = true;
  Serial.print(F("Portão fechado!"));
  } else
  Serial.print(F("Portão ja fechado!"));
}

// -------------------------------------------------------------------------
// ------------ FUNÇÃO DE ABRIR E FECHAR PORTÃO DA GARAGEM -----------------
// -------------------------------------------------------------------------
void abrirPortaoGaragem() {
  if (portaoGaragemFechado) {
    Serial.println(F("Abrindo garagem..."));
    meuServo1.attach(2);
    delay(100);
    meuServo1.write(50);
    delay(1000); 
    meuServo1.detach();      
    delay(100);             
    Serial.println(F("Garagem aberta!"));
    portaoGaragemFechado = false;
  }
}

void fecharPortaoGaragem() {
  if (!portaoGaragemFechado) {
    Serial.println(F("Fechando garagem..."));
    meuServo1.attach(2);
    delay(100);
    meuServo1.write(165);
    delay(1000);
    meuServo1.detach();
    delay(100);              
    Serial.println(F("Garagem fechada!"));
    portaoGaragemFechado = true;
  }
}

// -------------------------------------------------------------------------
// ---------------- FUNÇÃO DE ABRIR E FECHAR JANELAS -----------------------
// -------------------------------------------------------------------------
void abrirJanelaBanheiro() {
  if (janelaBanheiroFechado) {
    Serial.println(F("Abrindo janela do Banheiro..."));
    meuServo2.attach(5);
    delay(100);
    meuServo2.write(0);
    delay(1000); 
    meuServo2.detach();      
    delay(100);  
    Serial.println(F("Janela do Banheiro aberta!"));
    janelaBanheiroFechado = false;
  }
}

void fecharJanelaBanheiro() {
  if (!janelaBanheiroFechado) {
    Serial.println(F("Fechando janela do Banheiro..."));
    meuServo2.attach(5);
    delay(100);
    meuServo2.write(110);
    delay(1000); 
    meuServo2.detach(); 
    delay(100);           
    Serial.println(F("Janela do Banheiro fechada!"));
    janelaBanheiroFechado = true;
  }
}

void abrirJanelaVaranda() {
  if (janelaVarandaFechado) {
    Serial.println(F("Abrindo janela da Varanda..."));
    meuServo3.attach(6);
    delay(100);
    meuServo3.write(110);
    delay(1000); 
    meuServo3.detach();     
    delay(100);    
    Serial.println(F("Janela da Varanda aberta!"));
    janelaVarandaFechado = false;
  }
}

void fecharJanelaVaranda() {
  if (!janelaVarandaFechado) {
    Serial.println(F("Fechando janela da Varanda..."));
    meuServo3.attach(6);
    delay(100);
    meuServo3.write(0);
    delay(1000); 
    meuServo3.detach(); 
    delay(100);  
    Serial.println(F("Janela da Varanda fechada!"));
    janelaVarandaFechado = true;
  }
}

// -------------------------------------------------------------------------
// ------------------ FUNÇÃO DE RECOLHER VARAL -----------------------------
// -------------------------------------------------------------------------
void estenderVaral() {
  if (varalRecolhido) {
    Serial.println(F("Estendendo Varal..."));
    meuServo4.attach(7);
    delay(100);
    meuServo4.write(5);
    delay(1000); 
    meuServo4.detach(); 
    delay(100);     
    Serial.println(F("Varal estendido!"));
    varalRecolhido = false;
  }
}

void recolherVaral() {
  if (!varalRecolhido) {
    Serial.println(F("Recolendo Varal..."));
    meuServo4.attach(7);
    delay(100);
    meuServo4.write(180);
    delay(1000); 
    meuServo4.detach();
    delay(100);
    Serial.println(F("Varal recolhido!"));
    varalRecolhido = true;
  }
}

// -------------------------------------------------------------------------
// ------------------ FUNÇÃO LUZES GERAL -----------------------------------
// -------------------------------------------------------------------------

void ligarTodasLuzes() {
  luzQuarto = true;
  luzVaranda = true;
  luzSala = true;
  luzCozinha = true;
  luzBanheiro = true;
  luzGaragem = true;
  
  digitalWrite(pinoQuarto, HIGH);
  digitalWrite(pinoVaranda, HIGH);
  digitalWrite(pinoSala, HIGH);
  digitalWrite(pinoCozinha, HIGH);
  digitalWrite(pinoBanheiro, HIGH);
  digitalWrite(pinoGaragem, HIGH);
  if(!estadoDoAlarme)
  Serial.println(F("Todas as luzes foram ligadas"));
}

void desligarTodasLuzes() {
  luzQuarto = false;
  luzVaranda = false;
  luzSala = false;
  luzCozinha = false;
  luzBanheiro = false;
  luzGaragem = false;
  
  digitalWrite(pinoQuarto, LOW);
  digitalWrite(pinoVaranda, LOW);
  digitalWrite(pinoSala, LOW);
  digitalWrite(pinoCozinha, LOW);
  digitalWrite(pinoBanheiro, LOW);
  digitalWrite(pinoGaragem, LOW);
  if(!estadoDoAlarme)
  Serial.println(F("Todas as luzes foram desligadas"));
}

void ligarPinos(){
    pinMode(pinoPortaoCasa1, OUTPUT);
    pinMode(pinoPortaoCasa2, OUTPUT);
    pinMode(pinoPortaoCasa3, OUTPUT);
    pinMode(pinoPortaoCasa4, OUTPUT);
}

void desligarPinos(){
    pinMode(pinoPortaoCasa1, INPUT);
    pinMode(pinoPortaoCasa2, INPUT);
    pinMode(pinoPortaoCasa3, INPUT);
    pinMode(pinoPortaoCasa4, INPUT);
}
