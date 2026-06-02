#!/usr/bin/env python3
"""
特斯拉远程控制 - HTTP → MQTT 桥接服务
iPhone快捷指令通过HTTP请求调用，发布MQTT消息到ESP32

用法: python3 mqtt-http-bridge.py
HTTP接口: GET http://192.168.0.112:5000/publish?topic=tesla/control&msg=unlock
"""

import json
import traceback
from http.server import HTTPServer, BaseHTTPRequestHandler
from paho.mqtt import publish
import sys

# ========== 配置 ==========
MQTT_BROKER = "127.0.0.1"
MQTT_PORT = 1883

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/publish"):
            try:
                from urllib.parse import parse_qs, urlparse
                parsed = urlparse(self.path)
                params = parse_qs(parsed.query)
                
                topic = params.get("topic", [""])[0]
                msg = params.get("msg", [""])[0]
                
                if not topic or not msg:
                    self.send_response(400)
                    self.send_header("Content-Type", "application/json")
                    self.end_headers()
                    self.wfile.write(json.dumps({"error": "缺少topic或msg参数"}).encode())
                    return
                
                # 发布MQTT消息
                publish.single(
                    topic,
                    payload=msg,
                    qos=1,
                    hostname=MQTT_BROKER,
                    port=MQTT_PORT
                )
                
                print(f"[桥接] 发布成功 → topic={topic}, msg={msg}")
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"status": "ok", "topic": topic, "msg": msg}).encode())
                
            except Exception as e:
                print(f"[桥接] 错误: {e}")
                traceback.print_exc()
                self.send_response(500)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"error": str(e)}).encode())
        else:
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({
                "service": "MQTT-HTTP Bridge",
                "endpoints": ["/publish?topic=xxx&msg=yyy"],
                "example": "/publish?topic=tesla/control&msg=unlock"
            }).encode())
    
    def log_message(self, format, *args):
        print(f"[HTTP] {args[0]}")

if __name__ == "__main__":
    PORT = 5000
    server = HTTPServer(("", PORT), Handler)
    print(f"🚀 MQTT-HTTP桥接服务启动，端口：{PORT}")
    print(f"   示例: curl 'http://127.0.0.1:{PORT}/publish?topic=tesla/control&msg=unlock'")
    sys.stdout.flush()
    server.serve_forever()