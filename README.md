# 特斯拉 Model X 100D 远程控制 - ESP32 固件

## 项目简介

通过 ESP32 + ML307R 4G 模块实现特斯拉 Model X 100D 的远程控制。

---

## 硬件接线

| ESP32 | → | ML307R |
|-------|---|--------|
| 5V | → | VIN（5V供电） |
| GND | → | GND |
| GPIO17(TX2) | → | RX |
| GPIO16(RX2) | → | TX |

**注意**：
- ML307R 需要 **5V 供电**，不是 3.3V！
- 使用 GPIO16/17（Serial2），不要用板子上标的 RX/TX（那是 GPIO1/3，用于烧录）

---

## 通信架构

```
MQTT Broker（Mosquitto on 192.168.0.112:1883）
    ↑
    │ 订阅：tesla/control
    │ 发布：tesla/status
    │
MQTT-HTTP 桥接（Python，端口45321）
    ↑
    │ HTTP POST /publish
    │
ESP32 + ML307R 4G模块（MQTT Client）
```

---

## 已实现功能

| 指令 | 功能 | 状态 |
|------|------|------|
| `lock` | 锁车（GPIO13 单次 LOW 300ms） | ✅ 已实现 |
| `unlock` | 解锁（GPIO13 两次 LOW） | ✅ 已实现 |
| `left_open` | 左鹰翼门（GPIO14 两次 LOW） | ✅ 已实现 |
| `right_open` | 右鹰翼门（GPIO25 两次 LOW） | ✅ 已实现 |
| `trunk_open` | 后备箱（GPIO26 两次 LOW） | ✅ 已实现 |
| `frunk_open` | 前备箱（GPIO27 两次 LOW） | ✅ 已实现 |
| `heartbeat` | 心跳保活，重置自动锁车计时器 | ✅ 已实现 |
| `auto_lock_10min` | 解锁后10分钟无心跳自动锁车 | ✅ 已实现 |

---

## 完整指令URL

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

## 进行中功能：语音模拟器（核心新方案）

### 方案概述

用 ESP32 + 喇叭 模拟语音指令，控制车机空调和其他功能。

**核心思路**：ESP32 = 语音遥控器，代替人说话去控制车机

```
用户按方向盘按钮
    ↓
车机发出"滴"声（这个信号可以触发ESP32）
    ↓
ESP32 收到触发信号
    ↓
ESP32 通过喇叭播放预录语音指令
    ↓
车机听到并执行命令
```

### 本地语音库（离线，省流量）

ESP32 存储预录语音片段，通过 MQTT 触发播放：

| 指令 | 语音内容 |
|------|---------|
| `ac_on` | "打开空调" |
| `ac_off` | "关闭空调" |
| `ac_21` | "21度" |
| `ac_22` | "22度" |
| `ac_23` | "23度" |
| `ac_24` | "24度" |
| `ac_auto` | "空调自动" |
| `ac_max` | "空调最大" |
| `ac_outer` | "外循环" |
| `ac_inner` | "内循环" |
| `seat_heat` | "座椅加热" |
| `seat_heat_off` | "关闭座椅加热" |
| `wheel_heat` | "方向盘加热" |
| `wheel_heat_off` | "关闭方向盘加热" |
| `ac_recording` | "打开录音模式" |

### 触发信号检测

车内有人时，需要检测「有人在车里」的信号：

| 信号源 | 难度 | 说明 |
|--------|------|------|
| 方向盘按钮 | ✅ 简单 | 默认高电平，按下拉低，触发播放语音 |
| 刹车踏板 | ✅ 简单 | 踏板开关信号 |
| 鹰翼门 | ✅ 简单 | 解锁后门会动，开关信号 |

**实现方式**：方向盘按钮默认高电平 → 按下时拉低 → ESP32 检测到下降沿 → 播放对应语音

### 硬件需求（新增）

| 组件 | 说明 |
|------|------|
| ESP32 DAC 或 MAX98357A I2S 模块 | 音频输出 |
| 3W 喇叭 | 语音播放 |
| 方向盘按钮信号线 | 触发信号输入（GPIO 输入） |

### ESP32 引脚分配（计划）

| GPIO | 功能 | 说明 |
|------|------|------|
| GPIO13 | 锁车/解锁 | 已验证 |
| GPIO14 | 左鹰翼门 | 已验证 |
| GPIO25 | 右鹰翼门 | 已验证 |
| GPIO26 | 后备箱 | 已验证 |
| GPIO27 | 前备箱 | 已验证 |
| GPIO34 | 方向盘按钮输入 | 新增（输入，上拉） |
| GPIO35 | 音频 DAC(I2S) | 新增 |

---

## 需要用户完成的硬件工作

1. **找到方向盘语音按钮**，测量按钮两根线的：
   - 默认电压（高还是低）
   - 按下时电压（拉高还是拉低）
   - 确认是按键开关信号（非触摸、非总线）

2. **准备音频组件**（三选一）：
   - 方案A：ESP32 内置 DAC（简单，便宜，但音质一般）
   - 方案B：MAX98357A I2S 模块（音质好，I2S通信）
   - 方案C：DFPlayer Mini MP3 模块（预存MP3，即插即用）

3. **拆开方向盘**，找到按钮线，跳出两根线接到 ESP32 GPIO34

---

## 状态指示

- 串口输出 `✅ ML307R通信正常` = 4G模块正常工作
- 串口输出 `>>> 执行：锁车` = 收到指令并执行
- 串口输出 `🔊 播放语音：ac_on` = 语音指令播放中

---

## 需要配合的硬件

- [x] ~~继电器模块~~ → **已否定方案**：改用纯IO直连（开漏输出）
- [x] 物联网SIM卡（ML307R用）→ ✅ **已购**
- [ ] 音频输出模块（MAX98357A 或 DAC）→ 待购
- [ ] 3W喇叭 → 待购
- [ ] 方向盘按钮信号线 → 待接入

---

## 相关文档

- [ESP32-ML307R接线文档](https://github.com/s159753s8/tesla-esp32-firmware/wiki)
- [MQTT Broker搭建SOP](./docs/Mosquitto-MQTT-Broker搭建SOP.md)
- [特斯拉语音控制方案](./docs/语音模拟器方案.md)