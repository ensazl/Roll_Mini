          #include <Wire.h>
          #include <LiquidCrystal_I2C.h>
          #include <avr/wdt.h>

          LiquidCrystal_I2C lcd(0x27, 16, 2);

          // Röle pinleri
          #define RELAY1 12
          #define RELAY2 11
          #define RELAY3 10

          // PWM çıkışı
          #define PWM_OUT 9

          // Buton pinleri
          #define BTN_START 2
          #define BTN_STOP 3
          #define BTN_ILERI 4
          #define BTN_GERI 5
          #define BTN_DEVAR 6
          #define BTN_DEVEKS 7

          // Sistem durumları
          enum SysState { IDLE, RUNNING, PAUSED, FINISHED };
          SysState state = IDLE;

          // Zaman takibi
          unsigned long startMillis = 0;
          unsigned long totalSeconds = 0;
          unsigned long displaySeconds = 0;

          // Kademe tablosu
          int kademeler[] = {40, 90, 150, 220, 255};
          const int kademeSayisi = sizeof(kademeler) / sizeof(kademeler[0]);

          // PWM
          int pwmValue = 0;

          // Buton durumu
          bool btnPrev[6] = {false, false, false, false, false, false};

          // Watchdog reset algılama
          bool wasWatchdogReset = false;

          void setup() {
            
            // Eğer reset nedeni Watchdog ise, bayrağı oku
            if (MCUSR & _BV(WDRF)) {
              wasWatchdogReset = true;
            }
            MCUSR = 0; // Reset nedenini temizle

            wdt_disable();        // Konfigürasyondan önce devre dışı bırak
            wdt_enable(WDTO_4S);  // ✅ 4 saniyelik Watchdog aktif


            lcd.init();
            lcd.backlight();
            lcd.clear();

            if (wasWatchdogReset) {
              lcd.setCursor(0,0);
              lcd.print("Sistem Resetlendi!");
              lcd.setCursor(0,1);
              lcd.print("Yeniden Basladi");
              delay(1500);
            } else {
              lcd.setCursor(0,0);
              lcd.print("  SISTEM HAZIR  ");
            }

            pinMode(RELAY1, OUTPUT);
            pinMode(RELAY2, OUTPUT);
            pinMode(RELAY3, OUTPUT);
            digitalWrite(RELAY1, LOW);
            digitalWrite(RELAY2, LOW);
            digitalWrite(RELAY3, LOW);

            pinMode(PWM_OUT, OUTPUT);
            analogWrite(PWM_OUT, 0);

            pinMode(BTN_START, INPUT_PULLUP);
            pinMode(BTN_STOP, INPUT_PULLUP);
            pinMode(BTN_ILERI, INPUT_PULLUP);
            pinMode(BTN_GERI, INPUT_PULLUP);
            pinMode(BTN_DEVAR, INPUT_PULLUP);
            pinMode(BTN_DEVEKS, INPUT_PULLUP);
          }

          void loop() {

            wdt_reset();  // ✅ Watchdog'u sürekli besle
            readButtons();

            if (state == RUNNING) {
              displaySeconds = totalSeconds + (millis() - startMillis) / 1000;
              showRunning(displaySeconds);
            }
          }

          void readButtons() {
            checkButton(BTN_START, 0);
            checkButton(BTN_STOP, 1);
            checkButton(BTN_ILERI, 2);
            checkButton(BTN_GERI, 3);
            checkButton(BTN_DEVAR, 4);
            checkButton(BTN_DEVEKS, 5);
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
            }
          }

// ===== START =====
          void onStart() {
            if (state == FINISHED) {
              resetSystem();
              lcd.clear();
              lcd.setCursor(0,0);
              lcd.print("   BASLATILDI ");
              delay(1000);
              state = RUNNING;
              startMillis = millis();
              digitalWrite(RELAY3, HIGH); // Çalışma başladığında aktif
              digitalWrite(RELAY1, HIGH); // Motorun dönmesi için başladığında aktif
              return;
            }

            if (state == PAUSED || state == IDLE) {
              digitalWrite(RELAY3, HIGH); // Röle3 start'ta aktif
              digitalWrite(RELAY1, HIGH); // Motorun dönmesi için başladığında aktif
              startMillis = millis();
              state = RUNNING;

              lcd.clear();
              lcd.setCursor(0,0);
              lcd.print("  CALISIYOR!!!");
              delay(700);
            } 
            else if (state == RUNNING) {
              lcd.clear();
              lcd.setCursor(0,0);
              lcd.print("  CALISIYOR!!!");
              delay(500);
            }
          }

// ===== STOP =====
          void onStop() {
            if (state == RUNNING) {
              totalSeconds += (millis() - startMillis) / 1000;
              state = PAUSED;

              digitalWrite(RELAY1, LOW);
              digitalWrite(RELAY2, LOW);
              // Röle3 burada kapanmaz, çalışmaya devam eder

              analogWrite(PWM_OUT, 0);
              pwmValue = 0;

              lcd.clear();
              lcd.setCursor(0,0);
              lcd.print("  DURDURULDU!!  ");
              lcd.setCursor(0,1);
              lcd.print("SURE: ");
              showTime(totalSeconds);
            } 
            else if (state == PAUSED) {
              state = FINISHED;

              digitalWrite(RELAY1, LOW);
              digitalWrite(RELAY2, LOW);
              digitalWrite(RELAY3, LOW); // Sadece BİTTİ’de kapanır
              analogWrite(PWM_OUT, 0);
              pwmValue = 0;

              lcd.clear();
              lcd.setCursor(0,0);
              lcd.print("    BITTI!!!");
              lcd.setCursor(0,1);
              lcd.print("TOPLAM: ");
              showTime(totalSeconds);

                // ✅ 10 saniye sonra sistemi otomatik hazır konumuna getir
             unsigned long finishTime = millis();
             while (millis() - finishTime < 10000) {
               wdt_reset();  // Watchdog resetlenmeye devam etsin
               delay(50);
             }
         
             resetSystem();
             lcd.clear();
             lcd.setCursor(0,0);
             lcd.print("  SISTEM HAZIR  ");
             state = IDLE;

            }
          }

// ===== ILERI =====
          void onIleri() {
            if (state == RUNNING) {
              digitalWrite(RELAY1, HIGH);
              digitalWrite(RELAY2, LOW);
              lcd.clear();
              lcd.setCursor(0,0);
              lcd.print("   ILERI YON   ");
              delay(500);
            }
          }

// ===== GERI =====
          void onGeri() {
            if (state == RUNNING) {
              digitalWrite(RELAY1, LOW);
              digitalWrite(RELAY2, HIGH);
              lcd.clear();
              lcd.setCursor(0,0);
              lcd.print("   GERI YON   ");
              delay(500);
            }
          }

// ===== PWM ARTI =====
          void onDevirArti() {
            if (state == RUNNING) {
              // Mevcut kademe bul
              int mevcutKademe = 0;
              for (int i = 0; i < kademeSayisi; i++) {
                if (pwmValue == kademeler[i]) {
                  mevcutKademe = i + 1;
                  break;
                }
              }
          
              // Bir sonraki kademeye geç
              if (mevcutKademe < kademeSayisi) {
                pwmValue = kademeler[mevcutKademe];
                analogWrite(PWM_OUT, pwmValue);
          
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("HIZ ARTTI -> ");
                lcd.print(mevcutKademe + 1);
                lcd.print(". Kademe");
                delay(500);
              }
            }
          }
          
// ===== PWM EKSI =====
          void onDevirEksi() {
            if (state == RUNNING) {

              // Mevcut kademe bul
              int mevcutKademe = 0;
              for (int i = 0; i < kademeSayisi; i++) {
                if (pwmValue == kademeler[i]) {
                  mevcutKademe = i + 1;
                  break;
                }
              }
          
              // Bir önceki kademeye geç
              if (mevcutKademe > 1) {
                pwmValue = kademeler[mevcutKademe - 2];
                analogWrite(PWM_OUT, pwmValue);
          
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("HIZ AZALDI -> ");
                lcd.print(mevcutKademe - 1);
                lcd.print(". Kademe");
                delay(500);
              }
              // Eğer zaten en düşükteyse hiçbir şey yapma
            }
          }
          
          
// ===== RESET =====
          void resetSystem() {
            totalSeconds = 0;
            displaySeconds = 0;
            pwmValue = 0;
            analogWrite(PWM_OUT, 0);
            digitalWrite(RELAY1, LOW);
            digitalWrite(RELAY2, LOW);
            digitalWrite(RELAY3, LOW);
          }

// ===== LCD =====
          void showRunning(unsigned long seconds) {
            lcd.setCursor(0,0);
            lcd.print("SURE: ");
            showTime(seconds);
          
            lcd.setCursor(0,1);
            lcd.print("HIZ: ");
          
            int kademe = pwmValue / 50;
          
            if (kademe == 0) {
              lcd.print("KAPALI   ");
            } else {
              lcd.print(kademe);
              lcd.print(". Kademe ");
            }
          }
/*
          void showPWM() {
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("PWM: ");
            lcd.print(map(pwmValue,0,255,0,100));
            lcd.print("%");
            delay(300);
          }
*/
          void showTime(unsigned long seconds) {
            unsigned int hours = seconds / 3600;
            unsigned int minutes = (seconds % 3600) / 60;
            unsigned int secs = seconds % 60;
      
            if (hours > 99) hours = 99;  // Güvenlik için sınır
      
            // Saat
            if (hours < 10) lcd.print("0");
            lcd.print(hours);
            lcd.print(":");
      
            // Dakika
            if (minutes < 10) lcd.print("0");
            lcd.print(minutes);
            lcd.print(":");
      
            // Saniye
            if (secs < 10) lcd.print("0");
            lcd.print(secs);
      
            lcd.print("   "); // önceki uzun yazıları temizlemek için boşluk
          }

