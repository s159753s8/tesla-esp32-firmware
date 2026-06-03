# HTTP-MQTT 桥接服务部署SOP

> 部署 mqtt-http-bridge.py 桥接服务，端口45321

---

## 作用

将 HTTP GET 请求转换为 MQTT 消息，让 iOS 快捷指令（不支持原生 MQTT）能间接控制 ESP32。

## 部署位置

- 宿主机：192.168.0.112
- 端口：**45321**（不用5000，常用端口可能被占用）
- Docker网络：--network host

## HTTP 接口

### URL格式
```
http://<host>:<port>/publish?topic=<topic>&msg=<message>
```

### 示例
```
http://www.owill.shopping:45321/publish?topic=tesla/control&msg=unlock
http://127.0.0.1:45321/publish?topic=tesla/control&msg=lock
```

### 响应
```json
{"status": "ok", "topic": "tesla/control", "msg": "unlock"}
```

## 文件位置

`/home/ow/DIY项目/特斯拉4G/mqtt-http-bridge.py`

## Docker 构建

```bash
cd /home/ow/DIY项目/特斯拉4G
docker build -t mqtt-http-bridge .
docker run -d \
  --name mqtt-http-bridge \
  --network host \
  --restart always \
  mqtt-http-bridge
```

## 验证命令

```bash
# 本地测试
curl http://127.0.0.1:45321/

# 应返回: {"service": "MQTT-HTTP Bridge", ...}

# 发控制指令
curl "http://127.0.0.1:45321/publish?topic=tesla/control&msg=unlock"

# 应返回: {"status": "ok", ...}
```

## 路由器端口映射

| 外部 | 内部 | 协议 |
|------|------|------|
| **45321** → 192.168.0.112:**45321** | TCP | HTTP桥接 |

## 端口变更历史

- 原计划：5000（被用户拒绝，常用端口）
- 现使用：**45321**（高位随机端口，避免扫描和占用）