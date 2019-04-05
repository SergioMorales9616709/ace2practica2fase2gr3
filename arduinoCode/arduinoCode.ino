#include <PID_v1.h>
#include <LMotorController.h>
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"

#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include "Wire.h"
#endif

#define MIN_ABS_SPEED 20

//Definiciones para el sonar
int PIN_TRIGGER = A8;
int PIN_ECHO = A9;
#define MAX_DISTANCE 400
int tiempoAtras = 0;
int tiempoAtras2 = 0;
//int encendido = 0;
int DISTANCIA = 10;
//*****************************

//Definiciones de pines de lectura de sensores IR
// 1= negro , 0 = blanco
#define IRPinR A0
#define IRPinC A1
#define IRPinL A2

MPU6050 mpu;
//NewPing sonar(PIN_TRIGGER, PIN_ECHO, MAX_DISTANCE);

//Pines de Bluetooth
char dato = '\0';

//******************

/******************************************
DROP THE PACKAGE
SERVOMOTOR
******************************************/
#include <Servo.h>
Servo servo1;
//\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\//

// MPU control/status vars
bool dmpReady = false; // set true if DMP init was successful
uint8_t mpuIntStatus; // holds actual interrupt status byte from MPU
uint8_t devStatus; // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize; // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount; // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

// orientation/motion vars
Quaternion q; // [w, x, y, z] quaternion container
VectorFloat gravity; // [x, y, z] gravity vector
float ypr[3]; // [yaw, pitch, roll] yaw/pitch/roll container and gravity vector

//PID
double originalSetpoint = 183; /* before value 170, best values 182, 183*/
double setpoint = originalSetpoint;
double movingAngleOffset = 0.95; /*0.1 , 0.12 , 0.15, 0.3 *//* 0.5, 0.8, 0.85 best value 0.95 */
double input, output;

//adjust these values to fit your own design
double Kp = 90; /* original value 60 *//* if Kp is less than 60, the rover reactions slower // best value 90, 120*/
double Kd = 1.0; /* with value equal to 1.0 , it's a little bit better //best value 1.0 *//* original value 1.4 */
double Ki = 5; /* with values equal since 3 until 10 , it's a little bit better // best value 10 *//* original value 70 */
PID pid(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);

/* right  sonic sensor left */
double motorSpeedFactorLeft = 0.31; /* 0.30 best value 0.3, 0.32 */
double motorSpeedFactorRight = 0.26; /* 0.25 best value 0.24 */
//MOTOR CONTROLLER
int ENA = 5;
int IN1 = 6;
int IN2 = 7;
int IN3 = 8;
int IN4 = 9;
int ENB = 10;
LMotorController motorController(ENA, IN1, IN2, ENB, IN3, IN4, motorSpeedFactorLeft, motorSpeedFactorRight);

//Variables de movimiento
int moveState = 0; //0 = balance; 1 = atras; 2 = adelante;
bool sequenceStatus = false;

//Variables de lectura de sensores IR - 0 Linea negra; 1 Color detectado
byte IRL, IRC, IRR;
//Variables de lectura de sensor ultrasonico
float distance;
const float blockageDistance = 10; //10 cm como rango de deteccion de bloqueo
bool blocked = false;
int blockWait = 3000;
unsigned long blockTime;

volatile bool mpuInterrupt = false; // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
    mpuInterrupt = true;
}

/***************
VARS STATUS
***************/
int STATUS_ROVER = 0;
int STATUS_MOVE = 2;
int btnStart = 1;

//******************************************
//              VARS FOR MOVING
//******************************************
#define MAX_SPEED 255
int DRIVER_MODE = 0; // encuentra un obstaculo y espera que lo quiten y que envien un 7
float TIEMPO_INICIA_ESPERA = 0;
float TIEMPO_ESPERA = 10; //en segundos

// VARS TO TEST
int SIMULA_OBJETO = 0;

/* right  sonic sensor left */
double mSpeedFacLeftB = 0.31; /* best value 0.400 */
double mSpeedFacRightB = 0.285; /* best value 0.395 */
LMotorController motorControllerB(ENA, IN1, IN2, ENB, IN3, IN4, mSpeedFacLeftB,	mSpeedFacRightB);

//******************************************
//       VARS RECEIVED FROM ANDROID
//******************************************
int MEASURE_STOP = 10;
int STANDBY_TIME = 10;

void setup() {
    // join I2C bus (I2Cdev library doesn't do this automatically)
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Wire.begin();
    TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz)
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
    Fastwire::setup(400, true);
#endif

    mpu.initialize();

    devStatus = mpu.dmpInitialize();

    // supply your own gyro offsets here, scaled for min sensitivity
    mpu.setXGyroOffset(28); /* 220 */
    mpu.setYGyroOffset(-28); /* 76 */
    mpu.setZGyroOffset(-4); /* -85 */
    mpu.setZAccelOffset(2117); /* 1788 */ // 1688 factory default for my test chip

    // make sure it worked (returns 0 if so)
    if (devStatus == 0) {
        // turn on the DMP, now that it's ready
        mpu.setDMPEnabled(true);

        // enable Arduino interrupt detection
        attachInterrupt(0, dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        dmpReady = true;

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();

        //setup PID
        pid.SetMode(AUTOMATIC);
        pid.SetSampleTime(10);
        pid.SetOutputLimits(-255, 255);
    } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        // Serial.print(F("DMP Initialization failed (code "));
        // Serial.print(devStatus);
        // Serial.println(F(")"));
    }

    /******************************************
        Code added
        xD
        ******************************************/

    //********Sonico***********
    pinMode(PIN_TRIGGER, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    //*************************

    //Bluetooth***********
    //pinMode(A10, OUTPUT); //salida del Led para probar
    Serial.begin(9600);
    //*********************

    /******************************************
        DROP THE PACKAGE
        SERVOMOTOR
        ******************************************/
    servo1.attach(12, 600, 1500);
    servo1.write(83);
    //\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\//
    /******************************************
        START BOTTON

        ******************************************/
    pinMode(A15, INPUT);
    //\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\//
    //Inicializacion de tiempos para sensor ultrasonico
    //tiempoAtras2 = micros();
    //tiempoAtras  = micros();
}
//*******************************
//          temp vars
//*******************************
int tmpContador = 0;
//////////////////////////////////
void loop() {
    // get values
    btnStart = digitalRead(A15);
    getValueLineSensor();

    if (btnStart == HIGH) {
        if (STATUS_ROVER == 0) {
            balancingF();
            STATUS_ROVER = 1;
        } else if (STATUS_ROVER == 1) {
            STATUS_ROVER = 0;
        }
    }
    if (STATUS_ROVER == 0) {
        //moveFront(5);
        //moveBack(5);
    }
    else if (STATUS_ROVER == 1) { // THE ROVER IS JUST BALANCING
        balancingF();
        getValueLineSensor();
        if (IRL == 1 && IRC == 0 and IRR == 1) { // Condicion de inicio 101
            STATUS_ROVER = 2;
        }
        else {
            STATUS_ROVER = 1;
        }

        /* //IT WAS A GOOD IDEA, BUT IT DIDN'T WORK
                //******************************************
                //              FOR MOVING
                //******************************************

                // STATUS WHEN THE ROVER IS MOVING TO FRONT
                if ( STATUS_MOVE == 2 ) {
                //goAhead();
                //balancingF();
        }
        // STATUS WHEN THE ROVER IS MOVING TO BACK
        else if ( STATUS_MOVE == 3 ) {
        //goBack();
        //balancingF();
}
// STATUS WHEN THE ROVER IS MOVING TO LEFT
else if ( STATUS_MOVE == 4 ) {
}
// STATUS WHEN THE ROVER IS MOVING TO RIGHT
else if ( STATUS_MOVE == 5 ) {
}
*/
    }
    else if (STATUS_ROVER == 2) { // WHEN THE ROVER IS IN THE INITIAL POINT
        Serial.println("State 2.");
        delay(1000);
        moveFront(10);
        getValueLineSensor();
        if (IRL == 0 && IRC == 1 and IRR == 0) { // IF IT ENTERS, THE ROVER IS ALREADY ON THE LINE
            STATUS_ROVER = 3;
        }
    }
    else if (STATUS_ROVER == 3) { // WHEN THE ROVER IS MOVING ON THE LINE
        //balancingF();
        Serial.println("State 3.");
        getValueLineSensor();
        if ( itsGoingToCrash() ) {
            STATUS_ROVER = 4;
            return;
        }
        // ********** CENTRADO ***********
        else if (IRL == 0 && IRC == 1 and IRR == 0) {
            moveFront(10);
        }
        // ********** CORRIDO DERECHA ***********
        else if ( IRL == 1 && IRC == 1 and IRR == 0) { // THE ROVER IS MOVED TO RIGHT
            turnLeftB(0.35);
        }
        // ********** CORRIDO IZQUIERDA ***********
        else if (IRL == 0 && IRC == 1 and IRR == 1) { // THE ROVER IS MOVED TO LEFT
            turnRightB(0.35);
        }
        // ********** MUY CORRIDO DERECHA ***********
        else if ( IRL == 1 && IRC == 0 and IRR == 0) { // THE ROVER IS MOVED TO RIGHT
            turnLeftB(0.35);
            //moveBack(4);
        }
        // ********** MUY CORRIDO IZQUIERDA ***********
        else if (IRL == 0 && IRC == 0 and IRR == 1) { // THE ROVER IS MOVED TO LEFT
            turnRightB(0.35);
            //moveBack(4);
        }
        else { // THE ROVER IS LOST, IT'S SO SAD
            Serial.println("Other states on the road.");
            //STATUS_ROVER = 4;
            if ( IRL == 1 && IRC == 1 and IRR == 1) { // THE FINAL POSITION
                Serial.println("Llego al final.");
                STATUS_ROVER = 6;
            }
            if (IRL == 1 && IRC == 0 and IRR == 1) { // the other final position
                Serial.println("Regreso al inicio.");
                STATUS_ROVER = 0;

            }
            else if ( IRL == 0 && IRC == 0 and IRR == 0) { // trato de esquivar y se perdio
                /* call method error */
                // reset arduino
                STATUS_ROVER = 0;
            }
        }
    }
    else if (STATUS_ROVER == 4) { // WHEN THE ROVER IS IN STANDBY STATE
        Serial.println("State 4.");
        // CODE MISSED. HERE IT SHOULD HAVE ALMOST TWO OPTIONS: FIRST CRONNOS SECOND **ERROR
        if ( DRIVER_MODE == 0 ){ // ** MANUAL MODE, ESPERA HASTA QUE LE QUITEN EL OBSTACULO y recibir opcion continue
            moveFront(5);
            moveBack(5);
        }
        else if ( DRIVER_MODE == 1 ){ // ** AUTOMATIC MODE, ESPERA EL TIEMPO CONFIGURADO Y LUEGO LLAMA EVADIR OBSTACULO
            /* call method stanby or balancing */
            if ( ( millis() - TIEMPO_INICIA_ESPERA) / 1000 < STANDBY_TIME ) {
                moveFront(5);
                moveBack(5);
                STATUS_ROVER = 4;
                DRIVER_MODE = 1;
            }
            else {
                STATUS_ROVER = 5;
            }
        }
        else if ( DRIVER_MODE == 2 ) { // DESPUES DE QUITAR EL OBSTACULO ENVIAR DESDE ANDROID Y SETEAR DRIVER_MODE == 2
            STATUS_ROVER = 3;
        }
    }
    else if ( STATUS_ROVER == 5 ) { // WHEN THE ROVER IS MAKING A CURVE AROUND THE OBJECT
        Serial.println("State 5.");
        passObject();
        STATUS_ROVER = 3;
    }
    else if (STATUS_ROVER == 6) { // WHEN THE ROVER IS IN THE FINAL POINT
        Serial.println("Descargar Paquete.\nState 6");
        dropPackage();
        //dropPackage();
        STATUS_ROVER = 7;
    }
    else if (STATUS_ROVER == 7) { // WHEN THE ROVER IS SWITCHING START AND FINAL POSITION
        Serial.println("SWITCHING POSITION.\nState 6");
        turnCenterL(0.75);
        moveFront(15);
        STATUS_ROVER = 3;
    }
    //**********************************************************************
    //                              BLUETOOTH
    //**********************************************************************
    if (Serial.available()) {
        LecturaDato();
    }
    //**********************************************************************
}
void simulBalanc(){
    moveFront(5);
    moveBack(5);
}
bool itsGoingToCrash() { // ** IF THERE'S SOMETHING, RETURN TRUE
//    int duracion, distGot;

//    digitalWrite(PIN_TRIGGER, LOW);
//    delayMicroseconds(2);
//    digitalWrite(PIN_TRIGGER, HIGH);
//    delayMicroseconds(10);
//    digitalWrite(PIN_TRIGGER, LOW);

//    duracion = pulseIn(PIN_ECHO, HIGH);
//    distGot = (duracion/2)/29;
//            if ( distGot <= DISTANCIA) {
//                Serial.print("Dista: ");
//                        Serial.print( distGot );
//                return true;
//            }
//            return false;

    if ( SIMULA_OBJETO == 1 ) {
        return true;
    }
    return false;
}
void getValueLineSensor() { // ** READING THE LINE SENSORS **
    //Evaluacion de lecturas de sensores
    IRL = digitalRead(IRPinL);
    IRC = digitalRead(IRPinC);
    IRR = digitalRead(IRPinR);
}
void moveBack(int centm) {  // ** IT MOVES THE ROVER TO BACK **
    int i;
    for (i = 0; i < centm; i++) {
        /* [50 , 20] [70 , 30] [75 , 40]*/
        motorControllerB.move(-MAX_SPEED, MIN_ABS_SPEED);
        delay(50); /* with 10 it doesn't work, 20 equals a quarter stepp */
        motorControllerB.stopMoving();
        delay(10); /* it has to be here, best value 10 */

        motorControllerB.move(MAX_SPEED, MIN_ABS_SPEED);
        delay(20); /* with 10 it doesn't work, 20 equals a quarter stepp */
        motorControllerB.stopMoving();
        delay(10); /* it has to be here, best value 10 */
    }
}
void moveFront(int centm) { // ** IT MOVES THE ROVER TO FRONT **
    int i;
    for (i = 0; i < centm; i++) {
        /* [50 , 20] [70 , 30] [75 , 40]*/
        motorControllerB.move(MAX_SPEED, MIN_ABS_SPEED);
        delay(50); /* with 10 it doesn't work, 20 equals  a quarter stepp */
        motorControllerB.stopMoving();
        delay(10); /* it has to be here, best value 10 */

        motorControllerB.move(-MAX_SPEED, MIN_ABS_SPEED);
        delay(20); /* with 10 it doesn't work, 20 equals a quarter stepp */
        motorControllerB.stopMoving();
        delay(10); /* it has to be here, best value 10 */
    }
}
void turnCenterL(double vuelta) { // ** THE ROVER WILL BE TURNED AT RIGHT **
    int turns = 30 * vuelta;
    for (int index = 0; index < turns; index++) {
        motorControllerB.turnCenterLeft (MAX_SPEED, MIN_ABS_SPEED);
        delay(31); /* with 10 it doesn't work, 20 equals to one stepp */
        motorControllerB.stopMoving();
        delay(10); /* it has to be here, best value 10 */

    }
}
void turnCenterR(double vuelta) { // ** THE ROVER WILL BE TURNED AT RIGHT **
    int turns = 30 * vuelta;
    for (int index = 0; index < turns; index++) {
        motorControllerB.turnCenterRight(MAX_SPEED, MIN_ABS_SPEED);
        delay(31); /* with 10 it doesn't work, 20 equals to one stepp */
        motorControllerB.stopMoving();
        delay(10); /* it has to be here, best value 10 */

    }
}
void turnLeftF(double vuelta) { // ** THE ROVER WILL BE TURNED AT LEFT **
    int turns = 30*vuelta;
    for (int index = 0; index < turns; index++) {
        motorControllerB.turnRight(MAX_SPEED, MIN_ABS_SPEED);
        delay(31); /* with 10 it doesn't work, 20 equals to one stepp */
        motorControllerB.stopMoving();
        delay(10); /* it has to be here, best value 10 */
    }
}
void turnLeftB(double vuelta) { // ** THE ROVER WILL BE TURNED AT LEFT **
    int turns = 30 * vuelta;
    for (int index = 0; index < turns; index++) {
        motorControllerB.turnRight(-MAX_SPEED, MIN_ABS_SPEED);
        delay(31); /* with 10 it doesn't work, 20 equals to one stepp */
        motorControllerB.stopMoving();
        delay(10); /* it has to be here, best value 10 */
    }
}
void turnRightB(double vuelta) { // ** THE ROVER WILL BE TURNED AT RIGHT **
    int turns = 30*vuelta;
    for (int index = 0; index < turns; index++) {
        motorControllerB.turnLeft(-MAX_SPEED, MIN_ABS_SPEED);
        delay(31); /* with 10 it doesn't work, 20 equals to one stepp */
        motorControllerB.stopMoving();
        delay(10); /* it has to be here, best value 10 */
    }
}
void turnRightF(double vuelta) { // ** THE ROVER WILL BE TURNED AT RIGHT **
    int turns = 30*vuelta;
    for (int index = 0; index < turns; index++) {
        motorControllerB.turnLeft(MAX_SPEED, MIN_ABS_SPEED);
        delay(31); /* with 10 it doesn't work, 20 equals to one stepp */
        motorControllerB.stopMoving();
        delay(10); /* it has to be here, best value 10 */
    }
}
void passObject() { // **  IT SHOULD BE ENOUGH TO PASS THE OBJECT **
    moveFront(10);
    delay(1000);
    turnRightB(0.51);
    moveFront(20);
    delay(1000);
    turnLeftF(0.51);
    delay(1000);
    moveFront(30);
    delay(1000);
    turnLeftF(0.51);
    delay(1000);
    moveFront(20);
    delay(1000);
    turnRightF(0.51); /* for half turns it works */
    delay(1000);
    moveFront(10);
    delay(3000);
}
void balancingF() { // ** THIS METHOD DOES MANY THINGS **
    // if programming failed, don't try to do anything
    if (!dmpReady) {
        //STATUS_ROVER = 0;
        return;
    }

    // wait for MPU interrupt or extra packet(s) available
    while (!mpuInterrupt && fifoCount < packetSize) {
        //no mpu data - performing PID calculations and output to motors
        pid.Compute();
        motorController.move(output, MIN_ABS_SPEED);
        //    Serial.print("Value output: ");
        //    Serial.println(output);
    }

    // reset interrupt flag and get INT_STATUS byte
    mpuInterrupt = false;
    mpuIntStatus = mpu.getIntStatus();

    // get current FIFO count
    fifoCount = mpu.getFIFOCount();

    // check for overflow (this should never happen unless our code is too inefficient)
    if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
        // reset so we can continue cleanly
        mpu.resetFIFO();
        //Serial.println(F("FIFO overflow!"));

        // otherwise, check for DMP data ready interrupt (this should happen frequently)
    } else if (mpuIntStatus & 0x02) {
        // wait for correct available data length, should be a VERY short wait
        while (fifoCount < packetSize)
            fifoCount = mpu.getFIFOCount();

        // read a packet from FIFO
        mpu.getFIFOBytes(fifoBuffer, packetSize);

        // track FIFO count here in case there is > 1 packet available
        // (this lets us immediately read more without waiting for an interrupt)
        fifoCount -= packetSize;

        mpu.dmpGetQuaternion(&q, fifoBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
        input = ypr[1] * 180 / M_PI + 180;
    }
}
void dropPackage() { // ** METHOD FOR DROPING THE PACKAGE **
    servo1.write(0);
    delay(100);
    servo1.write(30);
    delay(100);
    servo1.write(0);
    delay(100);
    servo1.write(30);
    delay(100);
    servo1.write(0);
    delay(100);
    servo1.write(84);
    delay(150);
}
void LecturaDato() { //********Bluetoth*****
    dato = Serial.read();
    /* manual */
    if (dato == '0') {
        STATUS_ROVER == 0;
    }
    /* automatic */
    else if (dato == '1') {
        if (STATUS_ROVER == 0) {
            STATUS_ROVER = 1;
        } else {
            STATUS_ROVER = 0;
        }
    }
    /* move to front */
    else if (dato == '2') {
        moveFront(5);
    }
    /* move to back */
    else if (dato == '3') {
        moveBack(10);
    }
    /* turn left */
    else if (dato == '4') {
        turnLeftB( 0.60 );
    }
    /* turn right */
    else if (dato == '5') {
        turnRightB( 0.60 );
    }
    /* turn 180 degress */
    else if (dato == '6') { //* encontro un obst y esta AUTOMATICO
        SIMULA_OBJETO = 1;
        DRIVER_MODE = 1;

        //turnCenterL(0.75);
    }
    /* turn 180 degress */
    else if (dato == '7') { //* ENCONTRO UN OBST Y ESTA EN ESPERA
        SIMULA_OBJETO = 1;
        DRIVER_MODE = 0;
        //turnCenterR(0.75);
    }
    /* pass object */
    else if (dato == '8') { //* CONTIINUAR MODO ESPERA
        DRIVER_MODE = 2;
    }
    /* drop the package */
    else if (dato == '9') {
        dropPackage();
    }
    /* tiempo */
    else if (dato == '#') { // ** llenar tiempo espera
        String tmp = " ";
        //STANDBY_TIME = Serial.parseInt();
        tmp = Serial.readString();
        //Serial.write( STANDBY_TIME );
        Serial.write( "STANDBY_TIME: " );
        Serial.println( tmp );
    }
    /* distancia */
    else if (dato == '|') {
        char cadena[15];
        String tmp = "";
        tmp = Serial.readString();
        tmp.toCharArray( cadena, 15 ); /* CADENA RECIBIDA COMPLETA */

        //MEASURE_STOP = tmp.toString();
        Serial.write( "MEASURE_STOP: " );
        Serial.println( cadena );
    }


}
bool isThereObject() {
    int duracion, distGot;

    digitalWrite(PIN_TRIGGER, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIGGER, HIGH);
    delayMicroseconds(10);

    duracion = pulseIn(PIN_ECHO, HIGH);
    distGot = (duracion/2)/29;

    if ( distGot <= DISTANCIA) {
        return true;
    }
    return false;

}
void Sonido() {
}
