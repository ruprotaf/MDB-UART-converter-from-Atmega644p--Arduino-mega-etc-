#include <avr/wdt.h>

#define BAUD 9600

#define ADDRESS_MASK      (0xF8)  // Первый байт сообщения состоит из адреса устройства (биты 7,6,5,4,3) 
#define COMMAND_MASK      (0x07)  // и кода команды (биты 2,1,0)

// Адреса всех устройств (MDB Version 4.2, page 25)
#define ADDRESS_VMC       (0x00)  // Reserved!!!
#define ADDRESS_CHANGER   (0x08)  // Coin Changer
#define ADDRESS_CD1       (0x10)  // Cashless Device 1
#define ADDRESS_GATEWAY   (0x18)  // Communication Gateway
#define ADDRESS_DISPLAY   (0x20)  // Display
#define ADDRESS_EMS       (0x28)  // Energy Management System
#define ADDRESS_VALIDATOR (0x30)  // Bill Validator
#define ADDRESS_USD1      (0x40)  // Universal satellite device 1
#define ADDRESS_USD2      (0x48)  // Universal satellite device 2
#define ADDRESS_USD3      (0x50)  // Universal satellite device 3
#define ADDRESS_COIN1     (0x58)  // Coin Hopper 1
#define ADDRESS_CD2       (0x60)  // Cashless Device 2
#define ADDRESS_AVD       (0x68)  // Age Verification Device
#define ADDRESS_COIN2     (0x70)  // Coin Hopper 2

//это автоматическая настройка скорости
#include <util/setbaud.h>

// MDB 9-bit определяется как два байта
struct MDB_Byte {
  byte data;
  byte mode;
};

//массив команд *POLL* для каждого из устройств (включая адрес)
byte POLL_ADDRESS[10]{0x0B, 0x12, 0x1A, 0x33, 0x42, 0x4A, 0x52, 0x5B, 0x62, 0x73};

byte EXT_UART_BUFFER[37]; //входящий буфер для полученной команды от VMC
struct MDB_Byte MDB_BUFFER[37]; //входящий буфер для полученной команды от MDB устройства
int EXT_UART_BUFFER_COUNT;
volatile int MDB_BUFFER_COUNT;

//флаги состояния получениях данных от MDB
int rcvcomplete;  //сообщение MDB получено
int mdboverflow;  //сообщение MDB ошибочно



void MDB_Setup() {
  // установка скорости порта через setbaud.h
  UBRR0H = UBRRH_VALUE;
  UBRR0L = UBRRL_VALUE;
  // Выключение USART rate doubler (arduino bootloader остается включен...)
  UCSR0A &= ~(1 << U2X0);
  // установка 9600-9-N-1 UART режима
  UCSR0C = (0<<UMSEL01)|(0<<UMSEL00)|(0<<UPM01)|(0<<UPM00)|(0<<USBS0)|(1<<UCSZ01)|(1<<UCSZ00);
  UCSR0B |= (1<<UCSZ02); // 9bit
  // включаем rx/tx
  UCSR0B |= (1<<RXEN0)|(1<<TXEN0);
}

void EXT_UART_Setup()
{
  Serial1.begin(9600);
}

void EXT_UART_read() {//получаем команду от VMC
  EXT_UART_BUFFER_COUNT = 0; //ставим размер буфера 0
  while (Serial1.available()) {
    //записываем все данные с порта в приемный буфер
    EXT_UART_BUFFER[EXT_UART_BUFFER_COUNT++]=Serial1.read(); //и за одно получаем размер этой команды
    delay(20); //задержка а то команда не успевает приняться целиком 
  }  
  if ((EXT_UART_BUFFER_COUNT > 0) && EXT_ChecksumValidate()) {//проверяем: если буфер больше 0 и контрольная сумма верна
  //Serial1.print("Recvd CMD: "); //отправляем на хост подтвержденеи принятой команды
  //for (int a = 0; a < EXT_UART_BUFFER_COUNT; a++){
  //  if (EXT_UART_BUFFER[a] < 16) Serial1.print("0");
   // Serial1.print(EXT_UART_BUFFER[a], HEX); 
   // }
  //Serial1.println(); 
    bool IsAddressValid = false;
    switch(EXT_UART_BUFFER[0] & ADDRESS_MASK) { //проверяем правильность первого байта, на соответствие из таблицы 
    case ADDRESS_CHANGER  :  IsAddressValid = true;  break;
    case ADDRESS_GATEWAY  :  IsAddressValid = true;  break;
    case ADDRESS_DISPLAY  :  IsAddressValid = true;  break;
    case ADDRESS_EMS      :  IsAddressValid = true;  break;
    case ADDRESS_VALIDATOR:  IsAddressValid = true;  break;
    case ADDRESS_AVD      :  IsAddressValid = true;  break;
    case ADDRESS_CD1      :  IsAddressValid = true;  break;
    case ADDRESS_CD2      :  IsAddressValid = true;  break;
    case ADDRESS_USD1     :  IsAddressValid = true;  break;
    case ADDRESS_USD2     :  IsAddressValid = true;  break;
    case ADDRESS_USD3     :  IsAddressValid = true;  break;
    case ADDRESS_COIN1    :  IsAddressValid = true;  break;
    case ADDRESS_COIN2    :  IsAddressValid = true;  break;
    
    default:
      break;
    }
    if (IsAddressValid){ //если команда правильная, то пробуем отправить ее на MDB
      struct MDB_Byte AddressByte;
      int addrtx = 0;
      AddressByte.data = EXT_UART_BUFFER[0];
      AddressByte.mode = 0x01;
      memcpy(&addrtx, &AddressByte, 2);
      MDB_write(addrtx);                                          //пишем адрес на MDB
      for (int i = 1; i < EXT_UART_BUFFER_COUNT; i++){            //пишем остальные байты команды из EXT, включая CRC 
        MDB_write(EXT_UART_BUFFER[i]);
      }
      processresponse(EXT_UART_BUFFER[0] & ADDRESS_MASK);
    }
  }
}

void MDB_checksumGenerate() {
  byte sum = 0;
  for (int i=0; i < (EXT_UART_BUFFER_COUNT); i++)
    sum += EXT_UART_BUFFER[i];
  EXT_UART_BUFFER[EXT_UART_BUFFER_COUNT++] = (sum & 0xFF);//only first 8 bits are checksum
}


void MDB_write(int data) {
  struct MDB_Byte b;
  memcpy(&b, &data, 2);
  write_9bit(b);
}

void write_9bit(struct MDB_Byte mdbb) {
  while ( !( UCSR0A & (1<<UDRE0)));
  if (mdbb.mode) {
     //turn on bit 9
    UCSR0B |= (1<<TXB80);
  } else {
     //turn off bit 9
    UCSR0B &= ~(1<<TXB80);
  }
  UDR0 = mdbb.data;
}

int MDB_Receive() {
  unsigned char resh, resl;
  char tmpstr[64];
  int rtr = 0;
  // Wait for data to be received
  while ((!(UCSR0A & (1<<RXC0))) and rtr < 50) {
    delay(1);
    rtr++;
  }
  if (rtr == 50){
    mdboverflow = 1;
    rcvcomplete = 1;
  }
  // Get 9th bit, then data from buffer
  resh = UCSR0B;
  resl = UDR0;
  // Filter the 9th bit, then return only data w\o mode bit
  resh = (resh >> 1) & 0x01;
  return ((resh << 8) | resl);
}

void MDB_getByte(struct MDB_Byte* mdbb) {
  int b;
  b = 0;
  b = MDB_Receive();
  memcpy (mdbb, &b, 2);  
}

byte EXT_ChecksumValidate() {
  byte sum = 0;
  for (int i=0; i < (EXT_UART_BUFFER_COUNT-1); i++)
    sum += EXT_UART_BUFFER[i];
  if (EXT_UART_BUFFER[EXT_UART_BUFFER_COUNT-1] == (sum & 0xFF))
    return 1;
  else
    return 0;
}

byte MDB_ChecksumValidate() {
  int sum = 0;
  for (int i=0; i < (MDB_BUFFER_COUNT-1); i++)
    sum += MDB_BUFFER[i].data;
  if (MDB_BUFFER[MDB_BUFFER_COUNT-1].data == (sum & 0xFF))
    return 1;
  else
    return 0;
}

void MDB_read() {
  MDB_getByte(&MDB_BUFFER[MDB_BUFFER_COUNT]);
  MDB_BUFFER_COUNT++;
  if (MDB_BUFFER_COUNT == 35){
    rcvcomplete = 1;
    mdboverflow = 1;
  }
  if (MDB_BUFFER[MDB_BUFFER_COUNT - 1].mode && MDB_ChecksumValidate()){
    rcvcomplete = 1;
  }
}

void MDBFlush(){
  MDB_BUFFER_COUNT = 0;
  Serial.flush();
} 

void processresponse(int addr){
  mdboverflow = 0;
  rcvcomplete = 0;
  while (!rcvcomplete){
    MDB_read();
  }
  if ((rcvcomplete) && (!mdboverflow))
  {
    if (MDB_BUFFER_COUNT > 1){
      MDB_write(0x00);// send ACK to MDB if peripheral answer is not just *ACK*, otherwise peripheral will try to send unconfirmed data with next polls
    } else{
      if (MDB_BUFFER_COUNT == 1){
        //just *ACK* received from peripheral device, no confirmation needed
      }
    }
    //finally, send data from peripheral to VMC via serial port as string representation of hex bytes
    char buff[5];
    //sprintf(buff, "%02x", addr);
    Serial1.write(addr);
    for (int a = 0; a < MDB_BUFFER_COUNT - 1; a++){
    //sprintf(buff, "%02x", MDB_BUFFER[a].data);
    Serial1.write(MDB_BUFFER[a].data);  
    }
    //last byte representation will be sent without following space but with EOL to easy handle
    //sprintf(buff, "%02x", MDB_BUFFER[MDB_BUFFER_COUNT - 1].data);
    Serial1.write(MDB_BUFFER[MDB_BUFFER_COUNT - 1].data);
    Serial1.write('\n');
    wdt_reset(); //сброс ватчдога. Если связи нет, контроллер будет перезагружен
  }
}

void PollDevice(byte devaddrbyte){
  struct MDB_Byte addrbyte;
  rcvcomplete = 0;
  int addrtx = 0;
  addrbyte.data = devaddrbyte;
  addrbyte.mode = 0x01;
  memcpy(&addrtx, &addrbyte, 2);
  MDB_write(addrtx);
  MDB_write(addrbyte.data);
  processresponse(addrbyte.data & ADDRESS_MASK);
}

void setup() {
  // put your setup code here, to run once:
wdt_reset(); // reset watchdog counter
wdt_disable();
//  delay(10000);
wdt_enable(WDTO_4S); // watchdog 8s timeout
  pinMode(0, OUTPUT);

  MDB_Setup();
  EXT_UART_Setup();
  MDB_BUFFER_COUNT = 0;

}

void loop() {
  // put your main code here, to run repeatedly:
  EXT_UART_read();
  MDBFlush();
 /* for (int q = 0; q < 10; q++){
    PollDevice(POLL_ADDRESS[q]); 
    MDBFlush();  
    delay(20); 
  }*/
}
