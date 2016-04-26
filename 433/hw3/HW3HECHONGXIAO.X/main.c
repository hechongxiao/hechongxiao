#include<xc.h>           // processor SFR definitions
#include<sys/attribs.h>  // __ISR macro
#include<math.h>
#include"i2c_master_noint.h"
#define CS LATBbits.LATB7       // chip select pin
#define SineCount 100
#define TriangleCount 200
#define Pi 3.14159265

static volatile float SineWaveform[SineCount];   // sine waveform
static volatile float TriangleWaveform[TriangleCount];   // triangle waveform
char read=0x00;
char checkGP7 = 0x00;

// DEVCFG0
#pragma config DEBUG = OFF // no debugging
#pragma config JTAGEN = OFF // no jtag
#pragma config ICESEL = ICS_PGx1 // use PGED1 and PGEC1
#pragma config PWP = OFF // no write protect
#pragma config BWP = OFF // no boot write protect
#pragma config CP = OFF // no code protect

// DEVCFG1
#pragma config FNOSC = PRIPLL // use primary oscillator with pll
#pragma config FSOSCEN = OFF // turn off secondary oscillator
#pragma config IESO = OFF // no switching clocks
#pragma config POSCMOD = HS // high speed crystal mode
#pragma config OSCIOFNC = OFF // free up secondary osc pins
#pragma config FPBDIV = DIV_1 // divide CPU freq by 1 for peripheral bus clock
#pragma config FCKSM = CSDCMD // do not enable clock switch
#pragma config WDTPS = PS1048576 // slowest wdt
#pragma config WINDIS = OFF // no wdt window
#pragma config FWDTEN = OFF // wdt off by default
#pragma config FWDTWINSZ = WINSZ_25 // wdt window at 25%

// DEVCFG2 - get the CPU clock to 48MHz
#pragma config FPLLIDIV = DIV_2 // divide input clock to be in range 4-5MHz
#pragma config FPLLMUL = MUL_24 // multiply clock after FPLLIDIV
#pragma config FPLLODIV = DIV_2 // divide clock after FPLLMUL to get 48MHz
#pragma config UPLLIDIV = DIV_2 // divider for the 8MHz input clock, then multiply by 12 to get 48MHz for USB
#pragma config UPLLEN = OFF // USB clock on

// DEVCFG3
#pragma config USERID = 0 // some 16bit userid, doesn't matter what
#pragma config PMDL1WAY = OFF // allow multiple reconfigurations
#pragma config IOL1WAY = OFF // allow multiple reconfigurations
#pragma config FUSBIDIO = ON // USB pins controlled by USB module
#pragma config FVBUSONIO = ON // USB BUSON controlled by USB module

void spi_init() {
  // set up the chip select pin as an output
  // the chip select pin is used by the sram to indicate
  // when a command is beginning (clear CS to low) and when it
  // is ending (set CS high)
  TRISBbits.TRISB7 = 0b0;
  CS = 1;

  // Master - SPI4, pins are: SDI4(F4), SDO4(F5), SCK4(F13).  
  // we manually control SS4 as a digital output (F12)
  // since the pic is just starting, we know that spi is off. We rely on defaults here
 
  // setup spi4
  SPI1CON = 0;              // turn off the spi module and reset it
  SPI1BUF;                  // clear the rx buffer by reading from it
  SPI1BRG = 0x1;            // baud rate to 10 MHz [SPI4BRG = (80000000/(2*desired))-1]
  SPI1STATbits.SPIROV = 0;  // clear the overflow bit
  SPI1CONbits.CKE = 1;      // data changes when clock goes from hi to lo (since CKP is 0)
  SPI1CONbits.MSTEN = 1;    // master operation
       // turn on spi 4
  SPI1CONbits.MODE32 = 0;   // use 8 bit mode
  SPI1CONbits.MODE16 = 0;
  
  //SS1Rbits.SS1R = 0b0100;   // assign SS1 to RB7
  SDI1Rbits.SDI1R = 0b0000; // assign SDI1 to RA1 
  RPB8Rbits.RPB8R = 0b0011; // assign SDO1 to RB8
  ANSELBbits.ANSB14 = 0;    // turn off AN10
  SPI1CONbits.ON = 1;    
  // send a ram set status command.
}

// send a byte via spi and return the response

char SPI1_IO(char write){
    SPI1BUF = write;
    while(!SPI1STATbits.SPIRBF) { // wait to receive the byte
     ;
     }
    return SPI1BUF; 
}

void setVoltage(char channel, float voltage){
    int temp = voltage;
    if(channel == 0) { // 0 for VoutA
        CS = 0; 
        SPI1_IO((temp >> 4) | 0b01110000); // 4 configuration bits
        SPI1_IO(temp << 4); // Data bits 
        CS = 1;   
    }
    if(channel == 1) { // 1 for VoutB
        CS = 0; 
        SPI1_IO((temp >> 4) | 0b11110000); // 4 configuration bits
        SPI1_IO(temp << 4); // Data bits
        CS = 1;   
    }
}

void makeSinWave(){
    int i;
    for(i = 0; i < SineCount; i++){
        SineWaveform[i] = 127+128*sin(2*Pi*10*i*0.001);
        }
}


void makeTriangleWave(){
    int j;
    for(j = 0; j < TriangleCount; j++){
        TriangleWaveform[j] = 255*(j*0.005); 
    }
}

void initExpander(){
    i2c_master_start();
    i2c_master_send(0x40);
    i2c_master_send(0x00);
    i2c_master_send(0xf0);
    i2c_master_stop();    
}

char getExpander(){
    i2c_master_start();
    i2c_master_send(0x40);
    i2c_master_send(0x09);
    i2c_master_restart();
    i2c_master_send(0x41);
    read=i2c_master_recv();
    i2c_master_ack(1);
    i2c_master_stop();
    return read;
}

unsigned char setLowBitOperation(int pin){
    unsigned char b1=0xff;
    unsigned char b2, b3, b4;
    b2 = b1 << (pin+1);
    b3 = b1 >> (8-pin);
    b4 = b2 ^ b3;
    return b4;
}

void setExpander(char pin, char level){
        getExpander();
        i2c_master_start();
        i2c_master_send(0x40);    
        i2c_master_send(0x0A);
        if(level == 1){
            i2c_master_send((1 << pin)|read);
        }
        if(level == 0){
            unsigned char temp;
            temp = setLowBitOperation(pin);
            i2c_master_send(read & temp);
        }
        i2c_master_stop();  
}
int main() {

    __builtin_disable_interrupts();

    // set the CP0 CONFIG register to indicate that kseg0 is cacheable (0x3)
    __builtin_mtc0(_CP0_CONFIG, _CP0_CONFIG_SELECT, 0xa4210583);

    // 0 data RAM access wait states
    BMXCONbits.BMXWSDRM = 0x0;

    // enable multi vector interrupts
    INTCONbits.MVEC = 0x1;

    // disable JTAG to get pins back
    DDPCONbits.JTAGEN = 0;
    
    // do your TRIS and LAT commands here
    TRISAbits.TRISA4=0;
    LATAbits.LATA4=1;
    TRISBbits.TRISB4=1;
    __builtin_enable_interrupts();
        
    makeSinWave();
    makeTriangleWave();
    spi_init();
    i2c_master_setup();
    initExpander();  
    
     while(1){
        _CP0_SET_COUNT(0);
        LATAINV = 0x10; // make sure timer2 works

      checkGP7 = (getExpander() >> 7);
        if(checkGP7 == 0){
            setExpander(1, 1);
        }
        else{setExpander(1,0);}
      
        static int count1 = 0;
        static int count2 = 0;
        setVoltage(0,SineWaveform[count1]); 
        setVoltage(1,TriangleWaveform[count2]);
        count1++;
        count2++;
        if(count1 >= SineCount){
            count1 = 0;
        }
        if(count2 >= TriangleCount){
            count2 = 0;
        }
        while(_CP0_GET_COUNT() < 24000) { 
            ;
        }
    }   
}