#include <AViShaESPCam.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid     = "h3";
const char* password = "rkar4424";

AViShaESPCam espcam;
WebServer server(80);

void handleStream() {
  WiFiClient client = server.client();

  server.sendContent(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n"
  );

  while (client.connected()) {
    FrameBuffer* frame = espcam.capture();
    if (!frame) continue;

    server.sendContent("--frame\r\n");
    server.sendContent("Content-Type: image/jpeg\r\n\r\n");
    client.write(frame->buf, frame->len);
    server.sendContent("\r\n");

    espcam.returnFrame(frame);
  }
}

void setup() {
  Serial.begin(115200);
  espcam.enableLogging(true);
  espcam.init(AI_THINKER(), QVGA);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Send IP over UART to Arduino
  Serial.println("STREAM:http://" + WiFi.localIP().toString() + "/stream");

  server.on("/stream", handleStream);
  server.begin();
}

void loop() {
  server.handleClient();
}