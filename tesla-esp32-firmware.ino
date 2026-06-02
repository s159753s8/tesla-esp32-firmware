/*
 * 特斯拉远程控制 - ESP32 + ML307R 固件
 * 功能：通过4G模块连接MQTT，订阅控制指令
 *
 * 接线：GPIO17(TX2)→RX, GPIO16(RX2)→TX, 5V→VIN, GND→GND
 *
 * 作者：s159753s8
 * 项目：特斯拉 Model X 100D 远程控制
 */

#include <HardwareSerial.h>

// ==================== MQTT配置 ====================
const char* mqtt_server = "www.owill.shopping";
const int mqtt_port = 1883;
const char* client_id = "esp32-tesla-001";
const char* sub_topic = "tesla/control";
const char* pub_topic = "tesla/status";

// ==================== 串口配置 ====================
HardwareSerial ML307R(1);  // UART2

// ==================== AT命令发送/接收 ====================
String sendAT(String cmd, int waitTime = 1000) {
  String response = "";
  Serial.println("[ESP32] 发送: " + cmd);
  ML307R.println(cmd);
  delay(waitTime);
  while (ML307R.available()) {
    char c = ML307R.read();
    response += c;
  }
  if (response.length() > 0) {
    Serial.println("[ESP32] 收到: " + response);
  }
  return response;
}

// ==================== 发送MQTT消息（ML307R专用）====================
void mqttPublish(const char* topic, const char* message, int qos = 0) {
  // ML307R MQTTPUB格式：AT+MQTTPUB=<connID>,"<topic>",<qos>,<retain>,<dup>,<msg_len>,<message>
  String cmd = "AT+MQTTPUB=0,\"";
  cmd += topic;
  cmd += "\",";
  cmd += String(qos);
  cmd += ",0,0,";
  cmd += String(strlen(message));
  cmd += ",\"";
  cmd += message;
  cmd += "\"";
  sendAT(cmd, 2000);
}

// ==================== 控制GPIO（开漏输出，安全策略）====================
// 详见需求文档：平时INPUT（高阻态），动作时OUTPUT+LOW
void safePulse(int gpioPin, int lowCount, int intervalMs) {
  // lowCount: 触发几次LOW（单次=锁车，两次=解锁）
  for (int i = 0; i < lowCount; i++) {
    pinMode(gpioPin, OUTPUT);        // 设为输出模式
    digitalWrite(gpioPin, LOW);       // 拉低（模拟按键按下）
    delay(intervalMs);
    pinMode(gpioPin, INPUT);          // 切回输入模式（高阻态，释放按键）
    if (i < lowCount - 1) delay(100); // 两次之间间隔100ms
  }
}

// ==================== 自动锁车计时器 ====================
unsigned long lastUnlockTime = 0;
bool unlockFlag = false;

void resetAutoLockTimer() {
  unlockFlag = true;
  lastUnlockTime = millis();
  Serial.println("[计时器] 解锁已记录，10分钟后自动锁车");
}

void checkAutoLock() {
  if (unlockFlag && (millis() - lastUnlockTime > 600000)) { // 10分钟
    unlockFlag = false;
    Serial.println("[自动锁车] 时间到，执行锁车");
    safePulse(13, 1, 300);  // GPIO13 单次300ms锁车
    mqttPublish(pub_topic, "auto_locked"); // 通知手机
  }
}

// ==================== 初始化 ====================
void setup() {
  Serial.begin(115200);      // 调试串口
  ML307R.begin(115200, SERIAL_8N1, 16, 17);

  Serial.println("=== ESP32 启动 ===");
  delay(2000);

  // 测试ML307R通信
  Serial.println("=== 测试ML307R模块 ===");
  String resp = sendAT("AT", 2000);

  if (resp.indexOf("OK") >= 0) {
    Serial.println("✅ ML307R通信正常");
  } else {
    Serial.println("❌ ML307R无响应，检查接线！");
  }

  // 查询信号强度
  sendAT("AT+CSQ", 2000);

  // 配置MQTT连接
  Serial.println("=== 配置MQTT连接 ===");
  String mqttCmd = "AT+MQTTCONN=0,\"";
  mqttCmd += mqtt_server;
  mqttCmd += "\",";
  mqttCmd += String(mqtt_port);
  mqttCmd += ",\"";
  mqttCmd += client_id;
  mqttCmd += "\",\"\",\"";

  sendAT(mqttCmd, 3000);

  // 订阅主题
  Serial.println("=== 订阅主题 ===");
  String subCmd = "AT+MQTTSUB=0,\"";
  subCmd += sub_topic;
  subCmd += "\",1";
  sendAT(subCmd, 2000);

  // 发送上线消息
  Serial.println("=== 发送上线状态 ===");
  mqttPublish(pub_topic, "ESP32已上线");

  Serial.println("=== 初始化完成，进入监听模式 ===");
}

// ==================== 主循环 ====================
void loop() {
  // 检查ML307R收到的消息
  while (ML307R.available()) {
    String msg = ML307R.readString();
    Serial.print("[MQTT收到] ");
    Serial.println(msg);

    // ML307R收到MQTT消息后会上报 +MQTTURC: "publish",...
    // 格式：+MQTTURC: "publish",<connID>,<qos>,"<topic>",<msg_len>,<msg>
    // 注意：ESP32通过串口接收时，URC可能跨多条输出，需要解析
    if (msg.indexOf("+MQTTURC") >= 0 && msg.indexOf("publish") >= 0) {
      Serial.println("✅ 收到MQTT指令！");

      // 提取消息内容：查找最后一个逗号后面的内容
      int lastComma = msg.lastIndexOf(',');
      if (lastComma > 0) {
        String command = msg.substring(lastComma + 1);
        command.trim(); // 去掉空白

        Serial.print(">>> 指令内容: ");
        Serial.println(command);

        if (command == "lock") {
          Serial.println(">>> 执行：锁车");
          safePulse(13, 1, 300);       // GPIO13 单次300ms
          mqttPublish(pub_topic, "locked");
          unlockFlag = false;          // 取消自动锁车计时
        }
        else if (command == "unlock") {
          Serial.println(">>> 执行：解锁");
          safePulse(13, 2, 300);       // GPIO13 两次300ms（间隔100ms）
          mqttPublish(pub_topic, "unlocked");
          resetAutoLockTimer();        // 启动10分钟自动锁车
        }
        else if (command == "left_open") {
          Serial.println(">>> 执行：左鹰翼门");
          safePulse(14, 2, 300);
          mqttPublish(pub_topic, "left_open_ok");
        }
        else if (command == "right_open") {
          Serial.println(">>> 执行：右鹰翼门");
          safePulse(25, 2, 300);
          mqttPublish(pub_topic, "right_open_ok");
        }
        else if (command == "trunk_open") {
          Serial.println(">>> 执行：后备箱");
          safePulse(26, 2, 300);
          mqttPublish(pub_topic, "trunk_open_ok");
        }
        else if (command == "heartbeat") {
          // 重置自动锁车计时器
          if (unlockFlag) {
            lastUnlockTime = millis();
            Serial.println("[心跳] 自动锁车计时器已重置");
          }
          mqttPublish(pub_topic, "heartbeat_ok");
        }
      }
    }
  }

  // 检查自动锁车计时器
  checkAutoLock();

  // 定期发送心跳（每30秒）
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 30000) {
    lastHeartbeat = millis();
    Serial.println("[心跳] 设备在线");
    mqttPublish(pub_topic, "heartbeat");
  }

  delay(100);
}