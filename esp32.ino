#include <Arduino.h>

#include <SPI.h>
#include <SPIFFS.h>


#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>

#include <Adafruit_BMP280.h>
#include <Wire.h>

// définition des pins
#define pinAir 35
#define pinHum 32
#define pinILS 34
#define pinMos 33 	// pin du mosfet de controle du capteur de qualité d'air 

// définition des variables utiles 
#define pi 3.14159265359
#define RayonDesBras    0.1 // en mètre de l'anénomètre
#define USE_SERIAL Serial

// facteur pour transformer des µsecondes en secondes 
#define usToSFactor 1000000

// Décalaration du mode Wifi
WiFiMulti wifiMulti;

// Déclatation du capteur BMP280, Barrométrie, température, Altitude
Adafruit_BMP280 BMP280;


float airQuality;
float humidity;
int pressure;
float temp;
float vitesseVent;


String ssid = "your_ssid";
String pass = "...........";
String server = "ip.ip.ip.ip:443";
String requette = "insert&device=test&value=4269"; 

float FEtalonage(1);



float getTurnsPerSecond(){

	// nombre tours = count / 2 = 5;
	// donc temps pour un tour vaut deltaTime /5

	float count = 1;
	float turnsPerSecond;
	float time1 = millis();
	float time2 = 0;
	bool isActive(false);
	bool isActiveOld(false);
	float deltaTime = 0;

	USE_SERIAL.println("Mesure en cours");
	
	while (deltaTime <= 10000){
	
		isActive = digitalRead(pinILS);
	
		if((isActive == true && isActiveOld == false)){
			count++; 
		} 
		time2 = millis();  
		deltaTime = time2-time1;  
		isActiveOld = isActive;
	}

	turnsPerSecond = (count/2)/10;
	USE_SERIAL.println("\nMesure terminée");
	
	return turnsPerSecond;
}

void sendData(String device, float value, String unit){
	if((wifiMulti.run() == WL_CONNECTED)){

		HTTPClient http;
		http.begin(String("http://")+ server +"/?insert&device="+device+"&value="+ String(value)+ "&unit=" + unit ); //HTTP

		int httpCode = http.GET();
		if (httpCode >0){
			USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
			// file found at server
			if(httpCode == HTTP_CODE_OK) {
				String payload = http.getString();
				USE_SERIAL.println(payload);
			}
		}else {
			USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
		}
	http.end();
	}
}


void setup() {
	 
	// ouverture de la connection serie avec l'ordinateur
	USE_SERIAL.begin(115200);
	USE_SERIAL.println();

	for(uint8_t t = 4; t > 0; t--) {
			USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
			USE_SERIAL.flush();
			delay(500);
	}

	// le capteur de qualité d'air doit s'initialiser pendant 1 minute au moins
	pinMode(pinMos,OUTPUT);
	digitalWrite(pinMos,HIGH);
	delay(60000);

	wifiMulti.addAP(ssid.c_str(), pass.c_str());
	
	/*-------------------------------------------------------------------------------------------------*/
	pinMode(pinAir,INPUT); // capteur qualité air
	pinMode(pinHum, INPUT); // capteur humidité
 
	
	if (!BMP280.begin()) {
		USE_SERIAL.println(F("Could not find a valid BMP280 sensor, check wiring!"));
		while (1);
 	}

	pinMode(pinILS, INPUT);
	/*********************************** Capteur I2C **************************************************/
	temp = BMP280.readTemperature();
	pressure = BMP280.readPressure();
 
	/********************************* Capteur humidité ***********************************************/

	float value = analogRead(pinHum);
	float Vout = ((value * 3.3)/4095)/0.64;
	float sensorRH = (Vout/(0.0062*5)) - 25.81;
	humidity = sensorRH/(1.0546-(0.00216*temp));

	/******************************** Capteur Qualité Air *********************************************/  

	airQuality = analogRead(pinAir);  
	float conversion = airQuality*(3.3/4095); // tension après pont = (valeur*3.3/4095)
	conversion = conversion/0.64;       // tension avant pont = tensionApres/0.64
	conversion = conversion *(1023/5);  // equivalence tension entre 0 et 1023 : tension*1023/5
	
	/*********************************** Anénomètre ***************************************************/

	float NombreTourSec = getTurnsPerSecond();
	vitesseVent = 2*pi*RayonDesBras*NombreTourSec*FEtalonage*3.6;

	
	/*********************************** Affichage ****************************************************/

	sendData("windSpeed",vitesseVent, "km/h");
	sendData("temperature",temp, "C");
	sendData("pressure", pressure, "Pa");
	sendData("humidity",humidity, "%RH");
	sendData("airQuality",conversion, "");

	// temps en secondes avant de redémarrer l'esp
	esp_sleep_enable_timer_wakeup(1740 * usToSFactor);
	esp_deep_sleep_start();
}

void loop() {}
