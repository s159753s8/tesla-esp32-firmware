# 特斯拉 Model X 100D 远程控制 - ESP32 固件

## 项目简介

通过 ESP32 + ML307R 4G 模块实现特斯拉 Model X 100D 的远程控制。

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

## MQTT 配置

- 服务器：`www.owill.shopping`
- 端口：`1883`
- 订阅主题：`tesla/control`
- 发布主题：`tesla/status`

## Arduino IDE 设置

1. **开发板**：工具 → 开发板 → ESP32 Arduino → `ESP32 Dev Module`
2. **端口**：选择对应的 COM 端口
3. **波特率**：串口监视器选 `115200`

## 烧录步骤

1. 打开 Arduino IDE
2. 新建sketch，复制 `tesla-esp32-firmware.ino` 内容粘贴
3. 工具 → 开发板 → 选 `ESP32 Dev Module`
4. 工具 → 端口 → 选 COM 号
5. 上传（Ctrl+U）
6. 上传完成后打开串口监视器（115200波特率）查看日志

## 支持的指令

| 指令 | 功能 |
|------|------|
| `lock` | 锁车 |
| `unlock` | 解锁 |
| `heartbeat` | 心跳保活 |

## 状态指示

- 串口输出 `✅ ML307R通信正常` = 4G模块正常工作
- 串口输出 `>>> 执行：锁车` = 收到指令并执行

## 需要配合的硬件

- [ ] 继电器模块（控制钥匙按钮）
- [ ] 物联网SIM卡（ML307R用）

## 相关文档

- [ESP32-ML307R接线文档](https://github.com/s159753s8/tesla-esp32-firmware/wiki)
- [MQTT Broker搭建SOP](./docs/Mosquitto-MQTT-Broker搭建SOP.md)