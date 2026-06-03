/*
 * 特斯拉 Model X 100D 远程控制 - ESP32 固件 v3.0（开漏输出版）
 * 
 * 架构：ESP32 + ML307R 4G模块 + 钥匙PCB直连
 * 
 * 安全策略（最高优先级）：
 * - 所有GPIO控制引脚必须使用开漏输出(OUTPUT_OPEN_DRAIN)
 * - 平时：digitalWrite(pin, HIGH) = 高阻态，物理断开
 * - 触发：digitalWrite(pin, LOW) = 接地，模拟按键按下
 * - 绝对禁止输出3.3V到钥匙信号线
 * 
 * 引脚分配：
 *   GPIO13 → 锁车(单次)/解锁(两次)
 *   GPIO14 → 左鹰翼门
 *   GPIO25 → 右鹰翼门
 *   GPIO26 → 后备箱
 *   GPIO27 → 前备箱
 */

#include <HardwareSerial.h>

// ==================== MQTT配置 ====================
const char* MQTT_SERVER = "www.owill.shopping";
const int   MQTT_PORT   = 1883;
const char* CLIENT_ID   = "esp32-tesla-001";
const char* SUB_TOPIC   = "tesla/control";
const char* PUB_TOPIC   = "tesla/status";

// ==================== 串口配置（ML307R）====================
HardwareSerial ML307R(1);  // UART2: GPIO16=RX, GPIO17=TX

// ==================== GPIO引脚定义 ====================
const int PIN_LOCK      = 13;  // 锁车/解锁
const int PIN_LEFT      = 14;  // 左鹰翼门
const int PIN_RIGHT     = 25;  // 右鹰翼门
const int PIN_TRUNK     = 26;  // 后备箱
const int PIN_FRUNK     = 27;  // 前备箱

// ==================== 时序参数（可调）====================
const int PRESS_MS      = 300;  // 按下时长（模拟手指按住时间）
const int INTERVAL_MS   = 100;  // 双击间隔
const int AUTO_LOCK_MS  = 600000; // 10分钟自动锁车

// ==================== 全局状态 ====================
bool  autoLockFlag    = false;
unsigned long autoLockStart = 0;
bool  actionLocked    = false;  // 防竞争锁（动作执行中忽略新指令）

// ==================== 工具函数 ====================

// 执行一次脉冲：LOW->等待PRESS_MS->恢复HIGH
void triggerOnce(int pin) {
  Serial.printf("[触发] pin=%d 执行单次脉冲(LOW%dms)\n", pin, PRESS_MS);
  digitalWrite(pin, LOW);   // 开漏输出拉低 = 模拟按键按下
  delay(PRESS_MS);
  digitalWrite(pin, HIGH);  // 恢复高阻态 = 模拟按键释放
  Serial.printf("[触发] pin=%d 完成\n", pin);
}

// 执行两次脉冲：LOW->等待PRESS_MS->HIGH->等待INTERVAL_MS->LOW->等待PRESS_MS->HIGH
void triggerTwice(int pin) {
  Serial.printf("[触发] pin=%d 执行双次脉冲(LOW×2,间隔%dms)\n", pin, INTERVAL_MS);
  // 第1次
  digitalWrite(pin, LOW);
  delay(PRESS_MS);
  digitalWrite(pin, HIGH);
  Serial.printf("[触发] pin=%d 第1次完成，间隔%dms\n", pin, INTERVAL_MS);
  delay(INTERVAL_MS);
  // 第2次
  digitalWrite(pin, LOW);
  delay(PRESS_MS);
  digitalWrite(pin, HIGH);
  Serial.printf("[触发] pin=%d 第2次完成\n", pin);
}

// ==================== 初始化GPIO（开漏输出）====================
void initPinOpenDrain(int pin) {
  pinMode(pin, OUTPUT_OPEN_DRAIN);
  digitalWrite(pin, HIGH);  // 默认高阻态，物理断开
}

String getPinName(int pin) {
  if (pin == 13) return "锁车";
  if (pin == 14) return "左鹰翼";
  if (pin == 25) return "右鹰翼";
  if (pin == 26) return "后备箱";
  if (pin == 27) return "前备箱";
  return "未知";
}

// ==================== MQTT发布 ====================
void mqttPublish(const char* message, int qos = 0) {
  char cmd[256];
  int msgLen = strlen(message);
  snprintf(cmd, sizeof(cmd),
    "AT+MQTTPUB=0,\"%s\",%d,0,0,%d,\"%s\"",
    PUB_TOPIC, qos, msgLen, message);
  
  Serial.printf("[MQTT发布] topic=%s, msg=%s\n", PUB_TOPIC, message);
  ML307R.println(cmd);
  delay(2000);
  while (ML307R.available()) {
    String r = ML307R.readString();
    Serial.print(r);
  }
}

// ==================== 自动锁车 ====================
void resetAutoLockTimer() {
  autoLockFlag = true;
  autoLockStart = millis();
  Serial.printf("[自动锁车] 已启动，%d分钟后执行\n", AUTO_LOCK_MS / 60000);
  mqttPublish("auto_lock_timer_started");
}

void checkAutoLock() {
  if (autoLockFlag && (millis() - autoLockStart > AUTO_LOCK_MS)) {
    autoLockFlag = false;
    Serial.println("[自动锁车] 时间到！执行锁车...");
    triggerOnce(PIN_LOCK);
    mqttPublish("auto_locked");
  }
}

// ==================== 指令解析与执行 ====================
void handleCommand(const String& cmd) {
  // 防竞争：动作执行中忽略新指令
  if (actionLocked) {
    Serial.println("[警告] 上个动作还在执行中，忽略本次指令");
    return;
  }
  actionLocked = true;

  Serial.printf("[指令解析] 收到命令: '%s'\n", cmd.c_str());

  // ==================== 实际控制指令 ====================
  if (cmd == "lock") {
    Serial.println(">>> 执行：锁车（GPIO13单次脉冲）");
    triggerOnce(PIN_LOCK);
    autoLockFlag = false;
    mqttPublish("locked");
  }
  else if (cmd == "unlock") {
    Serial.println(">>> 执行：解锁（GPIO13双次脉冲）");
    triggerTwice(PIN_LOCK);
    resetAutoLockTimer();
    mqttPublish("unlocked");
  }
  else if (cmd == "left_open") {
    Serial.println(">>> 执行：左鹰翼门（GPIO14双次脉冲）");
    triggerTwice(PIN_LEFT);
    mqttPublish("left_open_ok");
  }
  else if (cmd == "right_open") {
    Serial.println(">>> 执行：右鹰翼门（GPIO25双次脉冲）");
    triggerTwice(PIN_RIGHT);
    mqttPublish("right_open_ok");
  }
  else if (cmd == "trunk_open") {
    Serial.println(">>> 执行：后备箱（GPIO26双次脉冲）");
    triggerTwice(PIN_TRUNK);
    mqttPublish("trunk_open_ok");
  }
  else if (cmd == "frunk_open") {
    Serial.println(">>> 执行：前备箱（GPIO27双次脉冲）");
    triggerTwice(PIN_FRUNK);
    mqttPublish("frunk_open_ok");
  }

  // ==================== 测试指令（仅串口打印，不实际操作GPIO）====================
  else if (cmd == "test_lock") {
    Serial.println("[测试] 模拟锁车脉冲触发（GPIO13单次）");
    mqttPublish("test_lock_ok");
  }
  else if (cmd == "test_unlock") {
    Serial.println("[测试] 模拟解锁脉冲触发（GPIO13双次）");
    mqttPublish("test_unlock_ok");
  }
  else if (cmd == "test_left") {
    Serial.println("[测试] 模拟左鹰翼脉冲触发（GPIO14双次）");
    mqttPublish("test_left_ok");
  }
  else if (cmd == "test_right") {
    Serial.println("[测试] 模拟右鹰翼脉冲触发（GPIO25双次）");
    mqttPublish("test_right_ok");
  }
  else if (cmd == "test_trunk") {
    Serial.println("[测试] 模拟后备箱脉冲触发（GPIO26双次）");
    mqttPublish("test_trunk_ok");
  }
  else if (cmd == "test_frunk") {
    Serial.println("[测试] 模拟前备箱脉冲触发（GPIO27双次）");
    mqttPublish("test_frunk_ok");
  }

  // ==================== 其他指令 ====================
  else if (cmd == "heartbeat") {
    if (autoLockFlag) {
      autoLockStart = millis();
      Serial.println("[心跳] 自动锁车计时器已重置");
    }
    mqttPublish("heartbeat_ok");
  }
  else {
    Serial.printf("[警告] 未知指令: '%s'\n", cmd.c_str());
    mqttPublish("unknown_command");
  }

  actionLocked = false;
}

// ==================== AT命令发送 ====================
String sendAT(const String& cmd, int waitMs = 1000) {
  Serial.printf("[ESP32发送AT] %s\n", cmd.c_str());
  ML307R.println(cmd);
  delay(waitMs);
  String resp = "";
  while (ML307R.available()) {
    char c = ML307R.read();
    resp += c;
  }
  if (resp.length() > 0) {
    Serial.printf("[ML307R响应] %s\n", resp.c_str());
  }
  return resp;
}

// ==================== 初始化 ====================
void setup() {
  Serial.begin(115200);
  ML307R.begin(115200, SERIAL_8N1, 16, 17);

  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║  特斯拉 Model X 100D  ESP32固件 v3.0   ║");
  Serial.println("║  【开漏输出版】所有GPIO均为OUTPUT_OPEN_DRAIN║");
  Serial.println("╚══════════════════════════════════════╝");

  // 初始化所有控制GPIO为开漏输出，默认HIGH（高阻态）
  initPinOpenDrain(PIN_LOCK);
  initPinOpenDrain(PIN_LEFT);
  initPinOpenDrain(PIN_RIGHT);
  initPinOpenDrain(PIN_TRUNK);
  initPinOpenDrain(PIN_FRUNK);

  Serial.println("[初始化] 所有GPIO已设为OUTPUT_OPEN_DRAIN+HIGH(高阻态)");
  Serial.println("[初始化] 安全策略：平时断开，动作拉低，绝对不输出3.3V");

  delay(2000);

  // 测试ML307R通信
  Serial.println("=== 测试ML307R模块 ===");
  String resp = sendAT("AT", 2000);
  if (resp.indexOf("OK") >= 0) {
    Serial.println("✅ ML307R通信正常");
  } else {
    Serial.println("❌ ML307R无响应！检查接线（GPIO16/17）");
  }

  sendAT("AT+CSQ", 2000);

  // 连接MQTT
  Serial.println("=== 配置MQTT连接 ===");
  char mqttConnCmd[256];
  snprintf(mqttConnCmd, sizeof(mqttConnCmd),
    "AT+MQTTCONN=0,\"%s\",%d,\"%s\",\"\",\"\"",
    MQTT_SERVER, MQTT_PORT, CLIENT_ID);
  sendAT(mqttConnCmd, 3000);

  // 订阅控制主题
  Serial.println("=== 订阅主题 ===");
  char subCmd[128];
  snprintf(subCmd, sizeof(subCmd),
    "AT+MQTTSUB=0,\"%s\",1", SUB_TOPIC);
  sendAT(subCmd, 2000);

  // 上线通知
  Serial.println("=== 发送上线状态 ===");
  mqttPublish("ESP32已上线(v3.0-开漏输出版)");

  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║  初始化完成，进入指令监听模式         ║");
  Serial.println("║                                    ║");
  Serial.println("║  实际指令：lock/unlock/left_open/   ║");
  Serial.println("║          right_open/trunk_open/     ║");
  Serial.println("║          frunk_open/heartbeat       ║");
  Serial.println("║                                    ║");
  Serial.println("║  测试指令：test_lock/test_unlock/   ║");
  Serial.println("║          test_left/test_right/     ║");
  Serial.println("║          test_trunk/test_frunk     ║");
  Serial.println("╚══════════════════════════════════════╝");
}

// ==================== 主循环 ====================
void loop() {
  // 检查ML307R收到的MQTT消息
  while (ML307R.available()) {
    String msg = ML307R.readString();
    Serial.printf("[MQTT原始数据] %s\n", msg.c_str());

    if (msg.indexOf("+MQTTURC") >= 0 && msg.indexOf("publish") >= 0) {
      // 格式: +MQTTURC: "publish",0,10,"tesla/control",6,6,unlock
      int lastComma = msg.lastIndexOf(',');
      if (lastComma > 0) {
        String cmd = msg.substring(lastComma + 1);
        cmd.trim();
        handleCommand(cmd);
      }
    }
  }

  // 检查自动锁车计时器
  if (autoLockFlag) {
    unsigned long elapsed = millis() - autoLockStart;
    unsigned long remaining = (AUTO_LOCK_MS > elapsed) ? (AUTO_LOCK_MS - elapsed) / 1000 : 0;
    if (remaining % 60000 == 0 && remaining > 0) {
      Serial.printf("[自动锁车倒计时] 还剩%lus\n", remaining);
    }
    checkAutoLock();
  }

  // 定期心跳（每30秒）
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 30000) {
    lastHeartbeat = millis();
    Serial.println("[心跳] 设备在线");
    mqttPublish("heartbeat");
  }

  delay(100);
}