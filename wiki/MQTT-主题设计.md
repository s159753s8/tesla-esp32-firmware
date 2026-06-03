# MQTT 主题设计

> 项目使用的 MQTT 主题规划和消息格式

---

## 主题列表

| 主题 | 方向 | QoS | 用途 |
|------|------|-----|------|
| `tesla/control` | 手机 → ESP32 | 1 | 下发控制指令 |
| `tesla/status` | ESP32 → 手机 | 1 | 上报执行结果 |
| `tesla/heartbeat` | 手机 → ESP32 | 1 | 重置自动锁车计时器 |

## 控制指令 (tesla/control)

ESP32 订阅此主题，消息内容是简单的字符串指令：

| 消息内容 | 行为 | GPIO | 时序 |
|---------|------|------|------|
| `lock` | 锁车 | GPIO13 | 单次300ms |
| `unlock` | 解锁 | GPIO13 | 两次300ms（间隔100ms）|
| `left_open` | 左鹰翼门 | GPIO14 | 两次300ms |
| `right_open` | 右鹰翼门 | GPIO25 | 两次300ms |
| `trunk_open` | 后备箱 | GPIO26 | 两次300ms |
| `frunk_open` | 前备箱 | GPIO27 | 两次300ms |
| `heartbeat` | 重置计时器 | - | - |
| `test_lock` | 测试 | - | 串口打印，不实际控制 |
| `test_unlock` | 测试 | - | 串口打印，不实际控制 |
| `test_left` | 测试 | - | 串口打印，不实际控制 |
| `test_right` | 测试 | - | 串口打印，不实际控制 |
| `test_trunk` | 测试 | - | 串口打印，不实际控制 |
| `test_frunk` | 测试 | - | 串口打印，不实际控制 |

## 状态上报 (tesla/status)

ESP32 发布此主题，反馈执行结果：

| 消息内容 | 触发时机 |
|---------|----------|
| `ESP32已上线` | 启动时 |
| `locked` | 锁车完成 |
| `unlocked` | 解锁完成 |
| `left_open_ok` | 左鹰翼门完成 |
| `right_open_ok` | 右鹰翼门完成 |
| `trunk_open_ok` | 后备箱完成 |
| `frunk_open_ok` | 前备箱完成 |
| `auto_lock_timer_started` | 解锁后启动10分钟计时 |
| `auto_locked` | 10分钟自动锁车执行完成 |
| `heartbeat` | 每30秒心跳 |
| `heartbeat_ok` | 收到心跳重置 |
| `unknown_command` | 收到未知指令 |

## ML307R URC 格式（订阅消息通知）

当 MQTT 收到消息时，ML307R 通过串口输出 URC：

```
+MQTTURC: "publish",<connID>,<qos>,"<topic>",<msg_len>,<msg>
```

**示例**：
```
+MQTTURC: "publish",0,1,"tesla/control",6,6,unlock
```
- `connID=0`：连接ID
- `qos=1`：QoS等级
- `topic=tesla/control`：主题
- `msg_len=6`：消息长度（"unlock"是6字符）
- `msg=unlock`：消息内容

**代码解析**：
```cpp
int lastComma = msg.lastIndexOf(',');
if (lastComma > 0) {
  String cmd = msg.substring(lastComma + 1);
  cmd.trim();
  // cmd = "unlock"
}
```

## ML307R 发布格式

```cpp
AT+MQTTPUB=<connID>,"<topic>",<qos>,<retain>,<dup>,<msg_len>,"<message>"
```

**示例**：
```
AT+MQTTPUB=0,"tesla/status",0,0,0,8,"unlocked"
```
- `connID=0`：连接ID
- `topic=tesla/status`：目标主题
- `qos=0`：QoS等级
- `retain=0`：不保留
- `dup=0`：非重发
- `msg_len=8`：消息长度（"unlocked"是8字符）
- `message=unlocked`：消息内容

**注意**：消息内容必须用双引号包围，长度独立参数！