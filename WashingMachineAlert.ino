#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>

#include <HTTPClient.h>

#include <WiFiClientSecure.h>
#include <arduinoFFT.h>

#include "config.h"

// get it with openssl s_client -showcerts -connect discordapp.com:443
const char *rootCACertificate =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDzTCCArWgAwIBAgIQCjeHZF5ftIwiTv0b7RQMPDANBgkqhkiG9w0BAQsFADBa\n"
    "MQswCQYDVQQGEwJJRTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJl\n"
    "clRydXN0MSIwIAYDVQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTIw\n"
    "MDEyNzEyNDgwOFoXDTI0MTIzMTIzNTk1OVowSjELMAkGA1UEBhMCVVMxGTAXBgNV\n"
    "BAoTEENsb3VkZmxhcmUsIEluYy4xIDAeBgNVBAMTF0Nsb3VkZmxhcmUgSW5jIEVD\n"
    "QyBDQS0zMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEua1NZpkUC0bsH4HRKlAe\n"
    "nQMVLzQSfS2WuIg4m4Vfj7+7Te9hRsTJc9QkT+DuHM5ss1FxL2ruTAUJd9NyYqSb\n"
    "16OCAWgwggFkMB0GA1UdDgQWBBSlzjfq67B1DpRniLRF+tkkEIeWHzAfBgNVHSME\n"
    "GDAWgBTlnVkwgkdYzKz6CFQ2hns6tQRN8DAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0l\n"
    "BBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBIGA1UdEwEB/wQIMAYBAf8CAQAwNAYI\n"
    "KwYBBQUHAQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5j\n"
    "b20wOgYDVR0fBDMwMTAvoC2gK4YpaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL09t\n"
    "bmlyb290MjAyNS5jcmwwbQYDVR0gBGYwZDA3BglghkgBhv1sAQEwKjAoBggrBgEF\n"
    "BQcCARYcaHR0cHM6Ly93d3cuZGlnaWNlcnQuY29tL0NQUzALBglghkgBhv1sAQIw\n"
    "CAYGZ4EMAQIBMAgGBmeBDAECAjAIBgZngQwBAgMwDQYJKoZIhvcNAQELBQADggEB\n"
    "AAUkHd0bsCrrmNaF4zlNXmtXnYJX/OvoMaJXkGUFvhZEOFp3ArnPEELG4ZKk40Un\n"
    "+ABHLGioVplTVI+tnkDB0A+21w0LOEhsUCxJkAZbZB2LzEgwLt4I4ptJIsCSDBFe\n"
    "lpKU1fwg3FZs5ZKTv3ocwDfjhUkV+ivhdDkYD7fa86JXWGBPzI6UAPxGezQxPk1H\n"
    "goE6y/SJXQ7vTQ1unBuCJN0yJV0ReFEQPaA1IwQvZW+cwdFD19Ae8zFnWSfda9J1\n"
    "CZMRJCQUzym+5iPDuI9yP+kHyCREU3qzuWFloUwOxkgAyXVjBYdwRVKD05WdRerw\n"
    "6DEdfgkfCv4+3ao8XnTSrLE=\n"
    "-----END CERTIFICATE-----\n";

WiFiMulti WiFiMulti;

//// FFT vars
#define SAMPLES                                                                \
  2048 // SAMPLES-pt FFT. Must be a base 2 number. Limited by flash memory.
#define SAMPLING_FREQUENCY                                                     \
  7000 // Ts = Based on Nyquist, must be 2 times the highest expected frequency.

arduinoFFT FFT = arduinoFFT();

unsigned int samplingPeriod;
unsigned long microSeconds;

double vReal[SAMPLES]; // create vector of size SAMPLES to hold real values
double vImag[SAMPLES]; // create vector of size SAMPLES to hold imaginary values

uint8_t received_starting = 0;
uint8_t received_ending = 0;
unsigned int sum = 0;
////

const int potPin = 34;
unsigned long last = micros();

void publishMessage(String message) {
  WiFiClientSecure *client = new WiFiClientSecure;
  if (client) {
    client->setCACert(rootCACertificate);
    {
      HTTPClient https;

      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, DISCORD_BOT_HTTPS_URL)) { // HTTPS
        Serial.print("[HTTPS] POST...\n");
        // start connection and send HTTP header
        https.addHeader("Content-Type", "application/json");
        const int httpCode =
            https.POST(String("{\"content\": \"") + message + String("\"}"));

        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been
          // handled
          Serial.printf("[HTTPS] POST... code: %d\n", httpCode);

          // file found at server
          if (httpCode == HTTP_CODE_OK ||
              httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = https.getString();
            Serial.println(payload);
          }
        } else {
          Serial.printf("[HTTPS] POST... failed, error: %s\n",
                        https.errorToString(httpCode).c_str());
        }

        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }
    }
    delete client;
  } else {
    Serial.println("Unable to create client");
  }
}

void setup() {

  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(SSID, PASSWORD);

  // wait for WiFi connection
  while ((WiFiMulti.run() != WL_CONNECTED)) {
  }
  Serial.println("connected");
  samplingPeriod =
      round(1000000.0 / SAMPLING_FREQUENCY); // Period in microseconds
}

void loop() {
  sum = 0;
  /*Sample SAMPLES times*/
  for (int i = 0; i < SAMPLES; i++) {
    microSeconds = micros(); // Returns the number of microseconds since the
                             // Arduino board began running the current script.

    vReal[i] = analogRead(potPin); // Reads the value from analog pin 0 (A0),
                                   // quantize it and save it as a real term.
    vImag[i] = 0; // Makes imaginary term 0 always
    sum += vReal[i];
    /*remaining wait time between samples if necessary*/
    while (micros() < (microSeconds + samplingPeriod)) {
      // do nothing
    }
  }
  sum /= SAMPLES;

  // filtering too low and saturated volumes
  if (sum < 400 || sum > 3800) {
    return;
  }

  /*Perform FFT on samples*/
  FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);

  /*Find peak frequency and print peak*/
  const double peak = FFT.MajorPeak(vReal, SAMPLES, SAMPLING_FREQUENCY);

  // Starting washing machine frequency
  if (peak < 2700 && peak > 2600) {
    if (received_starting > -1) {
      Serial.println("Washing machine started!");
      Serial.print("sum: ");
      Serial.println(sum);
      publishMessage("Washing machine started!");
      // wait for 30 sec so we don't double detect
      delay(30000);
    }
    ++received_starting;
  } else {
    received_starting = 0;

    // if(peak < 2000 && peak > 1863){
    // if(peak < 1800 && peak > 1700){
    if (peak < 2000 && peak > 1930) {
      if (received_ending > -1) {
        Serial.println("Washing machine ended!");
        publishMessage("Washing machine ended!");
        // wait for 60 sec
        delay(60000);
      }

      ++received_ending;
    } else {
      received_ending = 0;
    }
  }

  Serial.print("peak: ");
  Serial.print(peak); // Print out the most dominant frequency.
  Serial.print(" hz, sum: ");
  Serial.println(sum);
}
