/*
  Made by SAKSHAM RAJPAL, Youtube Channel : iOT India
  YOUTUBE VIDEO LINK FOR GEMCAM AI : 

  🇮🇳 GEMCAM AI 🇮🇳

  This code captures an image using the ESP32-CAM module, processes it, 
  and sends it to Google's Gemini 2.0 Flash API for analysis. The API's response is 
  displayed on an OLED screen. The code includes features like Wi-Fi connectivity,
  image encoding, multiple prompts, flash mode and scrolling text display.

  Tested with:
  - Arduino IDE version 2.3.2
  - ESP32 boards package version 3.2.0
  - Adafruit GFX library version 1.11.11
  - Adafruit SSD1306 library version 2.5.13
  - ArduinoJson library version 7.4.1
  - Base64 library (default version with ESP32 boards package)

  Make sure to install these libraries and configure your environment 
  as specified above before running the code.
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include <Adafruit_GFX.h>  
#include <Wire.h>
#include <Adafruit_SSD1306.h> 
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "********";          // Your WiFi's name
const char* password = "********";      // Your WiFi's password

// Google Gemini API key
const String apiKey = "********";       // Your Gemini's Api Key

// Questions to be Asked about the image
String prompts[] = {
  "What can you see in this image?",
  "What is this?",
  "Solve it."
};
int currentPromptIndex = 0; // Default to first prompt (index 0)
bool flashEnabled = false;  // Default flash mode is OFF
bool isDisplayingResponse = false; // New flag to track if we're showing a response

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SCL 14
#define OLED_SDA 15
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin definitions for ESP32-CAM AI-Thinker module
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#define BUTTON_PIN 13
#define PROMPT_SELECTOR_BUTTON_PIN 2
#define FLASH_MODE_SELECTOR_BUTTON_PIN 12
#define FLASHLIGHT_PIN 4 

// Button debounce variables
unsigned long lastPromptButtonPress = 0;
unsigned long lastFlashButtonPress = 0;
unsigned long debounceDelay = 300; // Debounce time in milliseconds
unsigned long buttonPressStartTime = 0; // To track long press for cancellation

// Base64 encoding character set
const char* base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

// Function to encode data to base64
String encodeImageToBase64(uint8_t* data, size_t length) {
    String base64 = "";
    int i = 0;
    int j = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];

    while (length--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                base64 += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; j < i + 1; j++)
            base64 += base64_chars[char_array_4[j]];

        while (i++ < 3)
            base64 += '=';
    }

    return base64;
}

// Function to draw a thin border around the display
void drawBorder() {
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
}

void displayCenteredText(const String& text, int textSize = 1) {
  display.clearDisplay();
  drawBorder(); // Add thin border
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);

  int maxLineLength = 16;  // Assuming 16 characters fit per line at textSize 1
  String lineBuffer = "";
  String wordBuffer = "";
  int16_t x1, y1;
  uint16_t textWidth, textHeight;

  // Calculate line height
  display.getTextBounds("A", 0, 0, &x1, &y1, &textWidth, &textHeight);
  int lineHeight = textHeight + 2;

  // Calculate the total number of lines needed
  int lineCount = 0;
  for (size_t i = 0; i <= text.length(); i++) {
    char c = text.charAt(i);
    if (c == ' ' || c == '\n' || c == '\0') {
      if (lineBuffer.length() + wordBuffer.length() > maxLineLength) {
        lineCount++;
        lineBuffer = wordBuffer;
      } else {
        lineBuffer += (lineBuffer.isEmpty() ? "" : " ") + wordBuffer;
      }
      wordBuffer = "";

      if (c == '\n') {
        lineCount++;
        lineBuffer = "";
      }
    } else {
      wordBuffer += c;
    }
  }
  if (!lineBuffer.isEmpty()) lineCount++;  // Count the last line

  // Calculate the vertical offset to center the block of text
  int totalTextHeight = lineCount * lineHeight;
  int yOffset = (SCREEN_HEIGHT - totalTextHeight) / 2;

  // Render the text line by line, vertically centered
  int yPos = yOffset;
  lineBuffer = "";
  wordBuffer = "";
  for (size_t i = 0; i <= text.length(); i++) {
    char c = text.charAt(i);
    if (c == ' ' || c == '\n' || c == '\0') {
      if (lineBuffer.length() + wordBuffer.length() > maxLineLength) {
        // Render the current line
        display.setCursor((SCREEN_WIDTH - lineBuffer.length() * textWidth) / 2, yPos);
        display.print(lineBuffer);
        yPos += lineHeight;
        lineBuffer = wordBuffer;
      } else {
        lineBuffer += (lineBuffer.isEmpty() ? "" : " ") + wordBuffer;
      }
      wordBuffer = "";

      if (c == '\n' || c == '\0') {
        display.setCursor((SCREEN_WIDTH - lineBuffer.length() * textWidth) / 2, yPos);
        display.print(lineBuffer);
        yPos += lineHeight;
        lineBuffer = "";
      }
    } else {
      wordBuffer += c;
    }
  }

  display.display();
}

void displayMainScreen() {
  display.clearDisplay();
  drawBorder(); // Add thin border
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.print("PROMPT:");
  display.drawLine(2, 10, 44, 10, SSD1306_WHITE); 
  
  String currentPrompt = prompts[currentPromptIndex];
  
  int startX = 50; 
  int availableChars = 13; 
  
  if (currentPrompt.length() <= availableChars) {
    display.setCursor(startX, 2);
    display.print(currentPrompt);
  } else {
    display.setCursor(startX, 2);
    display.print(currentPrompt.substring(0, availableChars));
  
    String remainingText = currentPrompt.substring(availableChars);
    int maxCharsPerLine = 21; // Max chars per line
    
    // Split remaining text into lines
    for (int i = 0; i < remainingText.length(); i += maxCharsPerLine) {
      int lineNum = (i / maxCharsPerLine) + 1; // Line number (starting from 1)
      int endPos = min(i + maxCharsPerLine, (int)remainingText.length());
      
      // Only display up to 2 additional lines to prevent overflow
      if (lineNum <= 2) {
        display.setCursor(2, 12 + (lineNum - 1) * 10);
        display.print(remainingText.substring(i, endPos));
      }
    }
  }
  
  int middleY = 27; 
  String captureMsg = "Press button to";
  int16_t x1, y1;
  uint16_t textWidth, textHeight;
  display.getTextBounds(captureMsg, 0, 0, &x1, &y1, &textWidth, &textHeight);
  display.setCursor((SCREEN_WIDTH - textWidth) / 2, middleY);
  display.print(captureMsg);
  
  captureMsg = "capture image";
  display.getTextBounds(captureMsg, 0, 0, &x1, &y1, &textWidth, &textHeight);
  display.setCursor((SCREEN_WIDTH - textWidth) / 2, middleY + 10);
  display.print(captureMsg);
  
  String flashStatus = "Flash: ";
  flashStatus += flashEnabled ? "ON" : "OFF";
  display.getTextBounds(flashStatus, 0, 0, &x1, &y1, &textWidth, &textHeight);
  display.setCursor((SCREEN_WIDTH - textWidth) / 2, 50);
  display.print(flashStatus);
  
  display.display();
  
  isDisplayingResponse = false; // We're on the main screen now
}

void bootAnimation() {
  display.clearDisplay();
  drawBorder(); 
  
  String channelName = " iOT India";
  String projectTitleLine1 = "BOOTING UP THE";
  String projectTitleLine2 = " GEMCAM AI ";
  int16_t x1, y1;
  uint16_t textWidth, textHeight, title1Width, title2Width;
  
  display.setTextSize(2);
  display.getTextBounds(channelName, 0, 0, &x1, &y1, &textWidth, &textHeight);
  
  display.setTextSize(1);
  display.getTextBounds(projectTitleLine1, 0, 0, &x1, &y1, &title1Width, &textHeight);
  display.getTextBounds(projectTitleLine2, 0, 0, &x1, &y1, &title2Width, &textHeight);
  
  for (int step = 0; step <= SCREEN_WIDTH; step += 3) {
    display.clearDisplay();
    drawBorder(); 
    int channelX = (step * 2) - textWidth;  
    if (channelX > 0) channelX = 0;  
    
    
    int title1X = SCREEN_WIDTH - (step * 2);  
    int title2X = SCREEN_WIDTH - (step * 2) + (title1Width - title2Width) / 2;  
    
    // Stop titles at their final positions
    if (title1X < (SCREEN_WIDTH - title1Width) / 2) 
      title1X = (SCREEN_WIDTH - title1Width) / 2;  // Center line 1
    
    if (title2X < (SCREEN_WIDTH - title2Width) / 2) 
      title2X = (SCREEN_WIDTH - title2Width) / 2;  // Center line 2
    
    // Draw channel name
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(channelX, 5);
    display.print(channelName);
    
    // Draw project title (two lines)
    display.setTextSize(1);
    display.setCursor(title1X, 30);
    display.print(projectTitleLine1);
    
    display.setCursor(title2X, 40);
    display.print(projectTitleLine2);
    
    display.display();
    delay(5);
  }
  
  // Hold the final frame
  delay(2000);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  // Configure pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(PROMPT_SELECTOR_BUTTON_PIN, INPUT_PULLUP);
  pinMode(FLASH_MODE_SELECTOR_BUTTON_PIN, INPUT_PULLUP);
  pinMode(FLASHLIGHT_PIN, OUTPUT);
  
  digitalWrite(FLASHLIGHT_PIN, LOW); // Ensure flash is off at startup

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ;
  }

  // Display boot animation
  bootAnimation();
  displayCenteredText("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  displayCenteredText("WiFi Connected!",1.3);
  delay(2000);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    displayCenteredText("Camera Init Failed");
    return;
  }

  displayCenteredText("Camera Initialized");
  delay(2000);

  // Display main screen with initial settings
  displayMainScreen();
}

void captureAndAnalyzeImage() {
  Serial.println("Capturing image...");

  // Capture the image frame buffer
  camera_fb_t* fb = esp_camera_fb_get();  // Get the frame buffer
  if (!fb) {
    Serial.println("Camera capture failed");
    displayCenteredText("Capture Failed");
    return;
  }

  // After the new frame is obtained, ensure the buffer is returned (cleared)
  esp_camera_fb_return(fb);  // Release the frame buffer from the previous capture

  // Now, capture the new image
  fb = esp_camera_fb_get();  // Get the frame buffer again for the new image

  if (!fb) {
    Serial.println("Camera capture failed");
    displayCenteredText("Capture Failed");
    return;
  }

  Serial.println("Image captured");
  String base64Image = encodeImageToBase64(fb->buf, fb->len);

  // Use flash if enabled
  if (flashEnabled) {
    flash();
  }
  
  // Return the frame buffer after processing the image
  esp_camera_fb_return(fb);  // Return the frame buffer to free memory

  if (base64Image.isEmpty()) {
    Serial.println("Failed to encode the image!");
    displayCenteredText("Encode Failed");
    return;
  }
  // Send the image to Gemini for analysis
  AnalyzeImage(base64Image);
}

void AnalyzeImage(const String& base64Image) {
  Serial.println("Sending image for analysis...");
  displayCenteredText("Processing...");

  String result;

  // Prepare the payload for the Gemini API
  String url = "data:image/jpeg;base64," + base64Image;
  
  DynamicJsonDocument doc(4096);
  JsonArray contents = doc.createNestedArray("contents");
  JsonObject content = contents.createNestedObject();
  JsonArray parts = content.createNestedArray("parts");
  
  // Add text part (question) - use the currently selected prompt
  JsonObject textPart = parts.createNestedObject();
  textPart["text"] = prompts[currentPromptIndex];
  
  // Add image part
  JsonObject imagePart = parts.createNestedObject();
  JsonObject inlineData = imagePart.createNestedObject("inlineData");
  inlineData["mimeType"] = "image/jpeg";
  inlineData["data"] = base64Image;
  
  // Add generation config
  JsonObject genConfig = doc.createNestedObject("generationConfig");
  genConfig["maxOutputTokens"] = 400;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  // Send request and validate response
  if (sendPostRequest(jsonPayload, result)) {
    Serial.print("[Gemini] Response: ");
    Serial.println(result);

    // Clear the display before showing the new response
    display.clearDisplay();
    display.display();

    DynamicJsonDocument responseDoc(4096);
    deserializeJson(responseDoc, result);

    // Extract text from Gemini response
    String responseContent = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
    Serial.println("[Gemini] Parsed response: " + responseContent);

    // Set flag that we're displaying response now
    isDisplayingResponse = true;

    // Smooth scrolling and proper word wrapping
    display.clearDisplay();
    drawBorder(); // Add thin border
    int lineHeight = 8;     // Height of each line in pixels
    int maxLineChars = 21;  // Approx. max characters per line
    int visibleLines = 7;
    int scrollDelay = 3000;  // Delay for scrolling in milliseconds

    std::vector<String> lines;  // Store formatted lines for display

    // Split responseContent into words for word wrapping
    String word = "";
    String currentLine = "";

    for (int i = 0; i < responseContent.length(); i++) {
      char c = responseContent.charAt(i);
      if (c == ' ' || c == '\n') {
        if (currentLine.length() + word.length() <= maxLineChars) {
          currentLine += (currentLine.isEmpty() ? "" : " ") + word;
        } else {
          lines.push_back(currentLine);
          currentLine = word;
        }
        word = "";
      } else {
        word += c;
      }
    }
    if (!currentLine.isEmpty()) lines.push_back(currentLine);
    if (!word.isEmpty()) lines.push_back(word);

    // Display lines with scrolling effect - checking for cancellation during display
    for (size_t i = 0; i < lines.size(); i++) {
      // Check if we should continue showing the response
      if (!isDisplayingResponse) {
        break; // Exit the display loop if cancel was requested
      }
      
      display.clearDisplay();
      drawBorder(); // Add thin border
      for (size_t j = 0; j < visibleLines && (i + j) < lines.size(); j++) {
        display.setCursor(2, 2 + j * lineHeight); // Small margin from the border
        display.print(lines[i + j]);
      }
      display.display();
      
      // Check for cancel button during the delay period
      unsigned long startTime = millis();
      while (millis() - startTime < scrollDelay) {
        // Check for long-press to cancel display
        checkForCancelDisplay();
        
        // If cancel was requested during this check, break early
        if (!isDisplayingResponse) {
          break;
        }
        delay(100); // Small delay for button polling
      }
      
      // Break out of the display loop if cancel was requested
      if (!isDisplayingResponse) {
        break;
      }
    }

    // If we haven't been cancelled, show the content for a bit longer before returning to main screen
    if (isDisplayingResponse) {
      delay(3000);
    }
    
    // Clear display after the response
    display.clearDisplay();
    display.display();

    // Return to main screen with status information
    displayMainScreen();
  } else {
    Serial.print("[Gemini] Error: ");
    Serial.println(result);
    display.clearDisplay();
    drawBorder(); // Add thin border
    display.setCursor(2, 2);
    display.print("API Error");
    display.display();
    
    delay(3000);  // Show error for a moment
    displayMainScreen(); // Return to main screen
  }
}

bool sendPostRequest(const String& payload, String& result) {
  HTTPClient http;
  // Gemini API endpoint with API key included in URL
  String apiUrl = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + apiKey;
  http.begin(apiUrl);

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(20000);

  Serial.print("Payload size: ");
  Serial.println(payload.length());

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    result = http.getString();
    Serial.println("HTTP Response Code: " + String(httpResponseCode));
    Serial.println("Response Body: " + result);
    http.end();
    return true;
  } else {
    result = "HTTP request failed, response code: " + String(httpResponseCode);
    Serial.println("Error Code: " + String(httpResponseCode));
    Serial.println("Error Message: " + http.errorToString(httpResponseCode));
    http.end();
    return false;
  }
}

void flash() {
  digitalWrite(FLASHLIGHT_PIN, HIGH);
  delay(2000);
  digitalWrite(FLASHLIGHT_PIN, LOW);
}

// New function to check for long press to cancel display
void checkForCancelDisplay() {
  // Only check for cancellation if we're currently displaying a response
  if (isDisplayingResponse) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      // Button is pressed
      if (buttonPressStartTime == 0) {
        // First detection of button press
        buttonPressStartTime = millis();
      } else if (millis() - buttonPressStartTime >= 2000) {
        // Button has been held for 2 seconds or more
        Serial.println("Long press detected! Canceling display...");
        isDisplayingResponse = false;  // Set flag to stop displaying
        buttonPressStartTime = 0;      // Reset the timer
        
        // Show a brief cancellation message
        displayCenteredText("Cancelled");
        delay(1000);
      }
    } else {
      // Button is not pressed, reset the timer
      buttonPressStartTime = 0;
    }
  }
}

void loop() {
  // Check for cancel display long press
  checkForCancelDisplay();
  
  // Main capture button logic - only active when not displaying a response
  if (!isDisplayingResponse && digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Button pressed! Capturing image...");
    displayCenteredText("Capturing...");
    captureAndAnalyzeImage();
    delay(500);  // Small delay to debounce button press
  }
  
  // Prompt selector button logic - only active when not displaying a response
  if (!isDisplayingResponse && digitalRead(PROMPT_SELECTOR_BUTTON_PIN) == LOW) {
    unsigned long currentTime = millis();
    if (currentTime - lastPromptButtonPress > debounceDelay) {
      lastPromptButtonPress = currentTime;
      
      // Cycle through prompts
      currentPromptIndex = (currentPromptIndex + 1) % (sizeof(prompts) / sizeof(prompts[0]));
      
      Serial.print("Prompt changed to: ");
      Serial.println(prompts[currentPromptIndex]);
      
      // Update the display to show the new prompt
      displayMainScreen();
      delay(200); // Small delay to avoid bouncing
    }
  }
  
  // Flash mode selector button logic - only active when not displaying a response
  if (!isDisplayingResponse && digitalRead(FLASH_MODE_SELECTOR_BUTTON_PIN) == LOW) {
    unsigned long currentTime = millis();
    if (currentTime - lastFlashButtonPress > debounceDelay) {
      lastFlashButtonPress = currentTime;
      
      // Toggle flash mode
      flashEnabled = !flashEnabled;
      
      Serial.print("Flash mode changed to: ");
      Serial.println(flashEnabled ? "ON" : "OFF");
      
      // Update the display to show the new flash mode
      displayMainScreen();
      delay(200); // Small delay to avoid bouncing
    }
  }
}
