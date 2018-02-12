// ----------------------------------------------------------------------------
// Copyright 2016-2018 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------

#include "simplem2mclient.h"
#ifdef TARGET_LIKE_MBED
#include "mbed.h"
#endif
#include "application_init.h"
#include "common_button_and_led.h"
#include "blinky.h"

// event based LED blinker, controlled via pattern_resource
static Blinky blinky;

static void main_application(void);

int main(void)
{
    mcc_platform_run_program(main_application);
}

// Pointers to the resources that will be created in main_application().
static M2MResource* product_id;
static M2MResource* product_current_count;
static M2MResource* product_empty;

// Pointer to mbedClient, used for calling close function.
static SimpleM2MClient *client;


void pattern_updated(const char *)
{
    printf("PUT received, new value: %s\n", pattern_res->get_value_string().c_str());
}

void blink_callback(void *)
{
    String pattern_string = pattern_res->get_value_string();
    const char *pattern = pattern_string.c_str();
    printf("LED pattern = %s\n", pattern);

    // The pattern is something like 500:200:500, so parse that.
    // LED blinking is done while parsing.
    const bool restart_pattern = false;
    if (blinky.start((char*)pattern_res->value(), pattern_res->value_length(), restart_pattern) == false) {
        printf("out of memory error\n");
    }
}

void button_notification_status_callback(const M2MBase& object, const NoticationDeliveryStatus status)
{
    switch(status) {
        case NOTIFICATION_STATUS_BUILD_ERROR:
            printf("Notification callback: (%s) error when building CoAP message\n", object.uri_path());
            break;
        case NOTIFICATION_STATUS_RESEND_QUEUE_FULL:
            printf("Notification callback: (%s) CoAP resend queue full\n", object.uri_path());
            break;
        case NOTIFICATION_STATUS_SENT:
            printf("Notification callback: (%s) Notification sent to server\n", object.uri_path());
            break;
        case NOTIFICATION_STATUS_DELIVERED:
            printf("Notification callback: (%s) Notification delivered\n", object.uri_path());
            break;
        case NOTIFICATION_STATUS_SEND_FAILED:
            printf("Notification callback: (%s) Notification sending failed\n", object.uri_path());
            break;
        case NOTIFICATION_STATUS_SUBSCRIBED:
            printf("Notification callback: (%s) subscribed\n", object.uri_path());
            break;
        case NOTIFICATION_STATUS_UNSUBSCRIBED:
            printf("Notification callback: (%s) subscription removed\n", object.uri_path());
            break;
        default:
            break;
    }
}

// This function is called when a POST request is received for resource 5000/0/1.
void unregister(void *)
{
    printf("Unregister resource executed\n");
    client->close();
}

// This function is called when a POST request is received for resource 5000/0/2.
void factory_reset(void *)
{
    printf("Factory reset resource executed\n");
    client->close();
    kcm_status_e kcm_status = kcm_factory_reset();
    if (kcm_status != KCM_STATUS_SUCCESS) {
        printf("Failed to do factory reset - %d\n", kcm_status);
    } else {
        printf("Factory reset completed. Now restart the device\n");
    }
}

void main_application(void)
{
    const int max_cnt = ((rand() % 3) + 1) * 10; //10, 20, 30 possible in stock on this row
    const float sale_prob = rand();

    // IOTMORF-1712: DAPLINK starts the previous application during flashing a new binary
    // This is workaround to prevent possible deletion of credentials or storage corruption
    // while replacing the application binary.
#ifdef TARGET_LIKE_MBED
    wait(2);
#endif
    mcc_platform_sw_build_info();
    // run_application() will first initialize the program and then call main_application()

    if (mcc_platform_storage_init() != 0) {
        printf("Failed to initialize storage\n" );
        return;
    }

    if(mcc_platform_init() != 0) {
        printf("ERROR - platform_init() failed!\n");
        return;
    }

    // Print some statistics of the object sizes and their heap memory consumption.
    // NOTE: This *must* be done before creating MbedCloudClient, as the statistic calculation
    // creates and deletes M2MSecurity and M2MDevice singleton objects, which are also used by
    // the MbedCloudClient.
#ifdef MBED_HEAP_STATS_ENABLED
    print_m2mobject_stats();
#endif

    // SimpleClient is used for registering and unregistering resources to a server.
    SimpleM2MClient mbedClient;

    // application_init() runs the following initializations:
    //  1. trace initialization
    //  2. platform initialization
    //  3. print memory statistics if MBED_HEAP_STATS_ENABLED is defined
    //  4. FCC initialization.
    if (!application_init()) {
        printf("Initialization failed, exiting application!\n");
        return;
    }

    // Save pointer to mbedClient so that other functions can access it.
    client = &mbedClient;

#ifdef MBED_HEAP_STATS_ENABLED
    printf("Client initialized\r\n");
    print_heap_stats();
#endif
#ifdef MBED_STACK_STATS_ENABLED
    print_stack_statistics();
#endif

//    static M2MResource* product_id;
//    static M2MResource* product_current_count;
//    static M2MResource* product_empty;

    product_id = mbedClient.add_cloud_resource(10341, 0, 26341, "product_id", M2MResourceInstance::INTEGER,
                              M2MBase::GET_ALLOWED, 0, false, NULL, NULL)

    product_current_count = mbedClient.add_cloud_resource(10341, 0, 26342, "product_id", M2MResourceInstance::INTEGER,
                              M2MBase::GET_ALLOWED, 0, true, NULL, NULL)

    product_empty = mbedClient.add_cloud_resource(10341, 0, 26343, "product_id", M2MResourceInstance::INTEGER,
                              M2MBase::GET_ALLOWED, 0, true, NULL, NULL)

    // Create resource for unregistering the device. Path of this resource will be: 5000/0/1.
    mbedClient.add_cloud_resource(5000, 0, 1, "unregister", M2MResourceInstance::STRING,
                 M2MBase::POST_ALLOWED, NULL, false, (void*)unregister, NULL);

    // Create resource for running factory reset for the device. Path of this resource will be: 5000/0/2.
    mbedClient.add_cloud_resource(5000, 0, 2, "factory_reset", M2MResourceInstance::STRING,
                 M2MBase::POST_ALLOWED, NULL, false, (void*)factory_reset, NULL);

    mbedClient.register_and_connect();

    // Set a product ID
    product_id->set_value(rand() % 5); // 5 possible products
    product_current_count->set_value(max_cnt);

    // Check if client is registering or registered, if true sleep and repeat.
    while (mbedClient.is_register_called()) {
        int cnt_down = (rand() % 9900) + 100; // Random wait between 100 ms and 10s
        mcc_platform_do_wait(cnt_down);

        if (product_empty->get_value() == 1) {
            mcc_platform_do_wait(10000);
            product_empty->set_value(0);
        }
        product_current_count->set_value(product_current_count->get_value() - 1);
        //Sold
        if(rand() < sale_prob){}
        else{
            mcc_platform_do_wait(1000);
            product_current_count->set_value(product_current_count->get_value() + 1);
        }

        if(product_current_count->get_value() == 0){
            product_empty->set_value(1);
        }
    }

    // Client unregistered, exit program.
}
