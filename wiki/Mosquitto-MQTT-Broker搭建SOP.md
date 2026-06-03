# Mosquitto MQTT Broker 搭建SOP

> Docker部署Mosquitto，端口1883（MQTT）和8084（WSS）

---

## 部署位置

- 宿主机：192.168.0.112
- Docker网络：--network host（直接使用宿主机网络）

## 端口说明

| 端口 | 协议 | 用途 |
|------|------|------|
| 1883 | MQTT | 标准MQTT协议，ESP32连接用 |
| 8084 | WSS | WebSocket over TLS，MQTTX Web用 |
| 9001 | WebSocket | 旧版WebSocket（备用）|

## 配置文件

位置：`~/mosquitto/config/mosquitto.conf`

关键配置：
```conf
# 允许匿名访问（仅内网使用）
allow_anonymous true

# 监听1883端口（MQTT）
listener 1883 0.0.0.0

# 监听8084端口（WSS）
listener 8084 0.0.0.0
protocol websockets
cafile /mosquitto/certs/ca.crt
certfile /mosquitto/certs/server.crt
keyfile /mosquitto/certs/server.key
```

## 启动命令

```bash
docker run -d \
  --name mosquitto \
  --network host \
  --restart always \
  -v ~/mosquitto/config:/mosquitto/config \
  -v ~/mosquitto/data:/mosquitto/data \
  -v ~/mosquitto/certs:/mosquitto/certs \
  eclipse-mosquitto
```

## 测试连接

```bash
# MQTTX Web方式（推荐，免安装）
# 浏览器打开 https://www.owill.shopping:8084
# 接受证书，输入连接信息

# 或用mosquitto-clients
mosquitto_sub -h 192.168.0.112 -p 1883 -t 'tesla/#' -v
```

## TLS 证书

使用自签名证书，路径：`~/mosquitto/certs/`
- `ca.crt`：CA证书
- `server.crt`：服务器证书
- `server.key`：服务器私钥

## 验证命令

```bash
# 检查Mosquitto是否运行
docker ps | grep mosquitto

# 查看日志
docker logs mosquitto
```

## 路由器端口映射

| 外部 | 内部 | 协议 |
|------|------|------|
| 1883 → 192.168.0.112:1883 | TCP | MQTT |
| 8084 → 192.168.0.112:8084 | TCP | WSS |