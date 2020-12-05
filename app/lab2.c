#include "includes.h"
#include "temperature.h"
#include "cds.h"
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#define F_CPU 16000000UL	// CPU frequency = 16 Mhz
#define  TASK_STK_SIZE  OS_TASK_DEF_STK_SIZE
#define  N_TASKS        5
#define Temp 1
#define CDS 0
#define ON 1
#define OFF 0

OS_STK       TaskStk[N_TASKS][TASK_STK_SIZE];
OS_EVENT	  *checktemperbox;
OS_FLAG_GRP   *manageflag;
OS_FLAG_GRP   *checkTemperflag;

unsigned short temperature_value;
unsigned char  value_int, value_deci,num[4];
int index = -1;

volatile int signal = CDS;
volatile int mel_idx =0;
volatile int state = OFF;

INT8U  const  myMapTbl[]   = {0X01,0X03,0x0F,0x3F,0x7F};
unsigned char melody[2] = {43,114};

ISR(INT4_vect){
	signal = Temp;
	_delay_ms(10);
}

ISR(INT5_vect){
	INT8U i;
	signal = CDS;

	_delay_ms(10);
}

ISR(TIMER2_OVF_vect){
	if(state == ON){
		PORTB = 0x00;
		state = OFF;
	}
	else{
		PORTB = 0xff;
		state = ON;
	}
	TCNT0 = melody[mel_idx];
}

void  cdsTask(void *data);
void  TemperatureTask(void *data);
void  manageTask(void *data);
void  checkTemper(void *data);
void  FireEmergence(void *data);

int main (void){
  INT8U err;
  INT8U i;
  OSInit();
  OS_ENTER_CRITICAL();

  TCCR0 = 0x07;
  TIMSK = _BV(TOIE0);
  TCNT0 = 256 - (CPU_CLOCK_HZ / OS_TICKS_PER_SEC / 1024);
  checktemperbox = OSMboxCreate((void*)0);
  manageflag = OSFlagCreate(0x00,&err);
  checkTemperflag = OSFlagCreate(0x00,&err);
  
  OS_EXIT_CRITICAL();

  OSTaskCreate(manageTask, (void *)0,  (void *)&TaskStk[0][TASK_STK_SIZE - 1], 0);
  OSTaskCreate(TemperatureTask, (void *)0, (void *)&TaskStk[4][TASK_STK_SIZE - 1], 4);
  OSTaskCreate(cdsTask, (void *)0,(void *)&TaskStk[1][TASK_STK_SIZE - 1],1);
  OSTaskCreate(checkTemper, (void *)0,(void *)&TaskStk[3][TASK_STK_SIZE - 1],3);
  OSTaskCreate(FireEmergence, (void *)0,(void *)&TaskStk[2][TASK_STK_SIZE - 1],2);
  
  OSStart();

  return 0;
}

void manageTask(void *data){
	INT8U	err,i;
	data = data;
	DDRC = 0xff;
	DDRG = 0x0f;
	DDRE = 0xCF;
	EICRB = 0x0A;
	EIMSK = 0x30;
	SREG |= 1 << 7;
	while(1){
		if(signal==Temp){
			OSFlagPost(manageflag,0x01,OS_FLAG_SET,&err);
		}
		if(signal==CDS){
			index = -1;
			OSFlagPost(manageflag,0x02,OS_FLAG_SET,&err);
		}
		OSTimeDly(1);
	}
}

void cdsTask(void *data){
	INT8U	err;
	unsigned char value;
	DDRA = 0xff;
	DDRC = 0xff;
	DDRG = 0x0f;
	data = data;
	init_adc();
	while(1){
		OSFlagPend(manageflag,0x02,OS_FLAG_WAIT_SET_ALL+OS_FLAG_CONSUME,0,&err);
		value = read_adc();
		show_adc(value);
		OSTimeDly(1);
	}
}

float calculate(unsigned short number){
	unsigned short value;
	value = number;
	if((value & 0x8000) != 0x8000)  // Sign 비트 체크
			num[3] = 11;
	else{
			num[3] = 10;
			value = (~value)-1;   // 2’s Compliment
	}
	value_int = (unsigned char)((value & 0x7f00) >> 8);
	value_deci = (unsigned char)(value & 0x00ff);
	num[2] = (value_int / 10) % 10;
	num[1] = value_int % 10;
	num[0] = ((value_deci & 0x80) == 0x80) * 5;
	float num1 = ((num[2] - '0') * 10) +(num[1]-'0');
	float num2 = ((num[0] - '0') * 0.1);
	return num1 + num2;
}

void TemperatureTask (void *data){
  INT8U	err,i;
  unsigned short value;
   data = data;
   DDRA = 0xff;

  InitI2C();
  write_twi_1byte_nopreset(ATS75_CONFIG_REG, 0x00); // 9비트, Normal
  write_twi_0byte_nopreset(ATS75_TEMP_REG);
   while (1){
	OSFlagPend(manageflag,0x01,OS_FLAG_WAIT_SET_ALL+OS_FLAG_CONSUME,0,&err);
	value = read_twi_2byte_preset();
	DisplayTemp(value);
	if(index==-1){
		temperature_value = value;
		index = index +1;
	}
	else{
	    float value1 = calculate(value);
	    float value2 = calculate(temperature_value);
		if(value1 > value2){
			if(index==4){
				OSFlagPost(checkTemperflag,0xff,OS_FLAG_SET,&err);
				index = 0;
			}
			else{
				OSMboxPost(checktemperbox,(void*)&index);
				index = index + 1;
			}
		}
		if(value1 < value2){
			if(index >= 0){
				index = index - 1;
				OSMboxPost(checktemperbox,(void*)&index);
			}
		}
		temperature_value = value;
	}
  }
} 

void checkTemper(void * data){
	data = data;
	INT8U err, i;
	int value;
	DDRA = 0xff;
	while(1){
		if(index>=0 && signal == Temp){
			value= *(int*)OSMboxPend(checktemperbox,0,&err);
			if(value==-1)
				PORTA = 0x00;
			else
				PORTA = myMapTbl[value];			
		}
		OSTimeDly(1);
	}
}

void FireEmergence(void *data){
	INT8U err,i;
	int value;

	DDRA = 0xff;
	DDRB = 0x10;
	TCCR2 = 0x03;

	DDRE = 0xCF;
	EICRB = 0x0A;
	EIMSK = 0x30;

	TCNT2 = melody[mel_idx];
	sei();
	while(1){
		OSFlagPend(checkTemperflag,0xff,OS_FLAG_WAIT_SET_ALL+OS_FLAG_CONSUME,0,&err);
		for(i= 0;i<200; i++){
			if(signal == Temp){
				PORTA ^= 0xff;
				TIMSK = 0x40;
				_delay_ms(5000);
				mel_idx = mel_idx + 1;
				//_delay_ms(500);
				if(mel_idx == 2)
					mel_idx = 0;
			}
			else{
				PORTA = 0x00;
				TIMSK = 0x00;
				break;
			}
		}
		OSTimeDly(1);
	}
}
