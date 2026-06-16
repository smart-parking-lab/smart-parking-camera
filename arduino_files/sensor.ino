unsigned long beepEndTime = 0;
unsigned long debounce[4] = {0};


bool hasAvailableSlot()
{
  return (
      digitalRead(IR_SLOT_1) == 1 ||
      digitalRead(IR_SLOT_2) == 1
  );
}

void shortBeep()
{
    digitalWrite(BUZZER_PIN, HIGH);
    beepEndTime = millis() + 1000;
}

void checkGateIn()
{
    int ir_in = digitalRead(IR_GATE_IN);
    if (ir_in != last_ir_in)
    {
        if(debounce[0] == 0) debounce[0] = millis();
        if(millis() > (debounce[0]+50)){
            ir_in = digitalRead(IR_GATE_IN);
            if (ir_in != last_ir_in)
            {
                if (ir_in == LOW)
                {
                    publishMQTT(topic_sensor, "{\"sensor\":\"GATE_IN\",\"status\":\"CO_XE\"}");
                    shortBeep();
                }
                last_ir_in = ir_in;
            }
            debounce[0] = 0;
        }
    }
}

void checkGateOut()
{
    int ir_out = digitalRead(IR_GATE_OUT);
    if (ir_out != last_ir_out)
    {
        if(debounce[1] == 0) debounce[1] = millis();
        if(millis() > (debounce[1]+50)){
            ir_out = digitalRead(IR_GATE_OUT);
            if (ir_out != last_ir_out)
            {
                if (ir_out == LOW)
                {
                    publishMQTT(topic_sensor, "{\"sensor\":\"GATE_OUT\",\"status\":\"CO_XE\"}");
                    shortBeep();
                }
                last_ir_out = ir_out;
            }
            debounce[1] = 0;
        }
    }
}

void checkSlot1()
{
    int ir_slot1 = digitalRead(IR_SLOT_1);
    if (ir_slot1 != last_ir_slot1)
    {
        if(debounce[2] == 0) debounce[2] = millis();
        if(millis() > (debounce[2]+50)){
            ir_slot1 = digitalRead(IR_SLOT_1);
            if (ir_slot1 != last_ir_slot1)
            {
                if (ir_slot1 == LOW)
                {
                    publishMQTT(topic_sensor, "{\"sensor\":\"SLOT_1\",\"status\":\"CO_XE\"}");
                }
                else
                {
                    publishMQTT(topic_sensor, "{\"sensor\":\"SLOT_1\",\"status\":\"TRONG\"}");
                }
                last_ir_slot1 = ir_slot1;
            }
            debounce[2] = 0;
        }
    }
}

// ======================================
// SLOT 2
// ======================================

void checkSlot2()
{
    int ir_slot2 = digitalRead(IR_SLOT_2);
    if (ir_slot2 != last_ir_slot2)
    {
        if(debounce[3] == 0) debounce[3] = millis();
        if(millis() > (debounce[3]+50)){
            ir_slot2 = digitalRead(IR_SLOT_2);
            if (ir_slot2 != last_ir_slot2)
            {
                if (ir_slot2 == LOW)
                {
                    publishMQTT(topic_sensor, "{\"sensor\":\"SLOT_2\",\"status\":\"CO_XE\"}");
                }
                else
                {
                    publishMQTT(topic_sensor, "{\"sensor\":\"SLOT_2\",\"status\":\"TRONG\"}");
                }
                last_ir_slot2 = ir_slot2;
            }
            debounce[3] = 0;
        }
    }
}

void checkSensors()
{
    checkGateIn();
    checkGateOut();
    checkSlot1();
    checkSlot2();
    if(beepEndTime > 0 && millis() >= beepEndTime)
    {
        digitalWrite(BUZZER_PIN, LOW);
        beepEndTime = 0;
    }
}