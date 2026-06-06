#include "ble_bridge.h"
#include <NimBLEDevice.h>
#include <Arduino.h>
#include <string.h>

// Nordic UART Service UUID — 所有 BLE 串口示例都使用这些 UUID，
// 现有工具（nRF Connect、bluefy、Web Bluetooth 示例）无需自定义 UUID 即可通信。
#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// 接收字节缓冲在简单环形队列中，供 bleRead()/bleAvailable() 使用。
// 容量足以容纳一份转录快照 JSON 加余量；GATT 层会在我们跟不上时流控。
static const size_t RX_CAP = 2048;
static uint8_t  rxBuf[RX_CAP];
static volatile size_t rxHead = 0;
static volatile size_t rxTail = 0;

static NimBLEServer*         server = nullptr;
static NimBLECharacteristic* txChar = nullptr;
static NimBLECharacteristic* rxChar = nullptr;
static volatile bool         connected = false;
static volatile bool         secure = false;
static volatile uint32_t     passkey = 0;
static volatile uint16_t     mtu = 23;

static void rxPush(const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    size_t next = (rxHead + 1) % RX_CAP;
    if (next == rxTail) return;  // 满——丢弃（上游应跟上）
    rxBuf[rxHead] = p[i];
    rxHead = next;
  }
}

// RX 特征回调：客户端写入时将数据推入环形队列
class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& connInfo) override {
    auto v = c->getValue();
    if (v.length()) rxPush(v.data(), v.length());
  }
};

static RxCallbacks rxCb;

// 服务器回调：连接/断开/MTU/安全
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& connInfo) override {
    connected = true;
    mtu = s->getPeerMTU(connInfo.getConnHandle());
    Serial.println("[ble] connected");
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& connInfo, int reason) override {
    connected = false;
    secure = false;
    passkey = 0;
    mtu = 23;
    Serial.println("[ble] disconnected");
    // 重新开始广播，让下一个客户端能发现我们
    NimBLEDevice::startAdvertising();
  }
  void onMTUChange(uint16_t newMtu, NimBLEConnInfo& connInfo) override {
    mtu = newMtu;
    Serial.printf("[ble] mtu=%u\n", mtu);
  }
  // LE Secure Connections 密钥显示：我们是 DisplayOnly，中心端是 KeyboardOnly。
  // 协议栈选取随机 6 位密钥，调用 onPassKeyDisplay，用户在桌面端输入。
  // main.cpp 轮询 blePasskey() 来渲染密钥。
  uint32_t onPassKeyDisplay() override {
    // 密钥由协议栈生成，此处返回 0；实际密钥通过 onAuthenticationComplete
    // 中的 passkey 字段获取。但 NimBLE 的密钥显示流程是：
    // 协议栈调用 onPassKeyDisplay() 获取要显示的密钥。
    // 我们生成一个随机 6 位密钥。
    uint32_t pk = random(0, 999999);
    passkey = pk;
    Serial.printf("[ble] passkey %06lu\n", (unsigned long)pk);
    return pk;
  }
  void onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pin) override {
    // 不使用确认模式
  }
  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
    passkey = 0;
    secure = connInfo.isEncrypted();
    Serial.printf("[ble] auth %s\n", secure ? "ok" : "FAIL");
    if (!secure && server) {
      server->disconnect(connInfo.getConnHandle());
    }
  }
};

static ServerCallbacks serverCb;

void bleInit(const char* deviceName) {
  NimBLEDevice::init(deviceName);
  // 请求最大 MTU。macOS 通常协商到 185。
  NimBLEDevice::setMTU(517);

  // LE Secure Connections：绑定 + MITM + SC
  NimBLEDevice::setSecurityAuth(true, true, true);
  // DisplayOnly — 只显示密钥，不输入
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

  server = NimBLEDevice::createServer();
  server->setCallbacks(&serverCb);

  NimBLEService* svc = server->createService(NUS_SERVICE_UUID);

  // TX 特征（通知，加密）
  txChar = svc->createCharacteristic(
    NUS_TX_UUID,
    NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_ENC
  );

  // RX 特征（写入，加密）
  rxChar = svc->createCharacteristic(
    NUS_RX_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC
  );
  rxChar->setCallbacks(&rxCb);

  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->enableScanResponse(true);
  adv->setPreferredParams(0x06, 0x12);  // iOS 友好的连接间隔
  NimBLEDevice::startAdvertising();
  Serial.printf("[ble] advertising as '%s'\n", deviceName);
}

bool bleConnected() { return connected; }
bool bleSecure()    { return secure; }
uint32_t blePasskey() { return passkey; }

void bleClearBonds() {
  NimBLEDevice::deleteAllBonds();
  Serial.println("[ble] cleared all bonds");
}

size_t bleAvailable() {
  return (rxHead + RX_CAP - rxTail) % RX_CAP;
}

int bleRead() {
  if (rxHead == rxTail) return -1;
  int b = rxBuf[rxTail];
  rxTail = (rxTail + 1) % RX_CAP;
  return b;
}

size_t bleWrite(const uint8_t* data, size_t len) {
  if (!connected || !txChar) return 0;
  // ATT 通知载荷上限为 (MTU - 3)。macOS 协商到 185，所以
  // 182 字节分块在那里有效；使用实时 mtu 以避免对端
  // 只支持 23 字节默认值时通知被截断。
  size_t chunk = mtu > 3 ? mtu - 3 : 20;
  if (chunk > 180) chunk = 180;
  size_t sent = 0;
  while (sent < len) {
    size_t n = len - sent;
    if (n > chunk) n = chunk;
    txChar->setValue((uint8_t*)(data + sent), n);
    txChar->notify();
    sent += n;
    // 短暂让步，让 BLE 协议栈在下一块之前刷新。
    delay(4);
  }
  return sent;
}
