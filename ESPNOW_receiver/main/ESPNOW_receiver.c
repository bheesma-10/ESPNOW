/*@file   ESPNOW_receiver.c
  @brief  simple implementation of ESPNOW protocol (receiver end code)
  @author bheesma-10
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "driver/gpio.h"
#include "ESPNOW.h"


/*receiver and sender ESP32 mac addresses(change these as per your device)*/
uint8_t receiver_MAC[] = {0x3c,0x61,0x05,0x30,0x81,0x21};
uint8_t sender_MAC[]   = {0x3c,0x61,0x05,0x30,0xd8,0xf5};


/*led pin definition and pin mask*/
#define GPIO_OUTPUT_LED       GPIO_NUM_2
#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_OUTPUT_LED)

const char* TAG = "ESP32-ESP_NOW";

/*Queue handle*/
xQueueHandle espnow_queue;


/**
  * @brief  initialize gpio pin
  * @param  None
  * @retval None
  */
void gpio_init(void){
    gpio_config_t pin_conf;

    /*configure output led pin*/
    pin_conf.pin_bit_mask=GPIO_OUTPUT_PIN_SEL;
    pin_conf.mode=GPIO_MODE_OUTPUT;
    pin_conf.pull_up_en=false;
    pin_conf.pull_down_en=false;
    pin_conf.intr_type=GPIO_INTR_DISABLE;
    gpio_config(&pin_conf);
}

/**
  * @brief  function to turn led off
  * @param  None
  * @retval None
  */
static void led_off(void){
    ESP_ERROR_CHECK(gpio_set_level(GPIO_OUTPUT_LED, 0));
}

/**
  * @brief  function to turn led on
  * @param  None
  * @retval None
  */
static void led_on(void){
    ESP_ERROR_CHECK(gpio_set_level(GPIO_OUTPUT_LED, 1));
}

/**
  * @brief  task for led operation  after transmitting data
  * @param  task parameters 
  * @retval None
  */
void led_task(void* pvParameters){
   
    espnow_event_t evt;

    uint32_t delay = (uint32_t)pvParameters;
    
    gpio_init();
    led_on();
    for(int i=0;i<60000;i++){}             //some delay
    led_off();

    evt.id = ESPNOW_LED_TASK;

    xQueueReset(espnow_queue);
    if(xQueueSend(espnow_queue,&evt,200)!=pdTRUE){
    	ESP_LOGW(TAG,"send queue fail");
    }

    vTaskDelay(delay/portTICK_PERIOD_MS);
    vTaskDelete(NULL);
}


/**
  * @brief  sending callback of ESPNOW 
  * @param  mac address of sending device, status of the transmission 
  * @retval None
  */
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    espnow_event_t evt;
    espnow_event_send_cb_t send_cb;

    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }

    memcpy(send_cb.mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb.status = status;

    evt.id = ESPNOW_SEND_CB;
    evt.info.send_cb = send_cb;

    if (xQueueSend(espnow_queue, &evt, 200) != pdTRUE) {
        ESP_LOGW(TAG, "Send send queue fail");
    }
}

/**
  * @brief  receiving callback of ESPNOW 
  * @param  mac address of received device, data received and length of received data
  * @retval None
  */
static void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    espnow_event_t evt;
    espnow_event_recv_cb_t recv_cb;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    
    memcpy(recv_cb.mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb.data = data;
    recv_cb.data_len = len;

    evt.id = ESPNOW_RECV_CB;
    evt.info.recv_cb = recv_cb;

    if (xQueueSend(espnow_queue, &evt, 200) != pdTRUE) {
        ESP_LOGW(TAG, "Send receive queue fail");      
    }
}

/**
  * @brief  receiving callback of ESPNOW 
  * @param  mac address of received device, data received and length of received data
  * @retval None
  */
static void espnow_deinit(espnow_send_param_t *send_param)
{
    free(send_param->buffer);
    free(send_param);
    vSemaphoreDelete(espnow_queue);
    esp_now_deinit();
}

/**
  * @brief  ESPNOW main task
  * @param  task parameters
  * @retval None
  * @note   this task waits to receive data and then sends ack message back to the transmitter
  */
void esp_now_task(void* pvParameters){

	espnow_event_t evt;
	espnow_send_param_t send_param ;

    for(;;){

	while(xQueueReceive(espnow_queue,&evt,200)==pdTRUE){

		switch(evt.id){
			case ESPNOW_SEND_CB:   {

                                         ESP_LOGI(TAG,"send task complete");
                                         ESP_LOGI(TAG,"data sent to device:");
                                         for(int mac_size=0;mac_size<ESP_NOW_ETH_ALEN;mac_size++){
                                         	ESP_LOGI(TAG,"%02X",evt.info.send_cb.mac_addr[mac_size]);
                                         }
                                   }
			                       break;
			case ESPNOW_RECV_CB:    {

				                        ESP_LOGI(TAG,"data received");
				                        ESP_LOGI(TAG,"received data length:%d",evt.info.recv_cb.data_len);
				                        for(int datalen=0;datalen<evt.info.recv_cb.data_len;datalen++){
				                       	 ESP_LOGI(TAG,"%02X",evt.info.recv_cb.data[datalen]);
				                        }
				                        ESP_LOGI(TAG,"\r\n");                                        

                                        /****************************************************/
                                        xTaskCreate(led_task,"LED_TASK",2500,(void*)500,5,NULL);
                                        /****************************************************/
                                        

                                    }
                            


                                   break;
            case ESPNOW_LED_TASK : {
                                        char* ack = "ok";
                                        memcpy(send_param.dest_mac,sender_MAC,sizeof(sender_MAC));
                                        send_param.buffer = (uint8_t*)ack;
                                        send_param.len = strlen(ack);
                                     
                                        if(esp_now_send(send_param.dest_mac,send_param.buffer,send_param.len)!=ESP_OK){
                                               ESP_LOGE(TAG, "Send error");
                                               espnow_deinit(&send_param);
                                               vTaskDelete(NULL);
                                            }

                                   }
                                   break;
            default:
                                   break;
		
	    }
	  }  
	}
	vTaskDelete(NULL);


}


/**
  * @brief  Wifi mode init
  * @param  None
  * @retval None
  * @note   WiFi should start before using ESPNOW. also long range is enabled so TX power is high and bandwidth low
  */
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(ESPNOW_WIFI_MODE) );
    ESP_ERROR_CHECK( esp_wifi_start());

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif
}

/**
  * @brief  ESPNOW init
  * @param  None
  * @retval None
  * @note   initialize queue size, network params, peer list update and generating sending data
  */
void espnow_init(void){

    esp_now_peer_num_t num_peers;    
    espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    if (espnow_queue == NULL) {
        ESP_LOGE(TAG, "Create mutex fail");
    }

	
	ESP_ERROR_CHECK(esp_now_init());
	ESP_ERROR_CHECK( esp_now_register_send_cb(espnow_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(espnow_recv_cb) );

    /*set primary key*/
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t*)"PMK1233443433245"));

    /*add sender device address to peer list*/
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    memset(peer,0,sizeof(esp_now_peer_info_t));
    peer->channel = 1;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr,sender_MAC,sizeof(sender_MAC));
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);


    /*get number of peers and peer data from stored list*/
    ESP_ERROR_CHECK(esp_now_get_peer_num(&num_peers));
    ESP_LOGI(TAG,"no of peers in peers list:%d",num_peers.total_num);
    peer =(esp_now_peer_info_t*) malloc(sizeof(esp_now_peer_info_t));
    for(int num_peer=0;num_peer<num_peers.total_num;num_peer++){
        esp_now_get_peer(sender_MAC,peer);
        
        ESP_LOGI(TAG,"channel:%d",peer->channel);
        ESP_LOGI(TAG,"peer address");
        for(int address_size=0;address_size<ESP_NOW_ETH_ALEN;address_size++){
            ESP_LOGI(TAG,"%02X\r",peer->peer_addr[address_size]);
        }
    } 
    free(peer);

    /*create task*/
    xTaskCreate(esp_now_task,"ESP now Task",5000,NULL,2,NULL);
}

/**
  * @brief  main application
  * @param  None
  * @retval None
  */
void app_main(void)
{
	// Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    wifi_init();
    espnow_init();
}


























 