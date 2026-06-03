# ESP32-ML307R 接线文档

> 详细说明 ESP32 与 ML307R 4G模块的硬件连接

---

## 引脚定义

| ESP32 引脚 | 连接到 ML307R | 说明 |
|-----------|---------------|------|
| 5V | VIN | 5V 电源（不是 3.3V）|
| GND | GND | 共地，必须连接 |
| GPIO17 (TX2) | RX | ESP32发送 → ML307R接收 |
| GPIO16 (RX2) | TX | ESP32接收 ← ML307R发送 |

## 接线图

```
ESP32                    ML307R 4G模块
┌──────────┐            ┌──────────┐
│       5V ├────────────┤ VIN      │
│      GND ├────────────┤ GND      │
│  GPIO17  ├────────────┤ RX       │
│  GPIO16  ├────────────┤ TX       │
│          │            │          │
│  GPIO13  ├──── 锁车/解锁信号线 ────── 钥匙PCB
│  GPIO14  ├──── 左鹰翼门信号线 ────── 钥匙PCB
│  GPIO25  ├──── 右鹰翼门信号线 ────── 钥匙PCB
│  GPIO26  ├──── 后备箱信号线 ────── 钥匙PCB
│  GPIO27  ├──── 前备箱信号线 ────── 钥匙PCB
└──────────┘            └──────────┘
```

## 关键注意事项

### 1. 供电电压

⚠️ **ML307R 必须用 5V 供电**，不要用 3.3V（功率不足）

### 2. UART引脚

- ✅ 使用 **GPIO16/17**（Serial2）
- ❌ 不要用板子上标的 RX/TX（那是 GPIO1/3，用于烧录）

### 3. GND 共地

ESP32 的 GND 和钥匙 PCB 的 GND **必须连通**，否则GPIO13拉低不会影响钥匙

### 4. GPIO13 拉低 = 按下

ESP32 GPIO13 拉低（0V）= 模拟钥匙按钮按下 = 触发车锁动作

## ESP32 程序配置

```cpp
HardwareSerial ML307R(1);  // UART2
ML307R.begin(115200, SERIAL_8N1, 16, 17);  // RX=16, TX=17
```

## ML307R AT 指令测试

```bash
# 测试ML307R通信
AT
# 期望响应: OK

# 查询信号强度
AT+CSQ
# 期望响应: +CSQ: 18,0\nOK

# 连接MQTT
AT+MQTTCONN=0,"www.owill.shopping",1883,"esp32-tesla-001","",""
# 期望响应: OK

# 订阅主题
AT+MQTTSUB=0,"tesla/control",1
# 期望响应: OK
```