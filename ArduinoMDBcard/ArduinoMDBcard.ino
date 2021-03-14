
unsigned char RX_buffer_MDB[64];// буфер для приема сообщений MDB
unsigned char TX_buffer_MDB[8];// буфер для команд MDB (передача)
#include <SoftwareSerial.h>
SoftwareSerial Serial5(10, 11); // RX, TX
//====================переменные кардридера===========
byte card_scailing_factor;      //коэффициент масштабирования кардридера
byte card_decimal_places;      //позиация запятой в номинале кардридера
byte bill_acceptor_error;       //код ошибки кардридера
byte Miscellaneous_Options;     //дополнительные опции
long Avaliable_on_card_money; //баланс на карте
word Price = 3; //цена в отмасштабированных еденицах (2 байта)
bool inhibit=1; //запрет или разрешение работы кардридера
//=========================================================


void setup() {
  Serial.begin(9600); //порт для диагностики
  Serial5.begin(9600); //порт MDB
  Serial5.setTimeout(200); //таймаут приема сообщения MDB
  card_reader_prepare();
   //CASH_SALE();      //сообщение на сервер о продаже за наличные
}
void loop() {

  pool_cashless_MDB(); //опрашиваем кардридер

  delay(500); //задержка между командами
}


//===============================начало команд управления кардридером=====================
void card_reader_prepare() {
  card_reader_reset(); //сбрасываем кардридер
  card_reader_setup_request(); //запрашиваем настройки кардридера
  card_reader_cost_setup(); //установка максимальной\минимальной цены
  card_inhibit(inhibit); //включаем кардридер
}

void pool_cashless_MDB() {
  TX_buffer_MDB[0] = 0x12; //команда POOL
  MDB_send(1);
  byte status_state = MDB_read();
  if (status_state == 1) { //если данные есть, парсим ответ
    //===========================Начало парсинга ответа POOL===================
    if (RX_buffer_MDB[1] == 0x00 && RX_buffer_MDB[2] == 0x00) {
      Serial.println(F("кардридер был перезагружен"));
    }
    if (RX_buffer_MDB[1] == 0x03) { //принято сообщение BEGIN SESSION
      Serial.print(F("CR: BEGIN SESSION. Баланс на карте: "));
      Avaliable_on_card_money = RX_buffer_MDB[2] << 8 | RX_buffer_MDB[3];
      Serial.println(Avaliable_on_card_money);
      VEND_REQUEST();              //отвечаем типом продукта и ценой которую нужно снять
    } 
    else if (RX_buffer_MDB[1] == 0x04) { //принято сообщение SESSION CANCEL REQUEST
      Serial.println(F("CR: SESSION CANCEL REQUEST"));
    }
    else if (RX_buffer_MDB[1] == 0x05) { //принято сообщение VEND APPROVED
      Serial.print(F("CR: VEND APPROVED. Баланс на карте: "));
      Avaliable_on_card_money = RX_buffer_MDB[2] << 8 | RX_buffer_MDB[3];
      Serial.println(Avaliable_on_card_money);
      VEND_SUCCESS();              //считаем что товар отгружен. Отправляем команду успешной отгрузки
    } 
    if (RX_buffer_MDB[1] == 0x06) { //принято сообщение VEND DENIED
      Serial.println(F("CR: VEND DENIED"));
    }
    if (RX_buffer_MDB[1] == 0x07) { //принято сообщение END SESSION
      Serial.println(F("CR:  END SESSION"));
    }
    else if (RX_buffer_MDB[1] == 0x0B) { //принято сообщение COMMAND OUT OF SEQUENCE (неверная команда, нужно послать RESET в ответ)
      Serial.println(F("CR: COMMAND OUT OF SEQUENCE")); 
    } }}
    //===========================конец парсинга ответа POOL===================
 

void  card_inhibit(byte inhibit) {
  //Включаем кардридер на прием карт
  TX_buffer_MDB[0] = 0x14; //команда INHIBIT
  TX_buffer_MDB[1] = inhibit; //включение или выключение
  MDB_send(2);
  if (MDB_read() == 0) { //если ASK
    Serial.println(F("Команда card ENABLE/DISABLE Cashless получена"));
  }
  else {
  Serial.println(F("Ошибка запроса ENABLE/DISABLE Cashless"));}
}

void  card_reader_reset() {  //сброс кардридера
  TX_buffer_MDB[0] = 0x10; //команда RESET
  MDB_send(1);
  if (MDB_read() == 0) { //если ASK
    Serial.println(F("Команда перезагрузки кардридера получена"));
  }
  else {
  Serial.println(F("Ошибка перезагрузки кардридера"));}
}

void card_reader_setup_request() {
  //запрос настроек кардридера
  TX_buffer_MDB[0] = 0x11; //команда SETUP
  TX_buffer_MDB[1] = 0x00; //признак команды Configuration data
  TX_buffer_MDB[2] = 0x01; //команда установки уровня устройства. Ставим 1 уровень
  TX_buffer_MDB[3] = 0x02; //команда
  TX_buffer_MDB[4] = 0x00; //команда
  TX_buffer_MDB[5] = 0x00; //команда
  MDB_send(6);
  if (MDB_read() == 1) { //если данные есть, парсим ответ
    card_scailing_factor = RX_buffer_MDB[5];
    Serial.print(F("card_scailing_factor: "));
    Serial.println(card_scailing_factor, DEC);
    card_decimal_places = RX_buffer_MDB[6];
    Serial.print(F("card_decimal_places: "));
    Serial.println(card_decimal_places, DEC);
    Miscellaneous_Options = RX_buffer_MDB[7];
    Serial.print(F("Дополнительные опции: "));
    Serial.println(Miscellaneous_Options, BIN);

  }
  else {
  Serial.println(F("Ошибка запроса настроек кардридера"));}
}


void card_reader_cost_setup() {
  //установка максимальной-минимальной цены в моей реализации устройства ни на что не влияет
  TX_buffer_MDB[0] = 0x11;     //команда SETUP MIN MAX Prices
  TX_buffer_MDB[1] = 0x01;     //тип настроек: цены
  TX_buffer_MDB[2] = 0xFF;    //первый бит максимальной цены
  TX_buffer_MDB[3] = 0xFF;    //второй бит максимальной цены
  TX_buffer_MDB[4] = 0x00;    //первый бит минимальной цены
  TX_buffer_MDB[5] = 0x00;    //второй бит минимальной цены
  MDB_send(6);
  if (MDB_read() == 0) { //если ASK
    Serial.println(F("Команда card SETUP MIN MAX Prices Cashless получена"));
  }
  else Serial.println(F("Ошибка запроса SETUP MIN MAX Prices Cashless"));
}

void VEND_REQUEST() {
  //запрос VEND REQUEST (посылается после ответа BEGIN SESSION)
  TX_buffer_MDB[0] = 0x13; //команда VEND
  TX_buffer_MDB[1] = 0x00; //признак команды VEND REQUEST
  TX_buffer_MDB[2] = Price >> 8; //байт 1 цены
  TX_buffer_MDB[3] = Price & 0xff; //байт 0 цены
  TX_buffer_MDB[4] = 0xFF; //номер продукта
  TX_buffer_MDB[5] = 0xFF; //номер продукта
  MDB_send(6);
  if (MDB_read() == 0) { //если ASK
    Serial.println(F("CR:  VEND_REQUEST принят"));
  }
  else {
    Serial.println(F("VMC: Ошибка запроса VEND REQUEST"));
  }
}

void VEND_CANCEL()
{ //запрос VEND CANCEL (посылвается после ответа VEND APPROVED\DENIED)
  TX_buffer_MDB[0] = 0x13; //команда VEND
  TX_buffer_MDB[1] = 0x01; //признак команды VEND CANCEL
  TX_buffer_MDB[2] = 0x00; //байт цены 1
  TX_buffer_MDB[3] = 0x00; //байт цены 2
  TX_buffer_MDB[4] = 0xFF; //номер продукта
  TX_buffer_MDB[5] = 0xFF; //номер продукта
  MDB_send(6);
  if (MDB_read() == 0) { //если ASK
    Serial.println(F("CR:  VEND CANCEL принят"));
  }
  else {
    Serial.println(F("VMC: Ошибка запроса VEND CANCEL"));
  }
}

void VEND_SUCCESS()
{ //запрос VEND SUCCESS (посылается после ответа VEND APPROVED если товар был успешно отгружен)
  TX_buffer_MDB[0] = 0x13; //команда VEND
  TX_buffer_MDB[1] = 0x02; //признак команды VEND SUCCESS
  TX_buffer_MDB[2] = 0xFF; //номер продукта
  TX_buffer_MDB[3] = 0xFF; //номер продукта
  MDB_send(4);
  if (MDB_read() == 0) { //если ASK
    Serial.println(F("CR:  VEND SUCCESS принят"));
  }
  else {
    Serial.println(F("VMC: Ошибка запроса VEND SUCCESS"));
  }
}

void  VEND_FAILURE()
//запрос VEND FAILURE (посылвается если товар не был отгружен. Деньги возвращаются)
{ TX_buffer_MDB[0] = 0x13; //команда VEND
  TX_buffer_MDB[1] = 0x03; //признак команды VEND FAILURE
  MDB_send(2);
  if (MDB_read() == 0) { //если ASK
    Serial.println(F("CR:  VEND FAILURE принят"));
  }
  else {
    Serial.println(F("VMC: Ошибка запроса VEND FAILURE"));
  }
}

void  SESSION_COMPLETE()
{ //запрос SESSION_COMPLETE (посылвается при окончании сессии)
  TX_buffer_MDB[0] = 0x13; //команда VEND
  TX_buffer_MDB[1] = 0x04; //признак команды SESSION_COMPLETE
  MDB_send(2);
  if (MDB_read() == 0) { //если ASK
    Serial.println(F("CR:  SESSION_COMPLETE принят"));
    }
  else {
    Serial.println(F("VMC: Ошибка запроса SESSION_COMPLETE"));
  }
}

void  CASH_SALE()
{ //запрос CASH_SALE (информация о продаже за наличные для сохранении статистики на сайте кардридера)
  TX_buffer_MDB[0] = 0x13; //команда VEND
  TX_buffer_MDB[1] = 0x05; //признак команды CASH_SALE
  TX_buffer_MDB[2] = Price >> 8; //байт 0 цены
  TX_buffer_MDB[3] = Price & 0xff; //байт 1 цены
  TX_buffer_MDB[4] = 0xFF; //номер продукта
  TX_buffer_MDB[5] = 0xFF; //номер продукта
  MDB_send(6);
  if (MDB_read() == 0) { //если ASK
    Serial.println(F("CR:  CASH_SALE принят"));
  }
  else {
    Serial.println(F("VMC: Ошибка запроса CASH_SALE"));
  }
}

//===============================конец команд управления кардридером=====================

//===============================начало функций обработки MDB протокола=====================
void MDB_send(byte len) {

  TX_buffer_MDB[len + 1] = MDB_checksumGenerate(len);
  len = len + 1;
  for (int count = 0; count < len; count++) {
  Serial5.write(TX_buffer_MDB[count]);
  }
}

byte MDB_checksumGenerate(byte len) {
  byte sum = 0;
  for (int i = 0; i < (len); i++)
    sum += TX_buffer_MDB[i];
  TX_buffer_MDB[len++] = (sum & 0xFF);//only first 8 bits are checksum
  return sum;
}

int MDB_read() {
  int state = 0;
  int msg_len = Serial5.readBytesUntil('\n', RX_buffer_MDB, 128);
  if (msg_len > 0) {
  //Serial.print("Msg len: ");
  //Serial.println(msg_len);
    for (int count = 1; count < msg_len; count++) {
    //Serial.print(RX_buffer_MDB[count]);
    }
    //Serial.println();
    if (RX_buffer_MDB[1] == 0 && msg_len == 2) state = 0; // возвращаем 0 если ASK
    else {
      state = 1; // возвращаем 1 если есть данные к обработке
    }
  }
  else {
    state = 2; //возвращаем 2 если таймаут
  }
  return state;
}
//===============================конец функций обработки MDB протокола=====================
