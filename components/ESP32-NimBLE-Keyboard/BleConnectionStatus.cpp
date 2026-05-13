#include "BleConnectionStatus.h"

BleConnectionStatus::BleConnectionStatus(void) {
}

void BleConnectionStatus::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo)
{
  this->connected = true;
}

void BleConnectionStatus::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason)
{
  this->connected = false;
}
