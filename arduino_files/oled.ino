String errorMessage = "";
String currentMessage = "";

unsigned long errorDisplayTime = 0;
unsigned long messageDisplayTime = 0;

bool hasErrorMessage = false;
bool isDisplayingMessage = false;

DisplayMessage activeDisplayMsg;

void pushOLEDMessage(const String& text) {
  if(currentMessage != text){
    currentMessage = text;
    messageDisplayTime = millis();
    isDisplayingMessage = true;
  }
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // ==================================
  // ƯU TIÊN LỖI
  // ==================================

  if (hasErrorMessage) {
    if (millis() - errorDisplayTime > 3000) {
      hasErrorMessage = false;
      errorMessage = "";
    }
    else {
      display.setCursor(0, 0);
      display.println("=== ERROR ===");

      display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

      display.setCursor(0, 25);
      display.println(errorMessage);

      display.display();
      return;
    }
  }

  if (isDisplayingMessage) {
    if (millis() - messageDisplayTime < 3000) {
      display.setCursor(10, 25);
      display.println(currentMessage);
      display.display();
      return;
    }

    isDisplayingMessage = false;
    currentMessage = "";
  }

  // ==================================
  // MÀN HÌNH MẶC ĐỊNH
  // ==================================

  bool s1_occupied = (digitalRead(IR_SLOT_1) == 0);
  bool s2_occupied = (digitalRead(IR_SLOT_2) == 0);

  int slots_available = 0;

  if (!s1_occupied) slots_available++;
  if (!s2_occupied) slots_available++;

  display.setCursor(15, 0);
  display.println("SMART PARKING PTIT");

  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setCursor(0, 20);
  display.print("Slot 1: ");
  display.println(s1_occupied ? "CO XE" : "TRONG");

  display.setCursor(0, 35);
  display.print("Slot 2: ");
  display.println(s2_occupied ? "CO XE" : "TRONG");

  display.setCursor(0, 50);
  display.print("Trang thai: ");

  if (slots_available == 0) {
    display.println("DA DAY!");
  }
  else {
    display.print("CON ");
    display.print(slots_available);
    display.println(" CHO");
  }

  display.display();
}