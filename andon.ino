/* ESP32
INPUT_PULLUP          INPUT_PULLDOWN   OUTPUT
GPIO0 (G0)pz.prod     GPIO4 (G4)       GPIO2 (G2)
GPIO2 (G2)-paro       GPIO5 (G5)       GPIO4 (G4)
GPIO4 (G4)-logs       GPIO12 (G12)     GPIO16 (G16)
GPIO5 (G5)-bad pz     GPIO13 (G13)     GPIO18 (G18)
GPIO12 (G12)-verde    GPIO14 (G14)     GPIO19 (G19)
GPIO13 (G13)-amarillo GPIO15 (G15)     GPIO21 (G21)
GPIO14 (G14)-azul     GPIO16 (G16)     GPIO22 (G22)
GPIO15 (G15)          GPIO17 (G17)     GPIO23 (G23)
GPIO16 (G16)-calid    GPIO18 (G18)     GPIO27 (G27)
GPIO17 (G17)          GPIO19 (G19)
GPIO18 (G18)-mtto     GPIO21 (G21)
GPIO19 (G19)-analog   GPIO22 (G22)
GPIO21 (G21)          GPIO23 (G23)
GPIO22 (G22)          GPIO27 (G27)
GPIO23 (G23)
GPIO27 (G27)
*/
 
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <HTTPClient.h>

WiFiMulti wifiMulti;
 
String ig = "IG0044670087"; //IG de maquina asignada a tarjeta
int s, sa; // Variables para enviar datos cada cambio de status
int l, q, i; //Datos enviados a mysql
 
//PIEZAS BUENAS:
int s1_actual=0;
int s1_anterior=1;
int ciclos_maq = 15; //Señal de piezas producidas GPIo15 (G15)
unsigned long contador_ciclos = 0; //Variable para almacenar conteo de piezas
unsigned long periodo = 60000;
int temp = 0;
unsigned long t_ant = 0;
int produccion_paro = 0;
 
//PIEZAS MALAS:
int s2_actual=0;
int s2_anterior=1;
int bad_piece = 5; //GPIO5 (G5)
unsigned long contador_bad = 0; //Variable para almacenar conteo de piezas malas
int temp_2 = 0;
unsigned long t_ant_2 = 0;
int produccion_paro_2 = 0;
 
//Interruptores:
int btn_logistica = 12;      //Verde GPIO12 (G12)
int logistica = 0;
 
int btn_calidad = 13;        //Amarillo GPIO13 (G13)
int calidad = 0;
 
int btn_mantenimiento = 14; //Azul GPIO14 (G14)
int mantenimiento = 0;
 
//Estado de la maquina (Semáforo ANDON):
int maq_produccion_paro = 2;  //GPIO2 (G2)
int maq_logistica = 4;        //GPIO4 (G4)
int maq_calidad = 16;         //GPIO16 (G16)
int maq_mantenimiento = 18;   //GPIO18 (G18)

int analog_read = 35;
int velocidad;
HTTPClient http;
WiFiClient client;
String Datos;
 
void setup() {
  Serial.begin (115200);
  delay(10);
 
  //wifiMulti.addAP("AndonPro2", "richard123");
  wifiMulti.addAP("AndonPro3", "richard123");
  wifiMulti.addAP("AndonPro", "richard123");
  //wifiMulti.addAP("Andontool", "richard123tool");
  //wifiMulti.addAP("AndonProtool", "richard123tool");  
  //wifiMulti.addAP("Industria4", "Y-1-wpiBuo");  
 
  //Conexion a WiFi
  Serial.println("Connecting Wifi...");
  if(wifiMulti.run() == WL_CONNECTED) {
      Serial.println("Conectando a ");
      Serial.println(WiFi.SSID());
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
  }
 
  //Inicializamos los pines como entradas y salidas
  pinMode(ciclos_maq, INPUT_PULLUP); //Configura la entrada con resistencia pull-up
  pinMode(bad_piece, INPUT_PULLUP);  
 
  pinMode(btn_logistica, INPUT_PULLUP);
  pinMode(btn_calidad, INPUT_PULLUP);
  pinMode(btn_mantenimiento, INPUT_PULLUP);
 
  pinMode(maq_produccion_paro, OUTPUT);
  pinMode(maq_logistica, OUTPUT);
  pinMode(maq_calidad, OUTPUT);
  pinMode(maq_mantenimiento, OUTPUT);
}
 
void loop() {

  if(wifiMulti.run() != WL_CONNECTED) {
      Serial.println("WiFi not connected!");
      ESP.restart();
  }
     
  logistica = digitalRead(btn_logistica);
  calidad = digitalRead(btn_calidad);
  mantenimiento = digitalRead(btn_mantenimiento);
 
  ciclos();
  delay(10);
  bad_cutting();

  velocidad = analogRead(analog_read);
  //Serial.println(velocidad);
  s = produccion_paro + logistica + calidad + mantenimiento;
  if (s != sa) //Si el status de la maquina cambia, ejecuta funcion data
  {
    data(produccion_paro, logistica, calidad, mantenimiento, velocidad);
    sa = s;
  }
}
 
void EnviarDatos(){
  http.begin(client, "http://192.168.4.1/tema2/guardar_datos_nuevo.php"); //Especifique el destino de la solicitud
  http.addHeader("Content-Type", "application/x-www-form-urlencoded"); //Especificar encabezado de tipo de contenido
  int httpCode = http.POST(Datos); //Enviar la solicitud
  Serial.print("Codigo de retorno HTPP: ");
  Serial.println(httpCode); //Imprimir el codigo de retorno HTTP
  String payload = http.getString(); //Obtener la carga util de respuesta
  Serial.print("Datos enviados: ");
  Serial.println(Datos);
  Serial.print("Respuesta de impresion: ");
  Serial.println(payload); //Carga de respuesta de solicitud de impresion
  http.end();
  if(httpCode!=200)
  {
    contador_ciclos=contador_ciclos+1;
  }
  else{
    contador_bad=0;
    contador_ciclos=0;
   }
}
 
void ciclos(){
  s1_actual = digitalRead(ciclos_maq);
  if (s1_actual != s1_anterior) //Si la maquina detecta una pieza
  {  
    t_ant = millis();
    temp = 1; //Indica que temporizador esta activo
    produccion_paro = 1; //Produciendo  
       
      if(s1_actual == HIGH)
      {
        data(produccion_paro, logistica, calidad, mantenimiento, velocidad);
        contador_ciclos++;
        delay(100);
      }
      s1_anterior = s1_actual;
  }
 
  if((millis() - t_ant >= periodo) && temp == 1) //Si se rebasa el tiempo entre ciclos
  {
    produccion_paro = 0; //Paro
    temp = 0; //Se desactiva temporizador
  }
}
 
void bad_cutting(){
s2_actual = digitalRead(bad_piece);
  if (s2_actual != s2_anterior) //Si la maquina detecta una pieza
  {  
    t_ant_2 = millis();
    temp_2 = 1; //Indica que temporizador esta activo
    produccion_paro = 1; //Produciendo  
       
      if(s2_actual == HIGH)
      {
        contador_bad++;
        delay(100);
      }
      s2_anterior = s2_actual;
  }
 
  if((millis() - t_ant_2 >= periodo) && temp_2 == 1) //Si se rebasa el tiempo entre ciclos
  {
    produccion_paro = 0; //Paro
    temp_2 = 0; //Se desactiva temporizador
  }
}
 
 
void data(int produccion_paro,int logistica,int calidad,int mantenimiento, int velocidad)
{
  if(logistica == 1){l = 0;} else{l = 1;}
  if(calidad == 1){q = 0;} else{q = 1;}
  if(mantenimiento == 1){i = 0;} else{i = 1;}
 
  digitalWrite(maq_produccion_paro,produccion_paro);
  digitalWrite(maq_logistica,logistica);
  digitalWrite(maq_calidad,calidad);
  digitalWrite(maq_mantenimiento,mantenimiento);
 
  Datos="velocidad="+String(velocidad)+"&bad_cutting="+String(contador_bad)+"&good_cutting="+String(contador_ciclos-contador_bad)+"&ig="+String(ig)+"&P_STP="+String(produccion_paro)+"&L="+String(l)+"&Q="+String(q)+"&I="+String(i)+"";  
  EnviarDatos();
 
}
