/* MHZ library

    By Tobias Schürg
*/

#include "MHZ.h"
#include "HardwareSerial.h"

const int MHZ14A = 14;
const int MHZ19B = 19;

const int MHZ14A_RESPONSE_TIME = 60;
const int MHZ19B_RESPONSE_TIME = 120;

const int STATUS_NO_RESPONSE = -2;
const int STATUS_CHECKSUM_MISMATCH = -3;
const int STATUS_INCOMPLETE = -4;
const int STATUS_NOT_READY = -5;

unsigned long lastRequest = 0;

// MHZ::MHZ(uint8_t rxpin, uint8_t txpin, uint8_t pwmpin, uint8_t type) {
//   SoftwareSerial * ss = new SoftwareSerial(rxpin, txpin);
//   _pwmpin = pwmpin;
//   _type = type;

//   ss->begin(9600);
//   _serial = ss;
// }

MHZ::MHZ(HardwareSerial * serial, uint8_t pwmpin, uint8_t type) {
    _serial = serial;
    _pwmpin = pwmpin;
    _type = type;

    _serial->begin(9600);
}

/**
 * Enables or disables the debug mode (more logging).
 */
void MHZ::setDebug(boolean enable) {
  debug = enable;
  if (debug) {
    Serial.println(F("MHZ: debug mode ENABLED"));
  } else {
    Serial.println(F("MHZ: debug mode DISABLED"));
  }
}

boolean MHZ::isPreHeating() {
  if (_type == MHZ14A) {
    return millis() < (3 * 60 * 1000);
  } else if (_type == MHZ19B) {
    return millis() < (3 * 60 * 1000);
  } else {
    Serial.println(F("MHZ::isPreHeating() => UNKNOWN SENSOR"));
    return false;
  }
}

boolean MHZ::isReady() {
  if (isPreHeating()) return false;
  if (_type == MHZ14A)
    return lastRequest < millis() - MHZ14A_RESPONSE_TIME;
  else if (_type == MHZ19B)
    return lastRequest < millis() - MHZ19B_RESPONSE_TIME;
  else {
    Serial.print(F("MHZ::isReady() => UNKNOWN SENSOR \""));
    Serial.print(_type);
    Serial.println(F("\""));
    return true;
  }
}

int MHZ::readCO2UART() {
  //if (!isReady()) return STATUS_NOT_READY;
  debug = true;
  if (debug) Serial.println(F("-- read CO2 uart ---"));
  uint8_t cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  uint8_t response[9];  // for answer

  if (debug) Serial.print(F("  >> Sending CO2 request"));
  Serial2.write(cmd, 9);  // request PPM CO2
  lastRequest = millis();

  // clear the buffer
  memset(response, 0, 9);

  int waited = 0;
  while (_serial->available() == 0) {
    if (debug) Serial.print(".");
    delay(100);  // wait a short moment to avoid false reading
    if (waited++ > 10) {
      if (debug) Serial.println(F("No response after 10 seconds"));
      _serial->flush();
      return STATUS_NO_RESPONSE;
    }
  }
  if (debug) Serial.println();

  // The serial stream can get out of sync. The response starts with 0xff, try
  // to resync.
  // TODO: I think this might be wrong any only happens during initialization?
  boolean skip = false;
  while (_serial->available() > 0 && (unsigned char)_serial->peek() != 0xFF) {
    if (!skip) {
      Serial.print(F("MHZ: - skipping unexpected readings:"));
      skip = true;
    }
    Serial.print(" ");
    Serial.print(_serial->peek(), HEX);
    _serial->read();
  }
  if (skip) Serial.println();

  if (_serial->available() > 0) {
    int count = _serial->readBytes(response, 9);
    if (count < 9) {
      _serial->flush();
      return STATUS_INCOMPLETE;
    }
  } else {
    _serial->flush();
    return STATUS_INCOMPLETE;
  }

  if (debug) {
    // print out the response in hexa
    Serial.print(F("  << "));
    for (int i = 0; i < 9; i++) {
      Serial.print(response[i], HEX);
      Serial.print(F("  "));
    }
    Serial.println(F(""));
  }

  // checksum
  byte check = getCheckSum(response);
  if (response[8] != check) {
    Serial.println(F("MHZ: Checksum not OK!"));
    Serial.print(F("MHZ: Received: "));
    Serial.println(response[8], HEX);
    Serial.print(F("MHZ: Should be: "));
    Serial.println(check, HEX);
    temperature = STATUS_CHECKSUM_MISMATCH;
    _serial->flush();
    return STATUS_CHECKSUM_MISMATCH;
  }

  int ppm_uart = 256 * (int)response[2] + response[3];

  temperature = response[4] - 44;  // - 40;

  byte status = response[5];
  if (debug) {
    Serial.print(F(" # PPM UART: "));
    Serial.println(ppm_uart);
    Serial.print(F(" # Temperature? "));
    Serial.println(temperature);
  }

  // Is always 0 for version 14a  and 19b
  // Version 19a?: status != 0x40
  if (debug && status != 0) {
    Serial.print(F(" ! Status maybe not OK ! "));
    Serial.println(status, HEX);
  } else if (debug) {
    Serial.print(F(" Status  OK: "));
    Serial.println(status, HEX);
  }

  _serial->flush();
  return ppm_uart;
}

uint8_t MHZ::getLastTemperature() {
  //if (isPreHeating()) return STATUS_NOT_READY;
  return temperature;
}

byte MHZ::getCheckSum(byte* packet) {
  if (debug) Serial.println(F("  getCheckSum()"));
  byte i;
  unsigned char checksum = 0;
  for (i = 1; i < 8; i++) {
    checksum += packet[i];
  }
  checksum = 0xff - checksum;
  checksum += 1;
  return checksum;
}

int MHZ::readCO2PWM() {
  // if (!isReady()) return STATUS_NOT_READY; not needed?
  if (debug) Serial.print(F("-- reading CO2 from pwm "));
  unsigned long th, tl, ppm_pwm = 0;
  do {
    if (debug) Serial.print(".");
    th = pulseIn(_pwmpin, HIGH, 1004000) / 1000;
    tl = 1004 - th;
    ppm_pwm = 5000 * (th - 2) / (th + tl - 4);
  } while (th == 0);
  if (debug) {
    Serial.print(F("\n # PPM PWM: "));
    Serial.println(ppm_pwm);
  }
  return ppm_pwm;
}
