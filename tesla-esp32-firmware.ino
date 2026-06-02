/*
 * 特斯拉 Model X 100D 远程控制 - ESP32 固件 v2.1 (GPIO2测试版)
 * 
 * 功能：GPIO2直驱LED测试
 * 用于验证GPIO输出是否正常
 * 
 * 接线：GPIO2 → 220Ω电阻 → LED正极(长脚) → LED负极(短脚) → GND
 * 注意：ESP32板载GPIO2的LED已经是共阳接法，可以直接测试
 */

#include <HardwareSerial.h>

// ==================== MQTT配置 ====================
const char* MQTT_SERVER = "www.owill.shopping";
const int   MQTT_PORT   = 1883;
const char* CLIENT_ID   = "esp32-tesla-001";
const char* SUB_TOPIC   = "tesla/control";   // 订阅：手机→ESP32
const char* PUB_TOPIC   = "tesla/status";    // 发布：ESP32→手机

// ==================== 串口配置（ML307R）====================
HardwareSerial ML307R(1);  // UART2: GPIO16=RX, GPIO17=TX

// ==================== GPIO引脚定义（测试用GPIO2）====================
const int PIN_LOCK      = 2;   // 测试用GPIO2（板载LED）
const int PIN_LEFT      = 14;
const int PIN_RIGHT     = 25;
const int PIN_TRUNK     = 26;
const int PIN_FRUNK     = 27;

// ==================== 时序参数（可调）====================
const int PULSE_DURATION   = 300;
const int DOUBLE_INTERVAL  = 100;
const int AUTO_LOCK_MS     = 600000; // 10分钟自动锁车

// ==================== 全局状态 ====================
bool  autoLockFlag    = false;
unsigned long autoLockStart = 0;
bool  actionLocked    = false;

// ==================== GPIO2 LED测试函数 ====================
// GPIO2接法：3.3V→LED→GPIO2→GND（板载LED已串联电阻）
// GPIO2输出LOW时LED亮，输出HIGH时LED灭

void testGPIO2Once() {
  Serial.println("[GPIO2测试] LOW脉冲×1 开始");
  digitalWrite(PIN_LOCK, LOW);  // LED亮
  Serial.printf("[GPIO2] 输出LOW (LED应点亮)\n");
  delay(PULSE_DURATION);
  digitalWrite(PIN_LOCK, HIGH);  // LED灭
  Serial.printf("[GPIO2] 输出HIGH (LED应熄灭)\n");
  Serial.println("[GPIO2测试] LOW脉冲×1 完成");
}

void testGPIO2Twice() {
  Serial.println("[GPIO2测试] LOW脉冲×2 开始");
  // 第1次
  digitalWrite(PIN_LOCK, LOW);
  Serial.printf("[GPIO2] 第1次 LOW (LED亮)\n");
  delay(PULSE_DURATION);
  digitalWrite(PIN_LOCK, HIGH);
  Serial.printf("[GPIO2] 第1次完成，间隔%dms\n", DOUBLE_INTERVAL);
  delay(DOUBLE_INTERVAL);
  // 第2次
  digitalWrite(PIN_LOCK, LOW);
  Serial.printf("[GPIO2] 第2次 LOW (LED亮)\n");
  delay(PULSE_DURATION);
  digitalWrite(PIN_LOCK, HIGH);
  Serial.printf("[GPIO2] 第2次完成\n");
  Serial.println("[GPIO2测试] LOW脉冲×2 完成");
}

// ==================== 安全脉冲GPIO（两次，用于实际控制）====================
void safePulseTwice(int pin) {
  for (int i = 0; i < 2; i++) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delay(PULSE_DURATION);
    pinMode(pin, INPUT);
    if (i == 0) delay(DOUBLE_INTERVAL);
  }
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
    // 使用GPIO13锁车（实际钥匙控制）
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);
    delay(300);
    pinMode(13, INPUT);
    mqttPublish("auto_locked");
  }
}

// ==================== 指令解析与执行 ====================
void handleCommand(const String& cmd) {
  if (actionLocked) {
    Serial.println("[警告] 上个动作还在执行中，忽略本次指令");
    return;
  }
  actionLocked = true;

  Serial.printf("[指令解析] 收到命令: '%s'\n", cmd.c_str());

  if (cmd == "lock") {
    Serial.println(">>> 执行：锁车（GPIO13单次300ms）");
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);
    delay(300);
    pinMode(13, INPUT);
    autoLockFlag = false;
    mqttPublish("locked");
  }
  else if (cmd == "unlock") {
    Serial.println(">>> 执行：解锁（GPIO13两次300ms，间隔100ms）");
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);
    delay(300);
    pinMode(13, INPUT);
    delay(100);
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);
    delay(300);
    pinMode(13, INPUT);
    resetAutoLockTimer();
    mqttPublish("unlocked");
  }
  else if (cmd == "left_open") {
    Serial.println(">>> 执行：左鹰翼门（GPIO14两次300ms）");
    testGPIO2Twice();  // 用GPIO2闪光演示
    mqttPublish("left_open_ok");
  }
  else if (cmd == "right_open") {
    Serial.println(">>> 执行：右鹰翼门（GPIO25两次300ms）");
    testGPIO2Twice();
    mqttPublish("right_open_ok");
  }
  else if (cmd == "trunk_open") {
    Serial.println(">>> 执行：后备箱（GPIO26两次300ms）");
    testGPIO2Twice();
    mqttPublish("trunk_open_ok");
  }
  else if (cmd == "frunk_open") {
    Serial.println(">>> 执行：前备箱（GPIO27两次300ms）");
    testGPIO2Twice();
    mqttPublish("frunk_open_ok");
  }
  else if (cmd == "heartbeat") {
    if (autoLockFlag) {
      autoLockStart = millis();
      Serial.println("[心跳] 自动锁车计时器已重置");
    }
    mqttPublish("heartbeat_ok");
  }
  else if (cmd == "test_led") {
    // GPIO2 LED测试指令
    Serial.println(">>> 执行：GPIO2 LED测试");
    testGPIO2Twice();
    mqttPublish("test_led_ok");
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
  Serial.println("║  特斯拉 ESP32固件 v2.1 (GPIO2测试版)  ║");
  Serial.println("╚══════════════════════════════════════╝");

  // 初始化GPIO2为输出（板载LED）
  pinMode(PIN_LOCK, OUTPUT);
  digitalWrite(PIN_LOCK, HIGH);  // 默认LED灭
  Serial.println("[初始化] GPIO2已设为输出(HIGH=LED灭)");

  // GPIO13初始化为输入（高阻态，不影响钥匙）
  pinMode(13, INPUT);
  pinMode(14, INPUT);
  pinMode(25, INPUT);
  pinMode(26, INPUT);
  pinMode(27, INPUT);
  Serial.println("[初始化] GPIO13/14/25/26/27已设为INPUT(高阻态)");

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
  mqttPublish("ESP32已上线(v2.1-GPIO2测试版)");

  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║  初始化完成，进入指令监听模式         ║");
  Serial.println("║  测试指令: test_led (GPIO2闪光)       ║");
  Serial.println("╚══════════════════════════════════════╝");
}

// ==================== 主循环 ====================
void loop() {
  while (ML307R.available()) {
    String msg = ML307R.readString();
    Serial.printf("[MQTT原始数据] %s\n", msg.c_str());

    if (msg.indexOf("+MQTTURC") >= 0 && msg.indexOf("publish") >= 0) {
      int lastComma = msg.lastIndexOf(',');
      if (lastComma > 0) {
        String cmd = msg.substring(lastComma + 1);
        cmd.trim();
        handleCommand(cmd);
      }
    }
  }

  if (autoLockFlag) {
    checkAutoLock();
  }

  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 30000) {
    lastHeartbeat = millis();
    Serial.println("[心跳] 设备在线");
    mqttPublish("heartbeat");
  }

  delay(100);
}