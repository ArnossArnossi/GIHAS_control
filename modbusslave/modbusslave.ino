#include <Controllino.h> /* Usage of CONTROLLINO library allows you to use CONTROLLINO_xx aliases in your sketch. */
#include "ModbusRtu.h"   /* Usage of ModBusRtu library allows you to implement the Modbus RTU protocol in your sketch. */

// This MACRO defines Modbus slave address.
// For any Modbus slave devices are reserved addresses in the range from 1 to 247.
// Important note only address 0 is reserved for a Modbus master device!
#define SlaveModbusAdd 1

// This MACRO defines number of the comport that is used for RS 485 interface.
// For MAXI and MEGA RS485 is reserved UART Serial3.
#define RS485Serial 3

// The object ControllinoModbuSlave of the class Modbus is initialized with three parameters.
// The first parametr specifies the address of the Modbus slave device.
// The second parameter specifies type of the interface used for communication between devices - in this sketch - RS485.
// The third parameter can be any number. During the initialization of the object this parameter has no effect.
Modbus ControllinoModbusSlave(SlaveModbusAdd, RS485Serial, 0);

// This uint16 array specified internal registers in the Modbus slave device.
// Each Modbus device has particular internal registers that are available for the Modbus master.
uint16_t ModbusSlaveRegisters[52];


void setup()
{
    Serial.begin(9600);

    // Inputs (index 0 to 15)
    pinMode(CONTROLLINO_A0, INPUT);
    pinMode(CONTROLLINO_A1, INPUT);
    pinMode(CONTROLLINO_A2, INPUT);
    pinMode(CONTROLLINO_A3, INPUT);
    pinMode(CONTROLLINO_A4, INPUT);
    pinMode(CONTROLLINO_A5, INPUT);
    pinMode(CONTROLLINO_A6, INPUT);
    pinMode(CONTROLLINO_A7, INPUT);
    pinMode(CONTROLLINO_A8, INPUT);
    pinMode(CONTROLLINO_A9, INPUT);
    pinMode(CONTROLLINO_A10, INPUT);
    pinMode(CONTROLLINO_A11, INPUT);
    pinMode(CONTROLLINO_A12, INPUT);
    pinMode(CONTROLLINO_A13, INPUT);
    pinMode(CONTROLLINO_A14, INPUT);
    pinMode(CONTROLLINO_A15, INPUT);


    // Outputs (index 16 to 51, first relay index is 36)
    pinMode(CONTROLLINO_D0, OUTPUT);
    pinMode(CONTROLLINO_D1, OUTPUT);
    pinMode(CONTROLLINO_D2, OUTPUT);
    pinMode(CONTROLLINO_D3, OUTPUT);
    pinMode(CONTROLLINO_D4, OUTPUT);
    pinMode(CONTROLLINO_D5, OUTPUT);
    pinMode(CONTROLLINO_D6, OUTPUT);
    pinMode(CONTROLLINO_D7, OUTPUT);
    pinMode(CONTROLLINO_D8, OUTPUT);
    pinMode(CONTROLLINO_D9, OUTPUT);
    pinMode(CONTROLLINO_D10, OUTPUT);
    pinMode(CONTROLLINO_D11, OUTPUT);
    pinMode(CONTROLLINO_D12, OUTPUT);
    pinMode(CONTROLLINO_D13, OUTPUT);
    pinMode(CONTROLLINO_D14, OUTPUT);
    pinMode(CONTROLLINO_D15, OUTPUT);
    pinMode(CONTROLLINO_D16, OUTPUT);
    pinMode(CONTROLLINO_D17, OUTPUT);
    pinMode(CONTROLLINO_D18, OUTPUT);
    pinMode(CONTROLLINO_D19, OUTPUT);
    pinMode(CONTROLLINO_R0, OUTPUT);
    pinMode(CONTROLLINO_R1, OUTPUT);
    pinMode(CONTROLLINO_R2, OUTPUT);
    pinMode(CONTROLLINO_R3, OUTPUT);
    pinMode(CONTROLLINO_R4, OUTPUT);
    pinMode(CONTROLLINO_R5, OUTPUT);
    pinMode(CONTROLLINO_R6, OUTPUT);
    pinMode(CONTROLLINO_R7, OUTPUT);
    pinMode(CONTROLLINO_R8, OUTPUT);
    pinMode(CONTROLLINO_R9, OUTPUT);
    pinMode(CONTROLLINO_R10, OUTPUT);
    pinMode(CONTROLLINO_R11, OUTPUT);
    pinMode(CONTROLLINO_R12, OUTPUT);
    pinMode(CONTROLLINO_R13, OUTPUT);
    pinMode(CONTROLLINO_R14, OUTPUT);
    pinMode(CONTROLLINO_R15, OUTPUT);


    ControllinoModbusSlave.begin(19200); // Start the communication over the ModbusRTU protocol. Baund rate is set at 19200
}

void loop()
{
    // This instance of the class Modbus checks if there are any incoming data.
    // If any frame was received. This instance checks if a received frame is Ok.
    // If the received frame is Ok the instance poll writes or reads corresponding values to the internal registers
    // (ModbusSlaveRegisters).
    // Main parameters of the instance poll are address of the internal registers and number of internal registers.
    ControllinoModbusSlave.poll(ModbusSlaveRegisters, 52);

    // While receiving or sending data, the Modbus slave device periodically reads the values of the digital outputs
    // ,writes them to the register and updates the output states.
    ModbusSlaveRegisters[0] = digitalRead(CONTROLLINO_A0);
    ModbusSlaveRegisters[1] = digitalRead(CONTROLLINO_A1);
    ModbusSlaveRegisters[2] = digitalRead(CONTROLLINO_A2);
    ModbusSlaveRegisters[3] = digitalRead(CONTROLLINO_A3);
    ModbusSlaveRegisters[4] = digitalRead(CONTROLLINO_A4);
    ModbusSlaveRegisters[5] = digitalRead(CONTROLLINO_A5);
    ModbusSlaveRegisters[6] = digitalRead(CONTROLLINO_A6);
    ModbusSlaveRegisters[7] = digitalRead(CONTROLLINO_A7);
    ModbusSlaveRegisters[8] = digitalRead(CONTROLLINO_A8);
    ModbusSlaveRegisters[9] = digitalRead(CONTROLLINO_A9);
    ModbusSlaveRegisters[10] = digitalRead(CONTROLLINO_A10);
    ModbusSlaveRegisters[11] = digitalRead(CONTROLLINO_A11);
    ModbusSlaveRegisters[12] = digitalRead(CONTROLLINO_A12);
    ModbusSlaveRegisters[13] = digitalRead(CONTROLLINO_A13);
    ModbusSlaveRegisters[14] = digitalRead(CONTROLLINO_A14);
    ModbusSlaveRegisters[15] = digitalRead(CONTROLLINO_A15);
    digitalWrite(CONTROLLINO_D0, ModbusSlaveRegisters[16]);
    digitalWrite(CONTROLLINO_D1, ModbusSlaveRegisters[17]);
    digitalWrite(CONTROLLINO_D2, ModbusSlaveRegisters[18]);
    digitalWrite(CONTROLLINO_D3, ModbusSlaveRegisters[19]);
    digitalWrite(CONTROLLINO_D4, ModbusSlaveRegisters[20]);
    digitalWrite(CONTROLLINO_D5, ModbusSlaveRegisters[21]);
    digitalWrite(CONTROLLINO_D6, ModbusSlaveRegisters[22]);
    digitalWrite(CONTROLLINO_D7, ModbusSlaveRegisters[23]);
    digitalWrite(CONTROLLINO_D8, ModbusSlaveRegisters[24]);
    digitalWrite(CONTROLLINO_D9, ModbusSlaveRegisters[25]);
    digitalWrite(CONTROLLINO_D10, ModbusSlaveRegisters[26]);
    digitalWrite(CONTROLLINO_D11, ModbusSlaveRegisters[27]);
    digitalWrite(CONTROLLINO_D12, ModbusSlaveRegisters[28]);
    digitalWrite(CONTROLLINO_D13, ModbusSlaveRegisters[29]);
    digitalWrite(CONTROLLINO_D14, ModbusSlaveRegisters[30]);
    digitalWrite(CONTROLLINO_D15, ModbusSlaveRegisters[31]);
    digitalWrite(CONTROLLINO_D16, ModbusSlaveRegisters[32]);
    digitalWrite(CONTROLLINO_D17, ModbusSlaveRegisters[33]);
    digitalWrite(CONTROLLINO_D18, ModbusSlaveRegisters[34]);
    digitalWrite(CONTROLLINO_D19, ModbusSlaveRegisters[35]);

    digitalWrite(CONTROLLINO_R0, ModbusSlaveRegisters[36]);
    digitalWrite(CONTROLLINO_R1, ModbusSlaveRegisters[37]);
    digitalWrite(CONTROLLINO_R2, ModbusSlaveRegisters[38]);
    digitalWrite(CONTROLLINO_R3, ModbusSlaveRegisters[39]);
    digitalWrite(CONTROLLINO_R4, ModbusSlaveRegisters[40]);
    digitalWrite(CONTROLLINO_R5, ModbusSlaveRegisters[41]);
    digitalWrite(CONTROLLINO_R6, ModbusSlaveRegisters[42]);
    digitalWrite(CONTROLLINO_R7, ModbusSlaveRegisters[43]);
    digitalWrite(CONTROLLINO_R8, ModbusSlaveRegisters[44]);
    digitalWrite(CONTROLLINO_R9, ModbusSlaveRegisters[45]);
    digitalWrite(CONTROLLINO_R10, ModbusSlaveRegisters[46]);
    digitalWrite(CONTROLLINO_R11, ModbusSlaveRegisters[47]);
    digitalWrite(CONTROLLINO_R12, ModbusSlaveRegisters[48]);
    digitalWrite(CONTROLLINO_R13, ModbusSlaveRegisters[49]);
    digitalWrite(CONTROLLINO_R14, ModbusSlaveRegisters[50]);
    digitalWrite(CONTROLLINO_R15, ModbusSlaveRegisters[51]);
 
}