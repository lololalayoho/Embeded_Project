#include "includes.h"
#include "temperature.h"
#define F_CPU	16000000UL	// CPU frequency = 16 Mhz
#include <avr/io.h>
#include <util/delay.h>

#define ATS75_ADDR 0x98 // 0b10011000, 7비트를 1비트 left shift
#define ATS75_CONFIG_REG 1
#define ATS75_TEMP_REG 0
#define  TASK_STK_SIZE  OS_TASK_DEF_STK_SIZE
#define  N_TASKS        3

OS_STK       TaskStk[N_TASKS][TASK_STK_SIZE];
OS_EVENT	  *Mbox;
OS_FLAG_GRP   *Eflag;

volatile INT8U	FndNum;

void  TemperatureTask(void *data);
void  FndTask(void *data);
void  FndDisplayTask(void *data);


int main (void)
{
  INT8U err;
  OSInit();
  OS_ENTER_CRITICAL();
  TCCR0 = 0x07;
  TIMSK = _BV(TOIE0);
  TCNT0 = 256 - (CPU_CLOCK_HZ / OS_TICKS_PER_SEC / 1024);
  OS_EXIT_CRITICAL();

  Mbox = OSMboxCreate((void*)0);
  Eflag = OSFlagCreate(0x00,&err);

  OSTaskCreate(TemperatureTask, (void *)0, (void *)&TaskStk[0][TASK_STK_SIZE - 1], 0);
  OSTaskCreate(FndTask, (void *)0, (void *)&TaskStk[1][TASK_STK_SIZE - 1], 1);
  OSTaskCreate(FndDisplayTask, (void *)0, (void *)&TaskStk[2][TASK_STK_SIZE - 1], 2);

  OSStart();

  return 0;
}

void InitI2C()
{
    PORTD = 3; 						// For Pull-up override value
    SFIOR &= ~(1 << PUD); 			// PUD
    TWSR = 0; 						// TWPS0 = 0, TWPS1 = 0
    TWBR = 32;						// for 100  K Hz bus clock
	TWCR = _BV(TWEA) | _BV(TWEN);	// TWEA = Ack pulse is generated
									// TWEN = TWI 동작을 가능하게 한다
}

void TemperatureTask (void *data)
{
  int	value;
  int i;
  data = data;
  InitI2C();
  write_twi_1byte_nopreset(ATS75_CONFIG_REG, 0x00); // 9비트, Normal
  write_twi_0byte_nopreset(ATS75_TEMP_REG);
   while (1)  {
    OS_ENTER_CRITICAL();
	value = ReadTemperature();
	OS_EXIT_CRITICAL();
	// 작성
	OSMboxPost(Mbox,(void *)&value);
	OSTimeDly(50);
  }
}

void FndTask (void *data)
{
	INT8U	err;
	data = data;
	// 작성
	while (1) {
	 OS_ENTER_CRITICAL();
	 FndNum = *(INT8U*)OSMboxPend(Mbox,0,&err);
	 OSFlagPost(Eflag,0x0F,OS_FLAG_SET,&err);
	 OS_EXIT_CRITICAL();
	 OSTimeDly(50);
	}
}

void FndDisplayTask (void *data)
{	
	INT8U value;
	unsigned char FND_DATA[ ]= {
		0x3f, // 0
		0x06, // 1
		0x5b, // 2
		0x4f, // 3
		0x66, // 4
		0x6d, // 5
		0x7d, // 6
		0x27, // 7
		0x7f, // 8
		0x6f, // 9
		0x77, // A
		0x7c, // B
		0x39, // C
		0x5e, // D
		0x79, // E
		0x71, // F
		0x80, // .
		0x40, // -
		0x08  // _
	};
	data = data;
	DDRC = 0xff;
    DDRG = 0x0f;
	while(1){
		INT8U err;
		OSFlagPend(Eflag,0x0F,OS_FLAG_WAIT_SET_ALL+OS_FLAG_CONSUME,0,&err);
		value = FndNum;
		PORTC = FND_DATA[value % 10];
		PORTG = 0x01;
		_delay_ms(2);
		PORTC = FND_DATA[value / 10];
		PORTG = 0x02;
		_delay_ms(2);
	}
}
