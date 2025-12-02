# EaSyP:inet

Package for configuring internet connectivity on ESP32 microcontrollers.

## Overview

EaSyP:inet provides a small wrapper to configure Wi‑Fi network connectivity via
Kconfig and integrates with the ESP event loop to notify the application about
connection state changes.

## Configuration (Kconfig)

Configure the package with menuconfig. Key symbols and defaults:

- `ENABLE_EASYP_INET` (`bool`, default: `y`)  
  Enable the package.

- `EASYP_INET_WIFI_SSID` (`string`, default: `"myssid"`)  
  Wi‑Fi network SSID to connect to.

- `EASYP_INET_WIFI_PASSWORD` (`string`, default: `"mypassword"`)  
  Wi‑Fi password (WPA/WPA2/etc).

- `EASYP_INET_WIFI_MAX_RETRY` (`int`, default: `5`)  
  Maximum connection retry attempts.

- `EASYP_INET_WIFI_AUTH_MODE_THRESHOLD` (choice, default: `EASYP_INET_WIFI_AUTH_WPA2_PSK`)  
  Minimum accepted auth mode. Options:

  - `EASYP_INET_WIFI_AUTH_OPEN` (OPEN)
  - `EASYP_INET_WIFI_AUTH_WEP` (WEP)
  - `EASYP_INET_WIFI_AUTH_WPA_PSK` (WPA PSK)
  - `EASYP_INET_WIFI_AUTH_WPA2_PSK` (WPA2 PSK)
  - `EASYP_INET_WIFI_AUTH_WPA_WPA2_PSK` (WPA/WPA2 PSK)
  - `EASYP_INET_WIFI_AUTH_WPA3_PSK` (WPA3 PSK)
  - `EASYP_INET_WIFI_AUTH_WPA2_WPA3_PSK` (WPA2/WPA3 PSK)
  - `EASYP_INET_WIFI_AUTH_WAPI_PSK` (WAPI PSK)

- `EASYP_INET_NETIF_IP_MODE` (choice, default: `EASYP_INET_NETIF_IP_MODE_DHCP`)  
  Network interface IP mode: DHCP or STATIC.
  If STATIC is chosen, configure:
  - `EASYP_INET_NETIF_IP4_ADDR` (`string`, default: `"192.168.1.100"`)
  - `EASYP_INET_NETIF_IP4_NETMASK` (`string`, default: `"255.255.255.0"`)
  - `EASYP_INET_NETIF_IP4_GATEWAY` (`string`, default: `"192.168.1.1"`)

## API (include "easyp_inet.h")

Provided event base:

- `EASYP_INET_EVENT` (`esp_event_base_t`)

Events (`easyp_inet_event_t`):

- `EASYP_INET_CONNECTION_SUCCEEDED`
  - Posted when connection to Wi‑Fi network is successfully established.
  - Data: `easyp_inet_connection_succeeded_event_t`.
    - Contains assigned IP address information.
- `EASYP_INET_CONNECTION_FAILED`
  - Posted when connection to Wi‑Fi network fails after maximum retries.
- `EASYP_INET_DISCONNECTED`
  - Posted when disconnected from Wi‑Fi network.
  - Data: `easyp_inet_disconnected_event_t`.
    - Contains reason for disconnection.

Functions:

- `esp_err_t easyp_inet_connect(void)`
  - Initiates connection to the configured Wi‑Fi network.
  - Returns standard `esp_err_t` codes.
  - Starts monitoring connection state and posts events.
- `esp_err_t easyp_inet_disconnect(void)`
  - Initiates disconnection from the Wi‑Fi network.
  - Returns standard `esp_err_t` codes.

Return values follow standard `esp_err_t` codes.

## Usage example

```c
#include "easyp_inet.h"
#include <esp_event.h>

static void inet_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id) {
    case EASYP_INET_CONNECTION_SUCCEEDED:
        // connected: obtain IP, start network services
        break;
    case EASYP_INET_CONNECTION_FAILED:
        // connection failed: check config or retry policy
        break;
    case EASYP_INET_DISCONNECTED:
        // disconnected: cleanup or attempt reconnect
        break;
    }
}

void app_main(void)
{
    esp_event_loop_create_default();

    esp_event_handler_instance_t inst;
    esp_event_handler_instance_register(EASYP_INET_EVENT, ESP_EVENT_ANY_ID,
                                        inet_event_handler, NULL, &inst);

    esp_err_t err = easyp_inet_connect();
    // handle err
}
```
