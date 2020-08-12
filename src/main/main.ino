#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "sdk_structs.h"
#include "ieee80211_structs.h"
#include "string_utils.h"
#include "user_interface.h"

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <DS1302.h>

#include <SPI.h>
#include <SD.h>

#define a 100

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 10800);

String macList[a];
String apList[a];

String filename = "";

int j = 0;
int k = 0;

unsigned long zamanbir = 0;

//CE/RST, DATA, CLK
DS1302 rtc(0, 4, 5);

const int chipSelect = 4;

// According to the SDK documentation, the packet type can be inferred from the
// size of the buffer. We are ignoring this information and parsing the type-subtype
// from the packet header itself. Still, this is here for reference.

wifi_promiscuous_pkt_type_t packet_type_parser(uint16_t len)
{
  switch (len)
  {
    // If only rx_ctrl is returned, this is an unsupported packet
    case sizeof(wifi_pkt_rx_ctrl_t):
      return WIFI_PKT_MISC;

    // Management packet
    case sizeof(wifi_pkt_mgmt_t):
      return WIFI_PKT_MGMT;

    // Data packet
    default:
      return WIFI_PKT_DATA;
  }
}


// In this example, the packet handler function does all the parsing and output work.
// This is NOT ideal.

void wifi_sniffer_packet_handler(uint8_t *buff, uint16_t len)
{
  // First layer: type cast the received buffer into our generic SDK structure
  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
  // Second layer: define pointer to where the actual 802.11 packet is within the structure
  const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
  // Third layer: define pointers to the 802.11 packet header and payload
  const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
  const uint8_t *data = ipkt->payload;

  // Pointer to the frame control section within the packet header
  const wifi_header_frame_control_t *frame_ctrl = (wifi_header_frame_control_t *)&hdr->frame_ctrl;

  // Parse MAC addresses contained in packet header into human-readable strings
  char addr2[] = "00:00:00:00:00:00\0";

  mac2str(hdr->addr2, addr2);

  unsigned long zamanAP = millis();
  int boot = zamanAP - zamanbir;

  if (frame_ctrl->type == WIFI_PKT_MGMT && frame_ctrl->subtype == BEACON)
  {
    bool has = in_list(apList, (String)addr2, false);

    if (has == false && k < 100)
    {
      apList[k] = (String)addr2;
      k++;
    }

  }
  else
  {
    if (boot > 30000) {

      bool is_ap = in_list(apList, (String)addr2, true);

      if (is_ap == false)
      {
        String dataString = "";
        dataString += (String) addr2;
        dataString += " -> ";
        dataString += rtc.getDateStr();
        dataString += " *-* ";
        dataString += rtc.getTimeStr();

        File dataFile = SD.open(filename, FILE_WRITE);

        if (dataFile) {
          dataFile.println(dataString);
          dataFile.close();
          // print to the serial port too:
          Serial.print(j);
          Serial.print("  ->  ");
          Serial.println(dataString);
        }
        else {
          Serial.print("Error opening !!!!");
          Serial.print(filename);
          Serial.print("!!!! for add record");
        }

        bool has = in_list(macList, (String)addr2, false);

        if (has == false && j < 100)
        {
          macList[j] = (String)addr2;
          j++;
        }
      }
    }
  }
}

bool in_list(String list[], String el, bool ap_check)
{
  int i = 0;
  while (list[i] != '\0')
  {
    if (ap_check == false) {
      if (list[i] == el)
      {
        return true;
      }
    }
    else {
      String tmp = list[i].substring(0, 8);
      el = el.substring(0, 8);
      if (tmp == el) {
        return true;
      }
    }
    i++;
  }
  return false;
}

void print_list(String list[], int len) {
  for (int i = 0; i < len; i++) {
    Serial.println(list[i]);
  }
}

void connect_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin("orange", "orange123");
  Serial.println("Ağa Bağlanılıyor");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  Serial.println("Ağa Bağlandı");
}

void send_packet(char *host, String firstline, String data) {
  WiFiClient client;

  if (!client.connect(host, 80)) {
    Serial.println("Hosta Bağlanamadı");
    return;
  }

  client.println(firstline);
  client.print("Host: ");
  client.println(host);
  client.println("Accept: */*");
  client.println("Content-Type: text/plain");
  client.print("Content-Length: ");
  client.println(data.length());
  client.println();
  client.print(data);
  Serial.println("----------------------------------------------------");
  Serial.println("Respond");

  delay(500);

  while (client.available())
  {
    String line = client.readStringUntil('\r');
    Serial.println(line);
  }

  if (client.connected())
  {
    client.stop();
    Serial.println("Bağlantı Kapatıldı.");
  }
}

void initRTC() {
  rtc.halt(false);
  rtc.writeProtect(false);

  timeClient.begin();
  timeClient.update();

  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime);

  int cSec = ptm->tm_sec;
  int cMin = ptm->tm_min;
  int cHour = ptm->tm_hour;
  int cDay = ptm->tm_mday;
  int cMonth = ptm->tm_mon + 1;
  int cYear = ptm->tm_year + 1900;

  rtc.setTime(cHour, cMin, cSec);     // Set the time to 12:00:00 (24hr format)
  rtc.setDate(cDay, cMonth, cYear);   // Set the date to August 6th, 2010
  Serial.println("Saat ayarlandı");

  timeClient.end();
}

void initSnif() {
  wifi_set_channel(9);

  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(0);
  WiFi.disconnect();

  zamanbir = millis();
  j = 0;
  k = 0;

  Serial.println("Initializing SD card...");

  while (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
  }

  Serial.println("initialization done.");

  filename = "";
  filename += rtc.getDateStr();
  filename += "-";
  filename += rtc.getTimeStr();
  filename.replace(":", ".");
  filename += ".txt";

  Serial.print("-------------------FILENAME : ");
  Serial.println(filename);


  File iFile = SD.open(filename, FILE_WRITE);
  delay(300);
  String iFilename = "---" + filename + "---";

  if (iFile) {
    iFile.println(iFilename);
    iFile.close();
  }
  else {
    Serial.println("Error init file meta");
  }

  Serial.printf("\n\nDinleme Başlatılıyor...\n\n");
  Serial.println("-----------------------------------------------------------------------------");
  Serial.println("30 sn AP taraması başlatılıyor...");
  wifi_set_promiscuous_rx_cb(wifi_sniffer_packet_handler);
  wifi_promiscuous_enable(1);
}

void setup() {

  Serial.begin(115200);

  delay(10);

  connect_wifi();

  initRTC();

  Serial.println();
  Serial.println();

  initSnif();
}

void loop() {
  delay(10);

  if (j >= 90) {
    wifi_promiscuous_enable(0);

    File fFile = SD.open(filename, FILE_WRITE);
    String fFilename = "---" + filename + "---";
    if (fFile) {
      fFile.println(fFilename);
      fFile.close();
    }
    else {
      Serial.println("Error end file meta");
    }

    Serial.println("----------------------------AP LIST-------------------------        :  ");
    print_list(apList, k);

    Serial.println("----------------------------STA LIST------------------------        :  ");
    print_list(macList, j);

    connect_wifi();

    char *host = "webhook.site";
    String firstline = "POST /a8409f7d-35ac-47b6-8c30-9c53e7da9c24 HTTP/1.1";
    String data = fFilename;

    File rFile = SD.open(filename);
    if (rFile) {
      int rn = 0;
      while (rFile.available()) {
        if (rn == 100) {
          send_packet(host, firstline, data);
          data = fFilename;
          rn = 0;
        }
        data += rFile.readStringUntil('\n');
        rn++;
      }
      if (rn > 0 && rn < 100) {
        send_packet(host, firstline, data);
        data = "";
        rn = 0;
      }
      rFile.close();
    }
    else {
      Serial.print("Error opening !!!!");
      Serial.print(filename);
      Serial.print("!!!! for sending data to API");
    }
    Serial.println("Yeniden Tarama Başlatılıyor...");
    delay(5000);
    initSnif();
  }
}
