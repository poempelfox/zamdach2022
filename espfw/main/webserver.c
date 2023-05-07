
#include <esp_http_server.h>
#include <esp_log.h>
#include <time.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_crt_bundle.h>
#include <esp_netif.h>
#include "webserver.h"
#include "secrets.h"

/* These are in zamdach2022_main.c */
extern char chipid[30];
extern struct ev evs[2];
extern int activeevs;
extern int pendingfwverify;
extern long too_wet_ctr;
extern int forcesht4xheater;
/* This is in network.c */
extern esp_netif_t * mainnetif;

static const char startp_p1[] = R"EOSP1(
<!DOCTYPE html>

<html><head><title>ZAMDACH2022 - die Sensoren auf dem Dach des ZAM</title>
<style type="text/css">
body { background-color:#000000;color:#cccccc; }
table, th, td { border:1px solid #aaaaff;border-collapse:collapse;padding:5px; }
th { text-align:left; }
td { text-align:right; }
a:link, a:visited, a:hover { color:#ccccff; }
</style>
</head><body>
<h1>ZAMDACH2022</h1>
<noscript>Because JavaScript is disabled, the values shown will not update automatically -
 you will need to reload the page to see the current values.<br></noscript>
)EOSP1";

static const char startp_p2[] = R"EOSP2(
<script type="text/javascript">
var getJSON = function(url, callback) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.responseType = 'json';
    xhr.onload = function() {
      var status = xhr.status;
      if (status === 200) {
        callback(null, xhr.response);
      } else {
        callback(status, xhr.response);
      }
    };
    xhr.send();
};
function updrcvd(err, data) {
  if (err != null) {
    document.getElementById("ts").innerHTML = "Update failed.";
  } else {
    for (let k in data) {
      if (document.getElementById(k) != null) {
        if ((k === "ts") || (k === "lastsht4xheat")) {
          var jsts = new Date(data[k] * 1000);
          document.getElementById(k).innerHTML = data[k] + " (" + jsts.toISOString() + ")";
        } else {
          document.getElementById(k).innerHTML = data[k];
        }
      }
    }
  }
}
function updatethings() {
  getJSON('/json', updrcvd);
}
var myrefresher = setInterval(updatethings, 30000);
</script>
<br>The recommended way for using this data in scripts is to query
 <a href="/json">the JSON-output under /json</a>.<br><br>
Current firmware version:
)EOSP2";

static const char startp_fww[] = R"EOSPFWW(
<br>A new firmware has been flashed, and booted up - but it has not been marked as &quot;good&quot; yet.
Unless you mark the new firmware as &quot;good&quot;, on the next reset the old firmware will be
restored.
)EOSPFWW";

static const char startp_p3[] = R"EOSP3(
<h3>Admin-Actions:</h3>
<form action="/adminaction" method="POST">
Admin-Password:
<input type="text" name="updatepw">
<select name="action">
<option value="flashupdate">Flash Firmware Update</option>
<option value="reboot" selected>Reboot the Microcontroller</option>
<option value="forcesht4xheater">Force SHT4x Heater On</option>
)EOSP3";

static const char startp_p4[] = R"EOSP4(
</select>
<input type="submit" name="su" value="Execute Action"><br>
URL for firmware Update:
<input type="text" name="updateurl" value="https://www.poempelfox.de/espfw/zamdach2022.bin">
</form>
BE PATIENT after clicking "Flash Firmware Update" - it will take at
least 30 seconds before the webserver will show any sort of reply.
</body></html>
)EOSP4";

/********************************************************
 * End of embedded webpages definition                  *
 ********************************************************/

esp_err_t get_startpage_handler(httpd_req_t * req) {
  char myresponse[sizeof(startp_p1) + sizeof(startp_p2) + sizeof(startp_fww) + sizeof(startp_p3) + sizeof(startp_p4) + 2000];
  char * pfp;
  int e = activeevs;
  strcpy(myresponse, startp_p1);
  pfp = myresponse + strlen(startp_p1);
  pfp += sprintf(pfp, "<table><tr><th>UpdateTS</th><td id=\"ts\">%lld</td></tr>", evs[e].lastupd);
  pfp += sprintf(pfp, "<tr><th>LastSHT4xHeaterTS</th><td id=\"lastsht4xheat\">%lld</td></tr>", evs[e].lastsht4xheat);
  pfp += sprintf(pfp, "<tr><th>Temperature (C)</th><td id=\"temp\">%.2f</td></tr>", evs[e].temp);
  pfp += sprintf(pfp, "<tr><th>Humidity (%%)</th><td id=\"hum\">%.1f</td></tr>", evs[e].hum);
  pfp += sprintf(pfp, "<tr><th>PM 1.0 (&micro;g/m&sup3;)</th><td id=\"pm010\">%.1f</td></tr>", evs[e].pm010);
  pfp += sprintf(pfp, "<tr><th>PM 2.5 (&micro;g/m&sup3;)</th><td id=\"pm025\">%.1f</td></tr>", evs[e].pm025);
  pfp += sprintf(pfp, "<tr><th>PM 4.0 (&micro;g/m&sup3;)</th><td id=\"pm040\">%.1f</td></tr>", evs[e].pm040);
  pfp += sprintf(pfp, "<tr><th>PM 10.0 (&micro;g/m&sup3;)</th><td id=\"pm100\">%.1f</td></tr>", evs[e].pm100);
  pfp += sprintf(pfp, "<tr><th>Pressure (hPa)</th><td id=\"press\">%.3f</td></tr>", evs[e].press);
  pfp += sprintf(pfp, "<tr><th>Illuminance (lux)</th><td id=\"lux\">%.2f</td></tr>", evs[e].lux);
  pfp += sprintf(pfp, "<tr><th>UV-Index</th><td id=\"uvind\">%.2f</td></tr>", evs[e].uvind);
  pfp += sprintf(pfp, "<tr><th>Rain (mm/min)</th><td id=\"raing\">%.2f</td></tr>", evs[e].raing);
  pfp += sprintf(pfp, "<tr><th>Wind speed (km/h)</th><td id=\"windspeed\">%.1f</td></tr>", evs[e].windspeed);
  pfp += sprintf(pfp, "<tr><th>Wind speed max / gusts (km/h)</th><td id=\"windspmax\">%.1f</td></tr>", evs[e].windspmax);
  pfp += sprintf(pfp, "<tr><th>Wind direction</th><td id=\"winddirtxt\">%s</td></tr>", evs[e].winddirtxt);
  pfp += sprintf(pfp, "</table>");
  const esp_app_desc_t * appd = esp_app_get_description();
  strcat(myresponse, startp_p2);
  pfp = myresponse + strlen(myresponse);
  pfp += sprintf(pfp, "%s version %s compiled %s %s",
                 appd->project_name, appd->version, appd->date, appd->time);
  if (pendingfwverify > 0) {
    strcat(myresponse, startp_fww);
  }
  strcat(myresponse, startp_p3);
  if (pendingfwverify > 0) {
    strcat(myresponse, "<option value=\"markfwasgood\">Mark Firmware as Good</option>");
  }
  strcat(myresponse, startp_p4);
  /* The following two lines are the default und thus redundant. */
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=29");
  httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static httpd_uri_t uri_startpage = {
  .uri      = "/",
  .method   = HTTP_GET,
  .handler  = get_startpage_handler,
  .user_ctx = NULL
};

esp_err_t get_json_handler(httpd_req_t * req) {
  char myresponse[1100];
  char * pfp;
  int e = activeevs;
  strcpy(myresponse, "");
  pfp = myresponse;
  pfp += sprintf(pfp, "{\"ts\":\"%lld\",", evs[e].lastupd);
  pfp += sprintf(pfp, "\"lastsht4xheat\":\"%lld\",", evs[e].lastsht4xheat);
  pfp += sprintf(pfp, "\"temp\":\"%.2f\",", evs[e].temp);
  pfp += sprintf(pfp, "\"hum\":\"%.1f\",", evs[e].hum);
  pfp += sprintf(pfp, "\"pm010\":\"%.1f\",", evs[e].pm010);
  pfp += sprintf(pfp, "\"pm025\":\"%.1f\",", evs[e].pm025);
  pfp += sprintf(pfp, "\"pm040\":\"%.1f\",", evs[e].pm040);
  pfp += sprintf(pfp, "\"pm100\":\"%.1f\",", evs[e].pm100);
  pfp += sprintf(pfp, "\"press\":\"%.3f\",", evs[e].press);
  pfp += sprintf(pfp, "\"lux\":\"%.2f\",", evs[e].lux);
  pfp += sprintf(pfp, "\"uvind\":\"%.2f\",", evs[e].uvind);
  pfp += sprintf(pfp, "\"raing\":\"%.2f\",", evs[e].raing);
  pfp += sprintf(pfp, "\"windspeed\":\"%.1f\",", evs[e].windspeed);
  pfp += sprintf(pfp, "\"windspmax\":\"%.1f\",", evs[e].windspmax);
  pfp += sprintf(pfp, "\"winddirdeg\":\"%.1f\",", evs[e].winddirdeg);
  pfp += sprintf(pfp, "\"winddirtxt\":\"%s\"}", evs[e].winddirtxt);
  /* The following line is the default und thus redundant. */
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=29");
  httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static httpd_uri_t uri_json = {
  .uri      = "/json",
  .method   = HTTP_GET,
  .handler  = get_json_handler,
  .user_ctx = NULL
};

esp_err_t get_publicdebug_handler(httpd_req_t * req) {
  char myresponse[1100];
  char * pfp;
  strcpy(myresponse, "");
  pfp = myresponse;
  pfp += sprintf(pfp, "<html><head><title>Debug info (public part)</title></head><body>");
  pfp += sprintf(pfp, "too_wet_ctr: %ld<br>", too_wet_ctr);
  pfp += sprintf(pfp, "chipid: %s<br>", chipid);
  esp_netif_ip_info_t ip_info;
  pfp += sprintf(pfp, "My IP addresses:<br><ul>");
  if (esp_netif_get_ip_info(mainnetif, &ip_info) == ESP_OK) {
    pfp += sprintf(pfp, "<li>IPv4: " IPSTR "/" IPSTR " GW " IPSTR "</li>",
                   IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask),
                   IP2STR(&ip_info.gw));
  } else {
    pfp += sprintf(pfp, "<li>Failed to get IPv4 address information :(</li>");
  }
  esp_ip6_addr_t v6addrs[CONFIG_LWIP_IPV6_NUM_ADDRESSES + 5];
  int nv6ips = esp_netif_get_all_ip6(mainnetif, v6addrs);
  for (int i = 0; i < nv6ips; i++) {
    pfp += sprintf(pfp, "<li>IPv6: " IPV6STR "</li>",
           IPV62STR(v6addrs[i]));
  }
  pfp += sprintf(pfp, "</ul>");
  /* The following line is the default und thus redundant. */
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=29");
  httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static httpd_uri_t uri_debug = {
  .uri      = "/debug",
  .method   = HTTP_GET,
  .handler  = get_publicdebug_handler,
  .user_ctx = NULL
};

/* Unescapes a x-www-form-urlencoded string.
 * Modifies the string inplace! */
void unescapeuestring(char * s) {
  char * rp = s;
  char * wp = s;
  while (*rp != 0) {
    if (strncmp(rp, "&amp;", 5) == 0) {
      *wp = '&'; rp += 5; wp += 1;
    } else if (strncmp(rp, "%26", 3) == 0) {
      *wp = '&'; rp += 3; wp += 1;
    } else if (strncmp(rp, "%3A", 3) == 0) {
      *wp = ':'; rp += 3; wp += 1;
    } else if (strncmp(rp, "%2F", 3) == 0) {
      *wp = '/'; rp += 3; wp += 1;
    } else {
      *wp = *rp; wp++; rp++;
    }
  }
  *wp = 0;
}

esp_err_t post_adminaction(httpd_req_t * req) {
  char postcontent[600];
  char myresponse[1000];
  char tmp1[600];
  //ESP_LOGI("webserver.c", "POST request with length: %d", req->content_len);
  if (req->content_len >= sizeof(postcontent)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    strcpy(myresponse, "Sorry, your request was too large. Try a shorter update URL?");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  int ret = httpd_req_recv(req, postcontent, req->content_len);
  if (ret < req->content_len) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    strcpy(myresponse, "Your request was incompletely received.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  postcontent[req->content_len] = 0;
  ESP_LOGI("webserver.c", "Received data: '%s'", postcontent);
  if (httpd_query_key_value(postcontent, "updatepw", tmp1, sizeof(tmp1)) != ESP_OK) {
    httpd_resp_set_status(req, "400 Bad Request");
    strcpy(myresponse, "No updatepw submitted.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  unescapeuestring(tmp1);
  if (strcmp(tmp1, ZAMDACH_WEBIFADMINPW) != 0) {
    ESP_LOGI("webserver.c", "Incorrect AdminPW - UE: '%s'", tmp1);
    httpd_resp_set_status(req, "403 Forbidden");
    strcpy(myresponse, "Admin-Password incorrect.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  if (httpd_query_key_value(postcontent, "action", tmp1, sizeof(tmp1)) != ESP_OK) {
    httpd_resp_set_status(req, "400 Bad Request");
    strcpy(myresponse, "No adminaction selected.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  if (strcmp(tmp1, "flashupdate") == 0) {
    if (httpd_query_key_value(postcontent, "updateurl", tmp1, sizeof(tmp1)) != ESP_OK) {
      httpd_resp_set_status(req, "400 Bad Request");
      strcpy(myresponse, "No updateurl submitted.");
      httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
      return ESP_OK;
    }
    unescapeuestring(tmp1);
    ESP_LOGI("webserver.c", "UE UpdateURL: '%s'", tmp1);
    sprintf(myresponse, "OK, will try to update from: %s'<br>", tmp1);
    esp_http_client_config_t httpccfg = {
        .url = tmp1,
        .timeout_ms = 60000,
        .keep_alive_enable = true,
        .crt_bundle_attach = esp_crt_bundle_attach
    };
    esp_https_ota_config_t otacfg = {
        .http_config = &httpccfg
    };
    ret = esp_https_ota(&otacfg);
    if (ret == ESP_OK) {
      ESP_LOGI("webserver.c", "OTA Succeed, Rebooting...");
      strcat(myresponse, "OTA Update reported success. Will reboot.");
      httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
      vTaskDelay(3 * (1000 / portTICK_PERIOD_MS)); 
      esp_restart();
    } else {
      ESP_LOGE("webserver.c", "Firmware upgrade failed");
      strcat(myresponse, "OTA Update reported failure.");
      httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
      return ESP_OK;
    }
    /* This should not be reached. */
  } else if (strcmp(tmp1, "reboot") == 0) {
    ESP_LOGI("webserver.c", "Reboot requested by admin, Rebooting...");
    strcpy(myresponse, "OK, will reboot in 3 seconds.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    vTaskDelay(3 * (1000 / portTICK_PERIOD_MS)); 
    esp_restart();
    /* This should not be reached */
  } else if (strcmp(tmp1, "forcesht4xheater") == 0) {
    ESP_LOGI("webserver.c", "Forced SHT4x heating cycle requested by admin.");
    forcesht4xheater = 1;
    strcpy(myresponse, "OK, will do a SHT4x heating cycle after the next polling iteration.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  } else if (strcmp(tmp1, "markfwasgood") == 0) {
    if (pendingfwverify == 0) {
      httpd_resp_set_status(req, "400 Bad Request");
      strcpy(myresponse, "You're trying to mark an already marked firmware.");
      httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
      return ESP_OK;
    }
    ret = esp_ota_mark_app_valid_cancel_rollback();
    if (ret == ESP_OK) {
      ESP_LOGI("webserver.c", "markfirmwareasgood: Updated firmware is now marked as good.");
      strcpy(myresponse, "New firmware was successfully marked as good.");
      httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    } else {
      ESP_LOGE("webserver.c", "markfirmwareasgood: Failed to mark updated firmware as good, will rollback on next reboot.");
      strcpy(myresponse, "Failed to mark updated firmware as good, will rollback on next reboot.");
      httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    }
    pendingfwverify = 0;
  } else {
    httpd_resp_set_status(req, "400 Bad Request");
    strcpy(myresponse, "Unknown adminaction requested.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
  }
  return ESP_OK;
}

static httpd_uri_t uri_adminaction = {
  .uri      = "/adminaction",
  .method   = HTTP_POST,
  .handler  = post_adminaction,
  .user_ctx = NULL
};

void webserver_start(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  /* Documentation is - as usual - a bit patchy, but I assume
   * the following drops the oldest connection if the ESP runs
   * out of connections. */
  config.lru_purge_enable = true;
  config.server_port = 80;
  /* The default is undocumented, but seems to be only 4k. */
  config.stack_size = 10000;
  ESP_LOGI("webserver.c", "Starting webserver on port %d", config.server_port);
  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGE("webserver.c", "Failed to start HTTP server.");
    return;
  }
  httpd_register_uri_handler(server, &uri_startpage);
  httpd_register_uri_handler(server, &uri_json);
  httpd_register_uri_handler(server, &uri_debug);
  httpd_register_uri_handler(server, &uri_adminaction);
}

