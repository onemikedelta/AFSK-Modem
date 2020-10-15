#define PTT 9
#define CDATA 5
#define RDATA 7
#define SCLK 4
#define CS 6
#define RXFF A3       //rx freeformat, generally set to 1
#define FX809IRQ 3
#define KEYPAD A6     //read resistor divider keypad


#define GENERAL_RESET  0x01
#define CONTROL_REG  0x40
#define STATUS_REG   0x41
#define RXDATA_REG       0x42
#define TXDATA_REG       0x43
#define SYNC_REG         0x44

#define FX809SETTINGS 0b00111010    
//0x20 SYNT Prime,  0x10 SYNC Prime, 0x08 Interrupt Enable
//0x04 Powersave, 0x02 Checksum Enable, 0x01 TX Mode

byte PREAMBLE = 5;
byte POSTAMBLE = 5;
byte preamblecount;

byte tx_buffer[64];
byte tx_data_size = 0;
byte tx_buffer_counter = 0;

byte rx_buffer[64];
byte rx_buffer_counter = 0;
bool data_received = false;


void write_byte(byte data)
{
  delayMicroseconds(3);
  for(int i=7;i>0;i--)
  {digitalWrite(CDATA,(data&bit(i)));
   delayMicroseconds(1);
   digitalWrite(SCLK,1);
   delayMicroseconds(2);
   digitalWrite(SCLK,0);
   delayMicroseconds(2);
  }
 delayMicroseconds(3);  
}

void write_byte(byte data,byte addr)
{
  digitalWrite(CS,LOW);
  write_byte(addr);
  write_byte(data);
  digitalWrite(CS,HIGH);
}

byte read_byte(byte addr)
{
  byte rxdata = 0;
  write_byte(addr);
  delayMicroseconds(3);
  for(int i=7;i>0;i--)
  {
    digitalWrite(SCLK,1);
    delayMicroseconds(2);
    if(digitalRead(RDATA))
       rxdata+=bit(i);
    digitalWrite(SCLK,0);
    delayMicroseconds(2);
  }
  delayMicroseconds(3); 
  return rxdata;
}




void modem_init()
{
  pinMode(CDATA,OUTPUT);
  pinMode(SCLK,OUTPUT);
  pinMode(CS,OUTPUT);
  pinMode(RXFF,OUTPUT);
  pinMode(RDATA,INPUT);
  pinMode(FX809IRQ,INPUT_PULLUP);
  
  digitalWrite(RXFF, HIGH);   //turn on freeformat mode
  digitalWrite(CS, HIGH);     //setup chip select to idle
  digitalWrite(PTT, LOW);
  digitalWrite(SCLK, LOW);    //data is clocked in/out rising edge
  attachInterrupt(FX809IRQ,modem_ISR, FALLING); 
  
  digitalWrite(CS,LOW);
  write_byte(GENERAL_RESET);
  digitalWrite(CS,HIGH);
  delay(1);

  write_byte(FX809SETTINGS,CONTROL_REG);
  delay(1);

  digitalWrite(CS,LOW);
  write_byte(SYNC_REG);
  write_byte(0xC4);
  write_byte(0xD7);
  digitalWrite(CS,HIGH);
  delay(1);
  
}

void modem_ISR()
{
  byte modem_status = 0;
  
  digitalWrite(CS,LOW);
  modem_status = read_byte(STATUS_REG) & 0x3F;
  digitalWrite(CS,HIGH);
  delayMicroseconds(5);

  if(modem_status & 0x01)    //Rx Data ready
  {
      digitalWrite(CS,0);
      rx_buffer[rx_buffer_counter++] = read_byte(RXDATA_REG);
      digitalWrite(CS,1);
  }

    if(modem_status & 0x04)    //Tx Data Ready
  {
    if(preamblecount<PREAMBLE)
      {preamblecount++;
       write_byte(0xAA,TXDATA_REG);                             //transmit preamble
      }
   else if(preamblecount==PREAMBLE)                 //Send SYNC
        { write_byte(0xC4,TXDATA_REG);   
          preamblecount++;
        }   
   else if(preamblecount==PREAMBLE+1)               //Send SYNC
        { write_byte(0xD7,TXDATA_REG);   
          preamblecount++; 
        }   
   else if(preamblecount==PREAMBLE+2)               //Turn on internal checksum
        { write_byte(0x0B,CONTROL_REG);       //Interrupt, Checksum, TX   
          preamblecount++;
          tx_buffer_counter = 1; 
          write_byte(tx_buffer[0],TXDATA_REG);
        }   
   else if(tx_buffer_counter <= tx_data_size)       
      { 
        write_byte(tx_buffer[tx_buffer_counter++],TXDATA_REG);
      }
   else if(preamblecount < POSTAMBLE+PREAMBLE+2)       
      { write_byte(0xFF,TXDATA_REG);
        preamblecount++;
      }
   
    
    
    
    
    }






  if(modem_status & 0x02)    //RX Checksum true
  {
      data_received = true;
  }


    
  if(modem_status & 0x08)    //Tx Idle
  {
    write_byte(FX809SETTINGS,CONTROL_REG);  
    digitalWrite(PTT,LOW);
  }
  if(modem_status & 0x10)    //RX SYNC Detect
  {Serial.print("\nSYNC, ");}
  if(modem_status & 0x20)    //RX SYNT Detect
  {Serial.print("\nSYNT, ");}

  
}


void modem_txi()
{
  preamblecount = 1;
  tx_buffer_counter = 0;
  
  write_byte(0x09,CONTROL_REG);      //enable tx
  delay(10);
  digitalWrite(PTT,HIGH);        
  delay(100);
  write_byte(0xAA,TXDATA_REG);
    
}


/*
//UNTESTED IRQ pin pooling, TX function
void modem_tx()
{
  int counter = 0;
  
  digitalWrite(CS,LOW);
  write_byte(W_CONTROL_REG);
  write_byte(0x09);    //enable tx 
  digitalWrite(CS,HIGH);
  delay(10);
  digitalWrite(PTT,HIGH);            
  delay(200);
  detachInterrupt(FX809IRQ);      //disable interrupt
  
  
  while(counter< preamble)
  {
    digitalWrite(CS,LOW);  
    write_byte(W_TXDATA);
    write_byte(0xAA);
    digitalWrite(CS,HIGH);  
    counter++; 
    while(digitalRead(FX809IRQ));       //wait interrupt pin to indicate tx buffer is empty
    } 
  
  digitalWrite(CS,LOW);  
  write_byte(W_TXDATA);
  write_byte(0xC4);
  digitalWrite(CS,HIGH);  
  while(digitalRead(FX809IRQ));       //wait interrupt pin to indicate tx buffer is empty

  digitalWrite(CS,LOW);  
  write_byte(W_TXDATA);
  write_byte(0xD7);
  digitalWrite(CS,HIGH);
  counter = 0;  
  while(digitalRead(FX809IRQ));       //wait interrupt pin to indicate tx buffer is empty
    

    
  while(counter< tx_buffer_counter)
  { digitalWrite(CS,LOW);  
    write_byte(W_TXDATA);
    write_byte(tx_buffer[counter++]); 
    digitalWrite(CS,HIGH);
    while(digitalRead(FX809IRQ));       //wait interrupt pin to indicate tx buffer is empty
  }  
  
  delay(50);
  digitalWrite(PTT,LOW); 

  digitalWrite(CS,LOW);
  write_byte(W_CONTROL_REG);
  write_byte(FX809SETTINGS);    //enable rx 
  digitalWrite(CS,HIGH);
             
  attachInterrupt(FX809IRQ,FX809_ISR, FALLING); 
}

*/


void setup() {
  
  pinMode(PTT,OUTPUT);
  digitalWrite(PTT,LOW);
  
  Serial.begin(115200);
  Serial.println("FX809  AFSK Modem by TA1MD");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);   
  modem_init();
  delay(2000);                       
  digitalWrite(LED_BUILTIN, LOW);    
 
  
}

void loop() {
  
  digitalWrite(LED_BUILTIN, HIGH);   
  
  strcpy(tx_buffer,"testing12345");
  tx_buffer_counter = 12;
  modem_txi();
  Serial.print("tx");
                                    
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);
  
  if(data_received == true)
  {  for(int i=0;i<rx_buffer_counter;i++)
      {Serial.print(rx_buffer[i],HEX); Serial.print(",");}
       
    Serial.println("");
    Serial.print(rx_buffer_counter); Serial.println("  Bytes received.");
    rx_buffer_counter=0;
    data_received = false;
  }






    
}
