#include <avr/io.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include "OneWire.h"
#include "uart.h"
#include "parser.h"
#include "module.h"
#include "actuator.h"
#include "common.h"
#include "light_sensor.h"
#include "humidity_sensor.h"
#include "fona.h"
#include "tsmppt.h"
#include "self_identification.h"

#define MAX_DEVICES 100
#define NUM_SERIAL_TYPES 3


#define COMMAND_LONG (unsigned char *) \
  "COMMAND EXCEEDED TX_BUF_SIZE Characters\r\n"

typedef Module (* NEW_DEVICE_FUNC_TYPE) (uint8_t, Module);

Module devices[MAX_DEVICES]; // all devices currently running
uint8_t devices_valid[MAX_DEVICES]; // device exists if its index contains 1

#ifdef ACTUATOR_H_
  static const char actuator_str[] PROGMEM = ACTUATOR_IDENTIFIER_STRING;
#endif
#ifdef ONEWIRE_H
  static const char temp_sens_str[] PROGMEM = TEMP_SENSOR_IDENTIFIER_STRING;
#endif
#ifdef LIGHT_SENSOR_H_
  static const char light_sensor_str[] PROGMEM = LIGHT_SENSOR_IDENTIFIER_STRING;
#endif
#ifdef HUMIDITY_SENSOR_H_
  static const char humidity_sensor_str[] PROGMEM = HUMIDITY_SENSOR_IDENTIFIER_STRING;
#endif
#ifdef FONA_H_
  static const char fona_str[] PROGMEM = FONA_IDENTIFIER_STRING;
#endif
#ifdef TSMPPT_H_
  static const char tsmppt_str[] PROGMEM = TSMPPT_IDENTIFIER_STRING;
#endif


// type i points to a corresponding string
static PGM_P type_num_to_string_map[MAX_DEVICES] = {
#ifdef ACTUATOR_H_
  actuator_str,
#endif
#ifdef ONEWIRE_H
  temp_sens_str,
#endif
#ifdef LIGHT_SENSOR_H_
  light_sensor_str,
#endif
#ifdef HUMIDITY_SENSOR_H_
  humidity_sensor_str,
#endif
#ifdef FONA_H_
  fona_str,
#endif
#ifdef TSMPPT_H_
  tsmppt_str,
#endif
};
// type i points to a corresponding creation function
static NEW_DEVICE_FUNC_TYPE type_num_to_create_function_map [MAX_DEVICES] = {
#ifdef ACTUATOR_H_
  &new_actuator,
#endif
#ifdef ONEWIRE_H
  &new_temp_sensor,
#endif
#ifdef LIGHT_SENSOR_H_
  &new_light_sensor,
#endif
#ifdef HUMIDITY_SENSOR_H_
  &new_humidity_sensor,
#endif
#ifdef FONA_H_
  &new_fona,
#endif
#ifdef TSMPPT_H_
  &new_tsmppt,
#endif
};
// port map
volatile uint8_t *port_map[8] = {&PORTA, &PORTB, &PORTC, &PORTD, &PORTE, &PORTF,
  &PORTG, &PORTH};
// pin map
volatile uint8_t *pin_map[8] = {&PINA, &PINB, &PINC, &PIND, &PINE, &PINF,
  &PING, &PINH};
// ddr map
volatile uint8_t *ddr_map[8] = {&DDRA, &DDRB, &DDRC, &DDRD, &DDRE, &DDRF,
  &DDRG, &DDRH};
uint8_t num_types = 0; // track the number of different types
uint8_t device_next; // next index to create a device in
uint8_t devices_count = 0;

// resolve type string to num (just do a linear search)
// return -1 if name not found
int8_t resolve_type_string_to_num(char *target) {
  for (uint8_t i = 0; i < num_types; i++) {
    if (strlen(target) == strlen_P(type_num_to_string_map[i]) &&
        strcmp_P(target, type_num_to_string_map[i]) == 0) {
      return i;
    }
  }
  return -1;
}

// create a device on the next available spot in the array
// create by setting valid bit to 1, calling appropriate create function
//  and increasing the devices_count.
// also call init
void create_device(uint8_t type_num, const uint8_t *pin_map_index,
    const uint8_t *reg_bit, uint8_t pin_count) {
  if (devices_count == MAX_DEVICES) return;
  Module m = new_module();
  m.pin_count = pin_count;
  for (uint8_t i = 0; i < pin_count; i++) { // note that pin_count will be at most 8
    m.port[i] = port_map[pin_map_index[i]];
    m.pin[i] = pin_map[pin_map_index[i]];
    m.ddr[i] = ddr_map[pin_map_index[i]];
    m.reg_bit[i] = reg_bit[i];
  }
  for (uint8_t i = 0; 1; i++) {
    if (devices_valid[i] == 0) {
      devices[i] =
        type_num_to_create_function_map[type_num](type_num, m);
      devices_valid[i] = 1;
      uart_printf("Created new device of type ");
      uart_puts_P(type_num_to_string_map[type_num]);
      uart_printf(" on index %d with %d pins\r\n",
          i, pin_count);
      devices[i].init(devices[i]);
      devices_count++;
      break;
    }
  }
}

// destroy the device and set it's valid bit to 0
// also decrease device count
void remove_device(uint8_t device_index) {
  if (device_index >= MAX_DEVICES) return;
  devices[device_index].destroy(devices[device_index]);
  uart_printf("Removed device %d\r\n", device_index);
  devices_valid[device_index] = 0;
  devices_count--;
}
void gettype(PGM_P str, char *typeresult){
  int i;
  for (i = 0;; i++) {
    char c = pgm_read_byte(str + i);
    if (c == '\0') break;
    typeresult[i] = c;
  }
  typeresult[i] = '\0';
}
void getserialtype(PGM_P str, char *serialtyperesult){
  char serialtype[100];
  int i;
  for (i = 0;; i++) {
    char c = pgm_read_byte(str + i);
    if (c == '\0') break;
    serialtype[i] = c;
  }
  serialtype[i] = '\0';
  if(strcmp(serialtype,"ACTUATOR") == 0){
    strcpy(serialtyperesult,"RELAY");
  } else if(strcmp(serialtype,"TEMP_SENSOR") == 0){
    strcpy(serialtyperesult,"ONEWIRE");
  } else if(strcmp(serialtype,"TSMPPT") == 0){
    strcpy(serialtyperesult,"UART");
  } else if(strcmp(serialtype,"LIGHT_SENSOR") == 0){
    strcpy(serialtyperesult,"I2C");
  } else if(strcmp(serialtype,"HUMIDITY_SENSOR") == 0){
    strcpy(serialtyperesult,"I2C");
  } else {
    serialtyperesult[0] = '\0';
  }
  printf("serial type: %s\n", serialtyperesult);
}

int main(void){

  sei();

  uart_init(19200);

  for (int i = 0; i < MAX_DEVICES; i++)
    devices_valid[i] = 0; // initialize to 0

  // initialize num_types to number of types
  for (num_types = 0; type_num_to_string_map[num_types] != NULL; num_types++);

  unsigned char cmd[TX_BUF_SIZE + 1]; // max amount written by uart_ngetc()

  // FIXME: the fona hardcoding begins
  uint8_t fona_pin_count = 3;
  // FIXME: these pins are incorrect since FONA uses UART3 now but it doesn't
  // really matter since the pins are handled in a different way. re-designs
  // needed

  // PD2 = RX1, PD3 = TX1, PB3 = connect to RST
  uint8_t fona_ports[] = {3, 3, 1};
  uint8_t fona_bits[] = {2, 3, 3};
  create_device(resolve_type_string_to_num(FONA_IDENTIFIER_STRING), fona_ports,
      fona_bits, fona_pin_count);
  
  //--------------------------CODE FOR TUTORIAL AND CREATING SENSORS-----------------------
  //Ports: A = 0, B = 1, C = 2, D = 3, E = 4, F = 5, G = 6, H = 7

  //----------Humidity Sensor Template-------
  //uint8_t hum_pin_count = 2;
  // PD0 = SCL, PD1 = SDA
  //uint8_t hum_ports[] = {3, 3};
  //uint8_t hum_bits[] = {0, 1};
  //create_device(resolve_type_string_to_num(HUMIDITY_SENSOR_IDENTIFIER_STRING),
  //    hum_ports, hum_bits, hum_pin_count);

  //----------UART Sensor Template------------
  //uint8_t tsmppt_pin_count = 2;
  // PD2 = RX1, PD3 = TX1
  //uint8_t tsmppt_ports[] = {3, 3};
  //uint8_t tsmppt_bits[] = {2, 3};
  //create_device(resolve_type_string_to_num(TSMPPT_IDENTIFIER_STRING),
  //    tsmppt_ports, tsmppt_bits, tsmppt_pin_count);

  //----------Light Sensor Template-----------
  // uint8_t light_pin_count = 2;
  // // PD0 = SCL, PD1 = SDA
  // uint8_t light_ports[] = {3, 3};
  // uint8_t light_bits[] = {0, 1};
  // create_device(resolve_type_string_to_num(LIGHT_SENSOR_IDENTIFIER_STRING),
  //     light_ports, light_bits, light_pin_count);

  //--Temperature (1Wire) Sensor Template-----
  //uint8_t onewire_pin_count = 1;
  // PC6 = data
  //uint8_t onewire_ports[] = {2};
  //uint8_t onewire_bits[] = {6};
  //create_device(resolve_type_string_to_num(TEMP_SENSOR_IDENTIFIER_STRING),
  //    onewire_ports, onewire_bits, onewire_pin_count);

  //----------Relay Sensor Template-----------
  // uint8_t actuator_pin_count = 1;
  // // PE4 = switch for relay1 = PIN 2
  // uint8_t actuator_ports[] = {4};
  // uint8_t actuator_bits[] = {4};
  // create_device(resolve_type_string_to_num(ACTUATOR_IDENTIFIER_STRING),
  //     actuator_ports, actuator_bits, actuator_pin_count);
  // // clear the actuator on init
  // // note 1 is off, 0 is on
  // devices[devices_count - 1].write(devices[devices_count - 1], "1", 255);
     
  char fona_read[256];
  char fona_write[256];
  char data[256];
  char selfid[100];
  int no_receive_count = 0;
  for (;;) {
    //memset(fona_read, '\0', 256);
    devices[0].read(devices[0], fona_read, 255);
    //(fona_read[0] == '\0') break; // bad data or lost connection
    //for(int i = 0; i < strlen(fona_read); i++) uart_printf("fona READ %d\r\n", fona_read[i]);
    _delay_ms(2000);
    char *fona_parse = fona_read;
    Parser p = parse_cmd(fona_parse);
    	if (no_receive_count++ > 20) {
	       	strcpy(fona_write, "I didn't receive a message for about a minute" + '\0');
		      devices[0].write(devices[0], fona_write, 255);
	        devices[0].init(devices[0]); // reattempt conn
	        no_receive_count = 0;
      	}

    	memset(fona_write, '\0', 256);
    	memset(data,'\0', 256);
      memset(selfid,'\0', 256);
		//uint8_t type_num = devices[p.device_index].type_num;

		uart_printf("CMD: %c", p.cmd); // debug
		uart_printf("before switch %d\r\n", (uint8_t)p.cmd);
		switch(p.cmd) {
		  // This assumes the string has been parsed
		  // case CHAR_CREATE:
		  //   {
		  //     int8_t type = resolve_type_string_to_num((char *) p.ret_str);
		  //     if (type == -1) {
		  //       uart_printf("%s: Invalid Type String\r\n", p.ret_str);
		  //       break;
		  //     }
		  //     create_device(type, p.address_index, p.reg_bit, p.pin_count);
		  //   }
		  //   break;
		  // case CHAR_INIT:
		  //   if (p.device_index >= MAX_DEVICES ||
		  //       devices_valid[p.device_index] == 0) {
		  //     uart_printf("%d: Invalid Device Specified\r\n", p.device_index);
		  //     break;
		  //   }
		  //   devices[p.device_index].init(devices[p.device_index]);
		  //   break;
		  case CHAR_READ:
		    if (p.device_index >= MAX_DEVICES ||
		        devices_valid[p.device_index] == 0) {
		      uart_printf("%d: Invalid Device Specified\r\n", p.device_index);
		      break;
		    }
		    // FIXME: currently reads nothing
		    devices[p.device_index].read(devices[p.device_index], data, 255);
		    uart_printf("read data: %s\r\n", data);
		    //--------------- start construct packet to send to backend-------------------
        memset(fona_write, '\0', 256);
        uint8_t type_num = devices[p.device_index].type_num;
        char serialtypestr[100];
        char typestr[100];
        getserialtype(type_num_to_string_map[type_num],serialtypestr);
        gettype(type_num_to_string_map[type_num],typestr);
        // printf("Serial Type: %s",type_num_to_string_map[type_num]);
        // strcpy(fona_write, type_num_to_string_map[type_num]);

        //uart_printf("type num is %d\r\n", type_num);
//        char mystring[100];
        //strcpy(mystring,type_num_to_string_map[type_num]);
        //printf("type: %s\n",mystring);
        //char serialtype[100];
        //strcpy(serialtype,(char *)type_num_to_string_map[type_num]);
        //getserialtype(serialtype);
        //strcpy(serialtype,mys);
        // printf("serialtype: %s",serialtype);
        //TODO: change hardcoding of type to the actual serial type
        //probably need to add another array for having serial type
        strcpy(fona_write,typestr);
        strcat(fona_write, " ");
        strcat(fona_write,serialtypestr);
        strcat(fona_write, " ");
		    strcat(fona_write, data);
        printf("fona write: %s",fona_write);
        //--------------- start construct packet to send to backend-------------------
		    devices[0].write(devices[0], fona_write, 255);
        no_receive_count = 0;
		    break;
  		  // case CHAR_WRITE:
  		  //   if (p.device_index >= MAX_DEVICES ||
  		  //       devices_valid[p.device_index] == 0) {
  		  //     uart_printf("%d: Invalid Device Specified\r\n", p.device_index);
  		  //     break;
  		  //   }
  		  //   devices[p.device_index].write(devices[p.device_index],
  		  //       (char *) p.ret_str, strlen((char *) p.ret_str) + 1);
  		  //   break;
  		  // case CHAR_DESTROY:
  		  //   if (p.device_index >= MAX_DEVICES ||
  		  //       devices_valid[p.device_index] == 0) {
  		  //     uart_printf("%d: Invalid Device Specified\r\n", p.device_index);
  		  //     break;
  		  //   }
  		  //   devices[p.device_index].destroy(devices[p.device_index]);
  		  //   break;
  		  // case CHAR_KILL:
  		  //   if (p.device_index >= MAX_DEVICES ||
  		  //       devices_valid[p.device_index] == 0) {
  		  //     uart_printf("%d: Invalid Device Specified\r\n", p.device_index);
  		  //     break;
  		  //   }
        //   remove_device(p.devicminicome_index);
  		  //   break;
		  case CHAR_MAP:
		    // uart_printf("There are %d devices installed:\r\n", devices_count);
		    // uart_flushTX();
		    // uint8_t count = devices_count;
		    // for (uint8_t i = 0; count > 0; i++) {
		    //   if (devices_valid[i] == 0) continue;
		    //   uart_printf("\tdevice %d: type=", i);
		    //   uart_puts_P(type_num_to_string_map[devices[i].type_num]);
		    //   uart_printf(" pin_count=%d\r\n",
		    //       devices[i].pin_count);
		    //   uart_flushTX();
		    //   count--;
		    // }
		    // uart_printf("\r\n");
		    // uart_printf("There are %d different types of devices available\r\n",
		    //     num_types);
		    // uart_flushTX();
		    // for (uint8_t i = 0; i < num_types; i++) {
		    //   uart_printf("%d: ", i);
		    //   uart_puts_P(type_num_to_string_map[i]);
		    //   uart_printf("\r\n");
		    //   uart_flushTX();
		    // }
		    // uart_printf("\r\n");
		      uart_printf("in CHAR_MAP");
		      sprintf(fona_write, "%d", devices_count - 1);
          devices[0].write(devices[0], fona_write, 255);
          no_receive_count = 0;
          break;
      case SELF_ID:
          uart_printf("in selfid");
          self_id_read(selfid);
          uart_printf("my ID: %s\n", selfid);
          strcpy(fona_write, selfid + '\0');
          devices[0].write(devices[0], fona_write, 255);
          no_receive_count = 0;
          break;
      default:
          no_receive_count++;
          uart_flushTX();
          uart_printf("Invalid Command\r\n");
          uart_flushTX();
			    break;
		}
  }
  return 0;
}
