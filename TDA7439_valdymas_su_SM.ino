#include <Wire.h>

#define TDA7439_ADDRESS 0x44 

// TDA7439 registrų adresai
#define REG_INPUT_SELECT  0x00
#define REG_INPUT_GAIN    0x01
#define REG_VOLUME        0x02
#define REG_BASS          0x03
#define REG_MIDDLE        0x04
#define REG_TREBLE        0x05
#define REG_ATTEN_LEFT    0x06
#define REG_ATTEN_RIGHT   0x07

// Pakeistos pradinės reikšmės, kad iškart grotų (Garsumas 0, Gain +14dB)
uint8_t currentInput = 1;
uint8_t currentGain = 14;      // Pradedame nuo +14dB (kaip tavo veikiančiame kode)
int8_t currentVolume = 0;       // 0 yra garsiausiai
int8_t currentBass = 0;
int8_t currentMiddle = 0;
int8_t currentTreble = 0;
uint8_t currentAttenLeft = 0;   // Atidaryta (0 slopinimo)
uint8_t currentAttenRight = 0;  // Atidaryta (0 slopinimo)
boolean isMuted = false;

void setup() {
  Wire.begin();
  Wire.setClock(100000); // Užtikriname stabilų 100 kHz greitį
  Serial.begin(9600);
  
  delay(1000); // Pauzė mikroschemos stabilizacijai
  Serial.println(F("=== TDA7439 Serial Valdiklis Paleistas ==="));
  Serial.println(F("Iveskite komanda (pvz: VOL-15, IN2, BASS4, STATUS)"));
  
  applyAllSettings(); // Paleidžiame pataisytą seką
  printStatus();
}

void loop() {
  if (Serial.available() > 0) {
    String inputStr = Serial.readStringUntil('\n');
    inputStr.trim(); 
    inputStr.toUpperCase(); 
    
    processCommand(inputStr);
  }
}

// GRIEŽTA INICIJAVIMO SEKA, KURIĄ REIKALAUJA DATASHEET
void applyAllSettings() {
  // 1. Pirmiausia atidarome kanalus (slopinimas 0), kad mikroschema neužsiblokuotų
  setBalance(currentAttenLeft, currentAttenRight);
  delay(10);
  
  // 2. Pasirenkame įėjimą
  setInput(currentInput);
  delay(10);
  
  // 3. Nustatome įėjimo jautrumą
  setInputGain(currentGain);
  delay(10);
  
  // 4. Nustatome pagrindinį garsumą
  setVolume(currentVolume);
  delay(10);
  
  // 5. Nustatome tembrus
  setBass(currentBass);
  setMiddle(currentMiddle);
  setTreble(currentTreble);
}

// --- KOMANDŲ APDOROJIMO LOGIKA ---
void processCommand(String cmd) {
  if (cmd == "STATUS") {
    printStatus();
    return;
  }
  
  if (cmd.startsWith("IN")) {
    int val = cmd.substring(2).toInt();
    if (val >= 1 && val <= 4) {
      currentInput = val;
      setInput(currentInput);
      Serial.print(F("Iejimas pakeistas i: ")); Serial.println(currentInput);
    } else {
      Serial.println(F("Klaida: Iejimas nuo 1 iki 4"));
    }
  } 
  else if (cmd.startsWith("GAIN")) {
    int val = cmd.substring(4).toInt();
    if (val >= 0 && val <= 30) {
      currentGain = val;
      setInputGain(currentGain);
      Serial.print(F("Gain nustatytas: ")); Serial.print(currentGain); Serial.println(F(" dB"));
    } else {
      Serial.println(F("Klaida: Gain nuo 0 iki 30 dB"));
    }
  } 
  else if (cmd.startsWith("VOL")) {
    int val = cmd.substring(3).toInt();
    if (val <= 0 && val >= -47) {
      currentVolume = val;
      setVolume(currentVolume);
      Serial.print(F("Garsumas nustatytas: ")); Serial.print(currentVolume); Serial.println(F(" dB"));
    } else {
      Serial.println(F("Klaida: Volume nuo 0 iki -47 dB"));
    }
  } 
  else if (cmd.startsWith("BASS")) {
    int val = cmd.substring(4).toInt();
    if (val >= -14 && val <= 14) {
      currentBass = val;
      setBass(currentBass);
      Serial.print(F("Zemi dazniai: ")); Serial.print(currentBass); Serial.println(F(" dB"));
    } else {
      Serial.println(F("Klaida: Bass nuo -14 iki +14 dB"));
    }
  } 
  else if (cmd.startsWith("MID")) {
    int val = cmd.substring(3).toInt();
    if (val >= -14 && val <= 14) {
      currentMiddle = val;
      setMiddle(currentMiddle);
      Serial.print(F("Vidutiniai dazniai: ")); Serial.print(currentMiddle); Serial.println(F(" dB"));
    } else {
      Serial.println(F("Klaida: Middle nuo -14 iki +14 dB"));
    }
  } 
  else if (cmd.startsWith("TREB")) {
    int val = cmd.substring(4).toInt();
    if (val >= -14 && val <= 14) {
      currentTreble = val;
      setTreble(currentTreble);
      Serial.print(F("Auksti dazniai: ")); Serial.print(currentTreble); Serial.println(F(" dB"));
    } else {
      Serial.println(F("Klaida: Treble nuo -14 iki +14 dB"));
    }
  } 
  else if (cmd.startsWith("BAL")) {
    int firstSpace = cmd.indexOf(' ');
    int secondSpace = cmd.indexOf(' ', firstSpace + 1);
    if (firstSpace != -1 && secondSpace != -1) {
      int left = cmd.substring(firstSpace + 1, secondSpace).toInt();
      int right = cmd.substring(secondSpace + 1).toInt();
      if (left >= 0 && left <= 79 && right >= 0 && right <= 79) {
        currentAttenLeft = left;
        currentAttenRight = right;
        setBalance(currentAttenLeft, currentAttenRight);
        Serial.print(F("Slopinimas K/D: -")); Serial.print(currentAttenLeft);
        Serial.print(F(" dB / -")); Serial.print(currentAttenRight); Serial.println(F(" dB"));
      }
    }
  } 
  else if (cmd.startsWith("MUTE")) {
    int val = cmd.substring(4).toInt();
    isMuted = (val == 1);
    setMute(isMuted);
    Serial.print(F("Mute busena: ")); Serial.println(isMuted ? F("IJUNGTA") : F("ISJUNGTA"));
  }
}

// --- ŽEMO LYGIO VALDYMAS ---

void setInput(uint8_t input) {
  uint8_t val = 4 - input; 
  writeRegister(REG_INPUT_SELECT, val);
}

void setInputGain(uint8_t gainDb) {
  uint8_t val = gainDb / 2;
  writeRegister(REG_INPUT_GAIN, val);
}

void setVolume(int8_t volDb) {
  uint8_t val = abs(volDb);
  writeRegister(REG_VOLUME, val);
}

void setBass(int8_t db) {
  writeRegister(REG_BASS, convertEqualizer(db));
}

void setMiddle(int8_t db) {
  writeRegister(REG_MIDDLE, convertEqualizer(db));
}

void setTreble(int8_t db) {
  writeRegister(REG_TREBLE, convertEqualizer(db));
}

void setBalance(uint8_t attLeft, uint8_t attRight) {
  writeRegister(REG_ATTEN_LEFT, attLeft);
  writeRegister(REG_ATTEN_RIGHT, attRight);
}

void setMute(boolean state) {
  if (state) {
    writeRegister(REG_ATTEN_LEFT, 0x5F);  
    writeRegister(REG_ATTEN_RIGHT, 0x5F); 
  } else {
    setBalance(currentAttenLeft, currentAttenRight); 
  }
}

uint8_t convertEqualizer(int8_t db) {
  uint8_t steps = abs(db) / 2;
  if (db >= 0) {
    return 14 - steps; 
  } else {
    return 7 - steps;  
  }
}

void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(TDA7439_ADDRESS);
  Wire.write(reg);   
  Wire.write(value); 
  Wire.endTransmission();
}

void printStatus() {
  Serial.println(F("\n------ ESAMA TDA7439 BUSENA ------"));
  Serial.print(F(" Pasirinktas Iejimas : ")); Serial.println(currentInput);
  Serial.print(F(" Iejimo Gain        : ")); Serial.print(currentGain); Serial.println(F(" dB"));
  Serial.print(F(" Garsumas           : ")); Serial.print(currentVolume); Serial.println(F(" dB"));
  Serial.print(F(" Tembrai (B/M/T)    : ")); Serial.print(currentBass); Serial.print(F(" / ")); Serial.print(currentMiddle); Serial.print(F(" / ")); Serial.print(currentTreble); Serial.println(F(" dB"));
  Serial.print(F(" Slopinimas (K/D)   : -")); Serial.print(currentAttenLeft); Serial.print(F(" dB / -")); Serial.print(currentAttenRight); Serial.println(F(" dB"));
  Serial.print(F(" Mute funkcija      : ")); Serial.println(isMuted ? F("IJUNGTA") : F("ISJUNGTA"));
  Serial.println(F("----------------------------------\n"));
}
