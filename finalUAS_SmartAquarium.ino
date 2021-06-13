#include <ESP32Servo.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <MQTT.h>

const String typeESP = "ESP32_type_A";
const char *ssid     = "IndihomeMawar";
const char *password = "rahasiadong123";

//getTime
const long utcOffsetInSeconds = 25200;
char daysOfTheWeek[7][12] = {"Mingu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"};

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", utcOffsetInSeconds);

#define hari daysOfTheWeek[timeClient.getDay()]
String jam2 = String(timeClient.getHours());
#define jam timeClient.getHours()
#define menit timeClient.getMinutes()
#define detik timeClient.getSeconds()
#define waktu2 

char tanggalapi[11];

//Sensor UltraSonic-jarak
#define pinTrigger 13
#define pinEcho 12
int durasi;
int cm, minimalTinggiAir, maximalTinggiAir = 22, tinggiAquarium = 27;

//Servo-makan_Ikan
Servo myServo;
#define servoPin 15
int posisiServo = 0;
String makanIkanOtomatis1, makanIkanOtomatis2;
String waktu;


//Relay
#define relay 2
bool relayOn = HIGH;
bool relayOff = LOW;

//MQTT
String brokerIP = "34.228.8.64";
WiFiClient net;
MQTTClient client;
void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  
  Serial.print("ip add : ");
  Serial.println(WiFi.localIP());

  Serial.println("\nconnecting to Broker...");
  while (!client.connect("esp32KadekLED", "kadek", "12345678")) {
    Serial.println("try to connect mqtt!");
    delay(1000);
  }

  Serial.println("\nconnected!");
  //makan ikan
  client.subscribe("/rumah/aquarium/makanIkan");
  client.subscribe("/" + typeESP + "/rumah/aquarium/makanIkan/makanIkanManual");
  client.subscribe("/" + typeESP + "/rumah/aquarium/makanIkan/makanIkanOtomatis1");
  client.subscribe("/" + typeESP + "/rumah/aquarium/makanIkan/makanIkanOtomatis2");

   //pompa air
  client.subscribe("/rumah/aquarium/pompa");
  client.subscribe("/" + typeESP + "/rumah/aquarium/pompa/pompaManual");
  client.subscribe("/" + typeESP + "/rumah/aquarium/pompa/pompaOtomatis");
  client.subscribe("/" + typeESP + "/rumah/aquarium/pompa/setTinggiAir");
}

void messageReceive(String &topic, String &payload) {
  // makan Ikan
  if(topic == "/rumah/aquarium/makanIkan"){
    Serial.println("incoming: " + topic + " - " + payload);
    if(payload == "1"){    
      FeedTheFish();
      logMessageMakanIkan("manual");
    }
   }
   
   if(topic == "/" + typeESP + "/rumah/aquarium/makanIkan/makanIkanManual"){
    Serial.println("incoming: " + topic + " - " + payload);
    if(payload == "1"){    
      FeedTheFish();
      logMessageMakanIkan("manual");
    }
   }

  if(topic == "/" + typeESP + "/rumah/aquarium/makanIkan/makanIkanOtomatis1"){
    Serial.println("incoming: " + topic + " - " + payload);
    makanIkanOtomatis1 = payload;
    Serial.println("set Makan Ikan Otomatis 1 => " + makanIkanOtomatis1);
  }

  if(topic == "/" + typeESP + "/rumah/aquarium/makanIkan/makanIkanOtomatis2"){
    Serial.println("incoming: " + topic + " - " + payload);
    makanIkanOtomatis2 = payload;
    Serial.println("set Makan Ikan Otomatis 2 => " + makanIkanOtomatis2);
  } 

  //Pompa AIR
  if(topic == "/rumah/aquarium/pompa"){
    Serial.println("incoming: " + topic + " - " + payload);
    RelayRunning(payload);
    logMessagePompaAir("manual", payload);
  }
  
  if(topic == "/" + typeESP + "/rumah/aquarium/pompa/pompaManual"){
    Serial.println("incoming: " + topic + " - " + payload);
    RelayRunning(payload);
    logMessagePompaAir("manual", payload);
  }
  
  if(topic == "/" + typeESP + "/rumah/aquarium/pompa/setTinggiAir"){
    Serial.println("incoming: " + topic + " - " + payload);
    minimalTinggiAir = payload.toInt();
  }
}

void WaterLevel(){
  Serial.println("minimal tinggi air = " + String(minimalTinggiAir));
  int tinggiAir;
  digitalWrite (pinTrigger, 0);
  delayMicroseconds(2);
  digitalWrite (pinTrigger, 1);
  delayMicroseconds(10);
  digitalWrite (pinTrigger, 0);
  delayMicroseconds(2);
  
  durasi = pulseIn(pinEcho, HIGH);
  cm = (durasi * 0.0343)/2;
  tinggiAir = tinggiAquarium - cm;
  String pesan = String(cm) + " Cm";
  if(tinggiAir < minimalTinggiAir){
    RelayRunning("1");
    logMessagePompaAir("otomatis", "1");
  }else if(tinggiAir >= maximalTinggiAir){
    RelayRunning("0");
    logMessagePompaAir("otomatis", "0");
  }
  Serial.println("tinggi air => " + String(tinggiAir));
  Serial.println("minimal tinggi air => " + String(minimalTinggiAir));
  Serial.println("maximal tinggi air => " + String(maximalTinggiAir));
  Serial.println(pesan);
  client.publish("/" + typeESP + "/rumah/aquarium/pompa/tinggiAir", String(tinggiAir));
}

void FeedTheFish(){ //Hang karna murah
  for(posisiServo = 0; posisiServo <= 60; posisiServo++){
    myServo.write(posisiServo);
    delay(1);
  }
  delay(250);
  for(posisiServo = 60; posisiServo >= 0; posisiServo--){
    myServo.write(posisiServo);
    delay(1);
  }
}

void RelayRunning(String statusRelay){
  if(statusRelay == "1"){    
    digitalWrite(relay, relayOn);
    Serial.println("relay Hidup"); 
  }else{
    digitalWrite(relay, relayOff);
    Serial.println("relay Mati"); 
  }
}

void logMessageMakanIkan(String type){
  //timeClient.update();
  //client.onMessage(messageReceiveMakanIkan);
  //Serial.print(daysOfTheWeek[timeClient.getDay()]);
  //Serial.print(", ");
  //String hari = daysOfTheWeek[timeClient.getDay()];
  String jamTamp, menitTamp, detikTamp;

  if(jam >= 0 && jam <=9){
    jamTamp = "0" + String(jam);
  }else{
    jamTamp = String(jam);
  }

  if(menit >= 0 && menit <=9){
    menitTamp = "0" + String(menit);
  }else{
    menitTamp = String(menit);
  }

  if(detik >= 0 && detik <=9){
    detikTamp = "0" + String(detik);
  }else{
    detikTamp = String(detik);
  }

  waktu = jamTamp + ":" + menitTamp + ":" + detikTamp;
 
  Serial.println("logWaktu => " + waktu);
  client.publish("/" + typeESP + "/rumah/aquarium/makanIkan/logMakanIkan", type + " => \n" + waktu);
}

void logMessagePompaAir(String type, String statusRelay){
  Serial.println("masuk logMessagePompaAir");
  String statusPompa;
  if(statusRelay == "1"){
    statusPompa = "HIDUP";
  }else{
    statusPompa = "MATI";
  }
  Serial.println("kirim status Pompa : " + statusPompa);
  client.publish("/" + typeESP + "/rumah/aquarium/pompa/statusPompa", type + " => " + statusPompa);
}

unsigned long lasttime = 0;

void setup() {
  Serial.begin(115200);
  //unsigned long lasttime = 0;
  WiFi.begin(ssid, password);

  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.println( "." );
  }

  //dapatkan waktu dari API
  timeClient.begin();
  
  //SensorUltrasonic / jarak
  pinMode (pinTrigger, OUTPUT);
  pinMode (pinEcho, INPUT);

  //Servo
  myServo.attach(servoPin);

  //relay
  pinMode(relay, OUTPUT);
  
  //MQTT
  client.begin("34.228.8.64", net);
  client.onMessage(messageReceive);
  connect();
}

void loop() {
  //dapatkan waktu dari API
  timeClient.update();
  client.loop();
  delay(10);
  if (millis() - lasttime >= 1000) {
    
  //client.onMessage(messageReceiveMakanIkan);
    //Serial.print(daysOfTheWeek[timeClient.getDay()]);
    //Serial.print(", ");
    String jamTamp, menitTamp, detikTamp, waktu;
  
    if(jam >= 0 && jam <=9){
      jamTamp = "0" + String(jam);
    }else{
      jamTamp = String(jam);
    }
  
    if(menit >= 0 && menit <=9){
      menitTamp = "0" + String(menit);
    }else{
      menitTamp = String(menit);
    }
  
    if(detik >= 0 && detik <=9){
      detikTamp = "0" + String(detik);
    }else{
      detikTamp = String(detik);
    }
    
    waktu = jamTamp + ":" + menitTamp + ":" + detikTamp;
    Serial.println("waktu makan otomatis 1 => " + makanIkanOtomatis1);
    Serial.println("waktu makan otomatis 2 => " + makanIkanOtomatis2);
    Serial.println("waktu sekarang => " + waktu);
    
    //jalankan makan ikan otomatis
    if(makanIkanOtomatis1 == waktu){
      FeedTheFish();
      logMessageMakanIkan("Otomatis1");
    }
    if(makanIkanOtomatis2 == waktu){
      FeedTheFish();
      logMessageMakanIkan("Otomatis2");
    }
    
    //ketinggian Air
    WaterLevel();
    
    lasttime = millis();
  }
}
