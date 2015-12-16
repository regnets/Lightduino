/* IRLib.cpp from IRLib - an Arduino library for infrared encoding and decoding
 * Version 0.01   August 2015
 * Copyright 2014 by Chris Young http://cyborg5.com
 *
 * This library is a major rewrite of IRemote by Ken Shirriff which was covered by
 * GNU LESSER GENERAL PUBLIC LICENSE which as I read it allows me to make modified versions.
 * That same license applies to this modified version. See his original copyright below.
 * The latest Ken Shirriff code can be found at https://github.com/shirriff/Arduino-IRremote
 * My purpose was to reorganize the code to make it easier to add or remove protocols.
 * As a result I have separated the act of receiving a set of raw timing codes from the act of decoding them
 * by making them separate classes. That way the receiving aspect can be more black box and implementers
 * of decoders and senders can just deal with the decoding of protocols. It also allows for alternative
 * types of receivers independent of the decoding. This makes porting to different hardware platforms easier.
 * Also added provisions to make the classes base classes that could be extended with new protocols
 * which would not require recompiling of the original library nor understanding of its detailed contents.
 * Some of the changes were made to reduce code size such as unnecessary use of long versus bool.
 * Some changes were just my weird programming style. Also extended debugging information added.
 */
/*
 * IRremote
 * Version 0.1 July, 2009
 * Copyright 2009 Ken Shirriff
 * For details, see http://www.righto.com/2009/08/multi-protocol-infrared-remote-library.html http://www.righto.com/
 *
 * Interrupt code based on NECIRrcv by Joe Knapp
 * http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1210243556
 * Also influenced by http://zovirl.com/2008/11/12/building-a-universal-remote-with-an-arduino/
 */

#include "IRLib.h"
#include "IRLibMatch.h"
#include "IRLibRData.h"
#include <Arduino.h>

volatile irparams_t irparams;
/*
 * Returns a pointer to a flash stored string that is the name of the protocol received. 
 */
const __FlashStringHelper *Pnames(IRTYPES Type) {
  if(Type>LAST_PROTOCOL) Type=UNKNOWN;
  // You can add additional strings before the entry for hash code.
  const __FlashStringHelper *Names[LAST_PROTOCOL+1]={F("Unknown"),F("LIGHT_STRIKE")};
  return Names[Type];
};


#define TOPBIT 0x80000000

/*
 * The IRsend classes contain a series of methods for sending various protocols.
 * Each of these begin by calling enableIROut(unsigned char kHz) to set the carrier frequency.
 * It then calls mark(int usec) and space(inc usec) to transmit marks and
 * spaces of varying length of microseconds however the protocol defines.
 * Because we want to separate the hardware specific portions of the code from the general programming
 * portions of the code, the code for IRsendBase::IRsendBase, IRsendBase::enableIROut, 
 * IRsendBase::mark and IRsendBase::space can be found in the lower section of this file.
 */

/*
 * Most of the protocols have a header consisting of a mark/space of a particular length followed by 
 * a series of variable length mark/space signals.  Depending on the protocol they very the lengths of the 
 * mark or the space to indicate a data bit of "0" or "1". Most also end with a stop bit of "1".
 * The basic structure of the sending and decoding these protocols led to lots of redundant code. 
 * Therefore I have implemented generic sending and decoding routines. You just need to pass a bunch of customized 
 * parameters and it does the work. This reduces compiled code size with only minor speed degradation. 
 * You may be able to implement additional protocols by simply passing the proper values to these generic routines.
 * The decoding routines do not encode stop bits. So you have to tell this routine whether or not to send one.
 */
void IRsendBase::sendGeneric(unsigned long data, unsigned char Num_Bits, unsigned int Head_Mark, unsigned int Head_Space, 
                             unsigned int Mark_One, unsigned int Mark_Zero, unsigned int Space_One, unsigned int Space_Zero, 
							 unsigned char kHz, bool Use_Stop, unsigned long Max_Extent) {
  Extent=0;
  data = data << (32 - Num_Bits);
  enableIROut(kHz);
//Some protocols do not send a header when sending repeat codes. So we pass a zero value to indicate skipping this.
  if(Head_Mark) mark(Head_Mark); 
  if(Head_Space) space(Head_Space);
  for (int i = 0; i <Num_Bits; i++) {
    if (data & TOPBIT) {
      mark(Mark_One);  space(Space_One);
    } 
    else {
      mark(Mark_Zero);  space(Space_Zero);
    }
    data <<= 1;
  }
  if(Use_Stop) mark(Mark_One);   //stop bit of "1"
  if(Max_Extent) {

	space(Max_Extent-Extent); 
	}
	else space(Space_One);
};

void IRsendLightStrike::send(unsigned long data) {
  sendGeneric(data,32, 564*16, 564*8, 564, 564, 564*3, 564, 38, true);
};

/*
 * This method can be used to send any of the supported types except for raw and hash code.
 * There is no hash code send possible. You can call sendRaw directly if necessary.
 * Typically "data2" is the number of bits.
 */
void IRsend::send(IRTYPES Type, unsigned long data, unsigned int data2) {
  switch(Type) {
    case LIGHT_STRIKE:   IRsendLightStrike::send(data); break;
    }
}

/*
 * The irparams definitions which were located here have been moved to IRLibRData.h
 */

 /*
 * We've chosen to separate the decoding routines from the receiving routines to isolate
 * the technical hardware and interrupt portion of the code which should never need modification
 * from the protocol decoding portion that will likely be extended and modified. It also allows for
 * creation of alternative receiver classes separate from the decoder classes.
 */
IRdecodeBase::IRdecodeBase(void) {
  rawbuf=(volatile unsigned int*)irparams.rawbuf;
  IgnoreHeader=false;
  Reset();
};




/*
 * This routine is actually quite useful. Allows extended classes to call their parent
 * if they fail to decode themselves.
 */
bool IRdecodeBase::decode(void) {
  return false;
};

void IRdecodeBase::Reset(void) {
  decode_type= UNKNOWN;
  value=0;
  bits=0;
  rawlen=0;
};
#ifndef USE_DUMP
void DumpUnavailable(void) {Serial.println(F("DumpResults unavailable"));}
#endif
/*
 * This method dumps useful information about the decoded values.
 */
void IRdecodeBase::DumpResults(void) {
#ifdef USE_DUMP
  int i;unsigned long Extent;int interval;
  if(decode_type<=LAST_PROTOCOL){
    Serial.print(F("Decoded ")); Serial.print(Pnames(decode_type));
	Serial.print(F("(")); Serial.print(decode_type,DEC);
    Serial.print(F("): Value:")); Serial.print(value, HEX);
  };
  Serial.print(F(" ("));  Serial.print(bits, DEC); Serial.println(F(" bits)"));
  Serial.print(F("Raw samples(")); Serial.print(rawlen, DEC);
  Serial.print(F("): Gap:")); Serial.println(rawbuf[0], DEC);
  Serial.print(F("  Head: m")); Serial.print(rawbuf[1], DEC);
  Serial.print(F("  s")); Serial.println(rawbuf[2], DEC);
  int LowSpace= 32767; int LowMark=  32767;
  int HiSpace=0; int HiMark=  0;
  Extent=rawbuf[1]+rawbuf[2];
  for (i = 3; i < rawlen; i++) {
    Extent+=(interval= rawbuf[i]);
    if (i % 2) {
      LowMark=min(LowMark, interval);  HiMark=max(HiMark, interval);
      Serial.print(i/2-1,DEC);  Serial.print(F(":m"));
    } 
    else {
       if(interval>0)LowSpace=min(LowSpace, interval);  HiSpace=max (HiSpace, interval);
       Serial.print(F(" s"));
    }
    Serial.print(interval, DEC);
    int j=i-1;
    if ((j % 2)==1)Serial.print(F("\t"));
    if ((j % 4)==1)Serial.print(F("\t "));
    if ((j % 8)==1)Serial.println();
    if ((j % 32)==1)Serial.println();
  }
  Serial.println();
  Serial.print(F("Extent="));  Serial.println(Extent,DEC);
  Serial.print(F("Mark  min:")); Serial.print(LowMark,DEC);Serial.print(F("\t max:")); Serial.println(HiMark,DEC);
  Serial.print(F("Space min:")); Serial.print(LowSpace,DEC);Serial.print(F("\t max:")); Serial.println(HiSpace,DEC);
  Serial.println();
#else
  DumpUnavailable();
#endif
}

/*
 * Again we use a generic routine because most protocols have the same basic structure. However we need to
 * indicate whether or not the protocol varies the length of the mark or the space to indicate a "0" or "1".
 * If "Mark_One" is zero. We assume that the length of the space varies. If "Mark_One" is not zero then
 * we assume that the length of Mark varies and the value passed as "Space_Zero" is ignored.
 * When using variable length Mark, assumes Head_Space==Space_One. If it doesn't, you need a specialized decoder.
 */
bool IRdecodeBase::decodeGeneric(unsigned char Raw_Count, unsigned int Head_Mark, unsigned int Head_Space, 
                                 unsigned int Mark_One, unsigned int Mark_Zero, unsigned int Space_One, unsigned int Space_Zero) {
// If raw samples count or head mark are zero then don't perform these tests.
// Some protocols need to do custom header work.
  unsigned long data = 0;  unsigned char Max; offset=1;
  if (Raw_Count) {if (rawlen != Raw_Count) return RAW_COUNT_ERROR;}
  if(!IgnoreHeader) {
    if (Head_Mark) {
	  if (!MATCH(rawbuf[offset],Head_Mark)) return HEADER_MARK_ERROR(Head_Mark);
	}
  }
  offset++;
  if (Head_Space) {if (!MATCH(rawbuf[offset],Head_Space)) return HEADER_SPACE_ERROR(Head_Space);}


    Max=rawlen-1; //ignore stop bit
    offset=3;//skip initial gap plus two header items
    while (offset < Max) {
      if (!MATCH (rawbuf[offset],Mark_Zero)) return DATA_MARK_ERROR(Mark_Zero);
      offset++;
      if (MATCH(rawbuf[offset],Space_One)) {
        data = (data << 1) | 1;
      } 
      else if (MATCH (rawbuf[offset],Space_Zero)) {
        data <<= 1;
      } 
      else return DATA_SPACE_ERROR(Space_Zero);
      offset++;
    
    bits = (offset - 1) / 2 -1;//didn't encode stop bit
  }
  // Success
  value = data;
  return true;
}

/*
 * This routine has been modified significantly from the original IRremote.
 * It assumes you've already called IRrecvBase::GetResults and it was true.
 * The purpose of GetResults is to determine if a complete set of signals
 * has been received. It then copies the raw data into your decoder's rawbuf
 * By moving the test for completion and the copying of the buffer
 * outside of this "decode" method you can use the individual decode
 * methods or make your own custom "decode" without checking for
 * protocols you don't use.
 * Note: Don't forget to call IRrecvBase::resume(); after decoding is complete.
 */
bool IRdecode::decode(void) {
  if (IRdecodeLightStrike::decode()) return true;
//if (IRdecodeADDITIONAL::decode()) return true;//add additional protocols here
//Deliberately did not add hash code decoding. If you get decode_type==UNKNOWN and
// you want to know a hash code you can call IRhash::decode() yourself.
// BTW This is another reason we separated IRrecv from IRdecode.
  return false;
}



/* We have created a new receiver base class so that we can use its code to implement
 * additional receiver classes in addition to the original IRremote code which used
 * 50us interrupt sampling of the input pin. See IRrecvLoop and IRrecvPCI classes
 * below. IRrecv is the original receiver class with the 50us sampling.
 */
IRrecvBase::IRrecvBase(unsigned char recvpin)
{
  irparams.recvpin = recvpin;
  Init();
}
void IRrecvBase::Init(void) {
  irparams.blinkflag = 0;
  Mark_Excess=100;
}

unsigned char IRrecvBase::getPinNum(void){
  return irparams.recvpin;
}

/* Any receiver class must implement a GetResults method that will return true when a complete code
 * has been received. At a successful end of your GetResults code you should then call IRrecvBase::GetResults
 * and it will copy the data from the receiver structures into your decoder. Some receivers
 * provide results in rawbuf measured in ticks on some number of microseconds while others
 * return results in actual microseconds. If you use ticks then you should pass a multiplier
 * value in Time_per_Ticks.
 */
bool IRrecvBase::GetResults(IRdecodeBase *decoder, const unsigned int Time_per_Tick) {
  decoder->Reset();//clear out any old values.
  decoder->rawlen = irparams.rawlen;
/* Typically IR receivers over-report the length of a mark and under-report the length of a space.
 * This routine adjusts for that by subtracting Mark_Excess from recorded marks and
 * deleting it from a recorded spaces. The amount of adjustment used to be defined in IRLibMatch.h.
 * It is now user adjustable with the old default of 100;
 * By copying the the values from irparams to decoder we can call IRrecvBase::resume 
 * immediately while decoding is still in progress.
 */
  for(unsigned char i=0; i<irparams.rawlen; i++) {
    decoder->rawbuf[i]=irparams.rawbuf[i]*Time_per_Tick + ( (i % 2)? -Mark_Excess:Mark_Excess);
  }
  return true;
}

void IRrecvBase::enableIRIn(void) { 
  pinMode(irparams.recvpin, INPUT);
  resume();
}

void IRrecvBase::resume() {
  irparams.rawlen = 0;
}


/*
 * The remainder of this file is all related to interrupt handling and hardware issues. It has 
 * nothing to do with IR protocols. You need not understand this is all you're doing is adding 
 * new protocols or improving the receiving, decoding and sending of protocols.
 */

//See IRLib.h comment explaining this function
 unsigned char Pin_from_Intr(unsigned char inum) {
  const unsigned char PROGMEM attach_to_pin[]= {
#if defined(__AVR_ATmega256RFR2__)//Assume Pinoccio Scout
	4,5,SCL,SDA,RX1,TX1,7
#elif defined(__AVR_ATmega32U4__) //Assume Arduino Leonardo
	3,2,0,1,7
#elif defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)//Assume Arduino Mega 
	2,3, 21, 20, 1, 18
#else	//Assume Arduino Uno or other ATmega328
	2, 3
#endif
  };
#if defined(ARDUINO_SAM_DUE)
  return inum;
#endif
  if (inum<sizeof attach_to_pin) {//note this works because we know it's one byte per entry
	return attach_to_pin[inum];
  } else {
    return 255;
  }
}

// Provides ISR
#include <avr/interrupt.h>
// defines for setting and clearing register bits
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif
#define CLKFUDGE 5      // fudge factor for clock interrupt overhead
#ifdef F_CPU
#define SYSCLOCK F_CPU     // main Arduino clock
#else
#define SYSCLOCK 16000000  // main Arduino clock
#endif
#define PRESCALE 8      // timer clock prescale
#define CLKSPERUSEC (SYSCLOCK/PRESCALE/1000000)   // timer clocks per microsecond

#include <IRLibTimer.h>

/* 
 * This section contains the hardware specific portions of IRrecvBase
 */
/* If your hardware is set up to do both output and input but your particular sketch
 * doesn't do any output, this method will ensure that your output pin is low
 * and doesn't turn on your IR LED or any output circuit.
 */
void IRrecvBase::No_Output (void) {
#if defined(IR_SEND_PWM_PIN)
 pinMode(IR_SEND_PWM_PIN, OUTPUT);  
 digitalWrite(IR_SEND_PWM_PIN, LOW); // When not sending PWM, we want it low    
#endif
}

// enable/disable blinking of pin 13 on IR processing
void IRrecvBase::blink13(bool blinkflag)
{
  irparams.blinkflag = blinkflag;
  if (blinkflag)
     pinMode(BLINKLED, OUTPUT);
}

//Do the actual blinking off and on
//This is not part of IRrecvBase because it may need to be inside an ISR
//and we cannot pass parameters to them.
void do_Blink(void) {
  if (irparams.blinkflag) {
    if(irparams.rawlen % 2) {
      BLINKLED_ON();  // turn pin 13 LED on
    } 
    else {
      BLINKLED_OFF();  // turn pin 13 LED off
    }
  }
}

/*
 * The original IRrecv which uses 50µs timer driven interrupts to sample input pin.
 */
void IRrecv::resume() {
  // initialize state machine variables
  irparams.rcvstate = STATE_IDLE;
  IRrecvBase::resume();
}

void IRrecv::enableIRIn(void) {
  IRrecvBase::enableIRIn();
  // setup pulse clock timer interrupt
  cli();
  IR_RECV_CONFIG_TICKS();
  IR_RECV_ENABLE_INTR;
  sei();
}

bool IRrecv::GetResults(IRdecodeBase *decoder) {
  if (irparams.rcvstate != STATE_STOP) return false;
  IRrecvBase::GetResults(decoder,USECPERTICK);
  return true;
}

#define _GAP 5000 // Minimum map between transmissions
#define GAP_TICKS (_GAP/USECPERTICK)
/*
 * This interrupt service routine is only used by IRrecv and may or may not be used by other
 * extensions of the IRrecBase. It is timer driven interrupt code to collect raw data.
 * Widths of alternating SPACE, MARK are recorded in rawbuf. Recorded in ticks of 50 microseconds.
 * rawlen counts the number of entries recorded so far. First entry is the SPACE between transmissions.
 * As soon as a SPACE gets long, ready is set, state switches to IDLE, timing of SPACE continues.
 * As soon as first MARK arrives, gap width is recorded, ready is cleared, and new logging starts.
 */
ISR(IR_RECV_INTR_NAME)
{
  enum irdata_t {IR_MARK=0, IR_SPACE=1};
  irdata_t irdata = (irdata_t)digitalRead(irparams.recvpin);
  irparams.timer++; // One more 50us tick
  if (irparams.rawlen >= RAWBUF) {
    // Buffer overflow
    irparams.rcvstate = STATE_STOP;
  }
  switch(irparams.rcvstate) {
  case STATE_IDLE: // In the middle of a gap
    if (irdata == IR_MARK) {
      if (irparams.timer < GAP_TICKS) {
        // Not big enough to be a gap.
        irparams.timer = 0;
      } 
      else {
        // gap just ended, record duration and start recording transmission
        irparams.rawlen = 0;
        irparams.rawbuf[irparams.rawlen++] = irparams.timer;
        irparams.timer = 0;
        irparams.rcvstate = STATE_MARK;
      }
    }
    break;
  case STATE_MARK: // timing MARK
    if (irdata == IR_SPACE) {   // MARK ended, record time
      irparams.rawbuf[irparams.rawlen++] = irparams.timer;
      irparams.timer = 0;
      irparams.rcvstate = STATE_SPACE;
    }
    break;
  case STATE_SPACE: // timing SPACE
    if (irdata == IR_MARK) { // SPACE just ended, record it
      irparams.rawbuf[irparams.rawlen++] = irparams.timer;
      irparams.timer = 0;
      irparams.rcvstate = STATE_MARK;
    } 
    else { // SPACE
      if (irparams.timer > GAP_TICKS) {
        // big SPACE, indicates gap between codes
        // Mark current code as ready for processing
        // Switch to STOP
        // Don't reset timer; keep counting space width
        irparams.rcvstate = STATE_STOP;
      } 
    }
    break;
  case STATE_STOP: // waiting, measuring gap
    if (irdata == IR_MARK) { // reset gap timer
      irparams.timer = 0;
    }
    break;
  }
  do_Blink();
}

/*
 * The hardware specific portions of IRsendBase
 */
void IRsendBase::enableIROut(unsigned char khz) {
//NOTE: the comments on this routine accompanied the original early version of IRremote library
//which only used TIMER2. The parameters defined in IRLibTimer.h may or may not work this way.
  // Enables IR output.  The khz value controls the modulation frequency in kilohertz.
  // The IR output will be on pin 3 (OC2B).
  // This routine is designed for 36-40KHz; if you use it for other values, it's up to you
  // to make sure it gives reasonable results.  (Watch out for overflow / underflow / rounding.)
  // TIMER2 is used in phase-correct PWM mode, with OCR2A controlling the frequency and OCR2B
  // controlling the duty cycle.
  // There is no prescaling, so the output frequency is 16MHz / (2 * OCR2A)
  // To turn the output on and off, we leave the PWM running, but connect and disconnect the output pin.
  // A few hours staring at the ATmega documentation and this will all make sense.
  // See my Secrets of Arduino PWM at http://www.righto.com/2009/07/secrets-of-arduino-pwm.html for details.
  
  // Disable the Timer2 Interrupt (which is used for receiving IR)
 IR_RECV_DISABLE_INTR; //Timer2 Overflow Interrupt    
 pinMode(IR_SEND_PWM_PIN, OUTPUT);  
 digitalWrite(IR_SEND_PWM_PIN, LOW); // When not sending PWM, we want it low    
 IR_SEND_CONFIG_KHZ(khz);
 }

IRsendBase::IRsendBase () {
 pinMode(IR_SEND_PWM_PIN, OUTPUT);  
 digitalWrite(IR_SEND_PWM_PIN, LOW); // When not sending PWM, we want it low    
}

//The Arduino built in function delayMicroseconds has limits we wish to exceed
//Therefore we have created this alternative
void  My_delay_uSecs(unsigned int T) {
  if(T){if(T>16000) {delayMicroseconds(T % 1000); delay(T/1000); } else delayMicroseconds(T);};
}

void IRsendBase::mark(unsigned int time) {
 IR_SEND_PWM_START;
 IR_SEND_MARK_TIME(time);
 Extent+=time;
}

void IRsendBase::space(unsigned int time) {
 IR_SEND_PWM_STOP;
 My_delay_uSecs(time);
 Extent+=time;
}