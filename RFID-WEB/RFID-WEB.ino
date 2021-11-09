
/**
 * Trabalho da discplina de Comunicação de Dados (Faculdade UNISATC - Engenharia da Computação)
 * com objetivo de simular a autenticação de produtos de um estoque.
 * 
 * Foi disponibilizado o uso de um módulo RFID-RC522 para realizar a validação das TAGs
 * utilizadas 
 * 
 * @board       ESP8266 D1 R1
 */

/*    PINOS 
 *
 *  SDA   = D8
 *  SCK   = D5
 *  MOSI  = D7
 *  MISO  = D6
 *  GND  = GND
 *  RST   = D3
 *  VCC = 3.3V
 *  LD_VD = D2
 *  LD_VM = D4
 */

/* Inclusão de Bibliotecas */
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SPI.h>
#include <MFRC522.h>
#include <vector>

/* Definições e Constantes */
#define SS_PIN D8
#define RST_PIN D3
#define FILENAME "/Cadastro.txt"
#define LED_RED D4
#define LED_GREEN D2

using namespace std;

const char* ssid = "ID";
const char* password = "PASSWORD";

String info_data; //Informação do estoque.
String id_data;   //Id do RFID
int index_user_for_removal = -1;

String rfid_card = ""; //Codigo RFID obtido pelo Leitor
String sucess_msg = ""; 
String failure_msg = "";

// Cria um objeto  MFRC522.
MFRC522 mfrc522(SS_PIN, RST_PIN);   

// Cria um objeto AsyncWebServer que usará a porta 80
AsyncWebServer server(80);

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}
//Inicializa o sistema de arquivos.
bool initFS() {
  if (!SPIFFS.begin()) {
    Serial.println("Erro ao abrir o sistema de arquivos");
    return false;
  }
  Serial.println("Sistema de arquivos carregado com sucesso!");
  return true;
}
//Lista todos os arquivos salvos na flash.
void listAllFiles() {
  String str = "";
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    str += dir.fileName();
    str += " / ";
    str += dir.fileSize();
    str += "\r\n";
  }
  Serial.print(str);
}
//Faça a leitura de um arquivo e retorne um vetor com todas as linhas.
vector <String> readFile(String path) {
  vector <String> file_lines;
  String content;
  File myFile = SPIFFS.open(path.c_str(), "r");
  if (!myFile) {
    myFile.close();
    return {};
  }
  Serial.println("###################### - FILE- ############################");
  while (myFile.available()) {
    content = myFile.readStringUntil('\n');
    file_lines.push_back(content);
    Serial.println(content);
  }
  Serial.println("###########################################################");
  myFile.close();
  return file_lines;
}
//Faça a busca de um estoque pelo ID e pela INFO.
int findUser(vector <String> users_data, String id, String info) {
  String newID = "<td>" + id + "</td>";
  String newinfo = "<td>" + info + "</td>";

  for (int i = 0; i < users_data.size(); i++) {
//    if (users_data[i].indexOf(newID) > 0 || users_data[i].indexOf(newinfo) > 0) { //Restringe para não cadastrar objetos com mesmo nome.
      if (users_data[i].indexOf(newID) > 0) {
      return i;
    }
  }
  return -1;
}
//Adiciona um novo usuario ao sistema
bool addNewUser(String id, String data) {
  File myFile = SPIFFS.open(FILENAME, "a+");
  if (!myFile) {
    Serial.println("Erro ao abrir arquivo!");
    myFile.close();
    return false;
  } else {
    myFile.printf("<tr><td>%s</td><td>%s</td>\n", id.c_str(), data.c_str());
    Serial.println("Arquivo gravado");
  }
  myFile.close();
  return true;
}
//Remove um usuario do sistema
bool removeUser(int user_index) {
  vector <String> users_data = readFile(FILENAME);
  if (user_index == -1)//Caso objeto não exista retorne falso
    return false;

  File myFile = SPIFFS.open(FILENAME, "w");
  if (!myFile) {
    Serial.println("Erro ao abrir arquivo!");
    myFile.close();
    return false;
  } else {
    for (int i = 0; i < users_data.size(); i++) {
      if (i != user_index)
        myFile.println(users_data[i]);
    }
    Serial.println("Objeto removido");
  }
  myFile.close();
  return true;
}
//Esta função substitui trechos de paginas html marcadas entre %
String processor(const String& var) {
  String msg = "";
  if (var == "TABLE") {
    msg = "<table><tr><td>Código RFID</td><td>Informação</td><td>Excluir</td></tr>";
    vector <String> lines = readFile(FILENAME);
    for (int i = 0; i < lines.size(); i++) {
      msg += lines[i];
      msg += "<td><a href=\"get?remove=" + String(i + 1) + "\"><button>Excluir</button></a></td></tr>"; //Adiciona um botão com um link para o indice do objeto na tabela
    }
    msg += "</table>";
  }
  else if (var == "SUCESS_MSG")
    msg = sucess_msg;
  else if (var == "FAILURE_MSG")
    msg = failure_msg;
  return msg;
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(D9, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, LOW);

  SPI.begin();
  mfrc522.PCD_Init();   // Inicia MFRC522

  // Inicialize o SPIFFS
  if (!initFS())
    return;
  listAllFiles();
  readFile(FILENAME);

  // Conectando ao Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando WiFi..");
  }

  Serial.println(WiFi.localIP());
  //Rotas.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    rfid_card = "";
    request->send(SPIFFS, "/home.html", String(), false, processor);
  });
  server.on("/home", HTTP_GET, [](AsyncWebServerRequest * request) {
    rfid_card = "";
    request->send(SPIFFS, "/home.html", String(), false, processor);
  });
  server.on("/sucess", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/sucess.html", String(), false, processor);
  });
  server.on("/warning", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/warning.html");
  });
  server.on("/failure", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/failure.html");
  });
  server.on("/deletestock", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (removeUser(index_user_for_removal)) {
      sucess_msg = "Objeto excluído do registro.";
      request->send(SPIFFS, "/sucess.html", String(), false, processor);
    }
    else
      request->send(SPIFFS, "/failure.html", String(), false, processor);
    return;
  });
  server.on("/stylesheet.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/stylesheet.css", "text/css");
  });
  server.on("/rfid", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", rfid_card.c_str());
  });
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {
    vector <String> users_data = readFile(FILENAME);

    if (request->hasParam("info")) {
      info_data = request->getParam("info")->value();
      Serial.printf("info: %s\n", info_data.c_str());
    }
    if (request->hasParam("rfid")) {
      id_data = request->getParam("rfid")->value();
      Serial.printf("ID: %s\n", id_data.c_str());
    }
    if (request->hasParam("remove")) {
      String user_removed = request->getParam("remove")->value();
      Serial.printf("Remover o objeto da posição : %s\n", user_removed.c_str());
      index_user_for_removal = user_removed.toInt();
      index_user_for_removal -= 1;
      request->send(SPIFFS, "/warning.html");
      return;
    }
    if(id_data == "" || info_data == ""){
      failure_msg = "Informações de objeto estão incompletas.";
      request->send(SPIFFS, "/failure.html", String(), false, processor);
      return;
      }
    int user_index = findUser(users_data, id_data, info_data);
    if (user_index < 0) {
      Serial.println("Cadastrando novo objeto");
      addNewUser(id_data, info_data);
      sucess_msg = "Novo objeto cadastrado.";
      request->send(SPIFFS, "/sucess.html", String(), false, processor);
    }
    else {
      Serial.printf("Objeto numero %d ja existe no banco de dados\n", user_index);
      failure_msg = "Ja existe um objeto cadastrado.";
      request->send(SPIFFS, "/failure.html", String(), false, processor);
    }
  });
  server.on("/dataGIF.gif", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/dataGIF.gif", "image/jpg");
});
  
  server.onNotFound(notFound);
  // Inicia o serviço
  server.begin();
}
void loop() {
  // Procure por novos cartões.
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  //Faça a leitura do ID do cartão
  if (mfrc522.PICC_ReadCardSerial()) {
    Serial.print("UID da tag :");
    String rfid_data = "";
    for (int i = 0; i < mfrc522.uid.size; i++)
    {
      Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
      rfid_data.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
      rfid_data.concat(String(mfrc522.uid.uidByte[i], HEX));
    }
    Serial.println();
    rfid_data.toUpperCase();
    rfid_card = rfid_data;

    //Carregue o arquivo de cadastro
    vector <String> users_data = readFile(FILENAME);
    //Faça uma busca pelo id
    int user_index = findUser(users_data, rfid_data, "");
    
    if (user_index < 0) {
      Serial.printf("Nenhum objeto encontrado\n");
      static int i = 0;
      digitalWrite(LED_RED, HIGH);
      tone(D9, 2500, 1000);
      delay(350);
      noTone(D9);
      delay(350);
      tone(D9, 2500, 1000);
      delay(350);
      noTone(D9);
    }
    else {
      Serial.printf("Objeto %d encontrado\n", user_index);
      digitalWrite(LED_GREEN, HIGH);
      tone(D9, 2500, 1000);
      delay(150);
      noTone(D9);
      delay(150);
      tone(D9, 2500, 1000);
      delay(150);
      noTone(D9);
      delay(150);
      tone(D9, 2500, 1000);
      delay(150);
      noTone(D9);
    }
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, LOW);
    Serial.println();
  }
}
