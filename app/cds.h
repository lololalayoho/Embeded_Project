#define CDS_VALUE 200

void init_adc(){
	ADMUX=0x00;
	ADCSRA = 0x87;
}

unsigned short read_adc(){
	unsigned char adc_low,adc_high;
	unsigned short value;
	ADCSRA |= 0x40; // ADC start conversion, ADSC = '1'
	while((ADCSRA & (0x10)) != 0x10); // ADC 변환 완료 검사
	adc_low=ADCL;
	adc_high=ADCH;
	value = (adc_high <<8) | adc_low;
	return value;
}

void show_adc(unsigned short value){
	if(value < CDS_VALUE) {
		PORTA=0xff;
	}
	else{ 
        PORTA=0x00;
    }
}