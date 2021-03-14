
unsigned char RX_buffer[64];// буфер для приема сообщений
unsigned char TX_buffer[8];// буфер для команд (передача)
#include <SoftwareSerial.h>
SoftwareSerial mySerial(10, 11); // RX, TX
//====================переменные купюроприемника===========
word bill_scailing_factor;      //коэффициент масштабирования купюр
byte bill_decimal_places;      //позиация запятой в номинале купюр
long bill_nominals[16];  //здесь храним номиналы купуюр
int bill_quaintity[16];  //здесь храним количество принятых купюр
byte current_bill_channel; //канал текущей купюры
unsigned int money_value;  //номинал текущей монеты или купюры
byte bill_acceptor_error; //код ошибки купюроприемника
word stacker_capacity;     //емкость купюроприемника
//=========================================================


void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  mySerial.setTimeout(100);

  //сброс купюроприемника
  TX_buffer[0] = 0x30; //команда RESET
  MDB_send(1);
  if (MDB_read() == 0) { //если ASK
    Serial.println(F("Команда перезагрузки купюроприемника получена"));
  }
  else Serial.println(F("Ошибка перезагрузки купюроприемника"));
  delay(600); //после команды сброса нужна задержка минимум 500 мс, иначе команда не пройдет.
  TX_buffer[0] = 0x33; //команда STATUS
  MDB_send(1);
  if (MDB_read() == 1) { //если данные есть, парсим ответ
    if (RX_buffer[1] == B0000110 && RX_buffer[2] == B0000110) Serial.println(F("Купюроприемник перезагружен"));
  }

  //запрос настроек купюроприемника
  TX_buffer[0] = 0x31; //команда SETUP
  MDB_send(1);
  if (MDB_read() == 1) { //если данные есть, парсим ответ
    bill_scailing_factor = RX_buffer[4] << 8 | RX_buffer[5];
    Serial.print(F("bill_scailing_factor: "));
    Serial.println(bill_scailing_factor, DEC);
    bill_decimal_places = RX_buffer[6];
    Serial.print(F("bill_decimal_places: "));
    Serial.println(bill_decimal_places, DEC);
    stacker_capacity = RX_buffer[7] << 8 | RX_buffer[8];
    Serial.print(F("Емкость купюроприемника: "));
    Serial.println(stacker_capacity, DEC);
    Serial.println(F("Номиналы принимаемых купюр: "));
    for (int i = 0; i < 16; i++) {
      bill_nominals[i] = RX_buffer[i + 12]*10;  //умножаем на 10, так как к-т 1000 и точка на 2 разряда
      Serial.print(F("Канал "));
      Serial.print(i);
      Serial.print(F(": "));
      Serial.println(bill_nominals[i], 1);
    }
  }
  else Serial.println(F("Ошибка запроса настроек купюроприемника"));

  //настройка разрешенных монет к передаче 0c00ff00ff
  TX_buffer[0] = 0x34; //команда bill TYPE
  TX_buffer[1] = B11111111; //каждый бит отвечает за разрешение своего канала: 16-8
  TX_buffer[2] = B11111111;//каждый бит отвечает за разрешение своего канала: 8-0
  TX_buffer[3] = 0; //биты разрешения купюр условного депонирования
  TX_buffer[4] = 0; //биты разрешения купюр условного депонирования
  MDB_send(5);
  if (MDB_read() == 0) { //если ASK
    Serial.println(F("Команда bill TYPE получена"));
  }
  else Serial.println(F("Ошибка запроса bill TYPE"));
  Serial.println(F("Готов к приему купюр:"));
}
void loop() {

  TX_buffer[0] = 0x33; //команда STATUS
  MDB_send(1);
  byte status_state = MDB_read();
  if (status_state == 1) { //если данные есть, парсим ответ
    //===========================читаем купюры===================
    if (bitRead(RX_buffer[1], 7)) { //если последний бит=1, значит это сообщение о купюре
      Serial.print(F("Обнаружена купюра: "));
      current_bill_channel = RX_buffer[1] & B00001111; //номинал купюры хранится в правых 4х битах. Их оставляем, остальные ставим в 0
      money_value = bill_nominals[current_bill_channel]; //получаем номинал купюр в соотвествии с каналами монетоприемника
      bill_quaintity[current_bill_channel]++; //увеличиваем количество купюр
      Serial.println(money_value);
      if (bitRead(RX_buffer[1], 6) == 1 && bitRead(RX_buffer[1], 5) == 0 && bitRead(RX_buffer[1], 4) == 0) Serial.println(F("Отклонена(rejected)"));
      if (bitRead(RX_buffer[1], 6) == 0 && bitRead(RX_buffer[1], 5) == 0 && bitRead(RX_buffer[1], 4) == 0) Serial.println(F("Принята(accepted)"));
    }
    //==========================читаем ошибки=====================
    if (bitRead(RX_buffer[1], 7) == 0 && bitRead(RX_buffer[1], 6) == 0) { //обнаружена ошибка
      if (RX_buffer[1] == B0000001) Serial.println(F("Состояние: Defective Motor"));
      if (RX_buffer[1] == B0000010) Serial.println(F("Состояние: Sensor Problem"));
      if (RX_buffer[1] == B0000011) Serial.println(F("Состояние: Validator Busy"));
      if (RX_buffer[1] == B0000100) Serial.println(F("Состояние: ROM checksum error"));
      if (RX_buffer[1] == B0000101) Serial.println(F("Состояние: Validator Jammed"));
      if (RX_buffer[1] == B0000110) Serial.println(F("Состояние: Купюроприемник перезагружен"));
      if (RX_buffer[1] == B0001000) Serial.println(F("Состояние: Cash Box out of position"));
      if (RX_buffer[1] == B0001001) Serial.println(F("Состояние: Validator Disabled"));
      if (RX_buffer[1] == B0001011) Serial.println(F("Состояние: Bill Rejected"));
      if (RX_buffer[1] == B0001100) Serial.println(F("Состояние: Possible Credited Bill Removal"));
    }
  }
  if (status_state == 2) Serial.println(F("Нет ответа от купюроприемника")); //если функция вернула 2, значит купюроприемник не ответил

  //запрос наполнения стекера
  TX_buffer[0] = 0x36; //команда bill TYPE
  MDB_send(1);
  if (MDB_read() == 1) { //если ASK
    if (bitRead(RX_buffer[1], 7) == 0) {}
    else Serial.println(F("Стекер переполнен!"));
  }
  delay(200); //задержка между командами
}



void MDB_send(byte len) {

  TX_buffer[len + 1] = MDB_checksumGenerate(len);
  len = len + 1;
  for (int count = 0; count < len; count++) {
    mySerial.write(TX_buffer[count]);
  }
}

byte MDB_checksumGenerate(byte len) {
  byte sum = 0;
  for (int i = 0; i < (len); i++)
    sum += TX_buffer[i];
  TX_buffer[len++] = (sum & 0xFF);//only first 8 bits are checksum
  return sum;
}

int MDB_read() {
  int state = 0;
  int msg_len = mySerial.readBytesUntil('\r' + '\n', RX_buffer, 128);
  if (msg_len > 0) {
    if (RX_buffer[1] == 0) state = 0; // возвращаем 0 если ASK
    else {
      state = 1; // возвращаем 1 если есть данные к обработке
    }
  }
  else {
    state = 2; //возвращаем 2 если таймаут
  }
  return state;
}
