#include "mbed.h"
#include "wifi.h"
#include <sstream>
//#include "mbed-trace/mbed_trace.h"
/*------------------------------------------------------------------------------
Hyperterminal settings: 115200 bauds, 8-bit data, no parity

This example 
  - connects to a wifi network (SSID & PWD to set in mbed_app.json)
  - Connects to a TCP server (set the address in RemoteIP)
  - Sends "Hello" to the server when data is received

This example uses SPI3 ( PE_0 PC_10 PC_12 PC_11), wifi_wakeup pin (PB_13), 
wifi_dataready pin (PE_1), wifi reset pin (PE_8)
------------------------------------------------------------------------------*/

/* Private defines -----------------------------------------------------------*/
#include <events/mbed_events.h>
#include "ble/BLE.h"
#include "gatt_server_process.h"
#define WIFI_WRITE_TIMEOUT 10000
#define WIFI_READ_TIMEOUT  10000
#define CONNECTION_TRIAL_MAX          10

/* Private typedef------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
BufferedSerial pc(USBTX, USBRX);
uint8_t RemoteIP[] = {MBED_CONF_APP_SERVER_IP_1,MBED_CONF_APP_SERVER_IP_2,MBED_CONF_APP_SERVER_IP_3, MBED_CONF_APP_SERVER_IP_4};
uint8_t RxData [500];
char* modulename;
std::uint8_t TxData[] = "Error Sending\n";
uint16_t RxLen;
uint8_t  MAC_Addr[6]; 
uint8_t  IP_Addr[4]; 

static EventQueue event_queue(/* event count */ 10 * EVENTS_EVENT_SIZE);

class GattServerDemo : ble::GattServer::EventHandler {

    const static uint16_t EXAMPLE_SERVICE_UUID         = 0xA000;
    const static uint16_t WRITABLE_CHARACTERISTIC_UUID = 0xA001;

public:
    GattServerDemo()
    {
        const UUID uuid = WRITABLE_CHARACTERISTIC_UUID;
        _writable_characteristic = new ReadWriteGattCharacteristic<uint8_t> (uuid, &_characteristic_value);

        if (!_writable_characteristic) {
            printf("Allocation of ReadWriteGattCharacteristic failed\r\n");
        }
    }

    ~GattServerDemo()
    {
    }

    void start(BLE &ble, events::EventQueue &event_queue)
    {
        const UUID uuid = EXAMPLE_SERVICE_UUID;
        GattCharacteristic* charTable[] = { _writable_characteristic };
        GattService example_service(uuid, charTable, 1);

        ble.gattServer().addService(example_service);

        ble.gattServer().setEventHandler(this);

        printf("Example service added with UUID 0xA000\r\n");
        printf("Connect and write to characteristic 0xA001\r\n");
    }

private:
    /**
     * This callback allows the LEDService to receive updates to the ledState Characteristic.
     *
     * @param[in] params Information about the characterisitc being updated.
     */
    virtual void onDataWritten(const GattWriteCallbackParams &params)
    {
        if ((params.handle == _writable_characteristic->getValueHandle()) && (params.len == 1)) {
            printf("Message received. Sending '%x' over WIFI.\r\n", *(params.data));
            std::stringstream TxData;
            TxData << *(params.data);
        }
    }

private:
    ReadWriteGattCharacteristic<uint8_t> *_writable_characteristic = nullptr;
    uint8_t _characteristic_value = 0;
};

int main()
{
    int32_t Socket = -1;
    uint16_t Datalen;
    uint16_t Trials = CONNECTION_TRIAL_MAX;

    pc.set_baud(115200);

mbed_mpu_init();

    BLE &ble = BLE::Instance();

    printf("\r\nRunning Bluetooth Code\r\n");
    GattServerDemo demo;
    /* this process will handle basic setup and advertising for us */
    GattServerProcess ble_process(event_queue, ble);
    /* once it's done it will let us continue with our demo*/
    ble_process.on_init(callback(&demo, &GattServerDemo::start));
    printf("\r\nRunning WIFI Code\r\n");
    printf("\n");
    printf("************************************************************\n");
    printf("***   STM32 IoT Discovery kit for STM32L475 MCU          ***\n");
    printf("***      WIFI Module in TCP Client mode demonstration    ***\n\n");
    printf("*** TCP Client Instructions :\n");
    printf("*** 1- Make sure your Phone is connected to the same network that\n");
    printf("***    you configured using the Configuration Access Point.\n");
    printf("*** 2- Create a server by using the android application TCP Server\n");
    printf("***    with port(8002).\n");
    printf("*** 3- Get the Network Name or IP Address of your phone from the step 2.\n\n"); 
    printf("************************************************************\n");

    /*Initialize  WIFI module */
    if(WIFI_Init() ==  WIFI_STATUS_OK) {
        printf("> WIFI Module Initialized.\n");  
        if(WIFI_GetMAC_Address(MAC_Addr) == WIFI_STATUS_OK) {
            printf("> es-wifi module MAC Address : %X:%X:%X:%X:%X:%X\n",     
                   MAC_Addr[0],
                   MAC_Addr[1],
                   MAC_Addr[2],
                   MAC_Addr[3],
                   MAC_Addr[4],
                   MAC_Addr[5]);   
        } else {
            printf("> ERROR : CANNOT get MAC address\n");
        }
    
        if( WIFI_Connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, WIFI_ECN_WPA2_PSK) == WIFI_STATUS_OK) {
            printf("> es-wifi module connected \n");
            if(WIFI_GetIP_Address(IP_Addr) == WIFI_STATUS_OK) {
                printf("> es-wifi module got IP Address : %d.%d.%d.%d\n",     
                       IP_Addr[0],
                       IP_Addr[1],
                       IP_Addr[2],
                       IP_Addr[3]); 
        
                printf("> Trying to connect to Server: %d.%d.%d.%d:8002 ...\n",     
                       RemoteIP[0],
                       RemoteIP[1],
                       RemoteIP[2],
                       RemoteIP[3]);
        
                while (Trials--){ 
                    if( WIFI_OpenClientConnection(0, WIFI_TCP_PROTOCOL, "TCP_CLIENT", RemoteIP, 8002, 0) == WIFI_STATUS_OK){
                        printf("> TCP Connection opened successfully.\n"); 
                        Socket = 0;
                    }
                }
                if(!Trials) {
                    printf("> ERROR : Cannot open Connection\n");
                }
            } else {
                printf("> ERROR : es-wifi module CANNOT get IP address\n");
            }
        } else {
            printf("> ERROR : es-wifi module NOT connected\n");
        }
    } else {
        printf("> ERROR : WIFI Module cannot be initialized.\n"); 
    }
  
    while(1){
        ble_process.start();
        
        if(Socket != -1) {
            if(WIFI_ReceiveData(Socket, RxData, sizeof(RxData), &Datalen, WIFI_READ_TIMEOUT) == WIFI_STATUS_OK){
                if(Datalen > 0) {
                    if(WIFI_SendData(Socket, TxData, sizeof(TxData), &Datalen, WIFI_WRITE_TIMEOUT) != WIFI_STATUS_OK) {
                        printf("> ERROR : Failed to send Data Over WIFI.\n");
                    }
                } else {
                printf("> ERROR : Failed to Receive Data from Bluetooth.\n");
                }
            }
        }
    }
    return 0;   
}
