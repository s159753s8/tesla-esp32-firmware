# iOS快捷指令配置

> 在 iPhone 上配置 Siri 语音控制特斯拉

---

## 创建快捷指令

### 1. 解锁快捷指令

1. 打开「快捷指令」App
2. 点击右上角 + 创建新快捷指令
3. 添加动作：
   - 搜索"获取URL内容"
   - URL: `http://www.owill.shopping:45321/publish?topic=tesla/control&msg=unlock`
   - 方法: GET
4. 命名快捷指令为"解锁特斯拉"
5. 启用"添加到主屏幕"（可选）

### 2. 锁车快捷指令

同上，URL 改为：
```
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=lock
```

命名为"锁车"

### 3. 鸥翼门快捷指令

URL：
```
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=left_open
```
命名为"左鹰翼门"

### 4. 后备箱快捷指令

URL：
```
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=trunk_open
```
命名为"后备箱"

## Siri 语音控制

创建快捷指令后，对 Siri 说：
- "嘿Siri，解锁特斯拉"
- "嘿Siri，锁车"
- "嘿Siri，打开左鹰翼门"
- "嘿Siri，打开后备箱"

Siri 就会执行对应的快捷指令。

## 心跳保活（可选）

iOS 自动化：
1. 打开「快捷指令」→ 自动化
2. 创建"个人自动化"
3. 触发条件选择"快捷指令"
4. 在"解锁特斯拉"快捷指令执行后，自动发送心跳：
   ```
   http://www.owill.shopping:45321/publish?topic=tesla/control&msg=heartbeat
   ```
5. 设置为每5分钟重发一次心跳

这样可以防止 10分钟自动锁车误触发。