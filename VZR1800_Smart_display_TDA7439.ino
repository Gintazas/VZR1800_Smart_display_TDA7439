#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>
#include <Wire.h>

MCUFRIEND_kbv tft;

const int XP = 8, XM = A2, YP = A3, YM = 9;
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

#define PIN_AUDIO_AMP 22
#define PIN_MUSIC_LT 23
#define PIN_DEALER_MODE 24
#define ANALOG_BATT A15
#define JOY_X A8
#define JOY_Y A9
#define JOY_SW 25

#define CL_BG 0x0000
#define CL_ACCENT 0x07E0
#define CL_GREY 0x2104
#define CL_WHITE 0xFFFF
#define CL_BLUE 0x001F
#define CL_RED 0xF800
#define CL_ORANGE 0xFD20
#define CL_LIGHT_BLUE 0x5DFF
#define CL_DARK_GREY 0x3186   // Šviesesnė už tavo CL_GREY, bet vis dar neutrali fonui
#define CL_NAVY 0x000F        // Labai tamsiai mėlyna (atrodo prabangiai kaip fonas)
#define CL_DARK_RED 0x8000    // Tamsiai raudona (gerai stulpelio STP fonui, kai jis tuščias)
#define CL_DARK_GREEN 0x0400  // Tamsiai žalia (gerai stulpelio TPS fonui)
// Funkcinės spalvos
#define CL_YELLOW 0xFFE0   // Perspėjimams (pvz., kai temperatūra kyla, bet dar ne kritinė)
#define CL_CYAN 0x07FF     // Šviesiai žydra (labai gerai matoma tekstui nakties metu)
#define CL_MAGENTA 0xF81F  // Purpurinė (dažnai naudojama diagnostikos režimams žymėti)
#define CL_GOLD 0xFEA0     // Auksinė (maksimaliems pasiekimams/fiksavimams pažymėti)
// --- SHIFT LIGHT ---
#define SHIFT_RPM 5500
bool shiftFlashState = false;

// Kintamieji
int rpm = 0, speed = 0, coolant = 0, gear = 0;
float tps = 0, iat = 0, map_val = 0, inj = 0;
float volt = 0, ign_adv = 0, avgFuel = 0.0;
float o2_1 = 0, o2_2 = 0, stp = 0;
int isc = 0;
String dtc = "C00";
int lastVFill = -1;
int last_tps_val = -1;
int last_stp_val = -1;
int maxRpm = 0, maxSpeed = 0;
int currentPage = 0;
int tps_bar = 0;
int stp_bar = 0;
const int sidebarWidth = 92;
bool dealerModeActive = false;
bool ampState = false;
bool tdaState = false;
int l_vol = -1, l_bass = -1, l_mid = -1, l_treb = -1;
bool l_mute = false;
int l_input = -1;
int tdaVol = 30, tdaBass = 0, tdaMid = 0, tdaTreb = 0;
bool tdaMute = false;
int tdaInput = 1;
bool sidebarSelected = true;  // true = sonine juosta, false = pagrindinis langas
int sidebarIndex = 0;         // 0=AUDIO, 1=MUSIC, 2=DEALER
unsigned long dragStartTime = 0;
float dragTime = 0.0;
bool dragRunning = false;

byte sdsReply[60];

void setup() {
  Wire.begin();
  uint16_t ID = tft.readID();
  if (ID == 0xD3D3 || ID == 0x0) ID = 0x9486;
  tft.begin(ID);
  tft.setRotation(1);
  pinMode(PIN_AUDIO_AMP, OUTPUT);
  pinMode(PIN_MUSIC_LT, OUTPUT);
  pinMode(PIN_DEALER_MODE, OUTPUT);
  pinMode(JOY_SW, INPUT_PULLUP);

  digitalWrite(PIN_DEALER_MODE, LOW);  // Užtikrina, kad starto metu būtų 0V

  Serial1.begin(10411);
  sdsInit();
  drawStaticUI();
  drawDynamicPage();
}

void sdsInit() {
  pinMode(18, OUTPUT);
  digitalWrite(18, LOW);
  delay(25);
  digitalWrite(18, HIGH);
  delay(25);
  Serial1.begin(10411);
}

void updateSDS() {
  byte requestData[] = { 0x80, 0x11, 0xF1, 0x02, 0x21, 0x01, 0xA5 };
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 150) {
    Serial1.write(requestData, sizeof(requestData));
    lastUpdate = millis();
  }
  if (Serial1.available() > 20) {
    Serial1.readBytes(sdsReply, 60);
    if (sdsReply[0] == 0x80 || sdsReply[0] == 0x61) {
      rpm = (sdsReply[13] * 256 + sdsReply[14]) / 4;
      speed = sdsReply[15];
      coolant = sdsReply[16] - 40;
      tps = sdsReply[19] / 2.55;
      map_val = sdsReply[17];
      inj = (sdsReply[22] * 256 + sdsReply[23]) / 100.0;
      gear = sdsReply[34] & 0x0F;
      volt = sdsReply[30] / 10.0;
      stp = sdsReply[20] / 2.55;
      o2_1 = sdsReply[26] / 100.0;
      o2_2 = sdsReply[27] / 100.0;
      isc = sdsReply[28];
      iat = sdsReply[18] - 40;
      ign_adv = sdsReply[31] / 2.0 - 64.0;  // Nuskaitome 31-ą baitą ir paverčiame į laipsnius

      if (rpm > maxRpm) maxRpm = rpm;
      if (speed > maxSpeed) maxSpeed = speed;
      if (speed > 5) avgFuel = (inj * rpm * 0.00096) / speed * 100.0;
      else avgFuel = 0.0;
    }
  }
}

uint16_t getO2Color(float val) {
  if (val < 0.35) return CL_ORANGE;
  if (val > 0.65) return CL_LIGHT_BLUE;
  return CL_ACCENT;
}

void checkShiftLight() {
  if (rpm >= SHIFT_RPM) {
    shiftFlashState = !shiftFlashState;
    tft.fillScreen(shiftFlashState ? CL_RED : CL_BG);
    if (!shiftFlashState) {
      drawStaticUI();
      drawDynamicPage();
    }
  } else if (shiftFlashState) {
    shiftFlashState = false;
    tft.fillScreen(CL_BG);
    drawStaticUI();
    drawDynamicPage();
  }
}

void drawStaticUI() {
  tft.drawFastVLine(sidebarWidth + 1, 0, 320, CL_GREY);
  updateSidebarButtons();
}

void updateSidebarButtons() {
  drawSmallButton(5, 5, "AUDIO", "AMP", digitalRead(PIN_AUDIO_AMP), sidebarSelected && sidebarIndex == 0);
  drawSmallButton(5, 82, "MUSIC", "LIGHT", digitalRead(PIN_MUSIC_LT), sidebarSelected && sidebarIndex == 1);
  drawSmallButton(5, 159, "DEALER", "MODE", dealerModeActive, sidebarSelected && sidebarIndex == 2);
  drawSmallButton(5, 236, "TDA", "CTRL", tdaState, sidebarSelected && sidebarIndex == 3);
}

void drawSmallButton(int x, int y, const char* l1, const char* l2, bool state, bool selected) {

  int bw = sidebarWidth - 10;
  int bh = 72;
  uint16_t fillColor;

  if (state) fillColor = CL_BLUE;            // aktyvus – mėlynas
  else if (selected) fillColor = CL_ORANGE;  // pažymėtas – geltonas
  else fillColor = CL_GREY;                  // normalus – pilkas

  tft.fillRoundRect(x, y, bw, bh, 8, fillColor);
  tft.drawRoundRect(x, y, bw, bh, 8, selected ? CL_ORANGE : CL_WHITE);
  tft.setTextColor(CL_WHITE);
  tft.setTextSize(2);  //mygtuku srifto dydis
  tft.setCursor(x + 5, y + 15);
  tft.print(l1);
  tft.setCursor(x + 5, y + 30);
  tft.print(l2);
  tft.fillCircle(x + bw - 10, y + bh - 10, 5, state ? CL_ACCENT : 0x3800);
}

void drawDynamicPage() {
  int dataX = sidebarWidth + 2;
  int dataW = tft.width() - dataX;
  if (!shiftFlashState) tft.fillRect(dataX, 0, dataW, 320, CL_BG);
  tft.fillRect(dataX, 0, dataW, 40, CL_BLUE);
  tft.setTextSize(2);
  tft.setTextColor(CL_WHITE);
  tft.setCursor(dataX + 35, 12);

  if (currentPage == 0) tft.print("*VZR-1800* Kruizo parametrai");
  else if (currentPage == 1) tft.print("Kuro Ir Oro misinys");
  else if (currentPage == 2) tft.print("Elekrtos sistema");
  else if (currentPage == 3) tft.print("SDS Diagnostika");
  else if (currentPage == 4) tft.print("Maks.fiks.greitis,apsukos");
  else if (currentPage == 5) tft.print("Degimo ankstinimas (IGN)");
  else if (currentPage == 6) tft.print("0-100 km/h Drag Timer");
  else if (currentPage == 7) tft.print("Droselio sklendziu darbas");
  else if (currentPage == 8) tft.print("TDA VALDYMAS");
  l_vol = -1; l_bass = -99; l_mid = -99; l_treb = -99; l_mute = !tdaMute; l_input = -1;
}

void updateData() {
  if (shiftFlashState) return;
  int startX = sidebarWidth + 15;
  tft.setTextColor(CL_WHITE, CL_BG);

  if (currentPage == 0) {
    // --- TACHOMETRAS ---
    tft.setTextSize(2);
    tft.setTextColor(CL_RED, CL_BG);
    tft.setCursor(startX, 45);
    tft.print("Tacho: ");
    tft.print((rpm / 1000.0), 1);
    tft.print(" RPM");

    int barW = 160;
    int barH = 18;
    int rpmBarX = startX + 185;
    tft.drawRect(rpmBarX, 45, barW, barH, CL_WHITE);
    int rpmFill = map(constrain(rpm, 0, 6000), 0, 6000, 0, barW - 4);
    tft.fillRect(rpmBarX + 2, 47, rpmFill, barH - 4, CL_RED);
    tft.fillRect(rpmBarX + 2 + rpmFill, 47, barW - 4 - rpmFill, barH - 4, CL_BG);

    // --- Tacho skalė su skaičiais ---
    tft.setTextSize(1);
    tft.setTextColor(CL_WHITE);
    for (int i = 0; i <= 6000; i += 1000) {
      int xPos = rpmBarX + map(i, 0, 6000, 0, barW);
      tft.drawFastVLine(xPos, 45 + barH, 3, CL_WHITE);
      tft.setCursor(xPos - 3, 45 + barH + 5);
      tft.print(i / 1000);
    }
    // --- SANAUDOS IR DIDELIS GREITIS ---
    tft.setTextSize(2);
    tft.setTextColor(CL_WHITE, CL_BG);
    tft.setCursor(startX, 82);
    tft.print("SANAUDOS: ");
    tft.print(avgFuel, 1);
    tft.print(" l/100km ");

    tft.setTextSize(12);
    tft.setCursor(startX + 2, 110);
    if (speed < 100) tft.print(" ");
    if (speed < 10) tft.print(" ");
    tft.print(speed);
    tft.setTextSize(2);
    tft.setCursor(startX + 153, 210);
    tft.print("km/h");

    // --- TEMPERATŪRA ---
    tft.setTextColor(CL_RED, CL_BG);
    tft.setCursor(startX, 240);
    tft.print("Var.temp: ");
    tft.print(coolant);
    tft.print(" C");

    int tempBarX = startX + 185;
    tft.drawRect(tempBarX, 240, barW, barH, CL_WHITE);
    uint16_t tColor = (coolant > 100) ? CL_RED : (coolant < 40 ? CL_BLUE : CL_ACCENT);
    int tempFill = map(constrain(coolant, 40, 120), 40, 120, 0, barW - 4);
    tft.fillRect(tempBarX + 2, 242, tempFill, barH - 4, tColor);
    tft.fillRect(tempBarX + 2 + tempFill, 242, barW - 4 - tempFill, barH - 4, CL_BG);

    // --- Temp skalė su skaičiais (40-120) ---
    tft.setTextSize(1);
    tft.setTextColor(CL_WHITE);
    for (int i = 40; i <= 120; i += 10) {
      int xPos = tempBarX + map(i, 40, 120, 0, barW);
      tft.drawFastVLine(xPos, 240 + barH, 3, CL_WHITE);
      if (i % 20 == 0) {  // Skaičiai kas 20 laipsnių, kad netilptų per tankiai
        tft.setCursor(xPos - 6, 240 + barH + 5);
        tft.print(i);
      }
    }


    int gX = 360;
    uint16_t gearColor = (gear == 0) ? CL_ACCENT : CL_LIGHT_BLUE;
    tft.setTextSize(3);
    tft.setTextColor(gearColor, CL_BG);
    tft.setCursor(gX, 105);
    tft.print("G");
    tft.setCursor(gX, 135);
    tft.print("E");
    tft.setCursor(gX, 165);
    tft.print("A");
    tft.setCursor(gX, 195);
    tft.print("R");
    tft.setTextSize(15);
    tft.setCursor(gX + 25, 110);
    if (gear == 0) tft.print("N");
    else tft.print(gear);
  } else if (currentPage == 1) {
    tft.setTextSize(2);
    tft.setCursor(startX, 50);
    tft.print("PAGR. DROSELIS: ");
    tft.print(tps, 1);
    tft.print("% ");
    tft.setCursor(startX, 80);
    tft.print("ANTR. SKLENDES: ");
    tft.print(stp, 1);
    tft.print("% ");
    tft.setCursor(startX, 110);
    tft.print("KOLEKT. SLEG : ");
    tft.print(map_val, 1);
    tft.print("kPa");
    tft.setCursor(startX, 140);
    tft.print("PURKST. LAIKAS: ");
    tft.print(inj, 2);
    tft.print("ms");
    tft.setCursor(startX, 170);
    tft.print("LAISVOS APSUKOS: ");
    tft.print(isc);
    tft.print(" st");
    tft.setCursor(startX, 210);
    tft.print("MISINYS #1 (O2): ");
    tft.setTextColor(getO2Color(o2_1), CL_BG);
    tft.print(o2_1, 2);
    tft.setTextColor(CL_WHITE, CL_BG);
    tft.print(" V");
    tft.setCursor(startX, 240);
    tft.print("MISINYS #2 (O2): ");
    tft.setTextColor(getO2Color(o2_2), CL_BG);
    tft.print(o2_2, 2);
    tft.setTextColor(CL_WHITE, CL_BG);
    tft.print(" V");
    tft.setCursor(startX, 275);
    tft.print("ORO TEMPERATURA: ");
    tft.print(iat, 1);
    tft.print(" C");
  } else if (currentPage == 2) {
    // --- VOLTMETRO PUSLAPIS ---
    // ===== ECU JUOSTA (be mirgėjimo) =====

    int eX = startX;
    int eY = 60;
    int eW = 360;
    int eH = 110;

    // --- Fonas ir rėmelis piešiamas tik 1 kartą ---
    static bool ecuFrameDrawn = false;
    if (!ecuFrameDrawn) {
      tft.fillRect(eX, eY, eW, eH, CL_BG);
      tft.drawRect(eX, eY, eW, eH, CL_LIGHT_BLUE);
      ecuFrameDrawn = true;
    }

    // --- Atnaujinam tik jei pasikeitė įtampa ---
    static float lastVolt = -1;

    if (abs(volt - lastVolt) > 0.05) {

      lastVolt = volt;

      // Išvalom tik vidų
      tft.fillRect(eX + 2, eY + 2, eW - 4, eH - 4, CL_BG);

      // Užpildymo juosta
      int eFill = map(constrain(volt * 10, 100, 155), 100, 155, 0, eW - 4);

      tft.fillRect(eX + 2, eY + 2, eFill, eH - 4,
                   (volt < 12.0) ? CL_RED : CL_ACCENT);

      // Centruotas tekstas
      tft.setTextSize(3);

      String ecuText = "ECU: " + String(volt, 1) + " V";

      int16_t x1, y1;
      uint16_t w, h;
      tft.getTextBounds(ecuText, 0, 0, &x1, &y1, &w, &h);

      int textX = eX + (eW - w) / 2;
      int textY = eY + (eH - h) / 2;

      tft.setTextColor(CL_WHITE, CL_BG);
      tft.setCursor(textX, textY);
      tft.print(ecuText);
    }

    // Analoginis nuskaitymas (A15)
    int raw = analogRead(ANALOG_BATT);
    float realVolt = (raw * 5.0 / 1024.0) * 4.0;  // 1/4 įtampos daliklis

    int bX = startX, bY = 190, bW = 360, bH = 130;

    // ---- FONAS NUPIEŠIAMAS TIK KARTĄ (nemirksės) ----
    static bool battDrawn = false;
    if (!battDrawn) {
      tft.fillRect(bX, bY, bW, bH, CL_GREY);
      tft.drawRect(bX, bY, bW, bH, CL_LIGHT_BLUE);
      battDrawn = true;
    }

    // ---- CENTRUOTAS UŽRAŠAS ----
    tft.setTextSize(4);
    tft.setTextColor(CL_ORANGE, CL_GREY);

    String battText = "BATERIJA:" + String(realVolt, 1) + "V";

    // Apskaičiuojam teksto plotį
    int textWidth = battText.length() * 24;  // ~6px * size(4)
    int centerX = bX + (bW - textWidth) / 2;

    tft.setCursor(centerX, bY + 15);
    tft.print(battText);

    // ---- SKALĖ (perpiešiama be mirgėjimo) ----
    int vFill = map(constrain(realVolt * 10, 100, 155), 100, 155, 0, bW - 4);

    int barX = bX + 2;
    int barY = bY + 60;
    int barH = bH - 70;

    // jei reikšmė pasikeitė
    if (vFill != lastVFill) {
      if (vFill > lastVFill) {
        tft.fillRect(barX + lastVFill, barY,
                     vFill - lastVFill, barH,
                     (realVolt < 11.8) ? CL_RED : CL_ACCENT);
      } else {
        tft.fillRect(barX + vFill, barY,
                     lastVFill - vFill, barH,
                     CL_GREY);
      }

      lastVFill = vFill;
    }
  } else if (currentPage == 3) {
    // --- SDS DIAGNOSTIKA  ---
    tft.setTextSize(3);
    if (dtc == "C00") {
      tft.setTextColor(CL_ACCENT, CL_BG);
      tft.setCursor(startX, 60);
      tft.print("SISTEMA OK");
    } else {
      tft.setTextColor(CL_RED, CL_BG);
      tft.setCursor(startX, 60);
      tft.print("KLAIDA: ");
      tft.print(dtc);
    }
    tft.setTextSize(2);
    tft.setTextColor(CL_WHITE, CL_BG);
    tft.setCursor(startX, 110);
    if (dtc == "C00") tft.print("SISTEMA TVARKOJE");
    else if (dtc == "C12") tft.print("ALKUNINIO VELENO DAV.");
    else if (dtc == "C13") tft.print("ISIURBIMO SLEGIO DAV.");
    else if (dtc == "C14") tft.print("TPS (DROSELIO) DAV.");
    else if (dtc == "C15") tft.print("ECT (VANDENS TEMP) DAV.");
    else if (dtc == "C21") tft.print("IAT (ORO TEMP) DAV.");
    else if (dtc == "C23") tft.print("VIRSTIMO DAVIKLIS");
    else if (dtc == "C24") tft.print("UZDEGIMO RITE");
    else if (dtc == "C28") tft.print("STVA SKLENDZIU PAVARA");
    else if (dtc == "C31") tft.print("PAVAROS DAVIKLIS (GP)");
    else if (dtc == "C32") tft.print("PURKSTUKO GRANDINE");
    else if (dtc == "C41") tft.print("KURO SIURBLIO RELE");
    else if (dtc == "C42") tft.print("SPYNELES RYSYS (IGN)");
    else if (dtc == "C46") tft.print("EXVA ISMETIMO VOZTUVAS");
    else tft.print("NEZINOMA KLAIDA: " + dtc);

    tft.setCursor(startX, 200);
    if (dealerModeActive) {
      tft.setTextColor(CL_ACCENT, CL_BG);  // Žalia spalva
      tft.print("DEALER MODE: ACTIVE");
    } else {
      tft.setTextColor(CL_GREY, CL_BG);  // Pilka spalva
      tft.print("DEALER MODE: OFF   ");  // Tarpai gale nuvalymui
    }
  } else if (currentPage == 4) {
    tft.setTextSize(3);
    tft.setCursor(startX, 70);
    tft.print("MAKS. GREITIS:");
    tft.setTextColor(CL_LIGHT_BLUE, CL_BG);
    tft.setCursor(startX, 105);
    tft.print(maxSpeed);
    tft.print(" km/val");
    tft.setTextColor(CL_WHITE, CL_BG);
    tft.setCursor(startX, 170);
    tft.print("MAKS. APSUKOS:");
    tft.setTextColor(CL_RED, CL_BG);
    tft.setCursor(startX, 205);
    tft.print(maxRpm);
    tft.print(" rpm");

  } else if (currentPage == 5) {
    int startX = sidebarWidth + 20;
    // Didelis skaitmeninis rodmuo
    tft.setTextSize(3);
    tft.setTextColor(CL_WHITE, CL_BG);
    tft.setCursor(startX, 60);
    tft.print("KAMPAS:");

    tft.setTextSize(10);
    tft.setTextColor(CL_ACCENT, CL_BG);
    tft.setCursor(startX, 100);
    if (ign_adv < 10) tft.print(" ");  // Centravimas
    tft.print(ign_adv, 0);
    tft.setTextSize(4);
    tft.print(" deg");

    // Horizontali dinaminė skalė (0 - 50 laipsnių)
    int barW = 350;
    int barH = 30;
    int barX = startX;
    int barY = 220;
    tft.drawRect(barX, barY, barW, barH, CL_WHITE);

    // Užpildome skalę pagal ign_adv reikšmę
    int advFill = map(constrain(ign_adv, 0, 50), 0, 50, 0, barW - 4);
    tft.fillRect(barX + 2, barY + 2, advFill, barH - 4, CL_ORANGE);
    tft.fillRect(barX + 2 + advFill, barY + 2, barW - 4 - advFill, barH - 4, CL_BG);

    // Skalės padalos
    tft.setTextSize(2);
    tft.setTextColor(CL_GREY);
    for (int i = 0; i <= 50; i += 10) {
      int xP = barX + map(i, 0, 50, 0, barW);
      tft.drawFastVLine(xP, barY + barH, 5, CL_WHITE);
      tft.setCursor(xP - 5, barY + barH + 10);
      tft.print(i);
    }
  } else if (currentPage == 6) {
    int startX = sidebarWidth + 25;

    // --- LOGIKA ---
    if (speed > 2 && !dragRunning && dragTime == 0.0) {
      dragStartTime = millis();
      dragRunning = true;
    }
    if (dragRunning) {
      if (speed < 100) {
        dragTime = (millis() - dragStartTime) / 1000.0;
      } else {
        dragRunning = false;  // Sustojam ties 100 km/h
      }
    }
    // Nunulinimas: jei sustojame (speed == 0) po važiavimo
    if (speed == 0 && !dragRunning) {
      // Galima pridėti mygtuko paspaudimą nunulinimui, bet pradžiai palikime automatiškai
    }

    // --- VAIZDAS ---
    tft.setTextSize(3);
    tft.setTextColor(CL_WHITE, CL_BG);
    tft.setCursor(startX, 60);
    tft.print("LAIKAS:");

    tft.setTextSize(10);
    tft.setTextColor(dragRunning ? CL_ORANGE : CL_ACCENT, CL_BG);
    tft.setCursor(startX, 100);
    tft.print(dragTime, 2);
    tft.setTextSize(4);
    tft.print(" s");

    tft.setTextSize(2);
    tft.setTextColor(CL_GREY, CL_BG);
    tft.setCursor(startX, 210);
    if (dragRunning) tft.print("MATUOJAMA...");
    else if (dragTime > 0) tft.print("FINISAS! Sustokite reset'ui.");
    else tft.print("PASIRUOSE? Gazuokite!");

  } else if (currentPage == 7) {
    int startX = sidebarWidth + 20;
    int bW = 170;
    int bH = 240;
    int gap = 20;
    int bY = 50;
    int stpX = startX + bW + gap;

    // ---- FONAS NUPIEŠIAMAS TIK KARTĄ (nemirksės) ----
    static int lastDrawnPage = -1;
    if (lastDrawnPage != currentPage) {
      tft.fillRect(startX, bY, (bW * 2) + gap + 5, bH + 5, CL_BG);  // Išvalom plotą

      // TPS fonas
      tft.fillRect(startX, bY, bW, bH, CL_GREY);
      tft.drawRect(startX, bY, bW, bH, CL_WHITE);

      // STP fonas
      tft.fillRect(stpX, bY, bW, bH, CL_GREY);
      tft.drawRect(stpX, bY, bW, bH, CL_WHITE);

      // Užrašai viduryje (stacionarūs)
      tft.setTextSize(6);
      tft.setTextColor(CL_WHITE, CL_GREY);
      tft.setCursor(startX + (bW / 2) - 50, bY + (bH / 2) - 20);
      tft.print("TPS");
      tft.setCursor(stpX + (bW / 2) - 50, bY + (bH / 2) - 20);
      tft.print("STP");

      lastDrawnPage = currentPage;
      last_tps_val = -1;
      last_stp_val = -1;  // Resetinam užpildą
    }

    // ---- TPS SKALĖ (Vertikali, be mirgėjimo) ----
    int tpsFill = map(constrain((int)tps, 0, 100), 0, 100, 0, bH - 4);
    if (tpsFill != last_tps_val) {
      if (tpsFill > last_tps_val) {
        // Kyla į viršų: spalvinam naują dalį
        tft.fillRect(startX + 2, (bY + bH - 2) - tpsFill, bW - 4, tpsFill - last_tps_val, CL_ACCENT);
      } else {
        // Krenta žemyn: trinam seną dalį su CL_GREY
        tft.fillRect(startX + 2, (bY + bH - 2) - last_tps_val, bW - 4, last_tps_val - tpsFill, CL_GREY);
      }
      // Atnaujinam procentus viršuje
      tft.setTextSize(3);
      tft.setTextColor(CL_WHITE, CL_GREY);
      tft.setCursor(startX + (bW / 2) - 30, bY + 10);
      tft.print((int)tps);
      tft.print("% ");
      last_tps_val = tpsFill;
    }

    // ---- STP SKALĖ (Vertikali, be mirgėjimo) ----
    int stpFill = map(constrain((int)stp, 0, 100), 0, 100, 0, bH - 4);
    if (stpFill != last_stp_val) {
      if (stpFill > last_stp_val) {
        tft.fillRect(stpX + 2, (bY + bH - 2) - stpFill, bW - 4, stpFill - last_stp_val, CL_RED);
      } else {
        tft.fillRect(stpX + 2, (bY + bH - 2) - last_stp_val, bW - 4, last_stp_val - stpFill, CL_GREY);
      }

      tft.setTextSize(3);
      tft.setTextColor(CL_WHITE, CL_GREY);
      tft.setCursor(stpX + (bW / 2) - 30, bY + 10);
      tft.print((int)stp);
      tft.print("% ");
      last_stp_val = stpFill;
    }
  } 
 else if (currentPage == 8) {
    int sX = sidebarWidth + 15;
    int sY = 70; int sH = 165; int sW = 45; int gap = 25;
    
    // Mirksėjimo logika: kas 500ms keičiam spalvą, jei MUTE įjungtas
    bool blinkRed = tdaMute && ((millis() / 500) % 2 == 0);
    uint16_t warnCol = blinkRed ? CL_RED : CL_DARK_GREY;

    static unsigned long lastBlink = 0;
    bool blinkUpdate = tdaMute && (millis() - lastBlink > 500);
    if (blinkUpdate) lastBlink = millis();

    // 1. VOL
    if (tdaVol != l_vol || tdaMute != l_mute || blinkUpdate) {
      int fill = map(constrain(tdaVol, 0, 48), 0, 48, 0, sH - 4);
      tft.fillRect(sX + 2, sY, sW - 4, sH, CL_DARK_GREY);
      tft.drawRect(sX, sY, sW, sH, CL_WHITE);
      tft.fillRect(sX + 2, (sY + sH - 2) - fill, sW - 4, fill, tdaMute ? warnCol : CL_ACCENT);
      tft.setTextSize(2); tft.setTextColor(CL_WHITE, CL_BG);
      tft.setCursor(sX, sY + sH + 10); tft.print("VOL");
      tft.setCursor(sX, sY - 22); tft.print(tdaVol); tft.print("  ");
      l_vol = tdaVol;
    }

    // 2. BASS
    int bX = sX + (sW + gap);
    if (tdaBass != l_bass || tdaMute != l_mute || blinkUpdate) {
      int fill = map(constrain(tdaBass, -15, 15), -15, 15, 0, sH - 4);
      tft.fillRect(bX + 2, sY, sW - 4, sH, CL_DARK_GREY);
      tft.drawRect(bX, sY, sW, sH, CL_WHITE);
      uint16_t bCol = tdaMute ? warnCol : ((tdaBass < 0) ? CL_CYAN : (tdaBass > 10 ? CL_RED : CL_ORANGE));
      tft.fillRect(bX + 2, (sY + sH - 2) - fill, sW - 4, fill, bCol);
      tft.setCursor(bX, sY + sH + 10); tft.print("BAS");
      tft.setCursor(bX, sY - 22); if(tdaBass > 0) tft.print("+"); tft.print(tdaBass); tft.print("  ");
      l_bass = tdaBass;
    }

    // 3. MID
    int mX = sX + (sW + gap) * 2;
    if (tdaMid != l_mid || tdaMute != l_mute || blinkUpdate) {
      int fill = map(constrain(tdaMid, -15, 15), -15, 15, 0, sH - 4);
      tft.fillRect(mX + 2, sY, sW - 4, sH, CL_DARK_GREY);
      tft.drawRect(mX, sY, sW, sH, CL_WHITE);
      uint16_t mCol = tdaMute ? warnCol : ((tdaMid < 0) ? CL_CYAN : (tdaMid > 10 ? CL_RED : CL_ORANGE));
      tft.fillRect(mX + 2, (sY + sH - 2) - fill, sW - 4, fill, mCol);
      tft.setCursor(mX, sY + sH + 10); tft.print("MID");
      tft.setCursor(mX, sY - 22); if(tdaMid > 0) tft.print("+"); tft.print(tdaMid); tft.print("  ");
      l_mid = tdaMid;
    }

    // 4. TREBLE
    int tX = sX + (sW + gap) * 3;
    if (tdaTreb != l_treb || tdaMute != l_mute || blinkUpdate) {
      int fill = map(constrain(tdaTreb, -15, 15), -15, 15, 0, sH - 4);
      tft.fillRect(tX + 2, sY, sW - 4, sH, CL_DARK_GREY);
      tft.drawRect(tX, sY, sW, sH, CL_WHITE);
      uint16_t tCol = tdaMute ? warnCol : ((tdaTreb < 0) ? CL_CYAN : (tdaTreb > 10 ? CL_RED : CL_ORANGE));
      tft.fillRect(tX + 2, (sY + sH - 2) - fill, sW - 4, fill, tCol);
      tft.setCursor(tX, sY + sH + 10); tft.print("TRE");
      tft.setCursor(tX, sY - 22); if(tdaTreb > 0) tft.print("+"); tft.print(tdaTreb); tft.print("  ");
      l_treb = tdaTreb;
    }

    // 5. MUTE
    if (tdaMute != l_mute) {
      tft.fillRoundRect(390, sY, 80, sH, 8, tdaMute ? CL_RED : CL_GREY);
      tft.drawRoundRect(390, sY, 80, sH, 8, CL_WHITE);
      tft.setTextColor(CL_WHITE);
      tft.setCursor(390 + 34, sY + 15); tft.print("M");
      tft.setCursor(390 + 34, sY + 50); tft.print("U");
      tft.setCursor(390 + 34, sY + 85); tft.print("T");
      tft.setCursor(390 + 34, sY + 120); tft.print("E");
      l_mute = tdaMute;
    }

    // 6. INPUT (4 realūs įėjimai, centruoti)
    if (tdaInput != l_input) {
      int iX = sidebarWidth + 15;
      int iW = 470 - iX;
      int iY = 265;
      
      // Masyvas su 4 pavadinimais (indeksas 0 nenaudojamas, 1-4 naudojami)
      const char* inputNames[] = {"", "BLUETOOTH", "EXT MIC", "Nenaudojamas IN-3", "Nenaudojamas IN-4"};
      
      tft.fillRoundRect(iX, iY, iW, 45, 8, CL_BLUE);
      tft.drawRoundRect(iX, iY, iW, 45, 8, CL_WHITE);
      
      tft.setTextSize(2);
      tft.setTextColor(CL_WHITE);
      
      // Suformuojam tekstą (pvz., "INPUT: BLUETOOTH")
      String text = "INPUT: " + String(inputNames[tdaInput]);
      
      // Tikslus centravimas
      int16_t x1, y1; uint16_t w, h;
      tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(iX + (iW - w) / 2, iY + (45 - h) / 2); 
      
      tft.print(text);
      l_input = tdaInput;
    }
  }
}
void handleJoystick() {

  static unsigned long lastMove = 0;
  if (millis() - lastMove < 250) return;
  int xVal = analogRead(JOY_X);
  int yVal = analogRead(JOY_Y);
  bool pressed = digitalRead(JOY_SW) == LOW;

  // LEFT
  if (xVal < 300) {
    sidebarSelected = true;
    updateSidebarButtons();
    lastMove = millis();
  }

  // RIGHT
  else if (xVal > 700) {
    sidebarSelected = false;
    updateSidebarButtons();
    lastMove = millis();
  }

  // UP
  else if (yVal < 300) {

    if (sidebarSelected) {
      sidebarIndex--;
      if (sidebarIndex < 0) sidebarIndex = 3;
      updateSidebarButtons();
    } else {
      currentPage++;
      if (currentPage > 8) currentPage = 0;
      drawDynamicPage();
    }

    lastMove = millis();
  }

  // DOWN
  else if (yVal > 700) {

    if (sidebarSelected) {
      sidebarIndex++;
      if (sidebarIndex > 3) sidebarIndex = 0;
      updateSidebarButtons();
    } else {
      currentPage--;
      if (currentPage < 0) currentPage = 8;
      drawDynamicPage();
    }

    lastMove = millis();
  }

  // CLICK
  else if (pressed) {

    if (sidebarSelected) {

      if (sidebarIndex == 0) {
        ampState = !ampState;
        digitalWrite(PIN_AUDIO_AMP, ampState ? HIGH : LOW);
      } else if (sidebarIndex == 1) {
        digitalWrite(PIN_MUSIC_LT, !digitalRead(PIN_MUSIC_LT));
      } else if (sidebarIndex == 2) {
        dealerModeActive = !dealerModeActive;
        digitalWrite(PIN_DEALER_MODE, dealerModeActive ? HIGH : LOW);
        if (dealerModeActive) {
          currentPage = 3;  // Jei įjungiam - iškart rodom SDS
        } else {
          updateData();  // Perpiešia SDS langą su "DEALER MODE: OFF" užrašu
          delay(2000);   // 2 sekundžių pauzė apžiūrėjimui
          currentPage = tdaState ? 8 : 0;
        }
        drawDynamicPage();       // Perpiešiame pagrindinį langą
        updateSidebarButtons();  // Atnaujiname mygtuko spalvą (kad matytųsi, jog įjungta)
      } else if (sidebarIndex == 3) {
        tdaState = !tdaState;
        if (tdaState) {
          currentPage = 8;
        } else {
          currentPage = dealerModeActive ? 3 : 0;
        }
        drawDynamicPage();
      }
    }
    lastMove = millis();
  }
}

void loop() {
  static unsigned long lastRefresh = 0;
  if (millis() - lastRefresh > 100) {
    updateSDS();
    checkShiftLight();
    updateData();
    handleJoystick();
    tps_bar = constrain((int)tps, 0, 100);
    stp_bar = constrain((int)stp, 0, 100);
    lastRefresh = millis();
  }
  TSPoint p = ts.getPoint();
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);

  if (p.z > 100 && p.z < 2500) {
    int px = map(p.y, 920, 120, 0, 480);
    int py = map(p.x, 150, 920, 320, 0);
    
    if (px < sidebarWidth) {
      if (py < 80) {
        ampState = !ampState;
        digitalWrite(PIN_AUDIO_AMP, ampState ? HIGH : LOW);
      } else if (py >= 80 && py < 155) digitalWrite(PIN_MUSIC_LT, !digitalRead(PIN_MUSIC_LT));
      else if (py >= 155 && py < 230) {
        dealerModeActive = !dealerModeActive;
        digitalWrite(PIN_DEALER_MODE, dealerModeActive ? HIGH : LOW);
        if (dealerModeActive) {
          currentPage = 3;
        } else {
          currentPage = tdaState ? 8 : 0;
        }
      } else if (py >= 230) {
        tdaState = !tdaState;
        if (tdaState) {
          currentPage = 8;
        } else {
          currentPage = dealerModeActive ? 3 : 0;
        }
      }
      drawDynamicPage();
      updateSidebarButtons();
      delay(300);
    } else {
     if (currentPage == 8) {
        int sX = sidebarWidth + 15;
        int sY = 70; int sH = 165; int sW = 45; int gap = 25;

        // 1. SLAIDERIAI (Tikriname tik jei MUTE neaktyvus, kad nereaguotų mirksint)
        if (!tdaMute && py >= sY && py <= sY + sH) {
            if (px >= sX && px <= sX + sW) {
              tdaVol = map(constrain(py, sY, sY + sH), sY + sH, sY, 0, 48);
            }
            else if (px >= sX + (sW + gap) && px <= sX + (sW + gap) + sW) {
              tdaBass = map(constrain(py, sY, sY + sH), sY + sH, sY, -15, 15);
            }
            else if (px >= sX + (sW + gap) * 2 && px <= sX + (sW + gap) * 2 + sW) {
              tdaMid = map(constrain(py, sY, sY + sH), sY + sH, sY, -15, 15);
            }
            else if (px >= sX + (sW + gap) * 3 && px <= sX + (sW + gap) * 3 + sW) {
              tdaTreb = map(constrain(py, sY, sY + sH), sY + sH, sY, -15, 15);
            }
        }
        
        // 2. MUTE (Tikrinamas ATSKIRAI, ne per else if)
        if (px >= 390 && px <= 475 && py >= sY && py <= sY + sH) {
          tdaMute = !tdaMute;
          l_mute = !tdaMute; // Priverčiam updateData perpiešti rėmelius
          delay(350);
        }
        
        // 3. INPUT (Apatinė zona)
        else if (py >= 265 && py <= 315 && px >= sX && px <= 470) {
          tdaInput++;
          if (tdaInput > 4) tdaInput = 1;
          delay(350);
        }
      } 
      else {
        // Puslapių perjungimas likusiems puslapiams
        currentPage = (currentPage + 1) % 9;
        drawDynamicPage();
        delay(300);
      }
    }
  }
}
// --- TDA7439 VALDYMO FUNKCIJA ---
void sendTDA(byte reg, int val) {
  Wire.beginTransmission(0x44); // TDA7439 adresas
  Wire.write(reg);
  
  byte tdaVal = 0;

  if (reg == 0x02) { // VOLUME
    // TDA7439 Volume: 0 = 0dB (garsiausia), 47 = -47dB (tyla)
    // Kadangi tavo tdaVol yra 0..48 (kur 48 garsiausia):
    tdaVal = 48 - constrain(val, 0, 48);
  } 
  else if (reg >= 0x03 && reg <= 0x05) { // BASS, MID, TREBLE
    // Mapinam nuo -15..15 į TDA reikšmes 0..14 (neutralu = 7)
    tdaVal = map(constrain(val, -15, 15), -15, 15, 0, 14);
  } 
  else {
    tdaVal = (byte)val; // Input Selector (0..3)
  }

  Wire.write(tdaVal);
  Wire.endTransmission();
}