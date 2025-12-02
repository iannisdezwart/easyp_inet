#include <esp_log.h>
#include <esp_event.h>
#include <easyp_inet.h>

static const char *LOG_TAG = "example_connect_and_print_info";

bool s_connected = false;
void
connected_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
	void *event_data)
{
	s_connected = true;
}

bool s_disconnected = false;
void
disconnected_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
	void *event_data)
{
	s_disconnected = true;
}

void
failed_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
	void *event_data)
{
	ESP_LOGE(LOG_TAG, "Failed to connect to the internet.");
}

void
app_main(void)
{
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	ESP_LOGI(LOG_TAG, "Connecting...");
	ESP_ERROR_CHECK(esp_event_handler_register(EASYP_INET_EVENT,
		EASYP_INET_CONNECTION_SUCCEEDED, &connected_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(EASYP_INET_EVENT,
		EASYP_INET_CONNECTION_FAILED, &failed_handler, NULL));
	ESP_ERROR_CHECK(easyp_inet_connect());
	while (!s_connected)
	{
		vTaskDelay(pdMS_TO_TICKS(100));
	}

	ESP_LOGI(LOG_TAG, "Connected to the internet!");
	ESP_ERROR_CHECK(esp_event_handler_unregister(EASYP_INET_EVENT,
		EASYP_INET_CONNECTION_SUCCEEDED, &connected_handler));
	ESP_ERROR_CHECK(esp_event_handler_unregister(EASYP_INET_EVENT,
		EASYP_INET_CONNECTION_FAILED, &failed_handler));

	if (!s_connected)
	{
		return;
	}

	ESP_LOGI(LOG_TAG, "Disconnecting...");
	ESP_ERROR_CHECK(esp_event_handler_register(EASYP_INET_EVENT,
		EASYP_INET_DISCONNECTED, &disconnected_handler, NULL));
	ESP_ERROR_CHECK(easyp_inet_disconnect());
	while (!s_disconnected)
	{
		vTaskDelay(pdMS_TO_TICKS(100));
	}

	ESP_LOGI(LOG_TAG, "Disconnected from the internet!");
	ESP_ERROR_CHECK(esp_event_handler_unregister(EASYP_INET_EVENT,
		EASYP_INET_DISCONNECTED, &disconnected_handler));
}