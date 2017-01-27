/* Copyright Statement:
 *
 * (C) 2005-2016  MediaTek Inc. All rights reserved.
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. ("MediaTek") and/or its licensors.
 * Without the prior written permission of MediaTek and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 * You may only use, reproduce, modify, or distribute (as applicable) MediaTek Software
 * if you have agreed to and been bound by the applicable license agreement with
 * MediaTek ("License Agreement") and been granted explicit permission to do so within
 * the License Agreement ("Permitted User").  If you are not a Permitted User,
 * please cease any access or use of MediaTek Software immediately.
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT MEDIATEK SOFTWARE RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES
 * ARE PROVIDED TO RECEIVER ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 */

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "wifi_api.h"

#include "MQTTClient.h"
#include "mqtt.h"
#include "syslog.h"

#include "hal.h"
//#include "system_mt7687.h"
#include "top.h"

#define MQTT_SERVER		"iot.eclipse.org"
#define MQTT_PORT		"1883"
#define MQTT_TOPIC_PUB		"rcb_pub"
#define MQTT_TOPIC_SUB		"dmt7687"
#define MQTT_CLIENT_ID	"rcbsamsfmlient"
#define MQTT_MSG_VER	"0.50"

char buf_rxm[100];
static int arrivedcount = 0;

log_create_module(mqtt, PRINT_LEVEL_INFO);


/**
* @brief          MQTT message RX handle
* @param[in]      MQTT received message data
* @return         None
*/
static void messageArrived(MessageData *md)
{
    MQTTMessage *message = md->message;

    LOG_I(mqtt, "Message arrived: qos %d, retained %d, dup %d, packetid %d\n", 
        message->qos, message->retained, message->dup, message->id);
    LOG_I(mqtt, "Payload %d.%s\n", (size_t)(message->payloadlen), (char *)(message->payload));
	memcpy(buf_rxm, (char *)(message->payload), (size_t)(message->payloadlen));
	buf_rxm[(size_t)(message->payloadlen)] = 0x00;
    ++arrivedcount;
}

/**
* @brief          MQTT client example entry function
* @return         None
*/
int mqtt_client_example(void)
{
    int rc = 0;
    unsigned char msg_buf[100];     //Buffer for outgoing messages, such as unsubscribe.
    unsigned char msg_readbuf[100]; //Buffer for incoming messages, such as unsubscribe ACK.
    char buf[100];                  //Buffer for application payload.

    Network n;  //TCP network
    Client c;   //MQTT client
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    char *topic_p = MQTT_TOPIC_PUB;
	char *topic_s = MQTT_TOPIC_SUB;
    MQTTMessage message;

    //Initialize MQTT network structure
    NewNetwork(&n);

    //Connect to remote server
    LOG_I(mqtt, "Connect to %s:%s\n", MQTT_SERVER, MQTT_PORT);
    rc = ConnectNetwork(&n, MQTT_SERVER, MQTT_PORT);

    if (rc != 0) {
        LOG_I(mqtt, "TCP connect failed,status -%4X\n", -rc);
        return rc;
    }

    //Initialize MQTT client structure
    MQTTClient(&c, &n, 12000, msg_buf, 100, msg_readbuf, 100);

    //The packet header of MQTT connection request
    data.willFlag = 0;
    data.MQTTVersion = 3;
    data.clientID.cstring = MQTT_CLIENT_ID;
    data.username.cstring = NULL;
    data.password.cstring = NULL;
    data.keepAliveInterval = 10;
    data.cleansession = 1;

    //Send MQTT connection request to the remote MQTT server
    rc = MQTTConnect(&c, &data);

    if (rc != 0) {
        LOG_I(mqtt, "MQTT connect failed,status -%4X\n", -rc);
        return rc;
    }
    
    LOG_I(mqtt, "Subscribing to %s\n", topic_s);
    rc = MQTTSubscribe(&c, topic_s, QOS1, messageArrived);
    LOG_I(mqtt, "Client Subscribed %d\n", rc);

    // QoS 0
    sprintf(buf, "Hello World! QoS 0 message from app version %s\n", MQTT_MSG_VER);
    message.qos = QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void *)buf;
    message.payloadlen = strlen(buf) + 1;
    rc = MQTTPublish(&c, topic_p, &message);
	while(1)
	{
		while (arrivedcount < 1) {
			MQTTYield(&c, 1000);
			
		}
		arrivedcount = 0;
		LOG_I(mqtt, "Received MSG from Health Monitor = %s\n",buf_rxm);
		char* temp;
		uint32_t hr = 100,hrmax = 170,hrmin = 60,bt = 35,btmax = 40,btmin = 30,ecgreq = 0,flagack = 0, hf = 0,bf = 0,ff = 0,sf = 0;
		hal_gpio_status_t retpin;
		temp = strtok(buf_rxm, ",");
        hr = atoi(temp);
		temp = strtok(NULL, ",");
        hrmax = atoi(temp);
        temp = strtok(NULL, ",");
        hrmin = atoi(temp);
		temp = strtok(NULL, ",");
        bt = atoi(temp);
        temp = strtok(NULL, ",");
        btmax = atoi(temp);
        temp = strtok(NULL, ",");
        btmin = atoi(temp);
		temp = strtok(NULL, ",");
        hf = atoi(temp);
		temp = strtok(NULL, ",");
        bf = atoi(temp);
		temp = strtok(NULL, ",");
        ff = atoi(temp);
		temp = strtok(NULL, ",");
        sf = atoi(temp);
        LOG_I(mqtt, "%d  %d  %d  %d  %d  %d  %d  %d  %d  %d\n",hr,hrmax,hrmin,bt,btmax,btmin,hf,bf,ff,sf);
		
		if(hr == 31) 
		{
			retpin = hal_gpio_set_output(HAL_GPIO_4, HAL_GPIO_DATA_HIGH);
			retpin = hal_gpio_set_output(HAL_GPIO_5, HAL_GPIO_DATA_HIGH);
			retpin = hal_gpio_set_output(HAL_GPIO_7, HAL_GPIO_DATA_HIGH);
			retpin = hal_gpio_set_output(HAL_GPIO_24, HAL_GPIO_DATA_HIGH);
			retpin = hal_gpio_set_output(HAL_GPIO_25, HAL_GPIO_DATA_HIGH);
			retpin = hal_gpio_set_output(HAL_GPIO_26, HAL_GPIO_DATA_HIGH);
		}
		else 
		{
			if(hf == 0) 
			{
				retpin = hal_gpio_set_output(HAL_GPIO_4, HAL_GPIO_DATA_HIGH);
				retpin = hal_gpio_set_output(HAL_GPIO_5, HAL_GPIO_DATA_LOW);
				retpin = hal_gpio_set_output(HAL_GPIO_24, HAL_GPIO_DATA_LOW);
			}
			else if(hf == 1) 
			{
				retpin = hal_gpio_set_output(HAL_GPIO_4, HAL_GPIO_DATA_LOW);
				retpin = hal_gpio_set_output(HAL_GPIO_5, HAL_GPIO_DATA_LOW);
				retpin = hal_gpio_set_output(HAL_GPIO_24, HAL_GPIO_DATA_HIGH);
			}
			else if(hf == 2) 
			{
				retpin = hal_gpio_set_output(HAL_GPIO_4, HAL_GPIO_DATA_LOW);
				retpin = hal_gpio_set_output(HAL_GPIO_5, HAL_GPIO_DATA_HIGH);
				retpin = hal_gpio_set_output(HAL_GPIO_24, HAL_GPIO_DATA_LOW);
			}
			
			if(bf == 0) 
			{
				retpin = hal_gpio_set_output(HAL_GPIO_7, HAL_GPIO_DATA_LOW);
				retpin = hal_gpio_set_output(HAL_GPIO_25, HAL_GPIO_DATA_HIGH);
				retpin = hal_gpio_set_output(HAL_GPIO_26, HAL_GPIO_DATA_LOW);
			}
			else if(bf == 1) 
			{
				retpin = hal_gpio_set_output(HAL_GPIO_7, HAL_GPIO_DATA_LOW);
				retpin = hal_gpio_set_output(HAL_GPIO_25, HAL_GPIO_DATA_LOW);
				retpin = hal_gpio_set_output(HAL_GPIO_26, HAL_GPIO_DATA_HIGH);
			}
			else if(bf == 2) 
			{
				retpin = hal_gpio_set_output(HAL_GPIO_7, HAL_GPIO_DATA_HIGH);
				retpin = hal_gpio_set_output(HAL_GPIO_25, HAL_GPIO_DATA_LOW);
				retpin = hal_gpio_set_output(HAL_GPIO_26, HAL_GPIO_DATA_LOW);
			}
			vTaskDelay(100 / portTICK_RATE_MS);
		}
		
	}
	
    return 0;
    //LOG_I(mqtt, "example project test success.");

}



