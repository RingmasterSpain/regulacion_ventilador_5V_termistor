/*  ==================================================================

  Sketch control ventilador por PWM con ATTiny85 económico termistor NTC.
  Configurado para ventilador genérico; con autolimpieza al arranque.
  
  Autor: David Losada, basado en trabajos de Rafael Torrales y Antonio Moles.
  Version: 1.0
  Fecha: Octubre 2016. Última actualización 13/10/2016
  Compilado con Arduino 1.6.9

    ==================================================================   */

#include <EEPROMex.h> //Instalar esta rutina si no la tenéis; está en el repositorio general de Arduino 1.6.9
//Si anduviéramos cortos de memoria, habría que eliminar la librería EEPROM y el código relacionado

//******************* A CAMBIAR SEGÚN TU CONFIGURACIÓN ********************************
//Control temperatura
const float resistor = 6800; //El valor en ohmnios de la resistencia del termistor a 25ºC
const float voltage = 5.0; // El voltaje real en el punto 5Vcc de tu placa Arduino

//Deltas de temperatura
int tempMin = 30; //Temperatura mínima de activación del ventilador
int tempMax = 50; //temperatura a la que funcionará al 100%
//0,85V=velocidad mínima de vent. PS3 y 1,25V vel. máxima
double PWMMin = 0.8; //Voltaje PWM de funcionamiento con temp. mínima (el de la PS3 se pone en marcha con 0,8V)
double PWMMax = 5; //Voltaje PWM de funcionamiento al máximo ventilador
//Hay que tener en cuenta que los voltajes se ajustan a las temperaturas pero no se ciñen a ellas,
//es decir, que a tempMin el voltaje será PWMMin, pero a menor temp. el voltaje bajará, y lo mismo
//con el voltaje máximo; si sigue subiendo la temperatura, el voltaje PWM aumentará en consecuencia,
//hay que tener cuidado porque podríamos dar más voltaje del necesario al ventilador

//Definición pines digitales para activar relés
int TempPIN=1; //En el ATTiny85 las entradas son (1)=P2; (2)=P4; (3)=P3; (0)=P5 -> Pin al que conectamos el termistor
int placaPIN=0; //
//(Entre el GND y ése pin soldaremos una resistencia del mismo valor del termistor)
int motorPWM=4; //Pin PWM de control motor; P0, P1, P4 el en ATTiny85
const int frecseg = 1; //Tiempo en segundos entre cada ejecución del programa (recomendado entre 1 y 5 segundos)
int ledPIN=1; //Según la placa, en este caso el LED del ATTiny85 B es el 1

//********************************************************************************
//Variables
float Temp= 0; //Valor entrada sensor temp
int fanSpeed= 0; //Velocidad de motor
long timeLED=0; //Para almacenar tiempo
unsigned long millisInicio=0; //Para comprobar paso de horas

//Parte del cálculo temperatura
//Para ahorrar cálculos lo definimos como constante en esta parte del programa
const float K= 273.15; //Para pasar a grados Kelvin
const float e = 2.718281828459045; //Constante matemática 
//const float B = log(RPto2/RPto1)/(1/(TPto2+K)-(1/(TPto1+K))); //Valor Beta de tu termistor calculado de dos puntos
const float B = 3850; //Valor Beta del Datasheet; comentar esta línea si se meten los datos de los dos puntos
const float unodivr = 1/(resistor * pow(e,(-B/298.15))); //Con pow elevamos e al resultado

float T = 0; //Declaramos la variable Temperatura
int grados, decimas; //Para ponerle coma al resultado (en español)

void setup()   { 
//Resetea la EEPROM la primera vez que la usamos
EEPROM.setMemPool(0,512); //Establecemos inicio y tamaño de EEPROM de la tarjeta utilizada
EEPROM.setMaxAllowedWrites(128); //Mínimo para que pueda hacerse el primer borrado completo
//rellenamos de 0 los datos de EEPROM
//Attiny85 tiene 512 Bytes de EEPROM; ajustar según el que uséis
if (EEPROM.readLong(508)!=7091976) { //En ese punto, preferiblemente al final, guardo un valor para saber si ya la he reseteado
  for(int i=0; i<502; i=i+4) { //Ajustar el valor según el tamaño de la (EEPROM)-10
     EEPROM.update(i,0);
      }
  EEPROM.update(508,7091976); //Almacenamos un número indicativo de que ya se ha inicializado en la última posición
  //Serial.println("Borrado de EEPROM terminado");
}
  
//Definir pines
pinMode(TempPIN, INPUT);
pinMode(placaPIN,INPUT);
pinMode(motorPWM, OUTPUT);
pinMode(ledPIN, OUTPUT);

//Regla de tres; voltMax corresponde a 5V, convertimos a rango de valores PWM
PWMMax=(255*PWMMax)/5;
PWMMin=(255*PWMMin)/5;
millisInicio=millis();
//lcd.begin(16,2); //pantalla de 16 caracteres y 2 filas

//delay(5000); //La consola tarda 10 segundos en alimentar el ventilador desde que se enciende
//Para limpieza, ponemos el ventilador a tope 3 segundos
//for(int i=50; i<255; i=i+25) {
//  analogWrite(motorPWM,i); //Ponemos a tope el ventilador de forma progresiva
//  analogWrite(ledPIN,50); //enciende LED indicando funcionamiento
//  delay(50); //Esperamos 5 segundos en total
//  analogWrite(ledPIN,0); //enciende LED indicando funcionamiento
//  delay(50);
//}
//delay(3000); //dejamos que el motor coja máximas revoluciones
//analogWrite(motorPWM,0); //lo volvemos a parar

}
void loop() {
  //float B= (RPto2/RPto1);//(1/(TPto2+K)-(1/(TPto1+K)));
// Parte 1:  Leemos el puerto analógico 0 y convertimos el valor en voltios.

//Recoge 5 veces para sacar medias
 for(int x=0; x<5; x++) 
    {    
         Temp=Temp+analogRead(TempPIN);
      delay(25); // espera 25ms entre lecturas
    }
//Sacar media
Temp=Temp/5; 

//Convertimos los valores a la temperatura correcta
    float v2 = (voltage*float(Temp))/1024.0f;  //Convertimos a voltios :)  
  
 // Parte 2: Calcular la resistencia con el valor de los voltios mediante la ecuación del divisor de voltaje
  //voltage = 4.83
  //R2 = 10000
  //R1 = Thermistor resistance
  //V2= v2
  //so V2=(R2*V)/(R1+R2)
  //and r1=((r2*v)/v2)-r2 <--final
  
  float r1a = (voltage*float(resistor))/v2;  
  float r1 =r1a - resistor;


  //Parte 3: Calcular la temperatura basandose en la ecuación Steinhart-Hart y la ecuación del valor Beta.
  // T=B/ln(r1/rinfinit)


  float T = B/log(r1*unodivr);
  Temp=T-273.15; //Convertimos a ºC y ya tenemos la temperatura
 
//Comprobamos temperatura, y actuamos sobre motor en consecuencia
if (Temp>=tempMin) { //No nos interesa ponerlo en marcha a menos que se alcance la temp. mínima
  analogWrite(ledPIN,150);//Marcamos indicando que activamos el ventilador
  fanSpeed = doubleMap(Temp, tempMin,tempMax,PWMMin,PWMMax); // Calcula la velocidad a la que corresponde estar
  //(Necesitamos que según aumente la temperatura, lo haga fanSpeed)
  //Valor PWM de 255 en el ATTiny85=5V
  analogWrite(motorPWM, fanSpeed);  // Modifica la velocidad para bajar temp
  delay(100);
  analogWrite(ledPIN,0);
}
else {
  analogWrite(motorPWM, 0);
}
  //DEBUG por serial (no disponible en el aTTiny)
  //Serial.print("Temp: ");
  //Serial.println(Temp);
  //Serial.print("Velocidad: ");
  //Serial.println(fanSpeed); 
  //Serial.print("V: ");
  //Serial.print(fanSpeed*5/255);
  //Serial.print(",");
  //Serial.print(int(fanSpeed*5/2.55));
  //Serial.println("V ");
  //Serial.print("Tiempo encendido: ");   

  //Hacemos parpadear al LED comprobando el tiempo desde la última activación
  if (timeLED>millis()) { //Cuando pasen 50 dias resetear
    timeLED=millis();
  }
  if ((millis()-timeLED)>frecseg*1000*2) {
      analogWrite(ledPIN,50); //enciende LED indicando funcionamiento
      timeLED=millis();
  }
    if ((millis()-timeLED)>frecseg*1000) {
      analogWrite(ledPIN,0); //apaga led
    }

//Si ha pasado una hora desde la puesta en marcha, comprobamos si >= Horas limpieza
//Y si es así, ponemos al tope el ventilador 5 segundos
if ((millis()-millisInicio)>3600000) { //Ha pasado una hora
  millisInicio=millis(); 
  EEPROM.update(0,EEPROM.readLong(0)+1); //Añadimos una hora
}

  delay(frecseg*900); //Ponemos en espera al Atmel
}

// *************** RUTINA MAP MEJORADA (admite fracciones)
double doubleMap(double x, double in_min, double in_max, double out_min, double out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
