#include <NimBLEDevice.h>
#include <NimBLEUtils.h>
#include <NimBLEServer.h>
#include "NimBLEHIDDevice.h"
#include "HIDTypes.h"
#include "sdkconfig.h"

#include "BleConnectionStatus.h"
#include "KeyboardOutputCallbacks.h"
#include "BleKeyboard.h"

#if defined(CONFIG_ARDUHAL_ESP_LOG)
  #include "esp32-hal-log.h"
  #define LOG_TAG ""
#else
  #include "esp_log.h"
  static const char* LOG_TAG = "NimBLEDevice";
#endif


// Report IDs:
#define KEYBOARD_ID 0x01
#define MEDIA_KEYS_ID 0x02

static const uint8_t _hidReportDescriptor[] = {
  USAGE_PAGE(1),      0x01,          // USAGE_PAGE (Generic Desktop Ctrls)
  USAGE(1),           0x06,          // USAGE (Keyboard)
  COLLECTION(1),      0x01,          // COLLECTION (Application)
  // ------------------------------------------------- Keyboard
  REPORT_ID(1),       KEYBOARD_ID,   //   REPORT_ID (1)
  USAGE_PAGE(1),      0x07,          //   USAGE_PAGE (Kbrd/Keypad)
  USAGE_MINIMUM(1),   0xE0,          //   USAGE_MINIMUM (0xE0)
  USAGE_MAXIMUM(1),   0xE7,          //   USAGE_MAXIMUM (0xE7)
  LOGICAL_MINIMUM(1), 0x00,          //   LOGICAL_MINIMUM (0)
  LOGICAL_MAXIMUM(1), 0x01,          //   Logical Maximum (1)
  REPORT_SIZE(1),     0x01,          //   REPORT_SIZE (1)
  REPORT_COUNT(1),    0x08,          //   REPORT_COUNT (8)
  HIDINPUT(1),        0x02,          //   INPUT (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  REPORT_COUNT(1),    0x01,          //   REPORT_COUNT (1) ; 1 byte (Reserved)
  REPORT_SIZE(1),     0x08,          //   REPORT_SIZE (8)
  HIDINPUT(1),        0x01,          //   INPUT (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
  REPORT_COUNT(1),    0x05,          //   REPORT_COUNT (5) ; 5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
  REPORT_SIZE(1),     0x01,          //   REPORT_SIZE (1)
  USAGE_PAGE(1),      0x08,          //   USAGE_PAGE (LEDs)
  USAGE_MINIMUM(1),   0x01,          //   USAGE_MINIMUM (0x01) ; Num Lock
  USAGE_MAXIMUM(1),   0x05,          //   USAGE_MAXIMUM (0x05) ; Kana
  HIDOUTPUT(1),       0x02,          //   OUTPUT (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
  REPORT_COUNT(1),    0x01,          //   REPORT_COUNT (1) ; 3 bits (Padding)
  REPORT_SIZE(1),     0x03,          //   REPORT_SIZE (3)
  HIDOUTPUT(1),       0x01,          //   OUTPUT (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
  REPORT_COUNT(1),    0x06,          //   REPORT_COUNT (6) ; 6 bytes (Keys)
  REPORT_SIZE(1),     0x08,          //   REPORT_SIZE(8)
  LOGICAL_MINIMUM(1), 0x00,          //   LOGICAL_MINIMUM(0)
  LOGICAL_MAXIMUM(1), 0x65,          //   LOGICAL_MAXIMUM(0x65) ; 101 keys
  USAGE_PAGE(1),      0x07,          //   USAGE_PAGE (Kbrd/Keypad)
  USAGE_MINIMUM(1),   0x00,          //   USAGE_MINIMUM (0)
  USAGE_MAXIMUM(1),   0x65,          //   USAGE_MAXIMUM (0x65)
  HIDINPUT(1),        0x00,          //   INPUT (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
  END_COLLECTION(0),                 // END_COLLECTION
  // ------------------------------------------------- Media Keys
  USAGE_PAGE(1),      0x0C,          // USAGE_PAGE (Consumer)
  USAGE(1),           0x01,          // USAGE (Consumer Control)
  COLLECTION(1),      0x01,          // COLLECTION (Application)
  REPORT_ID(1),       MEDIA_KEYS_ID, //   REPORT_ID (3)
  USAGE_PAGE(1),      0x0C,          //   USAGE_PAGE (Consumer)
  LOGICAL_MINIMUM(1), 0x00,          //   LOGICAL_MINIMUM (0)
  LOGICAL_MAXIMUM(1), 0x01,          //   LOGICAL_MAXIMUM (1)
  REPORT_SIZE(1),     0x01,          //   REPORT_SIZE (1)
  REPORT_COUNT(1),    0x10,          //   REPORT_COUNT (16)
  USAGE(1),           0xB5,          //   USAGE (Scan Next Track)     ; bit 0: 1
  USAGE(1),           0xB6,          //   USAGE (Scan Previous Track) ; bit 1: 2
  USAGE(1),           0xB7,          //   USAGE (Stop)                ; bit 2: 4
  USAGE(1),           0xCD,          //   USAGE (Play/Pause)          ; bit 3: 8
  USAGE(1),           0xE2,          //   USAGE (Mute)                ; bit 4: 16
  USAGE(1),           0xE9,          //   USAGE (Volume Increment)    ; bit 5: 32
  USAGE(1),           0xEA,          //   USAGE (Volume Decrement)    ; bit 6: 64
  USAGE(2),           0x23, 0x02,    //   Usage (WWW Home)            ; bit 7: 128
  USAGE(2),           0x94, 0x01,    //   Usage (My Computer) ; bit 0: 1
  USAGE(2),           0x92, 0x01,    //   Usage (Calculator)  ; bit 1: 2
  USAGE(2),           0x2A, 0x02,    //   Usage (WWW fav)     ; bit 2: 4
  USAGE(2),           0x21, 0x02,    //   Usage (WWW search)  ; bit 3: 8
  USAGE(2),           0x26, 0x02,    //   Usage (WWW stop)    ; bit 4: 16
  USAGE(2),           0x24, 0x02,    //   Usage (WWW back)    ; bit 5: 32
  USAGE(2),           0x83, 0x01,    //   Usage (Media sel)   ; bit 6: 64
  USAGE(2),           0x8A, 0x01,    //   Usage (Mail)        ; bit 7: 128
  HIDINPUT(1),        0x02,          //   INPUT (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  END_COLLECTION(0)                 // END_COLLECTION
};

static const uint8_t _hidDataReportDescriptor[] = {
  // ------------------------------------------------- Custom
  USAGE_PAGE(2),      0x60, 0xFF,    // USAGE_PAGE (Vendor defined)
  USAGE(1),           0x61,          // Usage (0x61)
  COLLECTION(1),      0x01,          // COLLECTION (Application)
  USAGE(1),           0x62,          // Usage (0x62)
  LOGICAL_MINIMUM(1), 0x00,          //   LOGICAL_MINIMUM (0)
  LOGICAL_MAXIMUM(2), 0xFF, 0x00,    //   LOGICAL_MAXIMUM (255)
  REPORT_COUNT(1),    0x20,          //   REPORT_COUNT (32)
  REPORT_SIZE(1),     0x08,          //   REPORT_SIZE (8)
  HIDINPUT(1),        0x02,          //   INPUT (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  USAGE(1),           0x63,          // Usage (0x63)
  LOGICAL_MINIMUM(1), 0x00,          //   LOGICAL_MINIMUM (0)
  LOGICAL_MAXIMUM(2), 0xFF, 0x00,    //   LOGICAL_MAXIMUM (255)
  REPORT_COUNT(1),    0x20,          //   REPORT_COUNT (32)
  REPORT_SIZE(1),     0x08,          //   REPORT_SIZE (8)
  HIDOUTPUT(1),       0x02,          //   INPUT (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  END_COLLECTION(0)                  // END_COLLECTION
};

BleKeyboard::BleKeyboard(std::string deviceName,
        std::string deviceManufacturer,
        uint8_t batteryLevel,
        NimBLECharacteristicCallbacks *dataCallbacks) : hid(0)
{
  this->deviceName = deviceName;
  this->deviceManufacturer = deviceManufacturer;
  this->batteryLevel = batteryLevel;
  this->connectionStatus = new BleConnectionStatus();
  this->clearAll();
  this->dataCallbacks = dataCallbacks;
}

void BleKeyboard::begin(void)
{
  xTaskCreate(this->taskServer, "server", 20000, (void *)this, 5, NULL);
}

void BleKeyboard::end(void)
{
}

bool BleKeyboard::isConnected(void) {
  return this->connectionStatus->connected;
}

void BleKeyboard::setBatteryLevel(uint8_t level) {
  this->batteryLevel = level;
  if (hid != 0)
    this->hid->setBatteryLevel(this->batteryLevel);
}

void BleKeyboard::taskServer(void* pvParameter) {
  BleKeyboard* bleKeyboardInstance = (BleKeyboard *) pvParameter; //static_cast<BleKeyboard *>(pvParameter);
  NimBLEDevice::init(bleKeyboardInstance->deviceName);
  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(bleKeyboardInstance->connectionStatus);

  NimBLEHIDDevice * hid = new NimBLEHIDDevice(pServer);
  NimBLEHIDDevice * dataHid = new NimBLEHIDDevice(pServer);

  bleKeyboardInstance->hid = hid;
  bleKeyboardInstance->dataHid = dataHid;
  bleKeyboardInstance->inputKeyboard = hid->getInputReport(KEYBOARD_ID); // <-- input REPORTID from report map
  bleKeyboardInstance->outputKeyboard = hid->getOutputReport(KEYBOARD_ID);
  bleKeyboardInstance->inputMediaKeys = hid->getInputReport(MEDIA_KEYS_ID);
  bleKeyboardInstance->inputData = dataHid->getInputReport(0);
  bleKeyboardInstance->outputData = dataHid->getOutputReport(0);

  bleKeyboardInstance->connectionStatus->inputKeyboard = bleKeyboardInstance->inputKeyboard;
  bleKeyboardInstance->connectionStatus->outputKeyboard = bleKeyboardInstance->outputKeyboard;
  bleKeyboardInstance->connectionStatus->inputMediaKeys = bleKeyboardInstance->inputMediaKeys;
  bleKeyboardInstance->connectionStatus->inputData = bleKeyboardInstance->inputData;
  bleKeyboardInstance->connectionStatus->outputData = bleKeyboardInstance->outputData;

  bleKeyboardInstance->outputKeyboard->setCallbacks(new KeyboardOutputCallbacks());
  bleKeyboardInstance->outputData->setCallbacks(bleKeyboardInstance->dataCallbacks);
  bleKeyboardInstance->inputData->setCallbacks(bleKeyboardInstance->dataCallbacks);

  hid->setManufacturer(bleKeyboardInstance->deviceManufacturer);
  dataHid->setManufacturer(bleKeyboardInstance->deviceManufacturer);

  hid->setPnp(0x02, 0xe502, 0xa111, 0x0210);
  hid->setHidInfo(0x00,0x01);
  dataHid->setPnp(0x02, 0xe502, 0xa111, 0x0210);
  dataHid->setHidInfo(0x00,0x01);

  NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND);

  hid->setReportMap((uint8_t*)_hidReportDescriptor, sizeof(_hidReportDescriptor));
  dataHid->setReportMap((uint8_t*)_hidDataReportDescriptor, sizeof(_hidDataReportDescriptor));
  hid->startServices();
  dataHid->startServices();

  bleKeyboardInstance->onStarted(pServer);

  NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_KEYBOARD);
  pAdvertising->addServiceUUID(hid->getHidService()->getUUID());
  pAdvertising->start();
  bleKeyboardInstance->hid->setBatteryLevel(bleKeyboardInstance->batteryLevel);

  ESP_LOGD(LOG_TAG, "Advertising started!");
  vTaskDelay(portMAX_DELAY); //delay(portMAX_DELAY);
}

void BleKeyboard::sendReport(KeyReport* keys)
{
  if (this->isConnected())
  {
    this->inputKeyboard->setValue((uint8_t*)keys, sizeof(KeyReport));
    this->inputKeyboard->notify();
  }
}

void BleKeyboard::sendReport(MediaKeyReport* keys)
{
  if (this->isConnected())
  {
    this->inputMediaKeys->setValue((uint8_t*)keys, sizeof(MediaKeyReport));
    this->inputMediaKeys->notify();
  }
}

#define SHIFT 0x80
const uint8_t _asciimap[128] =
{
	0x00,             // NUL
	0x00,             // SOH
	0x00,             // STX
	0x00,             // ETX
	0x00,             // EOT
	0x00,             // ENQ
	0x00,             // ACK
	0x00,             // BEL
	0x2a,			// BS	Backspace
	0x2b,			// TAB	Tab
	0x28,			// LF	Enter
	0x00,             // VT
	0x00,             // FF
	0x00,             // CR
	0x00,             // SO
	0x00,             // SI
	0x00,             // DEL
	0x00,             // DC1
	0x00,             // DC2
	0x00,             // DC3
	0x00,             // DC4
	0x00,             // NAK
	0x00,             // SYN
	0x00,             // ETB
	0x00,             // CAN
	0x00,             // EM
	0x00,             // SUB
	0x00,             // ESC
	0x00,             // FS
	0x00,             // GS
	0x00,             // RS
	0x00,             // US

	0x2c,		   //  ' '
	0x1e|SHIFT,	   // !
	0x34|SHIFT,	   // "
	0x20|SHIFT,    // #
	0x21|SHIFT,    // $
	0x22|SHIFT,    // %
	0x24|SHIFT,    // &
	0x34,          // '
	0x26|SHIFT,    // (
	0x27|SHIFT,    // )
	0x25|SHIFT,    // *
	0x2e|SHIFT,    // +
	0x36,          // ,
	0x2d,          // -
	0x37,          // .
	0x38,          // /
	0x27,          // 0
	0x1e,          // 1
	0x1f,          // 2
	0x20,          // 3
	0x21,          // 4
	0x22,          // 5
	0x23,          // 6
	0x24,          // 7
	0x25,          // 8
	0x26,          // 9
	0x33|SHIFT,      // :
	0x33,          // ;
	0x36|SHIFT,      // <
	0x2e,          // =
	0x37|SHIFT,      // >
	0x38|SHIFT,      // ?
	0x1f|SHIFT,      // @
	0x04|SHIFT,      // A
	0x05|SHIFT,      // B
	0x06|SHIFT,      // C
	0x07|SHIFT,      // D
	0x08|SHIFT,      // E
	0x09|SHIFT,      // F
	0x0a|SHIFT,      // G
	0x0b|SHIFT,      // H
	0x0c|SHIFT,      // I
	0x0d|SHIFT,      // J
	0x0e|SHIFT,      // K
	0x0f|SHIFT,      // L
	0x10|SHIFT,      // M
	0x11|SHIFT,      // N
	0x12|SHIFT,      // O
	0x13|SHIFT,      // P
	0x14|SHIFT,      // Q
	0x15|SHIFT,      // R
	0x16|SHIFT,      // S
	0x17|SHIFT,      // T
	0x18|SHIFT,      // U
	0x19|SHIFT,      // V
	0x1a|SHIFT,      // W
	0x1b|SHIFT,      // X
	0x1c|SHIFT,      // Y
	0x1d|SHIFT,      // Z
	0x2f,          // [
	0x31,          // bslash
	0x30,          // ]
	0x23|SHIFT,    // ^
	0x2d|SHIFT,    // _
	0x35,          // `
	0x04,          // a
	0x05,          // b
	0x06,          // c
	0x07,          // d
	0x08,          // e
	0x09,          // f
	0x0a,          // g
	0x0b,          // h
	0x0c,          // i
	0x0d,          // j
	0x0e,          // k
	0x0f,          // l
	0x10,          // m
	0x11,          // n
	0x12,          // o
	0x13,          // p
	0x14,          // q
	0x15,          // r
	0x16,          // s
	0x17,          // t
	0x18,          // u
	0x19,          // v
	0x1a,          // w
	0x1b,          // x
	0x1c,          // y
	0x1d,          // z
	0x2f|SHIFT,    // {
	0x31|SHIFT,    // |
	0x30|SHIFT,    // }
	0x35|SHIFT,    // ~
	0				// DEL
};


uint8_t USBPutChar(uint8_t c);

// press() adds the specified key (printing, non-printing, or modifier)
// to the persistent key report and sends the report.  Because of the way
// USB HID works, the host acts like the key remains pressed until we
// call release(), releaseAll(), or otherwise clear the report and resend.
size_t BleKeyboard::press(uint8_t k)
{
	uint8_t i;
        uint8_t pressed_key = 0;
	if (k >= 136) {			// it's a non-printing key (not a modifier)
		pressed_key = k - 136;
	} else if (k >= 128) {	// it's a modifier key
		_keyReport.modifiers |= (1<<(k-128));
		pressed_key = 0;
	} else {				// it's a printing key
		pressed_key = _asciimap[k];
                printf("pressed %d -> %d???\n", k, pressed_key);
		if (!pressed_key) {
                        printf("press: not in the ascii map!\n");
			return 0;
		}
		if (pressed_key & 0x80) {						// it's a capital letter or other character reached with shift
			_keyReport.modifiers |= 0x02;	// the left shift modifier
			pressed_key &= 0x7F;
		}
	}

	// Add k to the key report only if it's not already present
	// and if there is an empty slot.
	if (_keyReport.keys[0] != pressed_key && _keyReport.keys[1] != pressed_key &&
		_keyReport.keys[2] != pressed_key && _keyReport.keys[3] != pressed_key &&
		_keyReport.keys[4] != pressed_key && _keyReport.keys[5] != pressed_key) {

		for (i=0; i<6; i++) {
			if (_keyReport.keys[i] == 0x00) {
				_keyReport.keys[i] = pressed_key;
				break;
			}
		}
		if (i == 6) {
                        printf("no empty wtf???\n");
			return 0;
		}
		for (i=0; i<6; i++) {
                    printf("reporting keys: %d\n", _keyReport.keys[i]);
                }
	}
	sendReport(&_keyReport);
	return 1;
}

size_t BleKeyboard::press(const MediaKeyReport k)
{
    uint16_t k_16 = k[1] | (k[0] << 8);
    uint16_t mediaKeyReport_16 = _mediaKeyReport[1] | (_mediaKeyReport[0] << 8);

    mediaKeyReport_16 |= k_16;
    _mediaKeyReport[0] = (uint8_t)((mediaKeyReport_16 & 0xFF00) >> 8);
    _mediaKeyReport[1] = (uint8_t)(mediaKeyReport_16 & 0x00FF);

	sendReport(&_mediaKeyReport);
	return 1;
}

// release() takes the specified key out of the persistent key report and
// sends the report.  This tells the OS the key is no longer pressed and that
// it shouldn't be repeated any more.
size_t BleKeyboard::release(uint8_t k)
{
	uint8_t i;
	if (k >= 136) {			// it's a non-printing key (not a modifier)
		k = k - 136;
	} else if (k >= 128) {	// it's a modifier key
		_keyReport.modifiers &= ~(1<<(k-128));
		k = 0;
	} else {				// it's a printing key
		k = _asciimap[k];
		if (!k) {
                        printf("release: not in the ascii map!\n");
			return 0;
		}
		if (k & 0x80) {							// it's a capital letter or other character reached with shift
			_keyReport.modifiers &= ~(0x02);	// the left shift modifier
			k &= 0x7F;
		}
	}

	// Test the key report to see if k is present.  Clear it if it exists.
	// Check all positions in case the key is present more than once (which it shouldn't be)
	for (i=0; i<6; i++) {
		if (0 != k && _keyReport.keys[i] == k) {
			_keyReport.keys[i] = 0x00;
		}
	}

	sendReport(&_keyReport);
	return 1;
}

size_t BleKeyboard::release(const MediaKeyReport k)
{
    uint16_t k_16 = k[1] | (k[0] << 8);
    uint16_t mediaKeyReport_16 = _mediaKeyReport[1] | (_mediaKeyReport[0] << 8);
    mediaKeyReport_16 &= ~k_16;
    _mediaKeyReport[0] = (uint8_t)((mediaKeyReport_16 & 0xFF00) >> 8);
    _mediaKeyReport[1] = (uint8_t)(mediaKeyReport_16 & 0x00FF);

	sendReport(&_mediaKeyReport);
	return 1;
}

void BleKeyboard::releaseAll(void)
{
    clearAll();
    sendReport(&_keyReport);
}

void BleKeyboard::clearAll(void)
{
    _keyReport.keys[0] = 0;
    _keyReport.keys[1] = 0;
    _keyReport.keys[2] = 0;
    _keyReport.keys[3] = 0;
    _keyReport.keys[4] = 0;
    _keyReport.keys[5] = 0;
    _keyReport.modifiers = 0;
    _mediaKeyReport[0] = 0;
    _mediaKeyReport[1] = 0;
}

size_t BleKeyboard::write(uint8_t c)
{
	uint8_t p = press(c);  // Keydown
	vTaskDelay(3);
	release(c);            // Keyup
	return p;              // just return the result of press() since release() almost always returns 1
}

size_t BleKeyboard::write(const MediaKeyReport c)
{
	uint16_t p = press(c);  // Keydown
	vTaskDelay(3);
	release(c);            // Keyup
	return p;              // just return the result of press() since release() almost always returns 1
}

size_t BleKeyboard::write(const uint8_t *buffer, size_t size) {
	size_t n = 0;
	while (size--) {
		if (*buffer != '\r') {
			if (write(*buffer)) {
			  n++;
			} else {
			  break;
			}
		}
		buffer++;
	}
	return n;
}
