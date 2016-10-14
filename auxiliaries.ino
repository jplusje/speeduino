/*
Speeduino - Simple engine management for the Arduino Mega 2560 platform
Copyright (C) Josh Stewart
A full copy of the license may be found in the projects root directory
*/
integerPID boostPID(&currentStatus.MAP, &boost_pwm_target_value, &boost_cl_target_boost, configPage3.boostKP, configPage3.boostKI, configPage3.boostKD, DIRECT); //This is the PID object if that algorithm is used. Needs to be global as it maintains state outside of each function call

/*
Fan control
*/
void initialiseFan()
{
if(configPage4.fanInv == 1) {fanHIGH = LOW, fanLOW = HIGH; }
else {fanHIGH = HIGH, fanLOW = LOW;}
digitalWrite(pinFan, fanLOW);         //Initiallise program with the fan in the off state
}

void fanControl()
{
   if (currentStatus.coolant >= (configPage4.fanSP - CALIBRATION_TEMPERATURE_OFFSET)) { digitalWrite(pinFan,fanHIGH); }
   else if (currentStatus.coolant <= (configPage4.fanSP - configPage4.fanHyster)) { digitalWrite(pinFan, fanLOW); }
}

#if defined(PROCESSOR_MEGA_ALL) 
void initialiseAuxPWM()
{
  TCCR1B = 0x00;          //Disbale Timer1 while we set it up
  TCNT1  = 0;             //Reset Timer Count
  TIFR1  = 0x00;          //Timer1 INT Flag Reg: Clear Timer Overflow Flag
  TCCR1A = 0x00;          //Timer1 Control Reg A: Wave Gen Mode normal (Simply counts up from 0 to 65535 (16-bit int)
  TCCR1B = (1 << CS12);   //Timer1 Control Reg B: Timer Prescaler set to 256. 1 tick = 16uS. Refer to http://www.instructables.com/files/orig/F3T/TIKL/H3WSA4V7/F3TTIKLH3WSA4V7.jpg 
  
  boost_pin_port = portOutputRegister(digitalPinToPort(pinBoost));
  boost_pin_mask = digitalPinToBitMask(pinBoost);
  vvt_pin_port = portOutputRegister(digitalPinToPort(pinVVT_1));
  vvt_pin_mask = digitalPinToBitMask(pinVVT_1);
  
  boost_pwm_max_count = 1000000L / (16 * configPage3.boostFreq * 2); //Converts the frequency in Hz to the number of ticks (at 16uS) it takes to complete 1 cycle. The x2 is there because the frequency is stored at half value (in a byte) to allow freqneucies up to 511Hz
  vvt_pwm_max_count = 1000000L / (16 * configPage3.vvtFreq * 2); //Converts the frequency in Hz to the number of ticks (at 16uS) it takes to complete 1 cycle
  //TIMSK1 |= (1 << OCIE1A); //Turn on the A compare unit (ie turn on the interrupt)  //Shouldn't be needed with closed loop as its turned on below
  TIMSK1 |= (1 << OCIE1B); //Turn on the B compare unit (ie turn on the interrupt)

  boostPID.SetOutputLimits(0, boost_pwm_max_count);
  boostPID.SetMode(AUTOMATIC); //Turn PID on
}

void boostControl()
{
  if(configPage3.boostEnabled)
  {
    if(currentStatus.MAP < 100) { TIMSK1 &= ~(1 << OCIE1A); digitalWrite(pinBoost, LOW); return; } //Set duty to 0 and turn off timer compare
    boost_cl_target_boost = get3DTableValue(&boostTable, currentStatus.TPS, currentStatus.RPM) * 2; //Boost target table is in kpa and divided by 2
    boostPID.SetTunings(configPage3.boostKP, configPage3.boostKI, configPage3.boostKD);
    boostPID.Compute();
    TIMSK1 |= (1 << OCIE1A); //Turn on the compare unit (ie turn on the interrupt)
  }
  else { TIMSK1 &= ~(1 << OCIE1A); } // Disable timer channel
}

void vvtControl()
{
  if(configPage3.vvtEnabled)
  {
    byte vvtDuty = get3DTableValue(&vvtTable, currentStatus.TPS, currentStatus.RPM);
    vvt_pwm_target_value = percentage(vvtDuty, vvt_pwm_max_count);
  }
#if defined(PROCESSOR_MEGA_ALL)
  else { TIMSK1 &= ~(1 << OCIE1B); } // Disable timer channel
#endif
}
  
//The interrupt to control the Boost PWM
ISR(TIMER1_COMPA_vect)
{
  if (boost_pwm_state)
  {
    *boost_pin_port &= ~(boost_pin_mask);  // Switch pin to low
    OCR1A = TCNT1 + (boost_pwm_max_count - boost_pwm_cur_value);
    boost_pwm_state = false;
  }
  else
  {
    *boost_pin_port |= (boost_pin_mask);  // Switch pin high
    OCR1A = TCNT1 + boost_pwm_target_value;
    boost_pwm_cur_value = boost_pwm_target_value;
    boost_pwm_state = true;
  }  
}

//The interrupt to control the VVT PWM
ISR(TIMER1_COMPB_vect)
{
  if (vvt_pwm_state)
  {
    *vvt_pin_port &= ~(vvt_pin_mask);  // Switch pin to low
    OCR1B = TCNT1 + (vvt_pwm_max_count - vvt_pwm_cur_value);
    vvt_pwm_state = false;
  }
  else
  {
    *vvt_pin_port |= (vvt_pin_mask);  // Switch pin high
    OCR1B = TCNT1 + vvt_pwm_target_value;
    vvt_pwm_cur_value = vvt_pwm_target_value;
    vvt_pwm_state = true;
  }  
}

#elif defined (PROCESSOR_TEENSY_3_x)
//YET TO BE IMPLEMENTED ON TEENSY
void initialiseAuxPWM() { }
void boostControl() { } 
void vvtControl() { }

#endif

