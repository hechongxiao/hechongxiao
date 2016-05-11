#include<xc.h>           // processor SFR definitions
#include<sys/attribs.h>  // __ISR macro
#include<math.h>
#include"i2c_master_noint.h"
#include"ILI9163C.h"

#define CS LATBbits.LATB7       // chip select pin
char read=0x00;
short arr[14];
short tem,gx,gy,gz,ax,ay,az;
float ax2,ay2;
char word[100];

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

void setlsm(char reg, char value){
    i2c_master_start();
    i2c_master_send(0b11010110);
    i2c_master_send(reg);
    i2c_master_send(value);
    i2c_master_stop();    
}

char readaddress(){
    i2c_master_start();
    i2c_master_send(0b11010110);
    i2c_master_send(0x0F);
    i2c_master_restart();
    i2c_master_send(0b11010111);
    read=i2c_master_recv();
    i2c_master_ack(1);
    i2c_master_stop();
    return read;
}


void i2c_readmulti(){
    i2c_master_start();
    i2c_master_send(0b11010110);
    i2c_master_send(0x20);
    i2c_master_restart();
    i2c_master_send(0b11010111);
    int i;
    for(i=0;i<13;i++)
    {
        arr[i]=i2c_master_recv();
        i2c_master_ack(0);
    }
    arr[13]=i2c_master_recv();
    i2c_master_ack(1);
    i2c_master_stop();
    tem=(arr[1]<<8)|(arr[0]);
    gx=(arr[3]<<8)|(arr[2]);
    gy=(arr[5]<<8)|(arr[4]);
    gz=(arr[7]<<8)|(arr[6]);
    ax=(arr[9]<<8)|(arr[8]);
    ay=(arr[11]<<8)|(arr[10]);
    az=(arr[13]<<8)|(arr[12]);
}

void __ISR(_TIMER_2_VECTOR, IPL5SOFT) PWMcontroller(void){
    ax2=3000.0*ax/32768.0+3000.0;
    ay2=3000.0*ay/32768.0+3000.0;
    OC1RS=(short)ax2;
    OC2RS=(short)ay2;
    IFS0bits.T2IF=0;
}
void printletter(unsigned char letter, short x0, short y0){
    int x=0;
    int y=0;
    
    for (x=0; x<5; x++) {
        for (y=7; y>-1; y--) {
            if (((ASCII[letter-0x20][x] >> (7-y)) & 1) == 1)
                LCD_drawPixel(x0+x, y0+(7-y), 0xd000);
            else
                LCD_drawPixel(x0+x, y0+(7-y), YELLOW);
        }
    }
}

void printword(char * word, int x, int y){
    int i = 0; 
    int x0=x;
    int y0=y;
    while(word[i]){ 
        printletter(word[i],x0,y0);
        x0 = x0+5;
        i++;
    }
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
    TRISBbits.TRISB4=1;
    i2c_master_setup();
   // readaddress();
    setlsm(0x10,0b10000000);
    setlsm(0x11,0b10000000);
    setlsm(0x12,0b00000100);
    __builtin_enable_interrupts();
    SPI1_init();
    LCD_init();
   // i2c_readmulti();
    //__builtin_enable_interrupts();
    LATAbits.LATA4=0;    
    LCD_clearScreen(YELLOW);
    while(1){
        _CP0_SET_COUNT(0);
        while(_CP0_GET_COUNT() < 10000) { ;}
        i2c_readmulti();
        sprintf(word,"gx: %2.4f dps ",245*gx/32768.0);
        printword(word,12,42);
        sprintf(word,"gy: %2.4f dps ",245*gy/32768.0);
        printword(word,12,52);
        sprintf(word,"gz: %2.4f dps ",245*gz/32768.0);
        printword(word,12,62);
        sprintf(word,"ax: %2.4f g  ",2*ax/32768.0);
        printword(word,12,72);
        sprintf(word,"ay: %2.4f g  ",2*ay/32768.0);
        printword(word,12,82);
        sprintf(word,"az: %2.4f g  ",2*az/32768.0);
        printword(word,12,92);
        sprintf(word,"temp: %2.4f deg. C  ",25+(tem/16.0));
        printword(word,12,102);
        }
    }
