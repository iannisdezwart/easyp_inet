#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <stdio.h>

#include "sdkconfig.h"
#include "easyp_inet.h"

ESP_EVENT_DEFINE_BASE(EASYP_INET_EVENT);

static const char *LOG_TAG         = "easyp_inet";
static esp_netif_t *s_wifi_netif   = NULL;
static int s_conn_retry_count      = 0;
static bool s_connected            = false;
static bool s_disconnect_requested = false;

static void
wifi_disconnect_handler(void *arg, esp_event_base_t event_base,
	int32_t event_id, void *event_data)
{
	s_conn_retry_count++;
	if (s_conn_retry_count > CONFIG_EASYP_INET_WIFI_MAX_RETRY)
	{
		ESP_LOGV(LOG_TAG,
			"Reached maximum retry limit, stopping connection "
			"attempts.");
		easyp_inet_disconnect();
		return;
	}

	wifi_event_sta_disconnected_t *disconn = event_data;
	if (disconn->reason == WIFI_REASON_ROAMING)
	{
		ESP_LOGV(LOG_TAG, "Station switching to a new AP...");
		return;
	}

	ESP_LOGV(LOG_TAG,
		"Disconnected from Wi-Fi (reason: %d), attempting to "
		"reconnect...",
		disconn->reason);
	esp_err_t err = esp_wifi_connect();
	if (err == ESP_ERR_WIFI_NOT_STARTED)
	{
		return;
	}
	ESP_ERROR_CHECK(err);
}

static void
wifi_connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
	void *event_data)
{
	ESP_LOGI(LOG_TAG, "Successfully connected to Wi-Fi.");
	esp_netif_create_ip6_linklocal(s_wifi_netif);
}

static void
ip_got_ip_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
	void *event_data)
{
	s_conn_retry_count = 0;

	ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;

	ESP_LOGV(LOG_TAG, "Got IPv4 address: " IPSTR,
		IP2STR(&event->ip_info.ip));
	ESP_LOGV(LOG_TAG, "Subnet mask: " IPSTR,
		IP2STR(&event->ip_info.netmask));
	ESP_LOGV(
		LOG_TAG, "Gateway address: " IPSTR, IP2STR(&event->ip_info.gw));

	s_connected = true;

	easyp_inet_connection_succeeded_event_t conn_event = {
		.ip_info = event->ip_info,
	};
	ESP_ERROR_CHECK(esp_event_post(EASYP_INET_EVENT,
		EASYP_INET_CONNECTION_SUCCEEDED, &conn_event,
		sizeof(conn_event), portMAX_DELAY));
}

static void
ip_got_ip6_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
	void *event_data)
{
	ip_event_got_ip6_t *event = (ip_event_got_ip6_t *) event_data;
	esp_ip6_addr_type_t ipv6_type =
		esp_netif_ip6_get_addr_type(&event->ip6_info.ip);

	ESP_LOGV(LOG_TAG, "Got IPv6 address: " IPV6STR,
		IPV62STR(event->ip6_info.ip));
	ESP_LOGV(LOG_TAG, "IPv6 address type: %d", ipv6_type);
}

static wifi_auth_mode_t
auth_threshold(void)
{
#if defined(CONFIG_EASYP_INET_WIFI_AUTH_WAPI_PSK)
	return WIFI_AUTH_WAPI_PSK;
#elif defined(CONFIG_EASYP_INET_WIFI_AUTH_WPA2_WPA3_PSK)
	return WIFI_AUTH_WPA2_WPA3_PSK;
#elif defined(CONFIG_EASYP_INET_WIFI_AUTH_WPA3_PSK)
	return WIFI_AUTH_WPA3_PSK;
#elif defined(CONFIG_EASYP_INET_WIFI_AUTH_WPA_WPA2_PSK)
	return WIFI_AUTH_WPA_WPA2_PSK;
#elif defined(CONFIG_EASYP_INET_WIFI_AUTH_WPA2_PSK)
	return WIFI_AUTH_WPA2_PSK;
#elif defined(CONFIG_EASYP_INET_WIFI_AUTH_WPA_PSK)
	return WIFI_AUTH_WPA_PSK;
#elif defined(CONFIG_EASYP_INET_WIFI_AUTH_WEP)
	return WIFI_AUTH_WEP;
#elif defined(CONFIG_EASYP_INET_WIFI_AUTH_OPEN)
	return WIFI_AUTH_OPEN;
#else
	return WIFI_AUTH_OPEN; // fallback
#endif
}

void
easyp_ip4addr_aton(const char *cp, esp_ip4_addr_t *addr)
{
	uint8_t bytes[4] = {0};
	sscanf(cp, "%hhu.%hhu.%hhu.%hhu", &bytes[0], &bytes[1], &bytes[2],
		&bytes[3]);
	addr->addr =
		esp_netif_ip4_makeu32(bytes[3], bytes[2], bytes[1], bytes[0]);
}

esp_err_t
easyp_inet_connect(void)
{
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
		WIFI_EVENT_STA_DISCONNECTED, &wifi_disconnect_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
		WIFI_EVENT_STA_CONNECTED, &wifi_connect_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(
		IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_got_ip_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(
		IP_EVENT, IP_EVENT_GOT_IP6, &ip_got_ip6_handler, NULL));

	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());

	s_wifi_netif = esp_netif_create_default_wifi_sta();
	if (s_wifi_netif == NULL)
	{
		ESP_LOGE(LOG_TAG, "Failed to create default Wi-Fi STA netif");
		return ESP_FAIL;
	}

	esp_netif_set_hostname(s_wifi_netif, CONFIG_EASYP_INET_HOSTNAME);

#if CONFIG_EASYP_MAC_ADDR_OVERRIDE
	uint8_t mac[6];
	sscanf(CONFIG_EASYP_MAC_ADDR, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0],
		&mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
	esp_wifi_set_mac(WIFI_IF_STA, mac);
#endif

#if CONFIG_EASYP_INET_NETIF_IP_MODE_STATIC == 1
	esp_netif_dhcpc_stop(s_wifi_netif);
	esp_netif_ip_info_t ip_info;
	easyp_ip4addr_aton(CONFIG_EASYP_INET_NETIF_IP4_ADDR, &ip_info.ip);
	easyp_ip4addr_aton(
		CONFIG_EASYP_INET_NETIF_IP4_NETMASK, &ip_info.netmask);
	easyp_ip4addr_aton(CONFIG_EASYP_INET_NETIF_IP4_GATEWAY, &ip_info.gw);
	esp_netif_set_ip_info(s_wifi_netif, &ip_info);
#endif

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());

	wifi_config_t wifi_config = {
		.sta =
			{
				.ssid        = CONFIG_EASYP_INET_WIFI_SSID,
				.password    = CONFIG_EASYP_INET_WIFI_PASSWORD,
				.scan_method = WIFI_ALL_CHANNEL_SCAN,
				.sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
				.threshold.rssi     = -127,
				.threshold.authmode = auth_threshold(),
			},
	};
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGV(LOG_TAG, "Connecting to Wi-Fi... (SSID: %s)",
		CONFIG_EASYP_INET_WIFI_SSID);
	esp_err_t err = esp_wifi_connect();
	if (err != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to initiate Wi-Fi connection: %s",
			esp_err_to_name(err));
		return err;
	}

	return ESP_OK;
}

esp_err_t
easyp_inet_disconnect(void)
{
	ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT,
		WIFI_EVENT_STA_DISCONNECTED, &wifi_disconnect_handler));
	ESP_ERROR_CHECK(esp_event_handler_unregister(
		WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &wifi_connect_handler));
	ESP_ERROR_CHECK(esp_event_handler_unregister(
		IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_got_ip_handler));
	ESP_ERROR_CHECK(esp_event_handler_unregister(
		IP_EVENT, IP_EVENT_GOT_IP6, &ip_got_ip6_handler));

	if (s_connected)
	{
		s_connected = false;

		easyp_inet_disconnected_event_t disconn_event = {
			.reason = s_disconnect_requested
				? EASYP_DISCONNECT_REASON_REQUESTED
				: EASYP_DISCONNECT_REASON_UNSOLICITED,
		};
		ESP_ERROR_CHECK(esp_event_post(EASYP_INET_EVENT,
			EASYP_INET_DISCONNECTED, &disconn_event,
			sizeof(disconn_event), portMAX_DELAY));
	}
	else
	{
		ESP_ERROR_CHECK(esp_event_post(EASYP_INET_EVENT,
			EASYP_INET_CONNECTION_FAILED, NULL, 0, portMAX_DELAY));
	}

	esp_err_t err = esp_wifi_disconnect();
	if (err != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to disconnect Wi-Fi: %s",
			esp_err_to_name(err));
	}

	if (s_wifi_netif)
	{
		esp_netif_destroy(s_wifi_netif);
		s_wifi_netif = NULL;
	}

	ESP_ERROR_CHECK(nvs_flash_deinit());
	return esp_wifi_stop();
}