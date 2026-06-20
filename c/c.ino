#include <ESP32-USB-Soft-Host.h>

#define DP_P0  16
#define DM_P0  17
#define LED    2

usb_pins_config_t USB_Pins_Config = {
  DP_P0, DM_P0,
  -1, -1, -1, -1, -1, -1
};

struct GamepadState {
  int8_t lx, ly, rx, ry;
  uint8_t hat;
  uint16_t buttons;
} gp;

static void onDetect(uint8_t usbNum, void *dev) {
  sDevDesc *d = (sDevDesc*)dev;
  Serial.printf("Connected VID:0x%04x PID:0x%04x\n", d->idVendor, d->idProduct);
}

static void onData(uint8_t usbNum, uint8_t byte_depth, uint8_t* data, uint8_t len) {
  if (len < 6) return;

  gp.buttons = data[0];
  gp.lx = data[1] - 128;
  gp.ly = data[2] - 128;
  gp.rx = data[3] - 128;
  gp.ry = data[4] - 128;
  gp.hat = data[5] & 0x0F;

  bool btnA      = gp.buttons & 0x01;
  bool btnB      = gp.buttons & 0x02;
  bool btnX      = gp.buttons & 0x04;
  bool btnY      = gp.buttons & 0x08;
  bool btnLB     = gp.buttons & 0x10;
  bool btnRB     = gp.buttons & 0x20;
  bool btnBack   = gp.buttons & 0x40;
  bool btnStart  = gp.buttons & 0x80;

  Serial.printf("Stick L(%4d,%4d) R(%4d,%4d) Hat:%d Buttons:%c%c%c%c%c%c%c%c\n",
    gp.lx, gp.ly, gp.rx, gp.ry, gp.hat,
    btnA     ? 'A' : '-',
    btnB     ? 'B' : '-',
    btnX     ? 'X' : '-',
    btnY     ? 'Y' : '-',
    btnLB    ? 'L' : '-',
    btnRB    ? 'R' : '-',
    btnBack  ? 'B' : '-',
    btnStart ? 'S' : '-'
  );
}

void setup() {
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  delay(3000);
  Serial.println("USB Gamepad - ESP32");

  USH.setOnConfigDescCB(Default_USB_ConfigDescCB);
  USH.setOnIfaceDescCb(Default_USB_IfaceDescCb);
  USH.setOnHIDDevDescCb(Default_USB_HIDDevDescCb);
  USH.setOnEPDescCb(Default_USB_EPDescCb);

  USH.init(USB_Pins_Config, onDetect, onData);
}

void loop() {
}
