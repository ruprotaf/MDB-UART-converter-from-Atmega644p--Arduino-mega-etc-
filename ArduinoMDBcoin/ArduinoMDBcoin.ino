
unsigned char RX_buffer[64];// буфер для приема сообщений
unsigned char TX_buffer[8];// буфер для команд (передача)
#include <SoftwareSerial.h>
SoftwareSerial mySerial(10, 11); // RX, TX
//====================переменные монетоприемника===========
byte coin_scailing_factor;//коэффициент масштабирования монет
byte coin_decimal_places;      //позиация запятой в номинале монет
int coin_nominals[16];  //здесь храним номиналы монет
int coin_quaintity[16];  //здесь храним количество принятых монет
byte current_coin_channel; //канал текущей монеты
unsigned int money_value;  //номинал текущей монеты
byte coin_acceptor_error; //код ошибки монетпориемника
//=========================================================


void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  mySerial.setTimeout(200);

  //сброс монетоприемника
  TX_buffer[0] = 0x8; //команда RESET
  MDB_send(1);
  if (MDB_read() == 0) { //если ASK
    Serial.println(F("Команда перезагрузки получена"));
  }
  else Serial.println(F("Ошибка перезагрузки монетоприемника"));
  delay(600); //после команды сброса нужна задержка минимум 500 мс, иначе команда не пройдет.
  TX_buffer[0] = 0xb; //команда STATUS
  MDB_send(1);
  if (MDB_read() == 1) { //если данные есть, парсим ответ
    if (RX_buffer[1] == 0xb && RX_buffer[2] == 0xb) Serial.println(F("Монетоприемник перезагружен"));
  }

  //запрос настроек монетоприемника
  TX_buffer[0] = 0x9; //команда SETUP
  MDB_send(1);
  if (MDB_read() == 1) { //если данные есть, парсим ответ
    coin_scailing_factor = RX_buffer[4];
    Serial.print(F("coin_scailing_factor: "));
    Serial.println(coin_scailing_factor, DEC);
    coin_decimal_places = RX_buffer[5];
    Serial.print(F("coin_decimal_places: "));
    Serial.println(coin_decimal_places, DEC);
    Serial.println(F("Номиналы принимаемых монет (разделить на 100): "));
    for (int i = 0; i < 16; i++) {
      coin_nominals[i] = RX_buffer[i + 8] * coin_scailing_factor;
      Serial.print(F("Канал "));
      Serial.print(i);
      Serial.print(F(": "));
      Serial.println(coin_nominals[i], 1);
    }
  }
  else Serial.println(F("Ошибка запроса настроек монетоприемника"));

  //настройка разрешенных монет к передаче 0c00ff00ff
  TX_buffer[0] = 0xC; //команда COIN TYPE
  TX_buffer[1] = B11111111; //каждый бит отвечает за разрешение своего канала: 16-8
  TX_buffer[2] = B11111111;//каждый бит отвечает за разрешение своего канала: 8-0
  TX_buffer[3] = 0; //биты разрешения выдачи монет(диспенсер)
  TX_buffer[4] = 0; //биты разрешения выдачи монет(диспенсер)
  MDB_send(5);
  if (MDB_read() == 0) { //если ASK
    Serial.println(F("Команда COIN TYPE получена"));
  }
  else Serial.println(F("Ошибка запроса COIN TYPE"));
  Serial.println(F("Готов к приему монет:"));
}
void loop() {

  TX_buffer[0] = 0xb; //команда STATUS
  MDB_send(1);
  byte status_state = MDB_read();
  if (status_state == 1) { //если данные есть, парсим ответ
    //===========================читаем монеты===================
    if (bitRead(RX_buffer[1], 7) == 0 && bitRead(RX_buffer[1], 6) == 1) {
      Serial.print(F("Обнаружена монета: "));
      current_coin_channel = RX_buffer[1] & B00001111; //номинал монеты хранится в правых 4х битах. Их оставляем, остальные ставим в 0
      money_value = coin_nominals[current_coin_channel]; //получаем номинал монеты в соотвествии с каналами монетоприемника
      coin_quaintity[current_coin_channel]++; //увеличиваем количество монет
      Serial.println(money_value);
      if (bitRead(RX_buffer[1], 5) == 1 && bitRead(RX_buffer[1], 4) == 1) Serial.println(F("Отклонена(rejected)"));
      if (bitRead(RX_buffer[1], 5) == 0 && bitRead(RX_buffer[1], 4) == 0) Serial.println(F("Принята(accepted)"));
    }
    //==========================читаем ошибки=====================
    if (bitRead(RX_buffer[1], 7) == 0 && bitRead(RX_buffer[1], 6) == 0 && bitRead(RX_buffer[1], 5) == 1) { //монета не распознана (не поддерживается или пуговица)
      coin_acceptor_error = RX_buffer[1] & B00011111; //удаляем код ошибки, оставляем данные
      Serial.print(F("Монета не поодерживается. Счетчик: "));
      Serial.println(coin_acceptor_error, DEC);
    }
    else {
      if (RX_buffer[1] == B0000001) Serial.println(F("Состояние: Нажата кнопка возврата"));
      if (RX_buffer[1] == B0000011) Serial.println(F("Состояние: NO CREDIT"));
      if (RX_buffer[1] == B0000101) Serial.println(F("Состояние: Double Arrival"));
      if (RX_buffer[1] == B0001000) Serial.println(F("Состояние: ROM checksum error"));
      if (RX_buffer[1] == B0001001) Serial.println(F("Состояние: Coin Routing Error"));
      if (RX_buffer[1] == B0001011) Serial.println(F("Состояние: Монетоприемник перезагружен"));
      if (RX_buffer[1] == B0001100) Serial.println(F("Состояние: Coin Jam"));
      if (RX_buffer[1] == B0001100) Serial.println(F("Состояние: Possible Credited Coin Removal"));
    }
  }
  if (status_state == 2) Serial.println(F("Нет ответа от монетоприемника")); //если функция вернула 2, значит монетоприемник не ответил
  delay(200); //задержка между опросами
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
    state = 2;//возвращаем 2 если таймаут
  } 
  return state;
}
