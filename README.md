# 特斯拉 Model X 100D 远程控制

> 通过 ESP32 + ML307R 4G 模块实现特斯拉 Model X 100D 的远程控制
> 车辆背景：2017款Model X 100D，无主账号（不能去4S店），钥匙已接入第三方便捷解锁盒子

---

## 项目状态

- 🎉 **锁车/解锁功能已验证通过**（2026-06-02）
- ✅ MQTT通信链路：iOS快捷指令 → HTTP桥接 → MQTT Broker → ESP32 → GPIO（稳定）
- ✅ GPIO13开漏输出保护已验证（不输出3.3V，保护钥匙主板）
- ⏳ 鸥翼门/后备箱/前备箱：待实车验证
- ⏳ iOS快捷指令集成：待配置
- ⏳ 10分钟自动锁车：待实车验证

---

## 硬件接线

### ESP32 + ML307R 4G模块（UART2通信）

| ESP32 | → | ML307R |
|-------|---|--------|
| 5V | → | VIN（5V供电） |
| GND | → | GND |
| GPIO17(TX2) | → | RX |
| GPIO16(RX2) | → | TX |

**注意**：
- ML307R 需要 **5V 供电**，不是 3.3V
- 使用 GPIO16/17（Serial2），不要用板子上标的 RX/TX（那是 GPIO1/3，用于烧录）

### ESP32 → 钥匙PCB（开漏输出）

| ESP32 GPIO | 车辆功能 | 时序 |
|-----------|----------|------|
| GPIO13 | 锁车/解锁 | 单次=锁，双次=解 |
| GPIO14 | 左鹰翼门 | 两次300ms（间隔100ms） |
| GPIO25 | 右鹰翼门 | 两次300ms（间隔100ms） |
| GPIO26 | 后备箱 | 两次300ms（间隔100ms） |
| GPIO27 | 前备箱 | 两次300ms（间隔100ms） |

**安全策略（开漏输出）**：
- 平时：OUTPUT_OPEN_DRAIN + digitalWrite(pin, HIGH) = 高阻态（物理断开，零干扰）
- 动作：digitalWrite(pin, LOW) = 接地（模拟按键按下）
- 绝对禁止输出3.3V到钥匙信号线（钥匙是3V供电，3.3V可能烧毁）

---

## 通信架构

```
iOS快捷指令
    ↓ HTTP GET请求
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=指令
    ↓
mqtt-http-bridge.py（Python服务，Docker运行）
    ↓ MQTT发布到主题 tesla/control
Mosquitto Broker（192.168.0.112:1883）
    ↓
ML307R 4G模块（ESP32通过AT指令连接）
    ↓ 串口AT指令
ESP32（订阅主题，解析指令）
    ↓ GPIO开漏输出
钥匙PCB（模拟按键按下/释放）
```

**MQTT主题**：
- `tesla/control`（手机→ESP32）：下发控制指令
- `tesla/status`（ESP32→手机）：上报执行结果
- `tesla/heartbeat`（手机→ESP32）：重置自动锁车计时器

---

## 已实现指令

### 实际控制指令

| 指令 | GPIO | 时序 | 功能 |
|------|------|------|------|
| `lock` | GPIO13 | 单次300ms | 锁车 ✅已验证 |
| `unlock` | GPIO13 | 两次300ms | 解锁 ✅已验证 |
| `left_open` | GPIO14 | 两次300ms | 左鹰翼门 |
| `right_open` | GPIO25 | 两次300ms | 右鹰翼门 |
| `trunk_open` | GPIO26 | 两次300ms | 后备箱 |
| `frunk_open` | GPIO27 | 两次300ms | 前备箱 |
| `heartbeat` | - | - | 重置10分钟自动锁车计时器 |

### 测试指令（不实际操作GPIO）

| 指令 | 功能 |
|------|------|
| `test_lock` | 串口打印"模拟锁车脉冲触发" |
| `test_unlock` | 串口打印"模拟解锁脉冲触发" |
| `test_left` | 串口打印"模拟左鹰翼脉冲触发" |
| `test_right` | 串口打印"模拟右鹰翼脉冲触发" |
| `test_trunk` | 串口打印"模拟后备箱脉冲触发" |
| `test_frunk` | 串口打印"模拟前备箱脉冲触发" |

---

## 完整URL指令列表

### 锁车/解锁（已验证成功 ✅）
```
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=lock
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=unlock
```

### 鸥翼门
```
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=left_open
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=right_open
```

### 后备箱/前备箱
```
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=trunk_open
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=frunk_open
```

### 心跳/自动锁车
```
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=heartbeat
```

### 测试指令（不实际操作GPIO）
```
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=test_lock
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=test_unlock
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=test_left
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=test_right
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=test_trunk
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=test_frunk
```

---

## 项目文件

| 文件 | 说明 |
|------|------|
| `tesla-esp32-firmware.ino` | ESP32 Arduino固件（v3.0开漏输出版）|
| `mqtt-http-bridge.py` | HTTP→MQTT桥接服务（端口45321）|
| `Dockerfile` | mqtt-http-bridge容器化构建文件 |

---

## 部署与运行

### 1. ESP32固件烧录

1. Arduino IDE 打开 `tesla-esp32-firmware.ino`
2. 工具 → 开发板 → ESP32 Dev Module
3. 上传速度：115200
4. 烧录
5. 打开串口监视器（115200波特率）

### 2. MQTT Broker

Docker运行Mosquitto，端口1883（MQTT）和8084（WSS）
配置位置：`~/mosquitto/config/mosquitto.conf`

### 3. HTTP桥接服务

Docker运行mqtt-http-bridge，端口45321
镜像构建：`docker build -t mqtt-http-bridge .`
运行：`docker run -d --network host --name mqtt-http-bridge mqtt-http-bridge`

### 4. 路由器端口映射

| 外部端口 | 内部IP | 内部端口 | 协议 |
|---------|--------|---------|------|
| 1883 | 192.168.0.112 | 1883 | TCP（MQTT）|
| 8084 | 192.168.0.112 | 8084 | TCP（WSS）|
| **45321** | 192.168.0.112 | **45321** | TCP（HTTP桥接）|

### 5. 动态域名

`www.owill.shopping` → 自动更新到公网IP

---

## 烧录后验证

1. 烧录后串口应显示：
```
╔══════════════════════════════════════╗
║  特斯拉 Model X 100D  ESP32固件 v3.0   ║
║  【开漏输出版】所有GPIO均为OUTPUT_OPEN_DRAIN║
╚══════════════════════════════════════╝
[初始化] 所有GPIO已设为OUTPUT_OPEN_DRAIN+HIGH(高阻态)
[初始化] 安全策略：平时断开，动作拉低，绝对不输出3.3V
✅ ML307R通信正常
╔══════════════════════════════════════╗
║  初始化完成，进入指令监听模式         ║
╚══════════════════════════════════════╝
```

2. 发测试指令验证（不实际操作GPIO）：
```
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=test_lock
```
串口应显示：`[测试] 模拟锁车脉冲触发（GPIO13单次）`

3. 发实际控制指令（已验证）：
```
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=unlock
```
串口应显示：
```
[指令解析] 收到命令: 'unlock'
>>> 执行：解锁（GPIO13双次脉冲）
[触发] pin=13 执行双次脉冲(LOW×2,间隔100ms)
[触发] pin=13 第1次 LOW (拉低300ms)
[触发] pin=13 第1次完成，间隔100ms
[触发] pin=13 第2次 LOW (拉低300ms)
[触发] pin=13 第2次完成
[自动锁车] 已启动，10分钟后执行
```

---

## 相关文档

- [ESP32-ML307R接线文档](https://github.com/s159753s8/tesla-esp32-firmware/wiki/ESP32-ML307R-接线文档)
- [ML307R MQTT AT指令参考](https://github.com/s159753s8/tesla-esp32-firmware/wiki/ML307R-MQTT-AT指令)
- [需求总纲](https://github.com/s159753s8/tesla-esp32-firmware/wiki/特斯拉-Model-X-100D-需求总纲)
- [实施路线](https://github.com/s159753s8/tesla-esp32-firmware/wiki/特斯拉项目-实施路线)
- [Mosquitto MQTT Broker搭建SOP](https://github.com/s159753s8/tesla-esp32-firmware/wiki/Mosquitto-MQTT-Broker搭建SOP)
- [HTTP-MQTT桥接部署SOP](https://github.com/s159753s8/tesla-esp32-firmware/wiki/HTTP-MQTT桥接部署SOP)
- [v3.0固件更新记录](https://github.com/s159753s8/tesla-esp32-firmware/wiki/v3.0固件更新记录)
- [阶段性胜利记录](https://github.com/s159753s8/tesla-esp32-firmware/wiki/阶段性胜利记录)

---

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| v3.0 | 2026-06-02 | 开漏输出版，triggerOnce/Twice函数，测试指令，防竞争锁 |
| v2.2 | 2026-06-02 | GPIO13改为OUTPUT+HIGH模式（和GPIO2一致） |
| v2.1 | 2026-06-02 | GPIO2测试版，直驱LED |
| v2.0 | 2026-06-02 | GPIO状态打印，全部GPIO支持 |
| v1.0 | 2026-06-02 | 初版，MQTT双向通信打通 |

---

## License

MIT License

---

## 作者

- 项目作者：s159753s8
- 协作AI：Hermes Agent
- 项目时间：2026-06-02

## 状态指示

- 串口输出 `✅ ML307R通信正常` = 4G模块正常工作
- 串口输出 `>>> 执行：锁车` = 收到指令并执行
- 串口输出 `🔊 播放语音：ac_on` = 语音指令播放中

## 需要配合的硬件

- [x] ~~继电器模块~~ → **已否定方案**：改用纯IO直连（开漏输出）
- [x] 物联网SIM卡（ML307R用）→ ✅ **已购**
- [ ] 音频输出模块（MAX98357A 或 DAC）→ 待购
- [ ] 3W喇叭 → 待购
- [ ] 方向盘按钮信号线 → 待接入

## 相关文档

- [ESP32-ML307R接线文档](https://github.com/s159753s8/tesla-esp32-firmware/wiki)
- [MQTT Broker搭建SOP](./docs/Mosquitto-MQTT-Broker搭建SOP.md)
- [特斯拉语音控制方案](./docs/语音模拟器方案.md)