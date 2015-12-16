/* IRLib.h from IRLib � an Arduino library for infrared encoding and decoding
 * Version 1.51   March 2015
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

#ifndef IRLib_h
#define IRLib_h
#include <Arduino.h>

// The following are compile-time library options.
// If you change them, recompile the library.
// If IRLIB_TRACE is defined, some debugging information about the decode will be printed
// IRLIB_TEST must be defined for the IRtest unittests to work.  It will make some
// methods virtual, which will be slightly slower, which is why it is optional.
//#define IRLIB_TRACE
// #define IRLIB_TEST

/* If not using the IRrecv class but only using IRrecvPCI or IRrecvLoop you can eliminate
 * some conflicts with the duplicate definition of ISR by turning this feature off.
 * Comment out the following define to eliminate the conflicts.
 */

/* Similarly some other libraries have conflicts with the built in Arduino functions
 * "attachInterrupt()" and "detachInterrupt()" which are used by the IRrecvPCI and
 * IRfrequency classes. If you're not using either of those classes and get conflicts
 * related to INT0_vect then comment out the following line to eliminate the conflicts.
 */

/* If not using either DumpResults methods of IRdecode nor IRfrequency you can
 * comment out the following define to eliminate considerable program space.
 */
#define USE_DUMP

// Only used for testing; can remove virtual for shorter code
#ifdef IRLIB_TEST
#define VIRTUAL virtual
#else
#define VIRTUAL
#endif

#define RAWBUF 100 // Length of raw duration buffer (cannot exceed 255)

typedef char IRTYPES; //formerly was an enum
#define UNKNOWN 0
#define LIGHT_STRIKE 1
//#define ADDITIONAL (number) //make additional protocol 8 and change HASH_CODE to 9
#define HASH_CODE 2
#define LAST_PROTOCOL HASH_CODE

const __FlashStringHelper *Pnames(IRTYPES Type); //Returns a character string that is name of protocol.

// Base class for decoding raw results
class IRdecodeBase
{
public:
  IRdecodeBase(void);
  IRTYPES decode_type;           // NEC, SONY, RC5, UNKNOWN etc.
  unsigned long value;           // Decoded value
  unsigned char bits;            // Number of bits in decoded value
  volatile unsigned int *rawbuf; // Raw intervals in microseconds
  unsigned char rawlen;          // Number of records in rawbuf.
  bool IgnoreHeader;             // Relaxed header detection allows AGC to settle
  virtual void Reset(void);      // Initializes the decoder
  virtual bool decode(void);     // This base routine always returns false override with your routine
  bool decodeGeneric(unsigned char Raw_Count, unsigned int Head_Mark, unsigned int Head_Space, 
                     unsigned int Mark_One, unsigned int Mark_Zero, unsigned int Space_One, unsigned int Space_Zero);
  virtual void DumpResults (void);
  void UseExtnBuf(void *P); //Normally uses same rawbuf as IRrecv. Use this to define your own buffer.
  void copyBuf (IRdecodeBase *source);//copies rawbuf and rawlen from one decoder to another
protected:
  unsigned char offset;           // Index into rawbuf used various places
};

class IRdecodeLightStrike: public virtual IRdecodeBase 
{
public:
  virtual bool decode(void);
};
// main class for decoding all supported protocols
class IRdecode: 
public virtual IRdecodeLightStrike
{
public:
  virtual bool decode(void);    // Calls each decode routine individually
};

//Base class for sending signals
class IRsendBase
{
public:
  IRsendBase();
  void sendGeneric(unsigned long data,  unsigned char Num_Bits, unsigned int Head_Mark, unsigned int Head_Space, 
                   unsigned int Mark_One, unsigned int Mark_Zero, unsigned int Space_One, unsigned int Space_Zero, 
				   unsigned char kHz, bool Stop_Bits, unsigned long Max_Extent=0);
protected:
  void enableIROut(unsigned char khz);
  VIRTUAL void mark(unsigned int usec);
  VIRTUAL void space(unsigned int usec);
  unsigned long Extent;
  unsigned char OnTime,OffTime,iLength;//used by bit-bang output.
};

class IRsendLightStrike: public virtual IRsendBase
{
public:
  void send(unsigned long data);
};


class IRsend: 
public virtual IRsendLightStrike
{
public:
  void send(IRTYPES Type, unsigned long data, unsigned int data2);
};

// Changed this to a base class so it can be extended
class IRrecvBase
{
public:
  IRrecvBase(void) {};
  IRrecvBase(unsigned char recvpin);
  void No_Output(void);
  void blink13(bool blinkflag);
  bool GetResults(IRdecodeBase *decoder, const unsigned int Time_per_Ticks=1);
  void enableIRIn(void);
  virtual void resume(void);
  unsigned char getPinNum(void);
  unsigned char Mark_Excess;
protected:
  void Init(void);
};

/* Original IRrecv class uses 50�s interrupts to sample input. While this is generally
 * accurate enough for everyday purposes, it may be difficult to port to other
 * hardware unless you know a lot about hardware timers and interrupts. Also
 * when trying to analyze unknown protocols, the 50�s granularity may not be sufficient.
 * In that case use either the IRrecvLoop or the IRrecvPCI class.
 */

class IRrecv: public IRrecvBase
{
public:
  IRrecv(unsigned char recvpin):IRrecvBase(recvpin){};
  bool GetResults(IRdecodeBase *decoder);
  void enableIRIn(void);
  void resume(void);
};

//Do the actual blinking off and on
//This is not part of IRrecvBase because it may need to be inside an ISR
//and we cannot pass parameters to them.
void do_Blink(void);

/* This routine maps interrupt numbers used by attachInterrupt() into pin numbers.
 * NOTE: these interrupt numbers which are passed to �attachInterrupt()� are not 
 * necessarily identical to the interrupt numbers in the datasheet of the processor 
 * chip you are using. These interrupt numbers are a system unique to the 
 * �attachInterrupt()� Arduino function.  It is used by both IRrecvPCI and IRfrequency.
 */
unsigned char Pin_from_Intr(unsigned char inum);
// Some useful constants
// Decoded value for NEC when a repeat code is received
#define REPEAT 0xffffffff


#endif //IRLib_h
