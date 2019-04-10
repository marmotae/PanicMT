# PanicMT
## Descripción
Desarrollo basado en Arduino para la creación de un detector de caidas y botón de pánico para personas de la tercera edad.
## Funcionalidad Actual
Actualmente el proyecto consiste en un detector de caidas basado en un [ESP32 con Batería](https://www.banggood.com/TTGO-T-Energy-ESP32-8MByte-PSRAM-WiFi-Bluetooth-Module-18650-Battery-ESP32-WROVER-IB-Development-Board-p-1427125.html) y un [MPU 6050](https://www.banggood.com/6DOF-MPU-6050-3-Axis-Gyro-With-Accelerometer-Sensor-Module-For-Arduino-p-80862.html)
## Limitaciones
### Limitaciones de Firmware
De momento este proyecto es resultado de la integración de varios desarrollos que realizé para probar distintos conceptos. En particular de momento hace falta portear y/o afinar lo siguiente en esta versión:

1. Migrar el código para interrupción vía el botón de pánico
2. Adecuar el código para integrar las rutinas que hacen que el _ESP32_ entre en modo de hibernación y se despierte ante una interrupción. Esta parte es escencial para poder ahorrar en consumo de batería
3. Integrar el código para la re captura de parámetros de conexión al servidor _MQTT_ de forma independiente a la configuración del _Access Point_ de WiFi, ya que de momento la única manera de cambiar los datos de conexión al servidor _MQTT_ consiste en resetear la totalidad de parámetros
### Limitaciones de Harware
De momento este proyecto está siendo re planteado con nuevos requerimientos, en particular la capacidad de dar conectividad fuera de zonas WiFi mediante el uso de _GPRS_, así como la integración de localización mediante _GPS_. Respecto de estas adecuaciones visualizo algunos retos importantes:
1. Energía: A pesar de que puedo ahorrar bastante energía mediante las rutinas de hibernación, el consumo de batería requerido para habilitar comunicación vía _GPRS_ así como la lectura de _GPS_ son particularmente altas.
2. Localización: A pesar de contemplar el uso de un GPS en la siguiente versión, existe otro reto adicional. En caso de que hicieramos una lectura dentro de un edificio, no podríamos detectar satélites y por lo tanto no podríamos ubicar el dispositivo. Los celulares y otras herramientas similares atacan este problema realizando una trazabilidad histórica, esto es, hacen varias mediciones para intentar al menos dar una ubicación aproximada cuando no se tiene de forma exacta. Esto implicaría un consumo mayor de bateria debido a las mediciones recurrentes. Estoy considerando en su defecto triangulación mediante red celular y mapa de _Access Points_ de Google.
### Otras Limitaciones
El STL provisto para la impresión 3D de la carcaza cambiará debido a los nuevos requerimientos ya que los requerimientos de espacio y energía cambian. Así mismo, se contempla hacer que la nueva carcasa sea a prueba de agua debido a que una buena proporción de las caidas de personas de la tercera edad se da en el baño.
