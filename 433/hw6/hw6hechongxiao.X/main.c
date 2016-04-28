#include<xc.h>           // processor SFR definitions
#include<sys/attribs.h>  // __ISR macro
#include<math.h>
#include"i2c_master_noint.h"
#define CS LATBbits.LATB7       // chip select pin
#define SineCount 100
#define TriangleCount 200
#define Pi 3.14159265

char read=0x00;
short arr[14];
short tem,gx,gy,gz,ax,ay,az;
float ax2,ay2;

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

void init_OC(){
    RPA0Rbits.RPA0R=0b0101;
    RPA1Rbits.RPA1R=0b0101;
    T2CONbits.TCKPS=0b011;
    PR2=5999;
    TMR2=0;
    OC1CONbits.OC32=0;
    OC1CONbits.OCTSEL=0;
    OC1CONbits.OCM=0b110;
    OC1RS=3000;
    OC1R=3000;
    OC2CONbits.OC32=0;
    OC2CONbits.OCTSEL=0;
    OC2CONbits.OCM=0b110;
    OC2RS=3000;
    OC2R=3000;
    T2CONbits.ON=1;
    OC1CONbits.ON=1;
    OC2CONbits.ON=1;
    
    IPC2bits.T2IP = 5;
    IPC2bits.T2IS = 0;
    IFS0bits.T2IF = 0;
    IEC0bits.T2IE = 1;
}

void __ISR(_TIMER_2_VECTOR, IPL5SOFT) PWMcontroller(void){
    ax2=3000.0*ax/32768.0+3000.0;
    ay2=3000.0*ay/32768.0+3000.0;
    OC1RS=(short)ax2;
    OC2RS=(short)ay2;
    IFS0bits.T2IF=0;
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
    //spi_init();
    i2c_master_setup();
   // readaddress();
    init_OC();
    setlsm(0x10,0b10000000);
    setlsm(0x11,0b10000000);
    setlsm(0x12,0b00000100);
   // i2c_readmulti();
    __builtin_enable_interrupts();
    
    LATAbits.LATA4=0;    
     while(1){
        _CP0_SET_COUNT(0);
        //LATAINV = 0x10; // make sure timer2 works
        /*
        if(PORTBbits.RB4==0)
        {
            if(read == 0x69)
            LATAbits.LATA4=1;
            else
            LATAbits.LATA4=0;
        }
        else
        LATAbits.LATA4=0;
         */
        while(_CP0_GET_COUNT() < 480000) { 
            ;
        }
i2c_readmulti();
    }   
}