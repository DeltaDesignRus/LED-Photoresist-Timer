#include <LiquidCrystal.h>
#include <OneButton.h>
#include <EEPROM.h>

OneButton PinPres1(6, false); // кнопка 1 (верхняя)
OneButton PinPres2(5, false); // кнопка 2 (нижняя)

int PinRelay = 13; //выход затвор транзистора
int PinCLK = 2; // энкодер
int PinDT = 3;
int PinSW = 4; // кнопка энкодера
long Preset0 = 0;  // начальное значение (сек.)
long Preset1 = 20;  // предустановка времени №1 (сек.) используемая по умолчанию
long Preset2 = 140;  // предустановка времени №2 (сек.) используемая по умолчанию
int sec_step = 5; // шаг энкодера (сек.)

unsigned long currentTime; // текущее время
unsigned long loopTime; // время окончания
unsigned long LimitTime = 0; // таймер
unsigned long Sum1 = 0; //переменная вычисляемого значения контрольной суммы для пресета 1
unsigned long Sum2 = 0; //переменная вычисляемого значения контрольной суммы для пресета 2
unsigned long CheckSum1 = 0; //переменная считываемого из EEPROM значения контрольной суммы для пресета 1
unsigned long CheckSum2 = 0; //переменная считываемого из EEPROM значения контрольной суммы для пресета 2
volatile boolean turn;
volatile boolean up;
long DefSaveReadNum = 18527258;
boolean running = false; // флаг запуска отсчета
int awake;

// чтение из EEPROM
unsigned long EEPROM_ulong_read(int addr) {
  byte raw[4];
  for (byte i = 0; i < 4; i++) raw[i] = EEPROM.read(addr + i);
  unsigned long &num = (unsigned long&)raw;
  return num;
}

// запись в EEPROM
void EEPROM_ulong_write(int addr, unsigned long num) {
  byte raw[4];
  (unsigned long&)raw = num;
  for (byte i = 0; i < 4; i++) EEPROM.write(addr + i, raw[i]);
}

LiquidCrystal lcd(12, 11, 10, 9, 8, 7); // экран

void setup() {

  Serial.begin(9600);
  Preset0 = Preset0 * 1000;
  Preset1 = Preset1 * 1000;
  Preset2 = Preset2 * 1000;
  sec_step = sec_step * 1000;
  LimitTime = Preset0;
  pinMode(PinCLK, INPUT); // входы
  pinMode(PinDT, INPUT);
  pinMode(PinSW, INPUT);
  pinMode(PinRelay, OUTPUT);
  attachInterrupt (0, isr, CHANGE); // прерывание энкодера
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("  UV LED TIMER  ");
  lcd.setCursor(0, 1);
  lcd.print("    ver. 1.1    ");
  delay(1000);
  lcd.clear();
  currentTime = millis();
  PinPres1.attachClick(PinPres1click);
  PinPres1.attachLongPressStart(PinPres1longPressStart);
  PinPres2.attachClick(PinPres2click);
  PinPres2.attachLongPressStart(PinPres2longPressStart);

    if (EEPROM_ulong_read(0) != DefSaveReadNum) { //проверка на первый запуск
    EEPROM_ulong_write(0, DefSaveReadNum);
  }
}

void loop() {

  // выбран режим отсчета
  /* ========================== */
  if (running == true) { //Если флаг запуска отсчета True, то:

    // отсчет времени
    currentTime = millis();
    loopTime = currentTime + LimitTime; // когда закончить
    while (currentTime < loopTime) { // сравниваем текущее время с временем окончания

      digitalWrite(PinRelay, HIGH); // включаем (подаем 5в на затвор транзистора подключая нагрузку через транзистор)

      // вывод на экран
      lcd.setCursor(0, 0);
      lcd.print("    EXPOSING    ");
      lcd.setCursor(4, 2);
      if (LimitTime / 60 / 60 / 1000 < 10) {
        lcd.print("0");
      }
      lcd.print(LimitTime / 60 / 60 / 1000);
      lcd.print(":");
      if (LimitTime / 60 / 1000 % 60 < 10) {
        lcd.print("0");
      }
      lcd.print((LimitTime / 60 / 1000) % 60);
      lcd.print(":");
      if (LimitTime / 1000 % 60 < 10) {
        lcd.print("0");
      }
      lcd.print(LimitTime / 1000 % 60);
      lcd.display();

      // кнопка энкодера
      if (digitalRead(PinSW) != awake) {
        awake = digitalRead(PinSW);
        if (awake && running == true) {
          running = false;
          digitalWrite(PinRelay, HIGH); // включаем
          LimitTime = Preset0;
          break;
        }
      }

      LimitTime = LimitTime - (millis() - currentTime); // уменьшаем таймер
      currentTime = millis(); // получаем новое время
    }

    // окончание работы таймера
    digitalWrite(PinRelay, LOW); // отключаем устройство

    lcd.setCursor(0, 0);
    lcd.print("             ");
    lcd.setCursor(0, 0);
    lcd.print("     FINISH     ");
    running = false;
    delay (3000);

  } else {

    digitalWrite(PinRelay, LOW); // отключаем устройство

    if (LimitTime > 86400000) LimitTime = Preset0; // отбрасываем некорректные значения

    // вывод на экран
    lcd.setCursor(0, 0);
    lcd.print("   Set timer:   ");
    lcd.setCursor(4, 2);
    if (LimitTime / 60 / 60 / 1000 < 10) {
      lcd.print("0");
    }
    lcd.print(LimitTime / 60 / 60 / 1000);
    lcd.print(":");
    if (LimitTime / 60 / 1000 % 60 < 10) {
      lcd.print("0");
    }
    lcd.print((LimitTime / 60 / 1000) % 60);
    lcd.print(":");
    if (LimitTime / 1000 % 60 < 10) {
      lcd.print("0");
    }
    lcd.print(LimitTime / 1000 % 60);
    lcd.display();

    // пресет №1
    PinPres1.tick();
    // пресет №2
    PinPres2.tick();

    // энкодер
    static long enc_pos = 0;

    if (digitalRead(PinSW) && enc_pos != 0) {
      enc_pos = 0;
    }
    if (turn) {
      if (up) {
        enc_pos--;
      } else {
        enc_pos++;
      }
      turn = false;
      LimitTime += enc_pos * sec_step;
    }

    // кнопка энкодера
    if (digitalRead(PinSW) != awake) {
      awake = digitalRead(PinSW);
      if (awake && LimitTime != 0) {
        running = true;
      }
    }
  }
}

// прерывание энкодера
void isr ()  {
  up = (digitalRead(PinCLK) == digitalRead(PinDT));
  turn = true;
}

void PinPres1click() {
  lcd.setCursor(0, 0);
  lcd.print("    Preset 1    ");

  CheckSum1 = EEPROM_ulong_read(12); //чтение контрольной суммы пресета 1 из памяти
  Sum1 = EEPROM_ulong_read(0) + EEPROM_ulong_read(4); //расчет контрольной суммы пресета 1

  if (CheckSum1 != Sum1 || CheckSum1 == 0) { //сравнение прочитанной и рассчитаной контрольных сумм  пресета 1
    LimitTime = Preset1; //Устанавливаем дефолтное значение пресета 1
  } else {
    LimitTime = EEPROM_ulong_read(4);  //читаем время пресета 1
  }
  delay(1000);
}

void PinPres1longPressStart() {
  lcd.setCursor(0, 0);
  lcd.print(" Preset 1: save ");
  if (EEPROM_ulong_read(0) != DefSaveReadNum) { //проверка на первый запуск
    EEPROM_ulong_write(0, DefSaveReadNum);
  }
  EEPROM_ulong_write(4, LimitTime); //запись в EEPROM времени пресета 1
  EEPROM_ulong_write(12, EEPROM_ulong_read(0) + EEPROM_ulong_read(4)); //запись контрольной суммы CheckSum1
  delay(1000);
}

void PinPres2click() {
  lcd.setCursor(0, 0);
  lcd.print("    Preset 2    ");

  CheckSum2 = EEPROM_ulong_read(16); //чтение контрольной суммы пресета 2 из памяти
  Sum2 = EEPROM_ulong_read(0) + EEPROM_ulong_read(8); //расчет контрольной суммы пресета 2

  if (CheckSum2 != Sum2 || CheckSum2 == 0) { //сравнение прочитанной и рассчитаной контрольных сумм  пресета 2
    LimitTime = Preset2; //Устанавливаем дефолтное значение пресета 2
  } else {
    LimitTime = EEPROM_ulong_read(8); //читаем время пресета 2
  }
  delay(1000);
}

void PinPres2longPressStart() {
  lcd.setCursor(0, 0);
  lcd.print(" Preset 2: save ");
  if (EEPROM_ulong_read(0) != DefSaveReadNum) { //проверка на первый запуск
    EEPROM_ulong_write(0, DefSaveReadNum);
  }
  EEPROM_ulong_write(8, LimitTime); //запись в EEPROM времени пресета 2
  EEPROM_ulong_write(16, EEPROM_ulong_read(0) + EEPROM_ulong_read(8)); //запись контрольной суммы CheckSum2
  delay(1000);
}
