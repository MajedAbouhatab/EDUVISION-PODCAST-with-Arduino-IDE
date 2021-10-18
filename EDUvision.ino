#include <M5Core2.h>
#include <WiFiConnect.h>
#include <HTTPClient.h>
#include <Audio.h>
#include "images.h"

#define GetTextFromTag(T, S, P) S.substring(S.indexOf("<" T ">", P) + String(T).length() + 2, S.indexOf("</" T ">", P))

Gesture swipeRight("SwipeRight", 80, DIR_RIGHT, 30, true);
Gesture swipeLeft("SwipeLeft", 80, DIR_LEFT, 30, true);

WiFiConnect wc;
WiFiClientSecure client;
HTTPClient http;
Audio audio;

bool PlayingAudio;
int EpisodeIndex, MaxVal, TimeRemaining;
String EpisodeArray[60] = {};
unsigned long Timestamp;

// Write any text on LCD
void LCDText(const char *txt, int C, int X, int Y, int S)
{
    M5.Lcd.setTextColor(C);
    M5.Lcd.setCursor(X, Y);
    M5.Lcd.setTextSize(S);
    M5.Lcd.printf(txt);
}

void UpdatePlaying(void)
{
    // Write over old text with background color
    LCDText(!PlayingAudio ? "Pause" : "Play", BLACK, 25, 215, 2);
    // Write new text
    LCDText(PlayingAudio ? "Pause" : "Play", M5.Lcd.color565(0, 152, 157), 25, 215, 2);
}

void UpdateVolume(void)
{
    // Cover old text with background
    M5.lcd.fillRect(195, 175, 35, 25, M5.Lcd.color565(242, 102, 41));
    // Write new text
    LCDText(String(audio.getVolume()).c_str(), WHITE, 200, 180, 2);
}

void StartAudio(void)
{
    // Play from URL
    audio.connecttohost(EpisodeArray[EpisodeIndex + 2].c_str());
    // Will update in main loop
    TimeRemaining = int(EpisodeArray[EpisodeIndex + 1].toInt());
    // Clear screen
    M5.Lcd.fillScreen(BLACK);
    // Draw banner
    M5.lcd.drawJpg(ArduinoEducation, sizeof(ArduinoEducation));
    // Display title of episode
    LCDText(EpisodeArray[EpisodeIndex].c_str(), M5.Lcd.color565(0, 152, 157), 0, 80, 2);
    PlayingAudio = true;
    // Show it's playing
    UpdatePlaying();
    // Show the volume level
    UpdateVolume();
    // Static button labels
    LCDText(String("Vol-     Vol+").c_str(), M5.Lcd.color565(0, 152, 157), 140, 215, 2);
}

// Change episode by swiping
void gestureHandler(Event &e)
{
    if (EpisodeIndex > 0 && e.gesture->direction == DIR_RIGHT)
        EpisodeIndex -= 3;
    else if (EpisodeIndex < MaxVal && e.gesture->direction == DIR_LEFT)
        EpisodeIndex += 3;
    else
        return;
    // Stop current audio
    audio.stopSong();
    // Start new audio
    StartAudio();
}

void PlayPause(Event &e)
{
    PlayingAudio = !PlayingAudio;
    audio.pauseResume();
    UpdatePlaying();
}

void VolUp(Event &e)
{
    // It will automatically stop at max (21)
    audio.setVolume(audio.getVolume() + 1);
    UpdateVolume();
}

void VolDown(Event &e)
{
    // If we let it go below 0 it will return to max
    if (audio.getVolume() != 0)
    {
        audio.setVolume(audio.getVolume() - 1);
        UpdateVolume();
    }
}

void setup()
{
    M5.begin(true, true, true, true);
    // Intro
    M5.lcd.drawJpg(CoverPage, sizeof(CoverPage));
    LCDText("Trying to Connect\n        to Wi-Fi ...", M5.Lcd.color565(0, 152, 157), 50, 180, 2);
    swipeRight.addHandler(gestureHandler, E_GESTURE);
    swipeLeft.addHandler(gestureHandler, E_GESTURE);
    M5.BtnA.addHandler(PlayPause, E_TOUCH);
    M5.BtnB.addHandler(VolDown, E_TOUCH);
    M5.BtnC.addHandler(VolUp, E_TOUCH);
    // Needed for https
    client.setInsecure();
    audio.setPinout(12, 0, 2);
    audio.setVolume(10);
    // Try to connect to last Wi-Fi or wait until connected
    if (!wc.autoConnect())
    {
        M5.Lcd.fillScreen(BLACK);
        wc.setAPName("");
        LCDText("  Connect me to\n   your Wi-Fi", RED, 10, 10, 2);
        LCDText(("\n\n   SSID:\n   " + String(wc.getAPName())).c_str(), GREEN, M5.Lcd.getCursorX(), M5.Lcd.getCursorY(), 2);
        LCDText("\n\n   http://192.168.4.1", BLUE, M5.Lcd.getCursorX(), M5.Lcd.getCursorY(), 2);
        wc.startConfigurationPortal(AP_WAIT);
        ESP.restart();
    }
    http.begin(client, "https://anchor.fm/s/6859654c/podcast/rss");
    http.GET();
    int TextPointer;
    String TextNode, HttpString = http.getString();
    delay(5000);
    // Remove extra text
    HttpString.replace("<![CDATA[", "");
    HttpString.replace("]]>", "");
    while (HttpString.length())
    {
        TextPointer = HttpString.indexOf("<item>", TextPointer + 1);
        if (TextPointer < 0)
            break;
        TextNode = GetTextFromTag("item", HttpString, TextPointer);
        EpisodeArray[EpisodeIndex] = GetTextFromTag("title", TextNode, 0);
        EpisodeArray[EpisodeIndex + 1] = GetTextFromTag("itunes:duration", TextNode, 0);
        EpisodeArray[EpisodeIndex + 2] = TextNode.substring(TextNode.indexOf("enclosure url") + 15, TextNode.indexOf("length=") - 2);
        EpisodeIndex += 3;
    }
    http.end();
    MaxVal = EpisodeIndex - 3;
    EpisodeIndex = 0;
    M5.Axp.SetSpkEnable(true);
    StartAudio();
}

void loop()
{
    M5.update();
    audio.loop();
    if (millis() - Timestamp > 1000)
    {
        Timestamp = millis();
        if (audio.isRunning())
        {
            TimeRemaining--;
            M5.lcd.fillRect(20, 175, 75, 25, M5.Lcd.color565(242, 102, 41));
            M5.Lcd.setTextColor(WHITE);
            M5.Lcd.setCursor(30, 180);
            // It'll be less than an hour
            M5.Lcd.printf("%02d:%02d", int(TimeRemaining) / 60, TimeRemaining % 60);
        }
    }
}
