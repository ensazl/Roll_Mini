            #include <Wire.h>
            #include <LiquidCrystal_I2C.h>
            #include <avr/wdt.h>
            #include <dimmable_light.h>

            // --- LCD Tanımı ---
            LiquidCrystal_I2C lcd(0x27, 16, 2);

            // --- Röle pinleri ---
            #define RELAY1 12
            #define RELAY2 11
            #define RELAY3 10

            // --- Dimmable Light (ısıtıcı ve motor) ---
            const int syncPin = 2;    // Zero-cross pini
            const int heaterPin = 3;  // Isıtıcı çıkışı
            const int motorPin = 9;   // Motor çıkışı (dimmable)
            DimmableLight* heater;
            DimmableLight* motor;
            
            // --- Buton pinleri ---
            #define BTN_START 7
            #define BTN_STOP 6
            #define BTN_ILERI 5
            #define BTN_GERI 4
            #define BTN_DEVAR 1
            #define BTN_DEVEKS 0
            #define BTN_HEATER 8  // Isıtıcı aç/kapa butonu

            // --- Sistem durumları ---
            enum SysState { IDLE, RUNNING, PAUSED, FINISHED };
            SysState state = IDLE;

            // --- Zaman değişkenleri ---
            unsigned long startMillis = 0;
            unsigned long totalSeconds = 0;
            unsigned long displaySeconds = 0;

            // --- PWM (motor) ---
            int pwmValue = 0;

            // --- Isıtıcı durumu ---
            bool heaterOn = false;

            // --- Buton geçmişi ---
            bool btnPrev[7] = {false, false, false, false, false, false, false};

            // --- Watchdog reset algılama ---
            bool wasWatchdogReset = false;

            // ======================================================
            void setup() {
            // --- Watchdog reset kontrolü ---
            if (MCUSR & _BV(WDRF)) wasWatchdogReset = true;
            MCUSR = 0;
            wdt_disable();
            wdt_enable(WDTO_4S); // 4 saniyelik watchdog

            // --- LCD Başlat ---
            lcd.init();
            lcd.backlight();
            lcd.clear();

            if (wasWatchdogReset) {
                lcd.setCursor(0,0); lcd.print("Sistem Resetlendi!");
                lcd.setCursor(0,1); lcd.print("Yeniden Basladi");
                delay(1500);
            } else {
                lcd.setCursor(0,0); lcd.print("  SISTEM HAZIR  ");
            }

            // --- Röleler ---
            pinMode(RELAY1, OUTPUT);
            pinMode(RELAY2, OUTPUT);
            pinMode(RELAY3, OUTPUT);
            digitalWrite(RELAY1, LOW);
            digitalWrite(RELAY2, LOW);
            digitalWrite(RELAY3, LOW);

            // --- Butonlar ---
            pinMode(BTN_START, INPUT_PULLUP);
            pinMode(BTN_STOP, INPUT_PULLUP);
            pinMode(BTN_ILERI, INPUT_PULLUP);
            pinMode(BTN_GERI, INPUT_PULLUP);
            pinMode(BTN_DEVAR, INPUT_PULLUP);
            pinMode(BTN_DEVEKS, INPUT_PULLUP);
            pinMode(BTN_HEATER, INPUT_PULLUP);

            // --- Dimmable Light başlat (ısıtıcı için) ---
            DimmableLight::setSyncPin(syncPin);
            DimmableLight::begin();
          
            heater = new DimmableLight(heaterPin);
            motor  = new DimmableLight(motorPin);
          
            heater->setBrightness(0);
            motor->setBrightness(0);
            }

            // ======================================================
            void loop() {
            wdt_reset();
            readButtons();

            if (state == RUNNING) {
                displaySeconds = totalSeconds + (millis() - startMillis) / 1000;
                showRunning(displaySeconds);
            }
            }

// ======================================================
// --- Buton okuma ---
            void readButtons() {
            checkButton(BTN_START, 0);
            checkButton(BTN_STOP, 1);
            checkButton(BTN_ILERI, 2);
            checkButton(BTN_GERI, 3);
            checkButton(BTN_DEVAR, 4);
            checkButton(BTN_DEVEKS, 5);
            checkButton(BTN_HEATER, 6);
            }

            void checkButton(int pin, int idx) {
            bool pressed = !digitalRead(pin);
            if (pressed && !btnPrev[idx]) {
                handleButton(idx);
                delay(180);
            }
            btnPrev[idx] = pressed;
            }

            void handleButton(int idx) {
            switch(idx) {
                case 0: onStart(); break;
                case 1: onStop(); break;
                case 2: onIleri(); break;
                case 3: onGeri(); break;
                case 4: onDevirArti(); break;
                case 5: onDevirEksi(); break;
                case 6: onHeaterToggle(); break; // Isıtıcı kontrol
            }
            }

// ======================================================
// --- START ---
            void onStart() {
            if (state == FINISHED) {
                resetSystem();
                lcd.clear();
                lcd.setCursor(0,0); lcd.print("   BASLATILDI ");
                delay(1000);
                state = RUNNING;
                startMillis = millis();
                digitalWrite(RELAY3, HIGH);
                digitalWrite(RELAY1, HIGH);
                return;
            }

            if (state == PAUSED || state == IDLE) {
                digitalWrite(RELAY3, HIGH);
                digitalWrite(RELAY1, HIGH);
                startMillis = millis();
                state = RUNNING;
                lcd.clear();
                lcd.setCursor(0,0); lcd.print("  CALISIYOR!!!");
                delay(700);
            } else if (state == RUNNING) {
                lcd.clear();
                lcd.setCursor(0,0); lcd.print("  CALISIYOR!!!");
                delay(500);
            }
            }
// ======================================================
// --- STOP ---
            void onStop() {
            if (state == RUNNING) {
                totalSeconds += (millis() - startMillis) / 1000;
                state = PAUSED;
                digitalWrite(RELAY1, LOW);
                digitalWrite(RELAY2, LOW);
                pwmValue = 0;  
                motor->setBrightness(0);
                heater->setBrightness(0);
                lcd.clear();
                lcd.setCursor(0,0); lcd.print("  DURDURULDU!!  ");
                lcd.setCursor(0,1); lcd.print(" SURE: ");
                showTime(totalSeconds);
            } 
            else if (state == PAUSED) {
                state = FINISHED;
                digitalWrite(RELAY1, LOW);
                digitalWrite(RELAY2, LOW);
                digitalWrite(RELAY3, LOW);
                pwmValue = 0;
                motor->setBrightness(0);
                heater->setBrightness(0);
                lcd.clear();
                lcd.setCursor(0,0); lcd.print("    BITTI!!!");
                lcd.setCursor(0,1); lcd.print("TOPLAM: ");
                showTime(totalSeconds);

                unsigned long finishTime = millis();
                while (millis() - finishTime < 10000) {
                wdt_reset();
                delay(50);
                }

                resetSystem();
                lcd.clear();
                lcd.setCursor(0,0); lcd.print("  SISTEM HAZIR  ");
                state = IDLE;
            }
            }
 // ======================================================
 // --- ILERI / GERI ---
            void onIleri() {
            if (state == RUNNING) {
                digitalWrite(RELAY1, HIGH);
                digitalWrite(RELAY2, LOW);
                lcd.clear();
                lcd.setCursor(0,0); lcd.print("   ILERI YON   ");
                delay(500);
            }
            }

            void onGeri() {
            if (state == RUNNING) {
                digitalWrite(RELAY1, LOW);
                digitalWrite(RELAY2, HIGH);
                lcd.clear();
                lcd.setCursor(0,0); lcd.print("   GERI YON   ");
                delay(500);
            }
            }
// ======================================================
// --- DEVRİ ARTTIR / AZALT ---
            void onDevirArti() {
              if (state == RUNNING) {
                if (pwmValue < 200) pwmValue += 50;
                else if (pwmValue == 200) pwmValue = 255;
            
                int dimValue = map(pwmValue, 0, 255, 0, 255);
                motor->setBrightness(dimValue);
              }
            }
            
            void onDevirEksi() {
              if (state == RUNNING) {
                if (pwmValue == 255) pwmValue = 200;
                else if (pwmValue >= 50) pwmValue -= 50;
            
                int dimValue = map(pwmValue, 0, 255, 0, 255);
                motor->setBrightness(dimValue);
              }
            }
// ======================================================
// --- ISITICI AÇ / KAPA ---
            void onHeaterToggle() {

             if (state == RUNNING) {    
            heaterOn = !heaterOn;
            if (heaterOn) {
                heater->setBrightness(128);  // %50 güç
               // lcd.clear();
               // lcd.setCursor(0,0);
               // lcd.print("Isitici: ACIK");
            } else {
                heater->setBrightness(0);
               // lcd.clear();
               // lcd.setCursor(0,0);
               // lcd.print("Isitici: KAPALI");
            }
            delay(700);
            }
            }
// ======================================================
// --- RESET ---
            void resetSystem() {
              totalSeconds = 0;
              displaySeconds = 0;
              pwmValue = 0;
            
              motor->setBrightness(0);
              heater->setBrightness(0);
              heaterOn = false;
            
              digitalWrite(RELAY1, LOW);
              digitalWrite(RELAY2, LOW);
              digitalWrite(RELAY3, LOW);
            }
// ======================================================
// --- LCD GÖRÜNTÜLEME ---
           void showRunning(unsigned long seconds) {
               lcd.setCursor(0, 0);
               lcd.print(" SURE: ");
               showTime(seconds);
           
               lcd.setCursor(0, 1);
               lcd.print(" HIZ=");
           
               int kademe = pwmValue / 50;
               lcd.print(kademe);  // sadece sayıyı göster
           
               // Isıtıcı durumu
               lcd.print("  ISI=");
               if (heaterOn) lcd.print("ON ");
               else lcd.print("OFF");
           }

            void showTime(unsigned long seconds) {
            unsigned int hours = seconds / 3600;
            unsigned int minutes = (seconds % 3600) / 60;
            unsigned int secs = seconds % 60;
            if (hours > 99) hours = 99;
            if (hours < 10) lcd.print("0");
            lcd.print(hours);
            lcd.print(":");
            if (minutes < 10) lcd.print("0");
            lcd.print(minutes);
            lcd.print(":");
            if (secs < 10) lcd.print("0");
            lcd.print(secs);
            lcd.print("  ");
            }
