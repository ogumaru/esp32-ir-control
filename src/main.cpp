#include <Arduino.h>
#include <IRremote.h>
#include <WiFi.h>
#include <stdint.h>

#include "credentials.hpp"
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "http_parser.h"

#define DECODE_NEC
#define DECODE_DISTANCE

// Pin Assignment
const uint8_t IR_RECEIVE_PIN = 15;
const uint8_t IR_SEND_PIN = 4;
const uint8_t LOAD_RESISTANCE = 33;

//  IR
const uint16_t IR_ADDRESS = 0x6d82;
const uint8_t IR_REPEATS = 4;

// HTTP Server
WiFiServer server(80);
const uint16_t MAX_BODY_LEN = 1024;
char body[MAX_BODY_LEN];
size_t bodylen = 0;
char url[128];

// Multi Tasking
bool is_request_end = false;
bool is_ir_locked = false;

//  Serial Port
const uint16_t SERIALPORT_BITRATE = 9600;

void process(void);

bool connect_wifi(void) {
  Serial.print("Connect to");
  WiFi.begin(SSID, PASSWORD);
  while (true) {
    delay(500);
    uint8_t status = WiFi.status();
    if (status == WL_CONNECTED) {
      Serial.println("Connected");
      Serial.println("IP: ");
      Serial.println(WiFi.localIP());
      return EXIT_SUCCESS;
    } else if (status == WL_CONNECT_FAILED) {
      Serial.println("Connect Failed");
      return EXIT_FAILURE;
    }
    Serial.print(".");
  }
}

void setup(void) {
  Serial.begin(SERIALPORT_BITRATE);

  connect_wifi();
  server.begin();

  IrReceiver.begin(IR_RECEIVE_PIN);
  IrSender.begin(IR_SEND_PIN, ENABLE_LED_FEEDBACK);
}

void emit_ir_ch1_toggle_three_state(void) {
  IrSender.sendNEC(IR_ADDRESS, 0xbf, IR_REPEATS);
}

void emit_ir_ch1_all_lights_up(void) {
  IrSender.sendNEC(IR_ADDRESS, 0xa6, IR_REPEATS);
}

void emit_ir_ch1_night_light(void) {
  IrSender.sendNEC(IR_ADDRESS, 0xbc, IR_REPEATS);
}

void emit_ir_ch1_toggle_on_off(void) {
  IrSender.sendNEC(IR_ADDRESS, 0xab, IR_REPEATS);
}

void receive_ir_data(void) {
  if (IrReceiver.decode()) {
    Serial.print(F("protocol: "));
    Serial.print(getProtocolString(IrReceiver.decodedIRData.protocol));
    Serial.print(F(" raw data: "));
    Serial.print(IrReceiver.decodedIRData.decodedRawData, HEX);
    Serial.print(F(", address: "));
    Serial.print(IrReceiver.decodedIRData.address, HEX);
    Serial.print(F(", command: "));
    Serial.println(IrReceiver.decodedIRData.command, HEX);
    IrReceiver.resume();
  }
}

void prevent_sleep(void) {
  static uint8_t time = 0;
  const uint8_t interval = 15;
  bool voltage = !(time > 0);
  delay(1000);

  if (time >= interval) {
    time = 0;
  } else {
    time++;
  }

  Serial.println(voltage);
  Serial.println(time);
  digitalWrite(LOAD_RESISTANCE, voltage);
}

void loop(void) {
  process();
  prevent_sleep();
}

void send_202(WiFiClient &client) {
  client.print("HTTP/1.1 202 Accepted\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Length: 0\r\n\r\n");
}

void send_400(WiFiClient &client) {
  client.print("HTTP/1.1 400 Bad Request\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Length: 0\r\n\r\n");
}

void send_404(WiFiClient &client) {
  client.print("HTTP/1.1 404 Not Found\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Length: 0\r\n\r\n");
}

void send_409(WiFiClient &client) {
  client.print("HTTP/1.1 409 Conflict\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Length: 0\r\n\r\n");
}

int on_url(http_parser *http_parser, const char *buf, size_t len) {
  if (sizeof(url) <= strlen(url) + len) {
    Serial.println("URL too long");
    return EXIT_FAILURE;
  }
  strncat(url, buf, len);

  return EXIT_SUCCESS;
}

int on_body(http_parser *http_parser, const char *buf, size_t len) {
  if (sizeof(body) < bodylen + len) {
    Serial.println("Body too long");
    return EXIT_FAILURE;
  }
  memcpy(body + bodylen, buf, len);
  bodylen += len;
  return EXIT_SUCCESS;
}

int on_message_complete(http_parser *http_parser) {
  is_request_end = true;
  return EXIT_SUCCESS;
}

int on_chunk_complete(http_parser *http_parser) {
  is_request_end = true;
  return EXIT_SUCCESS;
}

void task_ch1_toggle_light(void *) {
  Serial.println("send...");
  emit_ir_ch1_toggle_three_state();
  delay((RECORD_GAP_MICROS / 1000) + 5);
  is_ir_locked = false;
  Serial.println("send done");
  vTaskDelete(NULL);
}

void task_ch1_all_lights_up(void *) {
  Serial.println("send...");
  emit_ir_ch1_all_lights_up();
  delay((RECORD_GAP_MICROS / 1000) + 5);
  is_ir_locked = false;
  Serial.println("send done");
  vTaskDelete(NULL);
}

void task_ch1_night_light(void *) {
  Serial.println("send...");
  emit_ir_ch1_night_light();
  delay((RECORD_GAP_MICROS / 1000) + 5);
  is_ir_locked = false;
  Serial.println("send done");
  vTaskDelete(NULL);
}

void task_ch1_off(void *) {
  Serial.println("send...");
  emit_ir_ch1_toggle_on_off();
  delay((RECORD_GAP_MICROS / 1000) + 5);
  is_ir_locked = false;
  Serial.println("send done");
  vTaskDelete(NULL);
}

void task_ir_receive(void *) {
  Serial.println("wait ir signal...");
  receive_ir_data();
  is_ir_locked = false;
  Serial.println("recv done");
  vTaskDelete(NULL);
}

void process(void) {
  char buf[1024];
  http_parser parser;
  http_parser_settings settings;
  WiFiClient client = server.available();
  bool is_error = false;

  memset(url, 0, sizeof(url));
  is_request_end = false;
  bodylen = 0;
  http_parser_init(&parser, HTTP_REQUEST);
  http_parser_settings_init(&settings);
  settings.on_url = on_url;
  settings.on_body = on_body;
  settings.on_message_complete = on_message_complete;
  settings.on_chunk_complete = on_chunk_complete;

  if (!client) return;
  while (client.connected()) {
    if (client.available()) {
      size_t nread = client.readBytes(buf, sizeof(buf));
      size_t nparsed = http_parser_execute(&parser, &settings, buf, nread);
      if (nread != nparsed || parser.http_errno != HPE_OK) {
        is_error = true;
        break;
      }
      if (is_request_end) {
        break;
      }
    }
  }

  Serial.println(url);
  if (!is_request_end || is_error) {
    Serial.println("400");
    send_400(client);
  } else if (is_ir_locked) {
    send_409(client);
    Serial.println("409");
  } else if (strcmp(url, "/ch1/toggle") == 0) {
    is_ir_locked = true;
    xTaskCreate(task_ch1_toggle_light, "task_ch1_toggle_light", 2048, NULL, 10, NULL);
    send_202(client);
  } else if (strcmp(url, "/ch1/all_up") == 0) {
    is_ir_locked = true;
    xTaskCreate(task_ch1_all_lights_up, "task_ch1_all_lights_up", 2048, NULL, 10, NULL);
    send_202(client);
  } else if (strcmp(url, "/ch1/night") == 0) {
    is_ir_locked = true;
    xTaskCreate(task_ch1_night_light, "task_ch1_night_light", 2048, NULL, 10, NULL);
    send_202(client);
  } else if (strcmp(url, "/ch1/off") == 0) {
    is_ir_locked = true;
    xTaskCreate(task_ch1_off, "task_ch1_off", 2048, NULL, 10, NULL);
    send_202(client);
  } else if (strcmp(url, "/recv") == 0) {
    is_ir_locked = true;
    xTaskCreate(task_ir_receive, "task_ir_receive", 2048, NULL, 10, NULL);
    send_202(client);
  } else {
    send_404(client);
    Serial.println("404");
  }

  client.flush();
  client.stop();
}
