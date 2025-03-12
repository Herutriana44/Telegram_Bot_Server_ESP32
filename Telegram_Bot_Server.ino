#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <UrlEncode.h>

// ⚡ Konfigurasi WiFi
const char* ssid = "SSID WIFI";
const char* password = "PASSWORD WIFI";

// ⚡ Token Bot Telegram (dapatkan dari @BotFather)
const String BOT_TOKEN = "BOT_TOKEN_TELEGRAM";

// ⚡ Chat ID pengguna (dapatkan dari @userinfobot atau API getUpdates)
const String CHAT_ID = "CHAT_ID_PENGGUNA"; // ini belum dinamis

// ⚡ API Key Gemini (dapatkan dari Google AI Studio)
const String GEMINI_API_KEY = "GEMINI_API_KEY";

// ⚡ Pin untuk kontrol perangkat (contoh LED di GPIO 2)
#define LED_PIN 2

// Variabel untuk menyimpan ID pesan terakhir (agar tidak mengulang membaca pesan lama)
long lastMessageID = 0;

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    
    pinMode(LED_PIN, OUTPUT);
    
    Serial.print("Menghubungkan ke WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\n✅ Terhubung ke WiFi!");
}

void loop() {
    checkTelegram();
    delay(5000); // Cek pesan setiap 5 detik
}

void checkTelegram() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("🚨 Tidak ada koneksi WiFi!");
        return;
    }

    String url = "https://api.telegram.org/bot" + BOT_TOKEN + "/getUpdates?offset=" + String(lastMessageID + 1);
    
    HTTPClient http;
    http.begin(url);
    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
        String response = http.getString();
        Serial.println("📩 Respon API Telegram: " + response);

        DynamicJsonDocument doc(2048);
        deserializeJson(doc, response);

        JsonArray results = doc["result"].as<JsonArray>();
        for (JsonObject message : results) {
            long messageID = message["update_id"];
            String chatID = String(message["message"]["chat"]["id"].as<long>());
            String text = message["message"]["text"].as<String>();

            if (messageID > lastMessageID) {
                lastMessageID = messageID;
                handleCommand(chatID, text);
            }
        }
    } else {
        Serial.println("⚠️ Gagal mengambil data Telegram: " + String(httpResponseCode));
    }
    http.end();
}

void handleCommand(String chatID, String text) {
    if (text == "/start") {
        sendMessage(chatID, "🤖 Bot ESP32 siap! Kirim /status atau /led_on /led_off.");
    } else if (text == "/status") {
        sendMessage(chatID, "🔋 ESP32 berjalan dengan baik! IP: " + String(WiFi.localIP().toString()));
    } else if (text == "/led_on") {
        digitalWrite(LED_PIN, HIGH);
        sendMessage(chatID, "💡 LED dinyalakan.");
    } else if (text == "/led_off") {
        digitalWrite(LED_PIN, LOW);
        sendMessage(chatID, "🌙 LED dimatikan.");
    } else {
        String geminiResponse = getGeminiResponse(text);
        Serial.println("📝 Balasan dari Gemini: " + geminiResponse);
        sendMessage(chatID, geminiResponse);
    }
}

String getGeminiResponse(String userInput) {
    if (WiFi.status() != WL_CONNECTED) return "❌ Tidak ada koneksi WiFi!";

    String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + GEMINI_API_KEY;
    HTTPClient http;
    http.begin(url);
    http.setTimeout(100000);  // ⏳ Timeout 10 detik
    http.addHeader("Content-Type", "application/json");

    // 🔹 Template Prompting untuk membatasi panjang respons
    String prompt = "Jawab dengan singkat dan jelas. Pastikan respons tidak lebih dari 3900 karakter.\n\nPertanyaan: " + userInput;

    // Membuat JSON request body untuk Gemini API
    DynamicJsonDocument doc(512);
    JsonObject content = doc.createNestedObject("contents");
    JsonArray parts = content.createNestedArray("parts");
    JsonObject textObj = parts.createNestedObject();
    textObj["text"] = prompt;

    String requestBody;
    serializeJson(doc, requestBody);

    int httpResponseCode = http.POST(requestBody);
    String response = "";

    if (httpResponseCode == 200) {
        response = http.getString();
        Serial.println("📩 Respon Gemini: " + response);

        DynamicJsonDocument responseDoc(4096);
        deserializeJson(responseDoc, response);

        // Parsing respons dari Gemini API
        response = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();

        // 🔹 Jika respons masih lebih dari 3900 karakter, dipotong
        if (response.length() > 3900) {
            response = response.substring(0, 3900) + "...";
            Serial.println("⚠️ Respons terlalu panjang, dipotong menjadi 3900 karakter.");
        }
    } else {
        Serial.println("⚠️ Gagal menghubungi Gemini API: " + String(httpResponseCode));
        response = "⚠️ Terjadi kesalahan saat menghubungi Gemini.";
    }

    http.end();
    return response;
}

void sendMessage(String chatID, String text) {
    if (WiFi.status() != WL_CONNECTED) return;

    String url = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage";

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Encode teks agar karakter khusus bisa dikirim
    String encodedText = urlEncode(text);

    String requestBody = "chat_id=" + chatID + "&text=" + encodedText;

    int httpResponseCode = http.POST(requestBody);
    
    if (httpResponseCode == 200) {
        Serial.println("✅ Pesan terkirim: " + text);
        blinkLED();  // 🔴 Blink LED saat pesan berhasil dikirim
    } else {
        Serial.println("⚠️ Gagal mengirim pesan.");
    }
    http.end();
}

// 🔴 Fungsi Blink LED (2x kedipan)
void blinkLED() {
    for (int i = 0; i < 2; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(300);
        digitalWrite(LED_PIN, LOW);
        delay(300);
    }
}
