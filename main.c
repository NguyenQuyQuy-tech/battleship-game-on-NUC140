//------------------------------------------- main.c CODE STARTS -------------------------------------------------------------------------------------
#include <stdio.h>
#include "NUC100Series.h"

#define HXT_STATUS 1<<0
#define PLL_STATUS 1<<2
#define PLLCON_FB_DV_VAL 10
#define CPUCLOCKDIVIDE 1
#define TIMER0_COUNTS 100
#define BUZZER_BEEP_TIME 5
#define BUZZER_BEEP_DELAY 2000000
void Buzzer_beep(int beep_time);
void EINT1_IRQHandler(void);
//delay time for debouncing keys
#define BOUNCING_DLY 180000
volatile int arr1[2]={0,0}; //map cordinate
volatile int digit2 = 0; //cordinate selection -  so lan ban
volatile int map[8][8]={
{1,1,0,0,0,0,0,0},
{1,1,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{1,1,0,0,0,0,0,1},
{1,1,0,0,0,0,0,1}
};
volatile char ReceivedByte;
volatile	int game_state = 0;
volatile int Passed = 0; 
void System_Config(void);
void KeyPadEnable(void);
uint8_t KeyPadScanning(void);
void display(void);
void UART0_Config(void);
void UART0_SendChar(int ch);
char UART0_GetChar();
void UART02_IRQHandler(void);
volatile int row = 0;
volatile int column = 0;
volatile int score = 0;
volatile int digit=0;
void SPI3_Config(void);
void LCD_start(void);
void LCD_command(unsigned char temp);
void LCD_data(unsigned char temp);
void LCD_clear(void);
void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr);
int LoadMapDone = 0;
int BuzzerDone = 0;


//Gloabl Array to display on 7segment for NUC140 MCU
int pattern[] = {
               //   gedbaf_dot_c
                  0b10000010,  //Number 0          // ---a----
                  0b11101110,  //Number 1          // |      |
                  0b00000111,  //Number 2          // f      b
                  0b01000110,  //Number 3          // |      |
                  0b01101010,  //Number 4          // ---g----
                  0b01010010,  //Number 5          // |      |
                  0b00010010,  //Number 6          // e      c
                  0b11100110,  //Number 7          // |      |
                  0b00000010,  //Number 8          // ---d----
                  0b01000010,   //Number 9
                  0b11111111   //Blank LED 
                };    					
														
int main(void)
{    
	uint8_t pressed_key = 0;
  //--------------------------------
	//System initialization
	//--------------------------------
	System_Config();
	//SPI3 and UART0 initialization
	//--------------------------------
	SPI3_Config();
	UART0_Config();
	
	uint8_t i = 0;

	char score_txt[] = "0";

	//--------------------------------
	//LCD initialization
	//--------------------------------
	LCD_start();
	LCD_clear();

	//GPIO Interrupt configuration. GPIO-B15 is the interrupt source
	// Each GPIO pin can be set as chip interrupt source by setting correlative GPIOx_IEN bit and GPIOx_IMD
	PB->PMD &= (~(0x03 << 30)); // 00 = GPIO port [n] pin is in INPUT mode
	PB->IMD &= (~(1 << 15)); //  0 = Edge trigger interrupt 
	PB->IEN |= (1 << 15); // 1 = PIN[15] state lower level or high-to-low change interrupt Enable 
	
	//NVIC interrupt configuration for GPIO-B15 interrupt source
	NVIC->ISER[0] |= 1 << 3;
	NVIC->IP[0] &= (~(3 << 30));
	
	// De-bounce configuration  
	PB->DBEN |= (1 << 15); // enable the debouce
	GPIO->DBNCECON &= ~(0xF << 0);
	GPIO->DBNCECON |= (0b0111 << 0); // sample interrupt input once per 128 clocks 
	GPIO->DBNCECON |= (1<<4); // Debounce counter clock source is the internal 10kHz low speed oscillator 
	//--------------------------------
	//GPIO for key matrix
	//--------------------------------
	KeyPadEnable();
		
	//Configure GPIO for 7segment
	//Set mode for PC4 to PC7 - output push-pull
	GPIO_SetMode(PC, BIT4, GPIO_MODE_OUTPUT);
	GPIO_SetMode(PC, BIT5, GPIO_MODE_OUTPUT);
	GPIO_SetMode(PC, BIT6, GPIO_MODE_OUTPUT);
	GPIO_SetMode(PC, BIT7, GPIO_MODE_OUTPUT);		
	//Set mode for PE0 to PE7 - output push-pull
	GPIO_SetMode(PE, BIT0, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT1, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT2, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT3, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT4, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT5, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT6, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT7, GPIO_MODE_OUTPUT);		
	// GPB.11 Push-Pull Output
	PB->PMD &= ~(0b11 << 22);
	PB->PMD |= (0b01 << 22);

	
	while(1){
		switch (game_state) {
			case 0://loading state
				for (int i = 0; i < 8;  i++) {			
					for (int j = 0; j < 8; j++) {
						if (map[i][j]==2) {map[i][j]=1;}
					}
				}
				row = 0;
				column = 0;
				digit2=0;
				arr1[0]=0;
				arr1[1]=0;
				BuzzerDone=0;
				score=0;
				//welcome state code 
				printS_5x7(1, 32, "Starting game.");
				for (i = 0; i<3; i++) CLK_SysTickDelay(1000000);
				LCD_clear();

				game_state = 1; // state transition
				break;

			case 1://starting screen
				printS_5x7(1, 32, "press pb15 to play!");
				if (LoadMapDone > 0) {
					printS_5x7(10, 8, "Map Loaded Successfully."); 		
				}
				break;

			case 2:// loading static map
				LCD_clear();
				int indexRow = 2;
				int indexColumn = 0; 
					
				for (int i = 0; i < 8;  i++) {			
					for (int j = 0; j < 8; j++) {
						if (map[i][j]!=2) {
							printS_5x7(indexRow, indexColumn, "-");
						} else {
							printS_5x7(indexRow, indexColumn, "X");
						} 
						indexRow=indexRow+8;
					}
					indexColumn=indexColumn+8;
					indexRow=2;
				}
				
				sprintf(score_txt, "%d", score);
				printS_5x7(72, 0, score_txt);
		
				game_state = 3;
				break;
					
			case 3://game play mode 
				//main_game state code here
					display();

				pressed_key = KeyPadScanning();
				
				if(pressed_key==9){
					if(digit==0) {digit=1;} 
					else {digit=0;}
					CLK_SysTickDelay(BOUNCING_DLY);  
				} else { 
					if (pressed_key!=0) {
						arr1[digit]=pressed_key;
						CLK_SysTickDelay(BOUNCING_DLY);
					}
				}
				pressed_key=0;

				if (digit2 > 15) {
					LCD_clear();
					game_state = 4;
				}
				if (score ==10) {
					LCD_clear();
					game_state = 4;
				}
				break;

			case 4:
				//end_game code here
				PC->DOUT &= ~(1<<7);     //Logic 1 to turn on the digit
				PC->DOUT &= ~(1<<6);		//SC3
				PC->DOUT &= ~(1<<5);		//SC2
				PC->DOUT &= ~(1<<4);		//SC1		

				if (score ==10){
					printS_5x7(10, 8, "YOU WIN."); 
				
				} else {
				printS_5x7(10, 8, "YOU LOSE.");
				}
				
				printS_5x7(1, 32, "press pb15 to replay!");
				
				if(BuzzerDone==0){
					Buzzer_beep(BUZZER_BEEP_TIME);
					BuzzerDone=1;
				}
				for (i = 0; i<2; i++) CLK_SysTickDelay(10000);
				break;

			default: break;
		}
	}
}		

void System_Config (void){
		SYS_UnlockReg(); // Unlock protected registers
    //Enable clock sources
    CLK->PWRCON |= (1 << 0);
	while (!(CLK->CLKSTATUS & HXT_STATUS));
	//PLL configuration starts
	CLK->PLLCON &= ~(1 << 19); //0: PLL input is HXT
	CLK->PLLCON &= ~(1 << 16); //PLL in normal mode
	CLK->PLLCON &= (~(0x01FF << 0));
	CLK->PLLCON |= 48;
	CLK->PLLCON &= ~(1 << 18); //0: enable PLLOUT
	while (!(CLK->CLKSTATUS & PLL_STATUS));
	
	//PLL configuration ends
	
	// CPU clock source selection
CLK->CLKSEL0 &= (~(0x07 << 0));
CLK->CLKSEL0 |= (0x02 << 0);    
//clock frequency division
CLK->CLKDIV &= (~0x0F << 0);

//Timer 0 initialization start--------------
	//TM0 Clock selection and configuration
	CLK->CLKSEL1 &= ~(0b111 << 8);
	CLK->CLKSEL1 |= (0b000 << 8);
	CLK->APBCLK |= (1 << 2);
	TIMER0->TCSR &= ~(0xFF << 0);
	
	//reset Timer 0
	TIMER0->TCSR |= (1 << 26);
	
	//define Timer 0 operation mode
	TIMER0->TCSR &= ~(0x03 << 27);
	TIMER0->TCSR |= (0x01 << 27);
	TIMER0->TCSR &= ~(1 << 24);
	
	//TimeOut 
	TIMER0->TCMPR = TIMER0_COUNTS;
	
	//Enable interrupt in the Control Resigter of Timer istsle
	TIMER0->TCSR |= (1 << 29);
	
	//Set Timer0 in NVIC Set-Enable Control Register (NVIC_ISER)
	NVIC->ISER[0] |= 1 << 8;

//UART0 Clock selection and configuration
CLK->CLKSEL1 |= (0b11 << 24); // UART0 clock source is 22.1184 MHz
CLK->CLKDIV &= ~(0xF << 8); // clock divider is 1
CLK->APBCLK |= (1 << 16); // enable UART0 clock


    SYS_LockReg();  // Lock protected registers	
	
	//GPIO initialization start --------------------
	//GPIOC.12: output push-pull
    PC->PMD &= (~(0b11<< 24));		
    PC->PMD |= (0b01 << 24);    	
	//GPIO initialization end ----------------------
	

TIMER0->TCSR |= (1 << 30);
	
	//UART0 Clock selection and configuration
CLK->CLKSEL1 |= (0b11 << 24); // UART0 clock source is 22.1184 MHz
CLK->CLKDIV &= ~(0xF << 8); // clock divider is 1
CLK->APBCLK |= (1 << 16); // enable UART0 clock



	//enable clock of SPI3
	CLK->APBCLK |= 1 << 15;
SYS_LockReg();  // Lock protected registers    
}
void UART0_Config(void) {
	// UART0 pin configuration. PB.1 pin is for UART0 TX
	PB->PMD &= ~(0b11 << 2);
	PB->PMD |= (0b01 << 2); // PB.1 is output pin
	SYS->GPB_MFP |= (1 << 1); // GPB_MFP[1] = 1 -> PB.1 is UART0 TX pin
	SYS->GPB_MFP |= (1 << 0); // GPB_MFP[0] = 1 -> PB.0 is UART0 RX pin
	PB->PMD &= ~(0b11 << 0); // Set Pin Mode for GPB.0(RX - Input)
	// UART0 operation configuration
	UART0->LCR |= (0b11 << 0); // 8 data bit
	UART0->LCR &= ~(1 << 2); // one stop bit
	UART0->LCR &= ~(1 << 3); // no parity bit
	UART0->FCR |= (1 << 1); // clear RX FIFO
	UART0->FCR |= (1 << 2); // clear TX FIFO
	UART0->FCR &= ~(0xF << 16); // FIFO Trigger Level is 1 byte]
	//Baud rate config: BRD/A = 1, DIV_X_EN=0
	//--> Mode 0, Baud rate = UART_CLK/[16*(A+2)] = 22.1184 MHz/[16*(142+2)]= 460800 bps
	UART0->BAUD &= ~(0b11 << 28); // mode 0
	UART0->BAUD &= ~(0xFFFF << 0);
	UART0->BAUD |= 142;

	
	//interupt
	UART0->IER |= (1 << 0); // enable
	UART0->FCR |= (0000 << 4); // trigger level
	
	NVIC->ISER[0] |= 1<<12;
	NVIC->IP[1] &= (~(0b11<<22));

}

//function to set GPIO mode for key matrix 
void KeyPadEnable(void){
	GPIO_SetMode(PA, BIT0, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT1, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT2, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT3, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT4, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT5, GPIO_MODE_QUASI);
}

//function to scan the key pressed
uint8_t KeyPadScanning(void){
	// col 1
	PA0=1; PA1=1; PA2=0; PA3=1; PA4=1; PA5=1;
	if (PA3==0) return 1;
	if (PA4==0) return 4;
	if (PA5==0) return 7;
	// Col 2
	PA0=1; PA1=0; PA2=1; PA3=1; PA4=1; PA5=1;
	if (PA3==0) return 2;
	if (PA4==0) return 5;
	if (PA5==0) return 8;
	// Col 3
	PA0=0; PA1=1; PA2=1; PA3=1; PA4=1; PA5=1;
	if (PA3==0) return 3;
	if (PA4==0) return 6;
	if (PA5==0) return 9;
	return 0;
}

void display(void){
	//Select the 7segment one by one and display cordinate and number of shot
		while (Passed != 1);
			Passed = 0;
	PE->DOUT = pattern[arr1[0]];
	PC->DOUT |= (1<<7);     //Logic 1 to turn on the digit
	PC->DOUT &= ~(1<<6);		//SC3
	PC->DOUT &= ~(1<<5);		//SC2
	PC->DOUT &= ~(1<<4);		//SC1		
		while (Passed != 1);
			Passed = 0;
	PE->DOUT = pattern[arr1[1]];
	PC->DOUT &= ~ (1<<7);   //Logic 1 to turn on the digit
	PC->DOUT |=(1<<6);			//SC3
	PC->DOUT &= ~(1<<5);		//SC2
	PC->DOUT &= ~(1<<4);		//SC1		
		 while (Passed != 1);
				Passed = 0;
	PE->DOUT = pattern[digit2/10];
	PC->DOUT &= ~ (1<<7);   //Logic 1 to turn on the digit
	PC->DOUT &= ~(1<<6);		//SC3
	PC->DOUT |=(1<<5);			//SC2
	PC->DOUT &= ~(1<<4);		//SC1	
		 while (Passed != 1);
				Passed = 0;;
		//Select the first 7segment U11
	PE->DOUT = pattern[digit2%10];
	PC->DOUT &= ~ (1<<7);   //Logic 1 to turn on the digit
	PC->DOUT  &= ~(1<<6);		//SC3
	PC->DOUT &= ~(1<<5);		//SC2
	PC->DOUT |=(1<<4);			//SC1		
		while (Passed != 1);
				Passed = 0;
}

void EINT1_IRQHandler(void) {
	switch (game_state){
		case 1://start game
		LCD_clear();	
		CLK_SysTickDelay(20000);
		game_state=2;
			break;

		case 3://shot
			if(map[arr1[0]-1][arr1[1]-1]==1) {
				map[arr1[0]-1][arr1[1]-1]=2;
				score++;
				// flash led12 when hit
//				PC->DOUT ^= 1 << 12;
//				CLK_SysTickDelay(2000);
//				
//				PC->DOUT ^= 1 << 12;
//				CLK_SysTickDelay(2000);
//				
//				PC->DOUT ^= 1 << 12;
//				CLK_SysTickDelay(2000);
//				
//				PC->DOUT ^= 1 << 12;
//				CLK_SysTickDelay(2000);
//				
//				PC->DOUT ^= 1 << 12;
//				CLK_SysTickDelay(2000);
//				
//				PC->DOUT ^= 1 << 12;
//				CLK_SysTickDelay(2000);
			}
			if (digit2<16){digit2=digit2+1;}
				game_state=2;
				break;
		
		case 4://restart
			LCD_clear();	
			game_state=0;
			break;	
		
	}
	PB->ISRC |= (1 << 15);
}



void TMR0_IRQHandler(void) {
	Passed = 1;
	TIMER0->TISR |= (1 << 0);
} 

void UART0_SendChar(int ch){
	while(UART0->FSR & (0x01 << 23)); //wait until TX FIFO is not full
	UART0->DATA = ch;
	if(ch == '\n'){ // \n is new line
		while(UART0->FSR & (0x01 << 23));
		UART0->DATA = '\r'; // '\r' - Carriage return
	}
}


void UART02_IRQHandler(void){

	ReceivedByte = UART0->DATA;
	UART0_SendChar(ReceivedByte);

	if ((ReceivedByte == '0' ||ReceivedByte == '1')) { 	
		if(ReceivedByte == '0') map[row][column] = 0;
		if(ReceivedByte == '1') map[row][column] = 1;
	
		if (column == 7) { 
				column = 0;

				if (row == 7) {
					LoadMapDone = 1; 
					row = 0; 
				} else {
					row++;
				}
				 		
		} else {
			column += 1; 	
		}
	}
}	


void SPI3_Config(void) {
	SYS->GPD_MFP |= 1 << 11; //1: PD11 is configured for alternative function
	SYS->GPD_MFP |= 1 << 9; //1: PD9 is configured for alternative function
	SYS->GPD_MFP |= 1 << 8; //1: PD8 is configured for alternative function

	SPI3->CNTRL &= ~(1 << 23); //0: disable variable clock feature
	SPI3->CNTRL &= ~(1 << 22); //0: disable two bits transfer mode
	SPI3->CNTRL &= ~(1 << 18); //0: select Master mode
	SPI3->CNTRL &= ~(1 << 17); //0: disable SPI interrupt
	SPI3->CNTRL |= 1 << 11; //1: SPI clock idle high
	SPI3->CNTRL &= ~(1 << 10); //0: MSB is sent first
	SPI3->CNTRL &= ~(3 << 8); //00: one transmit/receive word will be executed in one data transfer

	SPI3->CNTRL &= ~(31 << 3); //Transmit/Receive bit length
	SPI3->CNTRL |= 9 << 3;     //9: 9 bits transmitted/received per data transfer

	SPI3->CNTRL |= (1 << 2);  //1: Transmit at negative edge of SPI CLK
	SPI3->DIVIDER = 0; // SPI clock divider. SPI clock = HCLK / ((DIVIDER+1)*2). HCLK = 50 MHz
}


void LCD_start(void)
{
	LCD_command(0xE2); // Set system reset
	LCD_command(0xA1); // Set Frame rate 100 fps
	LCD_command(0xEB); // Set LCD bias ratio E8~EB for 6~9 (min~max)
	LCD_command(0x81); // Set V BIAS potentiometer
	LCD_command(0xA0); // Set V BIAS potentiometer: A0 ()
	LCD_command(0xC0);
	LCD_command(0xAF); // Set Display Enable
}

void LCD_command(unsigned char temp)
{
	SPI3->SSR |= 1 << 0;
	SPI3->TX[0] = temp;
	SPI3->CNTRL |= 1 << 0;
	while (SPI3->CNTRL & (1 << 0));
	SPI3->SSR &= ~(1 << 0);
}

void LCD_data(unsigned char temp)
{
	SPI3->SSR |= 1 << 0;
	SPI3->TX[0] = 0x0100 + temp;
	SPI3->CNTRL |= 1 << 0;
	while (SPI3->CNTRL & (1 << 0));
	SPI3->SSR &= ~(1 << 0);
}

void LCD_clear(void)
{
	int16_t i;
	LCD_SetAddress(0x0, 0x0);
	for (i = 0; i < 132 * 8; i++)
	{
		LCD_data(0x00);
	}
}

void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr)
{
	LCD_command(0xB0 | PageAddr);
	LCD_command(0x10 | (ColumnAddr >> 4) & 0xF);
	LCD_command(0x00 | (ColumnAddr & 0xF));
}
void Buzzer_beep(int beep_time){
    int j;
    for(j=0;j<(beep_time*2);j++){
        PB->DOUT ^= (1 << 11);
        CLK_SysTickDelay(BUZZER_BEEP_DELAY);
        }   
    }

//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------------------			
