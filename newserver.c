/*---------------------------------------------------------------------------*/
#include "contiki.h"
//for serial input from the pie
#include "dev/serial-line.h"
/* The following libraries add IP/IPv6 support */
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"

/* This is quite handy, allows to print IPv6 related stuff in a readable way */
#include "net/ip/uip-debug.h"

/* The simple UDP library API */
#include "simple-udp.h"

/* Library used to read the metadata in the packets */
#include "net/packetbuf.h"

/* And we are including the example.h with the example configuration */
#include "example.h"

/* Plus sensors to send data */
#if CONTIKI_TARGET_ZOUL
#include "dev/adc-zoul.h"
#include "dev/zoul-sensors.h"
#else /* Default is Z1 mote */
#include "dev/adxl345.h"
#include "dev/battery-sensor.h"
#include "dev/i2cmaster.h"
#include "dev/tmp102.h"
#endif

// timers, LED, input and output, strings!
#include "sys/etimer.h"
#include "dev/leds.h"

#include <stdio.h>
#include <string.h>
#define N 15 //number of seconds interval 

#define THIS_SENSOR_ID 0 //!!!!!!!!CAUTION: CHANGE THIS FOR EACH NODE MANUALLY!!!!!!!!!
#define OUR_SERVER_ID 0

#define NO_CHANGE 0
#define INCREASE 2
#define DECREASE 1

//thresholds - temperature .
//unit: mili degree centigerades : m.C
#define TEMP_LOW_1 15000 //lower threshold for temperature of node 1
#define TEMP_LOW_2 16000 //lower threshold for temperature of node 2
#define TEMP_HIGH_1 20000 //upper threshold for temperature of node 1
#define TEMP_HIGH_2 22000 //upper threshold for temperature of node 2

#define HUM_LOW_1 0 //lower threshold for humidity of node 1
#define HUM_LOW_2 0 //lower threshold for humidity of node 2
#define HUM_HIGH_1 65530 //upper threshold for humidity of node 1
#define HUM_HIGH_2 65530 //upper threshold for humidity of node 2

/*---------------------------------------------------------------------------*/
#define SEND_INTERVAL	(N * CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
/* The structure used in the Simple UDP library to create an UDP connection */
static struct simple_udp_connection mcast_connection;
/*---------------------------------------------------------------------------*/
/* Create a structure and pointer to store the data to be sent as payload */
static struct my_msg_t msg;
static struct my_msg_t *msgPtr = &msg;
/*---------------------------------------------------------------------------*/
/* Keeps account of the number of messages sent */
static uint16_t counter = 0;
/*---------------------------------------------------------------------------*/

  /* Data container used to store the IPv6 addresses */
  uip_ipaddr_t addr;

/*---------------------------------------------------------------------------*/
PROCESS(mcast_example_process, "UDP multicast example process");
AUTOSTART_PROCESSES(&mcast_example_process);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static void
take_readings(int tempInst,int huInst, uint16_t sensorNum)
{
  uint32_t aux;
  counter++;

  msg.id      = 0xAB;
  msg.counter = counter;
  switch(tempInst){
	case NO_CHANGE:
		msg.value1=0;
		break;
	case DECREASE:
		msg.value1=1;
		break;
	case INCREASE:
		msg.value1=2;
		break;
	default:
		msg.value1=3;
	}
  
  switch(huInst){
	case NO_CHANGE:
		msg.value2=0;
		break;
	case DECREASE:
		msg.value2=1;
		break;
	case INCREASE:
		msg.value2=2;
		break;
	default:
		msg.value2=3;
	}
  msg.value3=THIS_SENSOR_ID;
  msg.value4 = sensorNum;

}
/*---------------------------------------------------------------------------*/
/*the following function*/
//checks the thresholds and sends commands to the sensor node. also sends command to the pi to let it know about its decision -, so 1- measurement; 2-connection, 3- printf
void check_thresholds(uint16_t id, uint16_t tempVal, uint16_t humVal)
{
	//threshold parameters

	uint16_t tempLow;
	uint16_t tempHigh;
	uint16_t humLow;
	uint16_t humHigh;

	//decisions

	int tempDecision = NO_CHANGE;
	int humDecision = NO_CHANGE;

	int validId = 0; //just to see if the id is valid or not. we must return if it is not valid

	//set the thresholds

	switch(id)
	{
		case 1: 
			tempLow = TEMP_LOW_1;
			tempHigh = TEMP_HIGH_1;
			humLow = HUM_LOW_1;
			humHigh = HUM_HIGH_1;
		break;
		case 2:
			tempLow = TEMP_LOW_2;
			tempHigh = TEMP_HIGH_2;
			humLow = HUM_LOW_2;
			humHigh = HUM_HIGH_2;
		break;
		default:
			validId = 1;
			
	}
	if(validId==1)
		return;
	
	//check the thresholds
	
	if(tempVal < tempLow)	//low temperature. must increase
		tempDecision = INCREASE;

	if(tempVal > tempHigh)	//high temperature. must decrease
		tempDecision = DECREASE;
	
	if(humVal < humLow)	//low humidity. must increase
		humDecision = INCREASE;

	if(humVal > humHigh)	//high humidity. must decrease
		humDecision = DECREASE;

	//if both decesions are nochange we have nothing else to do 
	
	if( (humDecision == NO_CHANGE) && (tempDecision == NO_CHANGE) )
		return;
	
	/*send the command to the sensor */

	//Create a link-local multicast address to all nodes 
	   uip_create_linklocal_allnodes_mcast(&addr);

	//uip_debug_ipaddr_print(&addr);

		//Take sensor readings and store into the message structure 
	    take_readings((int)tempDecision,(int)humDecision,id);


		//Send the multicast packet to all devices 
	    simple_udp_sendto(&mcast_connection, msgPtr, sizeof(msg), &addr);

	/*tell the raspberry pi*/
	printf("100 %u %u %u\n",id,tempDecision, humDecision);
}
/*---------------------------------------------------------------------------*/
/* This is the receiver callback, we tell the Simple UDP library to call this
 * function each time we receive a packet
 */
static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{

  /* Variable used to store the retrieved radio parameters */
  radio_value_t aux;

  /* Create a pointer to the received data, adjust to the expected structure */
  struct my_msg_t *msgPtr = (struct my_msg_t *) data;

//led blinks when message receives!

	//  printf("\n***\nMessage from: ");

  /* Converts to readable IPv6 address */
  //uip_debug_ipaddr_print(sender_addr);
/*
  printf("\nData received on port %d from port %d with length %d\n",
         receiver_port, sender_port, datalen);*/

/* The following functions are the functions provided by the RF library to
   * retrieve information about the channel number we are on, the RSSI (received
   * strenght signal indication), and LQI (link quality information)
   */


  NETSTACK_RADIO.get_value(RADIO_PARAM_CHANNEL, &aux);
  //printf("CH: %u ", (unsigned int) aux);

  aux = packetbuf_attr(PACKETBUF_ATTR_RSSI);
  //printf("RSSI: %ddBm ", (int8_t)aux);

  aux = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);
  //printf("LQI: %u\n", aux);



	  leds_toggle(LEDS_GREEN);
		//printf("message is from a sensor! LED blinked!\n");
  
		/* Print the received data *//*
		#if CONTIKI_TARGET_ZOUL 
			printf("sensor number: %u, ID: %u, core temperature: %d.%u, humidity: %u, battery: %u, counter: %u\n",
		        msgPtr->value3, msgPtr->id, msgPtr->value1/1000,msgPtr->value1%1000, msgPtr->value2, msgPtr->battery, msgPtr->counter);
		#else
		 printf("sensor number: %u, ID: %u, temperature: %d.%u, humidity: %u, battery: %u, counter: %u\n",
		        msgPtr->value3, msgPtr->id, msgPtr->value1/1000, msgPtr->value1%1000, msgPtr->value2, msgPtr->battery, msgPtr->counter);
		#endif*/
	printf("%d %d %d %d\n",msgPtr->value3,msgPtr->value1,msgPtr->value2,msgPtr->battery);//id, temperature, humidity, battery

	check_thresholds(msgPtr->value3,msgPtr->value1, msgPtr->value2); //id , temperature,humidity


}
/*---------------------------------------------------------------------------*/
static void
print_radio_values(void)
{
  radio_value_t aux;

  //printf("\n* Radio parameters:\n");

  NETSTACK_RADIO.get_value(RADIO_PARAM_CHANNEL, &aux);
  //printf("   Channel %u", aux);

  NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MIN, &aux);
 // printf(" (Min: %u, ", aux);

  NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MAX, &aux);
  //printf("Max: %u)\n", aux);

  NETSTACK_RADIO.get_value(RADIO_PARAM_TXPOWER, &aux);
 // printf("   Tx Power %3d dBm", aux);

  NETSTACK_RADIO.get_value(RADIO_CONST_TXPOWER_MIN, &aux);
//  printf(" (Min: %3d dBm, ", aux);

  NETSTACK_RADIO.get_value(RADIO_CONST_TXPOWER_MAX, &aux);
 // printf("Max: %3d dBm)\n", aux);

  /* This value is set in contiki-conf.h and can be changed */
 // printf("   PAN ID: 0x%02X\n", IEEE802154_CONF_PANID);
}
/*---------------------------------------------------------------------------*/
static void
set_radio_default_parameters(void)
{
  NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, EXAMPLE_TX_POWER);
  // NETSTACK_RADIO.set_value(RADIO_PARAM_PAN_ID, EXAMPLE_PANID);
  NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, EXAMPLE_CHANNEL);
}
PROCESS_THREAD(mcast_example_process, ev, data)
{
  PROCESS_BEGIN();

  /* Alternatively if you want to change the channel or transmission power, this
   * are the functions to use.  You can also change these values in runtime.
   * To check what are the regular platform values, comment out the function
   * below, so the print_radio_values() function shows the default.
   */
  set_radio_default_parameters();

  /* This blocks prints out the radio constants (minimum and maximum channel,
   * transmission power and current PAN ID (more or less like a subnet)
   */
	//printf("radio constants (minimum and maximum channel, transmission power and current PAN ID (more or less like a subnet)) are\n");
  //print_radio_values();

  /* Create the UDP connection.  This function registers a UDP connection and
   * attaches a callback function to it. The callback function will be
   * called for incoming packets. The local UDP port can be set to 0 to indicate
   * that an ephemeral UDP port should be allocated. The remote IP address can
   * be NULL, to indicate that packets from any IP address should be accepted.
   */
  simple_udp_register(&mcast_connection, UDP_CLIENT_PORT, NULL,
                      UDP_CLIENT_PORT, receiver);

  /* Activate the sensors *//*
#if CONTIKI_TARGET_ZOUL
  adc_zoul.configure(SENSORS_HW_INIT, ZOUL_SENSORS_ADC_ALL);
//humidity sensor will be added later //todo (maybe sth must be added)
#else 
//humidity sensor will be added later
  SENSORS_ACTIVATE(adxl345);
  SENSORS_ACTIVATE(tmp102);
  SENSORS_ACTIVATE(battery_sensor);
#endif*/

int comSenNum, tempCom, humCom;
 while(1) {
    PROCESS_YIELD();

 /*this is for manually commanding a sensor from raspberry pi without chacking the threshold, etc*/
	if(ev == serial_line_event_message) {
       	//printf("received line: %s\n", (char *)data);
	//command format: "sensorNumber temperatureCommand humidityCommand\n"
	//printf("new command ready to be sent");
	sscanf(data, "%d%d%d",&comSenNum, &tempCom, &humCom);
/*
	 printf("\n***\nSending packet to multicast adddress ");
	printf("sensor number %d, Temperature command: %d, Humidity command: %d\n", comSenNum, tempCom, humCom);
*/
	//Create a link-local multicast address to all nodes 
	   uip_create_linklocal_allnodes_mcast(&addr);

	//uip_debug_ipaddr_print(&addr);
	   // printf("\n");
		uint16_t comSenAddr = (uint16_t)comSenNum;
		//Take sensor readings and store into the message structure 
	    take_readings(tempCom,humCom,comSenAddr);


		//Send the multicast packet to all devices 
	    simple_udp_sendto(&mcast_connection, msgPtr, sizeof(msg), &addr);
		printf("100 %u %u %u\n",comSenAddr,tempCom, humCom);
	
	}
  }

 

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/