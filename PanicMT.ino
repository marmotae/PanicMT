

/**
 *   Define bibliotecas para el soporte de file system para 
 *   poder grabar datos dentro del micro controlador
 */
#include <FS.h>
#include <SPIFFS.h>


/**
 * Bibliotecas para hablar con el acelerómetro con el que 
 * detectaremos las caidas
 */
#include <Wire.h>
#include <MPU6050.h>
#define SDA 22
#define SCL 23
#define INT 14
#define THRESHOLD 20
#define DURATION 30

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

/**  
 *   Definimos las bibliotecas para controlar el ESP32
 *   en particular es de notar que la versión del WiFi manager
 *   usado es la versión del ramal de desarrollo ubicado en 
 *   
 *   https://github.com/tzapu/WiFiManager/tree/development
 *   
 *   Asi mismo los componentes del ESP32 vienen del repositorio
 *   del fabricante espressif ubicado en 
 *   
 *   https://github.com/espressif/arduino-esp32
 */
#include <WiFi.h>
#include <WiFiManager.h>
#include <DNSServer.h>
#include <WebServer.h>
/**
 *   Definimos la biblioteca para el manejo de cadenas de texto
 *   escritas en formato json. Para esto utilizamos la biblioteca
 *   publicada en:
 *   
 *   //https://github.com/bblanchon/ArduinoJson
 */
#include <ArduinoJson.h>
/**
 *   Definimos la biblioteca para el manejo de MQTT para gestionar
 *   la mensajeria del aplicativo. Para esto utilizamos la 
 *   biblioteca publicada en:
 *   
 *   https://github.com/knolleary/pubsubclient
 */
#include <PubSubClient.h>

/**
 *   Definimos las variables globales para el manejo de datos
 *   datos de conexión al servicio de mensajería MQTT
 */
char mqtt_server[40];
char mqtt_pwd[40];
char mqtt_user[40];
char mqtt_port[6] = "1883";
uint8_t freefallDetectionThreshold = 90;
uint8_t freefallDetectionDuration = 50;

String parameters;

/**
 *   Definimos las variable globales para la configuración del
 *   access point de configuración invocado por WiFiManager.
 *   
 *   Cabe mencionar que la contraseña debe ser un valor de entre
 *   8 a 63 caracteres ASCII con valor decimal entro 32 al 126
 */
char ap_name[60] = "FallDetector";
char ap_pwd[32] = "password";

/**
 *   Variable global para el servidor web
 */
WebServer server(80);

/**
 *   Variable global para el cliente de WiFi que se conectará al
 *   Access Point
 */
WiFiClient espClient;

/**
 *   Variable global para el cliente de MQTT
 */
PubSubClient client(espClient);

/**
 *   Variable para interfaz con acelerómetro
 */
MPU6050 mpu;
boolean freefallDetected = false;

/**
 *   Finalmente variable globales requeridas para la gestión lógica
 *   del aplicativo
 */
//Bandera para indicar si se deben salvar datos en la memoria
bool shouldSaveConfig = false;
//Bandera para indicar si estamos entrando en función de reset 
bool isResetting = false;
//Bandera para indicar si tenemos nuevos datos de MQTT
bool isNewMQTTConnectionData = false;

/**
 * Funciones utilitarias
 */

/**
 * Función de callback para indicar que se require salvar
 * los datos de configuración
 */
void saveConfigCallback () {
  Serial.println("Debemos salvar configuración");
  shouldSaveConfig = true;
}



/**
 * Función para montar el FileSystem y leer el JSON
 * de configuración, mismo que debe existir bajo el nombre
 * config.json
 */
void ReadJSON(){
  Serial.println("Montando el Sistema de Archivos");
  if (SPIFFS.begin(true)) {
    Serial.println("Sistema montado");
    if (SPIFFS.exists("/config.json")) {
      //el archivo de configuración existe
      Serial.println("carcando configuración");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("archivo abierto");
        size_t size = configFile.size();
        // Separamos un buffer para cargar el contenido
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, buf.get());
        //imprimimos la configuración leída
        serializeJson(doc, Serial);
        if (!error) {
          Serial.println("\narchivo procesado como JSON");
          //cargamos las variables
          strcpy(mqtt_server, doc["mqtt_server"]);
          strcpy(mqtt_user, doc["mqtt_user"]);
          strcpy(mqtt_pwd, doc["mqtt_pwd"]);
          strcpy(mqtt_port, doc["mqtt_port"]);
          freefallDetectionThreshold = doc["freefallDetectionThreshold"];
          freefallDetectionDuration = doc["freefallDetectionDuration"];
        } else {
          Serial.println("el archivo no parece ser un archivo JSON");
        }
      }
    }else{
      Serial.println("El archivo config.json no existe!");
    }
  } else {
    Serial.println("no se logró montar el sistema de archivos");
  }
}

/**
 * Función para salvar los valores de configuración al file system
 * esto suscede únicamente si estos están sucios
 */
void SaveJSON(){
  //Vemos si es necesario salvar
  if (shouldSaveConfig) {
    Serial.println("Salvando configuración");
    DynamicJsonDocument doc(1024);
    doc["mqtt_server"] = mqtt_server;
    doc["mqtt_port"] = mqtt_port;
    doc["mqtt_user"] = mqtt_user;
    doc["mqtt_pwd"] = mqtt_pwd;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("No pudimos abrir el archivo para salvar!");
    }

    serializeJson(doc, Serial);
    serializeJson(doc, configFile);
    configFile.close();
  }
}

String getParamOut(WiFiManagerParameter *_params[]){
  String page;
  int _paramsCount = sizeof(_params);
  if(_paramsCount > 0){

    String HTTP_PARAM_temp = FPSTR(HTTP_FORM_LABEL);
    HTTP_PARAM_temp += FPSTR(HTTP_FORM_PARAM);
    bool tok_I = HTTP_PARAM_temp.indexOf(FPSTR(T_I)) > 0;
    bool tok_i = HTTP_PARAM_temp.indexOf(FPSTR(T_i)) > 0;
    bool tok_n = HTTP_PARAM_temp.indexOf(FPSTR(T_n)) > 0;
    bool tok_p = HTTP_PARAM_temp.indexOf(FPSTR(T_p)) > 0;
    bool tok_t = HTTP_PARAM_temp.indexOf(FPSTR(T_t)) > 0;
    bool tok_l = HTTP_PARAM_temp.indexOf(FPSTR(T_l)) > 0;
    bool tok_v = HTTP_PARAM_temp.indexOf(FPSTR(T_v)) > 0;
    bool tok_c = HTTP_PARAM_temp.indexOf(FPSTR(T_c)) > 0;

    char valLength[5];
    // add the extra parameters to the form
    for (int i = 0; i < _paramsCount; i++) {
      if (_params[i] == NULL) {
        Serial.println("[ERROR] WiFiManagerParameter is out of scope");
        break;
      }

     // label before or after, @todo this could be done via floats or CSS and eliminated
     String pitem;
      switch (_params[i]->getLabelPlacement()) {
        case WFM_LABEL_BEFORE:
          pitem = FPSTR(HTTP_FORM_LABEL);
          pitem += FPSTR(HTTP_FORM_PARAM);
          break;
        case WFM_LABEL_AFTER:
          pitem = FPSTR(HTTP_FORM_PARAM);
          pitem += FPSTR(HTTP_FORM_LABEL);
          break;
        default:
          // WFM_NO_LABEL
          pitem = FPSTR(HTTP_FORM_PARAM);
          break;
      }

      // Input templating
      // "<br/><input id='{i}' name='{n}' maxlength='{l}' value='{v}' {c}>";
      // if no ID use customhtml for item, else generate from param string
      if (_params[i]->getID() != NULL) {
        if(tok_I)pitem.replace(FPSTR(T_I), (String)FPSTR(S_parampre)+(String)i); // T_I id number
        if(tok_i)pitem.replace(FPSTR(T_i), _params[i]->getID()); // T_i id name
        if(tok_n)pitem.replace(FPSTR(T_n), _params[i]->getID()); // T_n id name alias
        if(tok_p)pitem.replace(FPSTR(T_p), FPSTR(T_t)); // T_p replace legacy placeholder token
        if(tok_t)pitem.replace(FPSTR(T_t), _params[i]->getLabel()); // T_t title/label
        snprintf(valLength, 5, "%d", _params[i]->getValueLength());
        if(tok_l)pitem.replace(FPSTR(T_l), valLength); // T_l value length
        if(tok_v)pitem.replace(FPSTR(T_v), _params[i]->getValue()); // T_v value
        if(tok_c)pitem.replace(FPSTR(T_c), _params[i]->getCustomHTML()); // T_c meant for additional attributes, not html, but can stuff
      } else {
        pitem = _params[i]->getCustomHTML();
      }

      page += pitem;
    }
  }

  return page;
}

/**
 * Función para manejar el evento de navegación a la página raiz
 * del servidor web
 */
void handleParams(){
  Serial.println("Handling MQTT Configuration");
  String page;
  page += FPSTR(HTTP_HEAD_START);
  page.replace(FPSTR(T_v), "Configuración de Servicio MQTT");
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);
  page += "<h1>Configuración</h1>";
  
  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT User", mqtt_user, 40);
  WiFiManagerParameter custom_mqtt_pwd("password", "MQTT Password", mqtt_pwd, 40);
  WiFiManagerParameter *mArray[] = {&custom_mqtt_server,&custom_mqtt_port,&custom_mqtt_user,&custom_mqtt_pwd};
  page += getParamOut(mArray);
  
  page += "<p><button onclick=\"window.location.href='/params'\">Ajustar Servidor</button></p>";
  page += "<p><button onclick=\"window.location.href='/reset'\">Reiniciar Red</button></p>";
  page += FPSTR(HTTP_END);
  server.send(200, "text/html", page);
}

/**
 * Función para manejar el evento de navegación a la página raiz
 * del servidor web
 */
void handleRoot(){
  Serial.println("Handling Root");
  String page;
  page += FPSTR(HTTP_HEAD_START);
  page.replace(FPSTR(T_v), "Dispositivo Detector de Caidas");
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);
  page += "<h1>Detector de Caídas</h1>";
  page += "<p>Estado: ";
  if(client.connected()){
    page += "Conectado con " + String(mqtt_server) + ":" + String(mqtt_port);
  }else{
    page += "Desconectado";
  }
  page += "</p>";
  page += "<p><button onclick=\"window.location.href='/params'\">Ajustar Servidor</button></p>";
  page += "<p><button onclick=\"window.location.href='/reset'\">Reiniciar Red</button></p>";
  page += FPSTR(HTTP_END);
  server.send(200, "text/html", page);
}

/**
 * Función para manejar el evento de navegación a la página de reset
 * en donde se reinician los datos del wifi de forma manual y se 
 * reinicia el controlador. Se espera que esto vuelva a forzar la 
 * configuración de la red
 */
void handleReset(){
  Serial.println("Handling Reset");
  String page;
  page += FPSTR(HTTP_HEAD_START);
  page.replace(FPSTR(T_v), "Inicializar Dispositivo");
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);
  page += "<body><h1>Inicializando Dispositivo</h1><p>Se borrar&aacute; la informaci&oacute;n de conexi&oacute;n y luego se re iniciar&aacute; el dispositivo</p><p>Una vez hecho esto, conectarse mediante wifi a la red " + String(ap_name) + " </p>";
  page += FPSTR(HTTP_END);
  server.send(200, "text/html", page);
  delay(1000);
  WiFi.begin("0","0");
  ESP.restart();
}

/**
 * Rutina para el manejo de los mensajes MQTT entrantes
 */
void HandleMessage(char* topic, byte* message, unsigned int length) {
  Serial.print("Mensaje recibido en el canal: ");
  Serial.println(topic);
  
  Serial.print("Mensaje: ");
  String messageTemp;
  
  
  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }
  Serial.println(messageTemp);
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("Intentando conectar");
    if (client.connect("FallDetector_Client",mqtt_user,mqtt_pwd)) {
      Serial.println("Conectado");
      // Subscribe
      client.subscribe("falldetector/output");
    } else {
      Serial.print("Falló, rc=");
      Serial.print(client.state());
      Serial.println(" intentando en 5 segundos");
      delay(5000);
    }
  }
}

/**
 * Función que recibe la interrupción generada por la caida
 */
void IRAM_ATTR doInt()
{
  portENTER_CRITICAL(&mux);
  freefallDetected = true; 
  portEXIT_CRITICAL(&mux);
}

/**
 * Función para inicializar el acelerómetro
 */
void InitializeAccelerometer(){
  Serial.println ("Conectando el MPU");
  Wire.begin(SDA, SCL);
  mpu.initialize();
  //Ponemos el delay a 3 ms
  mpu.setAccelerometerPowerOnDelay(0b11);
  Serial.println("Testing device connections...");
  Serial.println(mpu.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
  //Encendemos y apagamos macros
  mpu.setIntFreefallEnabled(true);
  mpu.setIntZeroMotionEnabled(false);
  mpu.setIntMotionEnabled(false);
  //Ponemos el decremento a 1
  mpu.setFreefallDetectionCounterDecrement(0b01);
  //Ponemos el sampleo a 0.63 Hz
  mpu.setDHPFMode(0b100);
  mpu.setFreefallDetectionThreshold(THRESHOLD);
  mpu.setFreefallDetectionDuration(DURATION);
  pinMode(SDA, OUTPUT);
  digitalWrite(SDA, LOW);
  pinMode(INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INT), doInt, RISING);
  Serial.println ("MPU Inicializado");
  checkSettings();
 }

 /**
  * Indica los settings del MPU
  */
 void checkSettings()
{
  Serial.println();
  
  Serial.print(" * Sleep Mode:                ");
  Serial.println(mpu.getSleepEnabled() ? "Enabled" : "Disabled");

  Serial.print(" * Motion Interrupt:     ");
  Serial.println(mpu.getIntMotionEnabled() ? "Enabled" : "Disabled");

  Serial.print(" * Zero Motion Interrupt:     ");
  Serial.println(mpu.getIntZeroMotionEnabled() ? "Enabled" : "Disabled");

  Serial.print(" * Free Fall Interrupt:       ");
  Serial.println(mpu.getIntFreefallEnabled() ? "Enabled" : "Disabled");

  Serial.print(" * Free Fal Threshold:          ");
  Serial.println(mpu.getFreefallDetectionThreshold());

  Serial.print(" * Free FallDuration:           ");
  Serial.println(mpu.getFreefallDetectionDuration());
  
  Serial.print(" * Accelerometer:             ");
  switch(mpu.getFullScaleAccelRange())
  Serial.print(" * Accelerometer offsets:     ");
  Serial.print(mpu.getXAccelOffset());
  Serial.print(" / ");
  Serial.print(mpu.getYAccelOffset());
  Serial.print(" / ");
  Serial.println(mpu.getZAccelOffset()); 
  Serial.println();
  Serial.print(" * Freefall Detection Counter Decrement: ");
  Serial.println(mpu.getFreefallDetectionCounterDecrement());
}

/**
 * Funcion para leer los datos de acelerómetrro con offset
 */
 void PrintAcceleration(){
  Serial.print(" * Accelerometer:             ");
  switch(mpu.getFullScaleAccelRange())
  Serial.print(" * Accelerometer offsets:     ");
  Serial.print(mpu.getXAccelOffset());
  Serial.print(" / ");
  Serial.print(mpu.getYAccelOffset());
  Serial.print(" / ");
  Serial.println(mpu.getZAccelOffset()); 
  Serial.println();
 }

/**
 * Funcion para publicar un mensaje a MQTT
 */
void PublishMessage(char * msg){
  String msgStr =  String(msg);
  byte* p = (byte*)malloc(msgStr.length());
  msgStr.getBytes(p,msgStr.length());
  //memcpy(p,msgStr.data(),msgStr.length());
  if(client.connected()){
    client.publish("falldetector/output",p,msgStr.length());
  }
}

void setup() {
  //Inicializamos el serial
  Serial.begin(115200);
  Serial.println();
  //Leemos el la configuración desde el filesystem
  ReadJSON();
  /**
   * Definimos los parámetros a ser administrados por el WiFiManager
   * luego de que el WiFiManager ejecute, estos pueder ser 
   * recuperados usando la función <parametro>.getValue()
   * 
   * La definición se hace especificando:
   * - el nombre de la variable
   * - la etiqueta o leyenda a mostrar en el campo de captura
   * - el valor por omisión (aquí usamos la variable global)
   * - la longitud esperada
   */
  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT User", mqtt_user, 40);
  WiFiManagerParameter custom_mqtt_pwd("password", "MQTT Password", mqtt_pwd, 40);

  /**
   * Definimos el WiFiManager, esta definición se hace dentro de esta
   * rutina pues en caso de ser necesario, sólo suscedería como parte de 
   * el proceso de inicialización y dificilmente se usaría fuera de este
   * contexto
   */
  WiFiManager wifiManager;
  
  //Apuntamos el callback para manejo del caso de salvado de datos
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  //Agregamos los parámetros que definimos con anterioridad
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pwd);

  /**
   * Definimos el timeout para especificar por cuanto tiempo
   * debemos intentar conectarnos
   */
  wifiManager.setConnectTimeout(5000);
  wifiManager.setConfigPortalTimeout(180);


  /**
   * Aquí probamos definir como IP estática del portal
   * la dirección 8.8.8.8 para atender un bug en android
   */
  wifiManager.setAPStaticIPConfig(IPAddress(8,8,8,8), IPAddress(8,8,8,8), IPAddress(255,255,255,0));

   
  /**
   * Ahora intentaremos conectarnos al WiFi con las últimas 
   * credenciales válidas. En caso de no existir estas o bien que no
   * logremos establecer una conección se iniciará el portal de 
   * configuración y se encenderá el WiFi en modo AP en lugar de en 
   * modo cliente.
   * 
   * En caso de abrirse el AP, el mismo será abierto con los datos
   * definidos en las variable globales para nombre y password
   */
  if (!wifiManager.autoConnect(ap_name,ap_pwd)){
    Serial.println("No recibímos conexión para configuración");
    //esperamos 3 segundos
    delay(3000);
    //reiniciamos el controlador
    ESP.restart();
  }
  /**
   * Si llegamos aquí, quiere decir que nos conectamos exitosamente 
   * a una red WiFi
   */
  Serial.println("Conectados al WiFi");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());
  
  //Leemos los parámetros capturados por el configurador
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pwd, custom_mqtt_pwd.getValue());

  //Salvamos los datos en caso de ser necesario
  SaveJSON();

  /**
   * Configuramos el servidor web, estableciendo sus rutas
   * y acto seguido lo iniciamos
   */
  server.on("/", handleRoot);
  server.on("/reset",handleReset);
  server.on("/params",handleParams);
  server.begin();
  Serial.println("Servidor web iniciado");

 /**
  * Configuramos el cliente de MQTT con los datos
  * globales
  */
 Serial.println("Servidor de destino : "+String(mqtt_server)+":"+ String(mqtt_port));
 //Definimos el servidor y el puerto a usar para MQTT
 client.setServer(mqtt_server, atol(mqtt_port));
 //Definimos la rutina que procesará los mensajes entrantes
 client.setCallback(HandleMessage);

 /**
  * Inicializamos el acelerómetro
  */
 InitializeAccelerometer();
 
 /**
  * Finalmente definimos dos procesos paralelos, uno para el 
  * procesamiento de peticiones web y el otro para el procesamiento
  * de los mensajes MQTT
  */

 xTaskCreate(
  WebTask,          /* Task function. */
  "WebServer",        /* String with name of task. */
  10000,            /* Stack size in bytes. */
  NULL,             /* Parameter passed as input of the task */
  1,                /* Priority of the task. */
  NULL);            /* Task handle. */

 xTaskCreate(
  MQTask,          /* Task function. */
  "MQTT Client",        /* String with name of task. */
  10000,            /* Stack size in bytes. */
  NULL,             /* Parameter passed as input of the task */
  1,                /* Priority of the task. */
  NULL);            /* Task handle. */
}

void WebTask( void * parameter )
{
    while( true ){
      delay(1);
      server.handleClient();
      if (freefallDetected){
        portENTER_CRITICAL(&mux);
        freefallDetected=false;
        digitalWrite(SDA,false);
        portEXIT_CRITICAL(&mux);
        Serial.println("**********************************************\nInterrupcion por caida\n**********************************************");
        PublishMessage("Caida detectada!");
        PrintAcceleration();
      }
    }
    Serial.println("Ending task 1");
    vTaskDelete( NULL );
}
 
void MQTask( void * parameter)
{
    while( true ){
      delay(1);
      if (!client.connected()) {
        reconnect();
      }
      client.loop();
    }
    Serial.println("Ending task 2");
    vTaskDelete( NULL );
}

void Detector( void * parameter)
{
  while(true){
    if (freefallDetected){
      PublishMessage("Caida detectada!");
    }
    delay(1);
  }
}

void loop() {
  delay(10);
}
