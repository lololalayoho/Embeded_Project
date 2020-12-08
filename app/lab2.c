#include "includes.h"
#include "temperature.h"
#include "cds.h"
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#define F_CPU 16000000UL	// CPU frequency = 16 Mhz
#define  TASK_STK_SIZE  OS_TASK_DEF_STK_SIZE
#define  N_TASKS        9
#define Temp 1
#define CDS 0
#define ON 1
#define OFF 0

OS_STK       TaskStk[N_TASKS][TASK_STK_SIZE];
OS_EVENT	  *checktemperbox;
OS_EVENT      *checkcdsQ;
OS_EVENT      *checkcdsbox;
OS_FLAG_GRP   *manageflag;
OS_FLAG_GRP   *checkTemperflag;
OS_FLAG_GRP   *totalcntcheckflag;
OS_FLAG_GRP	  *checkcdsflag;		

unsigned short temperature_value;
unsigned char  value_int, value_deci,num[4];
void * checkcds[5];

int temperindex = -1;
int cdsindex = 0;
int index = 0;
int fnd[4];
volatile int signal = CDS;
volatile int mel_idx =0;
volatile int state = OFF;

INT8U  const  myMapTbl[]   = {0X01,0X03,0x0F,0x3F,0x7F};
INT8U  const  MapTbl[]   = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
unsigned char melody[2] = {43,114};
int cdss[5];

ISR(INT4_vect){
	signal = Temp;
	PORTA = 0x00;
	OSTimeDly(1);
}

ISR(INT5_vect){
	signal = CDS;
	PORTB = 0X00;
	PORTA = 0x00;
	state = OFF;
	index = 0;
	TIMSK = _BV(TOIE0);
	OSTimeDly(1);
}

ISR(TIMER2_OVF_vect){
	if(state == ON){
		PORTB = 0x00;
		state = OFF;
	}
	else{
		PORTB = 0x10;
		state = ON;
	}
	TCNT2 = melody[mel_idx];
}

void  cdsTask(void *data);
void  TemperatureTask(void *data);
void  manageTask(void *data);
void  checkTemper(void *data);
void  FireEmergence(void *data);
void CntCDS(void *data);
void LightON(void * data);
void LightOFF(void * data);
void DisplayCDS(void *data);

int main (void){
  INT8U err;
  INT8U i;
  OSInit();
  OS_ENTER_CRITICAL();

  TCCR0 = 0x07;
  TIMSK = _BV(TOIE0);
  TCNT0 = 256 - (CPU_CLOCK_HZ / OS_TICKS_PER_SEC / 1024);
  
  checktemperbox = OSMboxCreate((void*)0);
  checkcdsQ = OSQCreate(&checkcds[0],5);
  checkcdsbox = OSMboxCreate((void*)0);
  manageflag = OSFlagCreate(0x00,&err);
  checkTemperflag = OSFlagCreate(0x00,&err);
  checkcdsflag = OSFlagCreate(0x00, &err);
  totalcntcheckflag = OSFlagCreate(0x00, &err);
  
  OS_EXIT_CRITICAL();

  OSTaskCreate(manageTask, (void *)0,  (void *)&TaskStk[0][TASK_STK_SIZE - 1], 0);
  OSTaskCreate(TemperatureTask, (void *)0, (void *)&TaskStk[8][TASK_STK_SIZE - 1], 8);
  OSTaskCreate(cdsTask, (void *)0,(void *)&TaskStk[6][TASK_STK_SIZE - 1],6);
  OSTaskCreate(checkTemper, (void *)0,(void *)&TaskStk[4][TASK_STK_SIZE - 1],4);
  OSTaskCreate(FireEmergence, (void *)0,(void *)&TaskStk[3][TASK_STK_SIZE - 1],3);
  OSTaskCreate(CntCDS, (void *)0,(void *)&TaskStk[5][TASK_STK_SIZE - 1],5);
  OSTaskCreate(LightON, (void *)0,(void *)&TaskStk[1][TASK_STK_SIZE - 1],1);
  OSTaskCreate(LightOFF, (void *)0,(void *)&TaskStk[2][TASK_STK_SIZE - 1],2);
  OSTaskCreate(DisplayCDS, (void *)0,(void *)&TaskStk[7][TASK_STK_SIZE - 1],7);
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
			temperindex = -1;
			OSFlagPost(manageflag,0x02,OS_FLAG_SET,&err);
		}
		OSTimeDly(1);
	}
}

void cdsTask(void *data){
	INT8U	err;
	int message;
	unsigned char value;
	DDRA = 0xff;
	DDRC = 0xff;
	DDRG = 0x0f;
	data = data;
	init_adc();
	while(1){
		OSFlagPend(manageflag,0x02,OS_FLAG_WAIT_SET_ALL+OS_FLAG_CONSUME,0,&err);
		index = index + 1;
		value = read_adc();
		if(value < CDS_VALUE){
			if(cdsindex < 5){
				cdsindex = cdsindex + 1;
			}
			message = 1;
		}
		else{
			if(cdsindex > 0){
				cdsindex = cdsindex - 1;
			}
			message = 0;
		}
		OSQPost(checkcdsQ,(void *)&message);
		OSMboxPost(checkcdsbox,(void *)&cdsindex);
		OSTimeDly(50);
	}
}

void CntCDS(void *data){
	int i,j;
	INT8U err;
	int value;
	data = data;
	DDRA = 0xff;
	while(1){
		for(i = 0; i<5; i++){
			value = *(int *)OSQPend(checkcdsQ,0,&err);
		    cdss[i] = value;
		}
		int cnt = 0;
		for(i = 0; i<5; i++){
			if(cdss[i]==1){
				cnt = cnt + 1;
			}
		}
		if(cnt==5){
			OSFlagPost(checkcdsflag,0x01,OS_FLAG_SET,&err);
		}
		if(cnt==0){
			OSFlagPost(checkcdsflag,0x02,OS_FLAG_SET,&err);
		}
		OSTimeDly(1);
	}
}

void LightON(void *data){
	data = data;
	unsigned short avg;
	INT8U	err;
	while(1){
		OSFlagPend(checkcdsflag,0x01,OS_FLAG_WAIT_SET_ALL+OS_FLAG_CONSUME,0,&err);
		avg = 150;
		show_adc(avg);
		OSTimeDly(1);
	}
}

void LightOFF(void* data){
	data = data;
	unsigned short avg;
	INT8U	err;
	while(1){
		OSFlagPend(checkcdsflag,0x02,OS_FLAG_WAIT_SET_ALL+OS_FLAG_CONSUME,0,&err);
		avg = 250;
		show_adc(avg);
		OSTimeDly(1);
	}
}

void DisplayCDS(void *data){
	data = data;
	unsigned short avg;
	INT8U	err;
	DDRC = 0xff;
	DDRG = 0x0f;
	int time,i,j;
	unsigned char digit[12] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7c, 0x07, 0x7f, 0x67, 0x40, 0x00 };
	unsigned char fnd_sel[4] = {0x01, 0x02, 0x04, 0x08};
	while(1){
		time = *(int *)OSMboxPend(checkcdsbox,0,&err);
		fnd[0] = time%10;
		PORTC = digit[fnd[0]];
		PORTG = fnd_sel[0];
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
	if(temperindex==-1){
		temperature_value = value;
		temperindex = temperindex +1;
	}
	else{
	    float value1 = calculate(value);
	    float value2 = calculate(temperature_value);
		if(value1 > value2){
			if(temperindex==4){
				OSFlagPost(checkTemperflag,0xff,OS_FLAG_SET,&err);
				temperindex = 0;
			}
			else{
				OSMboxPost(checktemperbox,(void*)&temperindex);
				temperindex = temperindex + 1;
			}
		}
		if(value1 < value2){
			if(temperindex >= 0){
				temperindex = temperindex - 1;
				OSMboxPost(checktemperbox,(void*)&temperindex);
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
		if(temperindex>=0 && signal == Temp){
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
	DDRB = 0xff;
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
				_delay_ms(50);
				mel_idx = (mel_idx + 1)%2;
			}
			else{
				TIMSK = _BV(TOIE0);
				PORTA = 0x00;
				break;
			}
		}
		OSTimeDly(1);
	}
}

