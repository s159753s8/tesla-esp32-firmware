/*
 * 特斯拉 Model X 100D 远程控制 - ESP32 固件 v2.0
 * 
 * 功能：GPIO直连钥匙PCB，MQTT指令控制
 * 通信：ML307R 4G模块 (UART2, GPIO16/RX, GPIO17/TX)
 * 
 * 引脚分配：
 *   GPIO13 → 锁车(单次300ms)/解锁(两次，间隔100ms)
 *   GPIO14 → 左鹰翼门（两次300ms）
 *   GPIO25 → 右鹰翼门（两次300ms）
 *   GPIO26 → 后备箱（两次300ms）
 *   GPIO27 → 前备箱（两次300ms，需二次确认）
 * 
 * 安全策略：平时INPUT(高阻态)，动作时OUTPUT+LOW，绝对不写HIGH
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

// ==================== GPIO引脚定义 ====================
const int PIN_LOCK      = 13;  // 锁车/解锁（单次=锁，两次=解）
const int PIN_LEFT      = 14;  // 左鹰翼门
const int PIN_RIGHT     = 25;  // 右鹰翼门
const int PIN_TRUNK     = 26;  // 后备箱
const int PIN_FRUNK     = 27;  // 前备箱

// ==================== 时序参数（可调）====================
const int PULSE_DURATION   = 300;  // 单次脉冲持续时间(ms)
const int DOUBLE_INTERVAL  = 100;  // 两次之间间隔(ms)
const int AUTO_LOCK_MS     = 600000; // 10分钟自动锁车

// ==================== 全局状态 ====================
bool  autoLockFlag    = false;  // 自动锁车计时标志
unsigned long autoLockStart = 0; // 计时开始时间
bool  actionLocked    = false;  // 防竞争锁（动作执行中）

// ==================== 工具函数 ====================

// 打印GPIO状态（最直接的调试方式）
void printGPIOLow(int pin, const char* name) {
  Serial.printf("[GPIO%d→%s] LOW脉冲触发 开始\n", pin, name);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  Serial.printf("[GPIO%d→%s] LOW脉冲触发 完成(拉低%dms)\n", pin, name, PULSE_DURATION);
  delay(PULSE_DURATION);
  pinMode(pin, INPUT);  // 切回高阻态
  Serial.printf("[GPIO%d→%s] 已恢复INPUT(高阻态)\n", pin, name);
}

void printGPIOLowTwice(int pin, const char* name) {
  Serial.printf("[GPIO%d→%s] LOW脉冲×2 开始\n", pin, name);
  // 第1次
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  Serial.printf("[GPIO%d→%s] 第1次 LOW (拉低%dms)\n", pin, name, PULSE_DURATION);
  delay(PULSE_DURATION);
  pinMode(pin, INPUT);
  Serial.printf("[GPIO%d→%s] 第1次完成，间隔%dms\n", pin, name, DOUBLE_INTERVAL);
  delay(DOUBLE_INTERVAL);
  // 第2次
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  Serial.printf("[GPIO%d→%s] 第2次 LOW (拉低%dms)\n", pin, name, PULSE_DURATION);
  delay(PULSE_DURATION);
  pinMode(pin, INPUT);
  Serial.printf("[GPIO%d→%s] 第2次完成，已恢复INPUT\n", pin, name);
}

// 安全脉冲GPIO（单次）
void safePulseOnce(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(PULSE_DURATION);
  pinMode(pin, INPUT);  // 切回高阻态
}

// 安全脉冲GPIO（两次）
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
  // 读取响应
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
    safePulseOnce(PIN_LOCK);
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
    printGPIOLow(PIN_LOCK, "锁车");
    autoLockFlag = false;  // 取消自动锁车
    mqttPublish("locked");
  }
  else if (cmd == "unlock") {
    Serial.println(">>> 执行：解锁（GPIO13两次300ms，间隔100ms）");
    printGPIOLowTwice(PIN_LOCK, "解锁");
    resetAutoLockTimer();  // 启动10分钟计时
    mqttPublish("unlocked");
  }
  else if (cmd == "left_open") {
    Serial.println(">>> 执行：左鹰翼门（GPIO14两次300ms）");
    printGPIOLowTwice(PIN_LEFT, "左鹰翼");
    mqttPublish("left_open_ok");
  }
  else if (cmd == "right_open") {
    Serial.println(">>> 执行：右鹰翼门（GPIO25两次300ms）");
    printGPIOLowTwice(PIN_RIGHT, "右鹰翼");
    mqttPublish("right_open_ok");
  }
  else if (cmd == "trunk_open") {
    Serial.println(">>> 执行：后备箱（GPIO26两次300ms）");
    printGPIOLowTwice(PIN_TRUNK, "后备箱");
    mqttPublish("trunk_open_ok");
  }
  else if (cmd == "frunk_open") {
    Serial.println(">>> 执行：前备箱（GPIO27两次300ms）");
    printGPIOLowTwice(PIN_FRUNK, "前备箱");
    mqttPublish("frunk_open_ok");
  }
  else if (cmd == "heartbeat") {
    if (autoLockFlag) {
      autoLockStart = millis();  // 重置计时器
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
  Serial.begin(115200);  // 调试串口
  ML307R.begin(115200, SERIAL_8N1, 16, 17);

  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║  特斯拉 Model X 100D  ESP32固件 v2.0   ║");
  Serial.println("╚══════════════════════════════════════╝");

  // 初始化GPIO（全部设为INPUT高阻态）
  pinMode(PIN_LOCK, INPUT);
  pinMode(PIN_LEFT, INPUT);
  pinMode(PIN_RIGHT, INPUT);
  pinMode(PIN_TRUNK, INPUT);
  pinMode(PIN_FRUNK, INPUT);
  Serial.println("[初始化] 所有GPIO已设为INPUT(高阻态)");

  delay(2000);

  // 测试ML307R通信
  Serial.println("=== 测试ML307R模块 ===");
  String resp = sendAT("AT", 2000);
  if (resp.indexOf("OK") >= 0) {
    Serial.println("✅ ML307R通信正常");
  } else {
    Serial.println("❌ ML307R无响应！检查接线（GPIO16/17）");
  }

  // 查询信号
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
  mqttPublish("ESP32已上线(GPIO含状态打印版)");

  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║  初始化完成，进入指令监听模式         ║");
  Serial.println("║  发送指令后，串口将打印GPIO实际状态   ║");
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
    if (remaining % 60000 == 0) {  // 每分钟打印一次
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