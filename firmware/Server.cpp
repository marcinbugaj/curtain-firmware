#include "Server.h"

#include "pico/cyw43_arch.h"

#include "lwip/apps/httpd.h"

#include "dhcp.h"
#include "picow_ntp_client.h"

#include "UrlEncodedSettingsForm.h"

#include <atomic>

static std::atomic_bool finished = false;

/* cgi-handler triggered by a request for "/leds.cgi" */
const char *ommit_handler(int iIndex, int iNumParams, char *pcParam[],
                          char *pcValue[]) {

  finished = true;
  return "/ok.html";
}

const char *settings_handler(int iIndex, int iNumParams, char *pcParam[],
                             char *pcValue[]) {
  return "/settings.html";
}

UrlEncodedSettingsForm_t form = {0, 0, 0};

/* cgi-handler triggered by a request for "/leds.cgi" */
const char *settings_submit_handler(int iIndex, int iNumParams, char *pcParam[],
                                    char *pcValue[]) {
  int i = 0;

  /* We use this handler for one page request only: "/leds.cgi"
   * and it is at position 0 in the tCGI array (see above).
   * So iIndex should be 0.
   */
  printf("settings_handler called with index %d\n", iIndex);

  /* Check the query string. */
  for (i = 0; i < iNumParams; i++) {
    /* check iame of the param */
    if (strcmp(pcParam[i], "ssid") == 0) {
      /* look ar argument to find which led to turn on */
      printf("Ssid: %s\n", pcValue[i]);

      if (strlen(pcValue[i]) < sizeof(UrlEncodedSettingsForm_t::ssid)) {
        strcpy(form.ssid, pcValue[i]);
      }
    }

    if (strcmp(pcParam[i], "password") == 0) {
      /* look ar argument to find which led to turn on */
      printf("password: %s\n", pcValue[i]);

      if (strlen(pcValue[i]) < sizeof(UrlEncodedSettingsForm_t::password)) {
        strcpy(form.password, pcValue[i]);
      }
    }

    if (strcmp(pcParam[i], "wakeup") == 0) {
      /* look ar argument to find which led to turn on */
      printf("wakeup: %s\n", pcValue[i]);

      if (strlen(pcValue[i]) < sizeof(UrlEncodedSettingsForm_t::wakeup)) {
        strcpy(form.wakeup, pcValue[i]);
      }
    }
  }

  finished = true;

  /* Our response to the "SUBMIT" is to simply send the same page again*/
  return "/ok.html";
}

static const tCGI cgi_handlers[] = {
    {"/settings", settings_handler},
    {"/ommit", ommit_handler},
    {"/settings_submit", settings_submit_handler}};

void createOwnNetwork() {
  const char *ap_name = "picow";
  const char *password = "password";

  cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);
}

static void initializeCYW43() {
  if (cyw43_arch_init()) {
    printf("Failed to initialise CYW43 driver\n");
    abort();
  }
}

UrlEncodedSettingsForm_t runServerForXMintes() {
  initializeCYW43();

  createOwnNetwork();

  ip_addr_t gw;
  ip4_addr_t mask;
  IP4_ADDR(ip_2_ip4(&gw), 192, 168, 4, 1);
  IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

  // Start the dhcp server
  dhcp_server_t dhcp_server;
  dhcp_server_init(&dhcp_server, &gw, &mask);

  httpd_init();
  http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));

  const auto start = get_absolute_time();
  while (!finished &&
         (absolute_time_diff_us(start, get_absolute_time()) < 180e6)) {
    sleep_ms(1000);
  }

  sleep_ms(2000); // time required for server to send the reponse

  dhcp_server_deinit(&dhcp_server);

  cyw43_arch_deinit();

  return form;
}