#define ATS75_CONFIG_REG 1
#define ATS75_TEMP_REG 0
#define F_SCK 40000UL
#define F_CPU 16000000UL
#include <avr/io.h>
#define ATS75_ADDR 0x98 // 0b10011000, 7비트를 1비트 left shift

void InitI2C()
{
	DDRC = 0xff; 
	DDRG = 0x0f; // FND 출력 세팅
	PORTD = 3;   // For Internal pull-up for SCL & SCK
	SFIOR &= ~(1 << PUD); 			// PUD
	TWSR = 0; 						// TWPS0 = 0, TWPS1 = 0
	TWBR = 32;						// for 100  K Hz bus clock
	TWCR = _BV(TWEA) | _BV(TWEN);	// TWEA = Ack pulse is generated
}

void write_twi_1byte_nopreset(unsigned char reg, unsigned char  data)
{
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN); // START 전송
	while(((TWCR & (1 << TWINT)) == 0x00) || ((TWSR & 0xf8) != 0x08 &&   (TWSR & 0xf8) != 0x10)); // ACK를 기다림
	TWDR = ATS75_ADDR | 0;  // SLA+W 준비, W=0
	TWCR = (1 << TWINT) | (1 << TWEN);  // SLA+W 전송
	while(((TWCR & (1 << TWINT)) == 0x00) || (TWSR & 0xf8) != 0x18); 
	TWDR = reg;    // aTS75 Reg 값 준비
	TWCR = (1 << TWINT) | (1 << TWEN);  // aTS75 Reg 값 전송
	while(((TWCR & (1 << TWINT)) == 0x00) || (TWSR & 0xF8) != 0x28);
	TWDR = data;    // DATA 준비
	TWCR = (1 << TWINT) | (1 << TWEN);  // DATA 전송
	while(((TWCR & (1 << TWINT)) == 0x00) || (TWSR & 0xF8) != 0x28);
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); // STOP 전송
}
void write_twi_0byte_nopreset(unsigned char  reg)
{
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN); // START 전송
	while(((TWCR & (1 << TWINT)) == 0x00) || ((TWSR & 0xf8) != 0x08 &&   (TWSR & 0xf8) != 0x10));  // ACK를 기다림
	TWDR = ATS75_ADDR | 0; // SLA+W 준비, W=0
	TWCR = (1 << TWINT) | (1 << TWEN);  // SLA+W 전송
	while(((TWCR & (1 << TWINT)) == 0x00) || (TWSR & 0xf8) != 0x18); 
	TWDR = reg;    // aTS75 Reg 값 준비
	TWCR = (1 << TWINT) | (1 << TWEN);  // aTS75 Reg 값 전송
	while(((TWCR & (1 << TWINT)) == 0x00) || (TWSR & 0xF8) != 0x28);
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); // STOP 전송
}
unsigned short read_twi_2byte_preset()
{ 
	unsigned char high_byte, low_byte;
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN); // START 전송
	while(((TWCR & (1 << TWINT)) == 0x00) || ((TWSR & 0xf8) != 0x08 &&   (TWSR & 0xf8) != 0x10));  // ACK를 기다림
	TWDR = ATS75_ADDR | 1;  // SLA+R 준비, R=1
	TWCR = (1 << TWINT) | (1 << TWEN);  // SLA+R 전송
	while(((TWCR & (1 << TWINT)) == 0x00) || (TWSR & 0xf8) != 0x40); 
	TWCR = (1 << TWINT) | (1 << TWEN | 1 << TWEA);// 1st DATA 준비
	while(((TWCR & (1 << TWINT)) == 0x00) || (TWSR & 0xf8) != 0x50);
	high_byte = TWDR;    // 1 byte DATA 수신
	TWCR = (1 << TWINT) | (1 << TWEN | 1 << TWEA);// 2nd DATA 준비 
	while(((TWCR & (1 << TWINT)) == 0x00) || (TWSR & 0xf8) != 0x50); 
	low_byte = TWDR;    // 1 byte DATA 수신
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); // STOP 전송

	return ((high_byte<<8) | low_byte);  // 수신 DATA 리턴
}

void DisplayTemp (unsigned short value)
{	
	unsigned char digit[12] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7c, 0x07, 0x7f, 0x67, 0x40, 0x00 };
	unsigned char fnd_sel[4] = {0x01, 0x02, 0x04, 0x08};
	unsigned char value_int, value_deci, num[4];
	INT8U err;
	int i,j;
	for(j = 0; j<100; j++){
		if((value & 0x8000) != 0x8000)  // Sign 비트 체크
			num[3] = 11;
		else
		{
			num[3] = 10;
			value = (~value)-1;   // 2’s Compliment
		}

		value_int = (unsigned char)((value & 0x7f00) >> 8);
		value_deci = (unsigned char)(value & 0x00ff);

		num[2] = (value_int / 10) % 10;
		num[1] = value_int % 10;
		num[0] = ((value_deci & 0x80) == 0x80) * 5; 
		for(i=0; i<4; i++)
		{
			PORTC = digit[num[i]];
			PORTG = fnd_sel[i];
			if(i==1) PORTC |= 0x80;
			_delay_ms(2);
		}
	}
}