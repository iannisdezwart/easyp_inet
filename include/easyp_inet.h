#pragma once

#include <esp_err.h>
#include <esp_event.h>
#include <esp_netif.h>

ESP_EVENT_DECLARE_BASE(EASYP_INET_EVENT);

typedef enum
{
	// Connection was successful and IP address was obtained.
	EASYP_INET_CONNECTION_SUCCEEDED,
	// Connection attempt failed after maximum retries.
	EASYP_INET_CONNECTION_FAILED,
	// Disconnected from the network.
	EASYP_INET_DISCONNECTED,
} easyp_inet_event_t;

typedef struct
{
	esp_netif_ip_info_t ip_info;
} easyp_inet_connection_succeeded_event_t;

typedef enum
{
	// Disconnection was requested by the user.
	EASYP_DISCONNECT_REASON_REQUESTED = 0,
	// Disconnection was unsolicited (e.g., network issues).
	EASYP_DISCONNECT_REASON_UNSOLICITED = 1,
} easyp_disconnect_reason_t;

typedef struct
{
	easyp_disconnect_reason_t reason;
} easyp_inet_disconnected_event_t;

esp_err_t
easyp_inet_connect(void);
esp_err_t
easyp_inet_disconnect(void);