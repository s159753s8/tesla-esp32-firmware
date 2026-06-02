FROM python:3.12-slim

WORKDIR /app
RUN pip install --no-cache-dir paho-mqtt==2.1.0

COPY mqtt-http-bridge.py .
EXPOSE 5000

CMD ["python3", "-u", "mqtt-http-bridge.py"]