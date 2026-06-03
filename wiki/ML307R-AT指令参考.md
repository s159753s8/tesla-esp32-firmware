# ML307R AT 指令参考

> ML307R 4G模块的 MQTT 相关 AT 指令

---

## 模块信息

- **芯片**：ASR1602
- **支持**：AT 指令集
- **MQTT**：内置 MQTT client

## 基础 AT 指令

### 测试通信
```
AT
```
响应：`OK`

### 查询信号强度
```
AT+CSQ
```
响应：`+CSQ: 18,0`（18表示信号强度，0表示误码率）

### 查询模块信息
```
ATI
```

## MQTT 指令

### 配置 MQTT 连接
```
AT+MQTTCONN=<connID>,"<host>",<port>,"<clientID>","<username>","<password>"
```

**示例**：
```
AT+MQTTCONN=0,"www.owill.shopping",1883,"esp32-tesla-001","",""
```

响应：`OK`

### 订阅主题
```
AT+MQTTSUB=<connID>,"<topic>",<qos>
```

**示例**：
```
AT+MQTTSUB=0,"tesla/control",1
```

响应：`OK`

### 发布消息
```
AT+MQTTPUB=<connID>,"<topic>",<qos>,<retain>,<dup>,<msg_len>,"<message>"
```

**关键**：消息内容必须用双引号包围，长度独立参数

**示例**：
```
AT+MQTTPUB=0,"tesla/status",0,0,0,8,"unlocked"
```

响应：`+MQTTPUB: 0,<packetID>,<msg_len>` 然后 `OK`

### 断开 MQTT
```
AT+MQTTDISC=<connID>
```

## 订阅消息通知 (URC)

当 MQTT 收到订阅消息时，ML307R 输出 URC：

```
+MQTTURC: "publish",<connID>,<qos>,"<topic>",<msg_len>,<msg>
```

**示例**：
```
+MQTTURC: "publish",0,1,"tesla/control",6,6,unlock
```

**ESP32 解析代码**：
```cpp
int lastComma = msg.lastIndexOf(',');
if (lastComma > 0) {
  String cmd = msg.substring(lastComma + 1);
  cmd.trim();
  // cmd = "unlock"
}
```

## 常见错误

### 错误 1: MQTTPUB 格式错误

❌ 错误写法（不带双引号）：
```
AT+MQTTPUB=0,"tesla/status",0,0,0,8,unlocked
```

✅ 正确写法（消息加双引号）：
```
AT+MQTTPUB=0,"tesla/status",0,0,0,8,"unlocked"
```

### 错误 2: 长度参数错误

❌ 错误：把消息本身作为长度
```
AT+MQTTPUB=0,"tesla/status",0,0,0,"unlocked","unlocked"
```

✅ 正确：长度是数字，消息是字符串
```
AT+MQTTPUB=0,"tesla/status",0,0,0,8,"unlocked"
```

## 完整 MQTT 流程

```cpp
// 1. 连接
AT+MQTTCONN=0,"www.owill.shopping",1883,"esp32-tesla-001","",""

// 2. 订阅
AT+MQTTSUB=0,"tesla/control",1

// 3. 发布上线消息
AT+MQTTPUB=0,"tesla/status",0,0,0,11,"ESP32上线了"

// 4. 收到订阅消息时（URC）
+MQTTURC: "publish",0,1,"tesla/control",6,6,unlock
// 解析出 unlock，执行 GPIO13 triggerTwice()
```