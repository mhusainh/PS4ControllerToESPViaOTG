#include "EspUsbHost.h"
#include "usb/usb_host.h"

class GamepadHost : public EspUsbHost {
public:
  bool isReady = false;
  usb_transfer_t *inTransfer = NULL;
  usb_transfer_t *outTransfer = NULL;
  uint8_t inReportLen = 64;
  uint8_t claimedInterface = 0xFF;

  void cleanup() {
    if (inTransfer) { usb_host_transfer_free(inTransfer); inTransfer = NULL; }
    if (outTransfer) { usb_host_transfer_free(outTransfer); outTransfer = NULL; }
    if (claimedInterface != 0xFF && deviceHandle) {
      usb_host_interface_release(clientHandle, deviceHandle, claimedInterface);
      claimedInterface = 0xFF;
    }
    if (deviceHandle) { usb_host_device_close(clientHandle, deviceHandle); deviceHandle = NULL; }
    isReady = false;
  }

  void onDisconnect(void) override {
    Serial.println("Device disconnected, cleaning up...");
    cleanup();
    Serial.println("Waiting for new device...");
  }

  void sendPS5Activation() {
    if (!outTransfer) return;
    Serial.println("Sending PS5 activation...");

    uint8_t activate_report[64] = {0};
    activate_report[0] = 0x02;
    activate_report[1] = 0xFF;
    activate_report[2] = 0x00;
    activate_report[3] = 0x01;

    outTransfer->num_bytes = 64;
    memcpy(outTransfer->data_buffer, activate_report, 64);
    esp_err_t err = usb_host_transfer_submit(outTransfer);
    if (err == ESP_OK) {
      Serial.println("PS5 activation sent!");
    } else {
      Serial.printf("Activation failed: 0x%x\n", err);
    }
  }

  void onConfig(const uint8_t bDescriptorType, const uint8_t *p) override {
    switch (bDescriptorType) {
      case USB_B_DESCRIPTOR_TYPE_INTERFACE: {
        const usb_intf_desc_t *intf = (const usb_intf_desc_t *)p;
        Serial.printf("IF: cls=0x%02x sub=0x%02x proto=0x%02x num=%d\n",
                      intf->bInterfaceClass, intf->bInterfaceSubClass, intf->bInterfaceProtocol,
                      intf->bInterfaceNumber);
        esp_err_t err = usb_host_interface_claim(this->clientHandle, this->deviceHandle,
          intf->bInterfaceNumber, intf->bAlternateSetting);
        if (err == ESP_OK) {
          claimedInterface = intf->bInterfaceNumber;
        }
        break;
      }
      case USB_B_DESCRIPTOR_TYPE_ENDPOINT: {
        const usb_ep_desc_t *ep = (const usb_ep_desc_t *)p;
        bool isIn = ep->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK;
        bool isInt = (ep->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_INT;

        if (!isInt) return;

        if (isIn && inTransfer == NULL) {
          inReportLen = ep->wMaxPacketSize;
          if (usb_host_transfer_alloc(inReportLen, 0, &this->inTransfer) == ESP_OK) {
            this->inTransfer->device_handle = this->deviceHandle;
            this->inTransfer->bEndpointAddress = ep->bEndpointAddress;
            this->inTransfer->callback = this->_onData;
            this->inTransfer->context = this;
            Serial.printf("IN  EP 0x%02x %dms %d bytes\n", ep->bEndpointAddress, ep->bInterval, inReportLen);
          }
        } else if (!isIn && outTransfer == NULL) {
          uint8_t outLen = ep->wMaxPacketSize;
          if (usb_host_transfer_alloc(outLen, 0, &this->outTransfer) == ESP_OK) {
            this->outTransfer->device_handle = this->deviceHandle;
            this->outTransfer->bEndpointAddress = ep->bEndpointAddress;
            this->outTransfer->callback = this->_onOutDone;
            this->outTransfer->context = this;
            Serial.printf("OUT EP 0x%02x %dms %d bytes\n", ep->bEndpointAddress, ep->bInterval, outLen);
          }
        }

        if (inTransfer && outTransfer && !isReady) {
          isReady = true;
          Serial.println("Both endpoints ready!");
          sendPS5Activation();
          delay(100);
          inTransfer->num_bytes = inReportLen;
          usb_host_transfer_submit(inTransfer);
        }
        break;
      }
    }
  }

  static void _onOutDone(usb_transfer_t *transfer) {
    Serial.println("Out transfer done");
  }

  static void _onData(usb_transfer_t *transfer) {
    GamepadHost *h = (GamepadHost *)transfer->context;
    if (transfer->status != 0) {
      Serial.printf("ERR status=%d\n", transfer->status);
      if (h->inTransfer) {
        h->inTransfer->num_bytes = h->inReportLen;
        usb_host_transfer_submit(h->inTransfer);
      }
      return;
    }
    int len = transfer->actual_num_bytes;
    uint8_t *d = transfer->data_buffer;

    Serial.printf("[%d] ", len);
    for (int i = 0; i < len && i < 64; i++) {
      Serial.printf("%02x ", d[i]);
    }
    Serial.println();

    if (h->inTransfer) {
      h->inTransfer->num_bytes = h->inReportLen;
      usb_host_transfer_submit(h->inTransfer);
    }
  }

  void task(void) {
    EspUsbHost::task();
  }
};

GamepadHost usb;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("ESP32-S3 USB Host - PS5 DualSense");
  Serial.println("Colok PS5 sekarang...");
  usb.begin();
}

void loop() {
  usb.task();
}
