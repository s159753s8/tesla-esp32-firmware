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
  // 注意：message必须加引号，长度是数字
  String cmd = "AT+MQTTPUB=0,\"";
  cmd += topic;
  cmd += "\",";
  cmd += String(qos);
  cmd += ",0,0,";
  cmd += String(strlen(message));  // 长度是数字，不带引号
  cmd += ",\"";
  cmd += message;
  cmd += "\"";
  sendAT(cmd, 2000);
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

    // 解析MQTT消息
    if (msg.indexOf("+MQTTREAD") >= 0) {
      Serial.println("收到MQTT指令！");

      if (msg.indexOf("lock") >= 0) {
        Serial.println(">>> 执行：锁车");
      } else if (msg.indexOf("unlock") >= 0) {
        Serial.println(">>> 执行：解锁");
      }
    }
  }

  // 定期发送心跳（每30秒）
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 30000) {
    lastHeartbeat = millis();
    Serial.println("[心跳] 设备在线");
    mqttPublish(pub_topic, "heartbeat");
  }

  delay(100);
}