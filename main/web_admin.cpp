#include "web_admin.h"
#include "device_config.h"
#include "config_validation.h"
#include "cJSON.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
namespace {
char session[65]="";
int64_t expires=0, login_after=0;
const char settings_page[] =
#include "settings_page.inc"
;
esp_err_t reply(httpd_req_t *r,const char *s){httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");return httpd_resp_sendstr(r,s);}
esp_err_t error(httpd_req_t *r,const char *status,const char *message){httpd_resp_set_status(r,status);cJSON *o=cJSON_CreateObject();cJSON_AddStringToObject(o,"error",message);char *s=cJSON_PrintUnformatted(o);esp_err_t e=reply(r,s?s:"{}");cJSON_free(s);cJSON_Delete(o);return e;}
bool origin_ok(httpd_req_t *r){char origin[128],host[96],expected[128];if(httpd_req_get_hdr_value_str(r,"Origin",origin,sizeof(origin))!=ESP_OK||httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host))!=ESP_OK)return false;std::snprintf(expected,sizeof(expected),"https://%s",host);return std::strcmp(origin,expected)==0;}
bool authenticated(httpd_req_t *r){char supplied[65];if(!session[0]||esp_timer_get_time()>expires||httpd_req_get_hdr_value_str(r,"X-Admin-Token",supplied,sizeof(supplied))!=ESP_OK||std::strlen(supplied)!=64)return false;unsigned diff=0;for(int i=0;i<64;++i)diff|=supplied[i]^session[i];return diff==0;}
bool protect(httpd_req_t *r,bool mutation=true){if(mutation&&!origin_ok(r)){error(r,"403 Forbidden","Invalid request origin.");return false;}if(!authenticated(r)){error(r,"401 Unauthorized","Please sign in.");return false;}return true;}
cJSON *read_json(httpd_req_t *r){char type[64];if(httpd_req_get_hdr_value_str(r,"Content-Type",type,sizeof(type))!=ESP_OK||std::strncmp(type,"application/json",16)||r->content_len<=0||r->content_len>3072)return nullptr;char body[3073];int received=0;while(received<r->content_len){int n=httpd_req_recv(r,body+received,r->content_len-received);if(n<=0)return nullptr;received+=n;}body[received]=0;return cJSON_ParseWithLength(body,received+1);}
bool str(cJSON *o,const char *key,char *out,size_t size,bool optional=false){auto *v=cJSON_GetObjectItemCaseSensitive(o,key);if(!v)return optional;if(!cJSON_IsString(v)||std::strlen(v->valuestring)>=size)return false;std::snprintf(out,size,"%s",v->valuestring);return true;}
bool boolean(cJSON *o,const char *key,bool &out){auto *v=cJSON_GetObjectItemCaseSensitive(o,key);if(!cJSON_IsBool(v))return false;out=cJSON_IsTrue(v);return true;}
bool number(cJSON *o,const char *key,unsigned &out,unsigned max){auto *v=cJSON_GetObjectItemCaseSensitive(o,key);if(!cJSON_IsNumber(v)||!std::isfinite(v->valuedouble)||v->valuedouble<0||v->valuedouble>max||std::floor(v->valuedouble)!=v->valuedouble)return false;out=unsigned(v->valuedouble);return true;}
esp_err_t page(httpd_req_t *r){httpd_resp_set_type(r,"text/html; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'self'; img-src 'self' data:; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'");return httpd_resp_send(r,settings_page,sizeof(settings_page)-1);}
esp_err_t login(httpd_req_t *r){if(!origin_ok(r))return error(r,"403 Forbidden","Invalid request origin.");if(esp_timer_get_time()<login_after)return error(r,"429 Too Many Requests","Please wait briefly.");login_after=esp_timer_get_time()+1000000LL;cJSON *o=read_json(r);char password[129]="";bool valid=o&&str(o,"password",password,sizeof(password));cJSON_Delete(o);DeviceConfig c;device_config_read(c);if(!valid||!device_config_password(password,c))return error(r,"401 Unauthorized","Incorrect password.");std::memset(password,0,sizeof(password));uint8_t random[32];esp_fill_random(random,32);for(int i=0;i<32;++i)std::snprintf(session+i*2,3,"%02x",random[i]);expires=esp_timer_get_time()+900000000LL;char response[100];std::snprintf(response,sizeof(response),"{\"token\":\"%s\"}",session);return reply(r,response);}
esp_err_t get_config(httpd_req_t *r){if(!protect(r,false))return ESP_OK;DeviceConfig c;device_config_read(c);cJSON *o=cJSON_CreateObject();
#define S(k,v) cJSON_AddStringToObject(o,k,v)
#define B(k,v) cJSON_AddBoolToObject(o,k,v)
#define N(k,v) cJSON_AddNumberToObject(o,k,v)
 S("name",c.name);B("dhcp",c.network.dhcp);S("ip",c.network.ip);S("gateway",c.network.gateway);S("mask",c.network.mask);S("dns",c.network.dns);N("brightness",c.brightness);N("dim_brightness",c.dim_brightness);N("dim_seconds",c.dim_seconds);B("network_pending",device_config_pending_network());B("network_saved_pending",c.network_pending);
#undef S
#undef B
#undef N
 char *json=cJSON_PrintUnformatted(o);esp_err_t e=reply(r,json?json:"{}");cJSON_free(json);cJSON_Delete(o);return e;}
esp_err_t save_config(httpd_req_t *r){if(!protect(r))return ESP_OK;cJSON *o=read_json(r);if(!o)return error(r,"400 Bad Request","Invalid JSON data.");DeviceConfig c;device_config_read(c);unsigned brightness,dim,seconds;char password[129]="";
 bool valid=str(o,"name",c.name,sizeof(c.name))&&boolean(o,"dhcp",c.network.dhcp)&&str(o,"ip",c.network.ip,16)&&str(o,"gateway",c.network.gateway,16)&&str(o,"mask",c.network.mask,16)&&str(o,"dns",c.network.dns,16)&&number(o,"brightness",brightness,255)&&number(o,"dim_brightness",dim,255)&&number(o,"dim_seconds",seconds,86400)&&str(o,"password",password,sizeof(password),true);
 cJSON_Delete(o);if(!valid)return error(r,"400 Bad Request","Missing field or invalid value.");c.brightness=brightness;c.dim_brightness=dim;c.dim_seconds=seconds;
 if(const char *why=validate_config(c))return error(r,"400 Bad Request",why);
 bool changed=password[0];if(changed&&!device_config_set_password(c,password))return error(r,"400 Bad Request","Password must contain 12-128 characters.");if(!device_config_save(c))return error(r,"500 Internal Server Error","Could not save settings.");
 if(changed){session[0]=0;expires=0;return reply(r,"{\"saved\":true,\"login_required\":true}");}return reply(r,"{\"saved\":true,\"login_required\":false}");}
esp_err_t logout(httpd_req_t *r){if(!protect(r))return ESP_OK;session[0]=0;expires=0;return reply(r,"{\"logged_out\":true}");}
esp_err_t reboot(httpd_req_t *r){if(!protect(r))return ESP_OK;esp_err_t e=reply(r,"{\"restarting\":true}");device_config_restart();return e;}
esp_err_t confirm_network(httpd_req_t *r){if(!protect(r))return ESP_OK;if(!device_config_confirm_network())return error(r,"409 Conflict","No active network settings awaiting confirmation.");return reply(r,"{\"confirmed\":true}");}
esp_err_t update(httpd_req_t *r){if(!protect(r))return ESP_OK;const esp_partition_t *partition=esp_ota_get_next_update_partition(nullptr);if(!partition||r->content_len<288||size_t(r->content_len)>partition->size)return error(r,"400 Bad Request","Invalid firmware size.");char type[64];if(httpd_req_get_hdr_value_str(r,"Content-Type",type,sizeof(type))!=ESP_OK||std::strcmp(type,"application/octet-stream"))return error(r,"415 Unsupported Media Type","Send firmware as a .bin file.");
 esp_ota_handle_t handle=0;device_update_active(true);esp_err_t e=esp_ota_begin(partition,OTA_WITH_SEQUENTIAL_WRITES,&handle);char buffer[2048];int remaining=r->content_len;bool first=true;
 while(e==ESP_OK&&remaining>0){int n=httpd_req_recv(r,buffer,std::min<int>(remaining,sizeof(buffer)));if(n<=0){e=ESP_FAIL;break;}if(first){ // read enough to validate the project descriptor before any flash writes
 int prefix=n;while(prefix<288){int count=httpd_req_recv(r,buffer+prefix,288-prefix);if(count<=0){e=ESP_FAIL;break;}prefix+=count;}n=prefix;if(e!=ESP_OK)break;
 const auto *image=reinterpret_cast<const esp_image_header_t *>(buffer);esp_app_desc_t desc{};std::memcpy(&desc,buffer+sizeof(esp_image_header_t)+sizeof(esp_image_segment_header_t),sizeof(desc));
 if(image->magic!=ESP_IMAGE_HEADER_MAGIC||std::strncmp(desc.project_name,"ESP32TimeServer",sizeof(desc.project_name))){e=ESP_FAIL;break;}first=false;}
 e=esp_ota_write(handle,buffer,n);remaining-=n;
 }
 if(e!=ESP_OK){if(handle)esp_ota_abort(handle);device_update_active(false);return error(r,"400 Bad Request","Update aborted; the previous firmware remains active.");}
 e=esp_ota_end(handle);if(e==ESP_OK)e=esp_ota_set_boot_partition(partition);if(e!=ESP_OK){device_update_active(false);return error(r,"400 Bad Request","Firmware verification failed.");}
 esp_err_t response=reply(r,"{\"updated\":true,\"restarting\":true}");device_update_active(false);device_config_restart();return response;
}
}
void web_admin_register(httpd_handle_t server){
 const struct{const char *uri;httpd_method_t method;esp_err_t(*handler)(httpd_req_t *);} routes[]={
 {"/settings",HTTP_GET,page},{"/api/login",HTTP_POST,login},{"/api/config",HTTP_GET,get_config},{"/api/config",HTTP_POST,save_config},{"/api/logout",HTTP_POST,logout},{"/api/reboot",HTTP_POST,reboot},{"/api/network-confirm",HTTP_POST,confirm_network},{"/api/update",HTTP_POST,update}};
 for(const auto &route:routes){httpd_uri_t uri{};uri.uri=route.uri;uri.method=route.method;uri.handler=route.handler;ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_register_uri_handler(server,&uri));}
}
