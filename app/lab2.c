#include "includes.h"
#include "temperature.h"
#include "cds.h"
#include <avr/io.h>
#include <util/delay.h>
#define F_CPU 16000000UL	// CPU frequency = 16 Mhz
#define  TASK_STK_SIZE  OS_TASK_DEF_STK_SIZE
#define  N_TASKS        3
#define Temp 1
#define CDS 0

OS_STK       TaskStk[N_TASKS][TASK_STK_SIZE];
OS_EVENT	  *Mbox;
OS_FLAG_GRP   *Eflag;

volatile int signal = CDS;

ISR(INT4_vect){
	signal = Temp;
	_delay_ms(10);
}

ISR(INT5_vect){
	signal = CDS;
	_delay_ms(10);
}

void  cdsTask(void *data);
void  TemperatureTask(void *data);
void TempStart(void *data);
void manageTask(void *data);

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
  init_adc();
  InitI2C();
  OSTaskCreate(manageTask, (void *)0,  (void *)&TaskStk[0][TASK_STK_SIZE - 1], 0);
  OSTaskCreate(TemperatureTask, (void *)0, (void *)&TaskStk[1][TASK_STK_SIZE - 1], 1);
  OSTaskCreate(cdsTask, (void *)0,(void *)&TaskStk[2][TASK_STK_SIZE - 1],2);
  OSStart();

  return 0;
}

void manageTask(void *data){
	INT8U	err;
	data = data;
	DDRC = 0xff;
	DDRG = 0x0f;
	DDRE = 0xCF;
	EICRB = 0x0A;
	EIMSK = 0x30;
	SREG |= 1 << 7;
	while(1){
		if(signal==Temp){
			OSFlagPost(Eflag,0x01,OS_FLAG_SET,&err);
		}
		if(signal==CDS){
			OSFlagPost(Eflag,0x02,OS_FLAG_SET,&err);
		}
		OSTimeDly(50);
	}
}
void cdsTask(void *data){
	INT8U	err;
	unsigned char value;
	DDRA = 0xff;
	data = data;
	while(1){
		OSFlagPend(Eflag,0x02,OS_FLAG_WAIT_SET_ALL+OS_FLAG_CONSUME,0,&err);
		value = read_adc();
		show_adc(value);
		//OSTimeDly(50);
	}
}

void TemperatureTask (void *data)
{
  INT8U	err;
  unsigned short value;
  data = data;
  write_twi_1byte_nopreset(ATS75_CONFIG_REG, 0x00); // 9ºñÆ®, Normal
  write_twi_0byte_nopreset(ATS75_TEMP_REG);
   while (1)  {
	OSFlagPend(Eflag,0x01,OS_FLAG_WAIT_SET_ALL+OS_FLAG_CONSUME,0,&err);
	value = read_twi_2byte_preset();
	DisplayTemp(value);
	//OSTimeDly(50);
  }
}
