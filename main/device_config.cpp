#include "device_config.h"
#include "local_admin_seed.h"
#include "config_validation.h"
#include "config_compat.h"
#include <atomic>
#include <cstring>
#include <cstddef>
#include "nvs.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "psa/crypto.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
static_assert(offsetof(DeviceConfig, salt)==504 && sizeof(DeviceConfig)==552, "NVS v1 layout must remain compatible");
namespace {
DeviceConfig boot, saved;
SemaphoreHandle_t mutex;
std::atomic<uint8_t> brightness{100}, dim_brightness{80};
std::atomic<uint32_t> dim_seconds{60};
std::atomic<bool> pending{false}, updating{false}, ready{false};
std::atomic<int64_t> restart_at{0};
void apply_display(const DeviceConfig &c){brightness=c.brightness;dim_brightness=c.dim_brightness;dim_seconds=c.dim_seconds;}
bool persist(const DeviceConfig &c) {
 nvs_handle_t h;if(nvs_open("rack_config",NVS_READWRITE,&h)!=ESP_OK)return false;
 esp_err_t e=nvs_set_blob(h,"config_v1",&c,sizeof(c));if(e==ESP_OK)e=nvs_commit(h);nvs_close(h);return e==ESP_OK;
}
void maintenance(void *) {
 const int64_t start=esp_timer_get_time();bool validated=false;
 for(;;) {
  const auto now=esp_timer_get_time();
  if(!updating&&restart_at.load()>0&&now>=restart_at.load())esp_restart();
  if(!updating&&pending&&now-start>120000000LL)esp_restart(); // next boot reverts unconfirmed network
  if(!validated&&ready&&now-start>30000000LL){
   const esp_partition_t *running=esp_ota_get_running_partition();
   esp_ota_img_states_t state;
   if(running && running->subtype!=ESP_PARTITION_SUBTYPE_APP_FACTORY &&
      esp_ota_get_state_partition(running,&state)==ESP_OK && state==ESP_OTA_IMG_PENDING_VERIFY)
       esp_ota_mark_app_valid_cancel_rollback();
   validated=true;
  }
  vTaskDelay(pdMS_TO_TICKS(250));
 }
}
}
const DeviceConfig &device_config(){return boot;}
void device_config_init() {
 psa_crypto_init();
 mutex=xSemaphoreCreateMutex();
 nvs_handle_t h;size_t size=sizeof(saved);bool loaded=false;
 if(nvs_open("rack_config",NVS_READONLY,&h)==ESP_OK){loaded=nvs_get_blob(h,"config_v1",&saved,&size)==ESP_OK&&size==sizeof(saved)&&saved.version==1;nvs_close(h);}
 if(!loaded){saved=DeviceConfig{};std::memcpy(saved.salt,initial_salt,16);std::memcpy(saved.password_hash,initial_password_hash,32);if(!persist(saved))ESP_LOGE("config","Unable to persist defaults");}
 if(clear_legacy_config_bytes(saved)) persist(saved);
 if(saved.network_pending){
  if(saved.network_attempted){saved.network=saved.previous_network;saved.network_pending=saved.network_attempted=false;persist(saved);ESP_LOGW("config","Unconfirmed network reverted");}
  else {saved.network_attempted=true;persist(saved);pending=true;}
 }
 boot=saved;apply_display(saved);
 if(xTaskCreate(maintenance,"config_health",4096,nullptr,1,nullptr)!=pdPASS)ESP_LOGE("config","Maintenance task unavailable");
}
void device_config_read(DeviceConfig &out){xSemaphoreTake(mutex,portMAX_DELAY);out=saved;xSemaphoreGive(mutex);}
bool device_config_save(DeviceConfig &value){
 xSemaphoreTake(mutex,portMAX_DELAY);
 if(!network_equal(value.network,boot.network)){
  value.previous_network=boot.network;value.network_pending=true;value.network_attempted=false;
 }else{value.previous_network=saved.previous_network;value.network_pending=saved.network_pending;value.network_attempted=saved.network_attempted;}
 bool ok=persist(value);if(ok){saved=value;apply_display(value);}xSemaphoreGive(mutex);return ok;
}
bool device_config_confirm_network(){
 xSemaphoreTake(mutex,portMAX_DELAY);DeviceConfig next=saved;
 // Confirm only the network actually running, never a newly saved draft.
 bool ok=pending&&network_equal(next.network,boot.network);
 if(ok){next.network_pending=next.network_attempted=false;ok=persist(next);if(ok){saved=next;pending=false;}}
 xSemaphoreGive(mutex);return ok;
}
bool device_config_pending_network(){return pending;}
bool device_config_password(const char *password,const DeviceConfig &c){
 uint8_t data[144],hash[32]{};const size_t n=std::strlen(password);if(n>128)return false;
 std::memcpy(data,c.salt,16);std::memcpy(data+16,password,n);size_t written=0;
 const bool ok=psa_hash_compute(PSA_ALG_SHA_256,data,n+16,hash,sizeof(hash),&written)==PSA_SUCCESS&&written==32;
 unsigned diff=0;for(size_t i=0;i<32;++i)diff|=hash[i]^c.password_hash[i];
 std::memset(data,0,sizeof(data));return ok&&diff==0;
}
bool device_config_set_password(DeviceConfig &c,const char *password){
 const size_t n=std::strlen(password);if(n<12||n>128)return false;
 esp_fill_random(c.salt,16);uint8_t data[144];std::memcpy(data,c.salt,16);std::memcpy(data+16,password,n);size_t written=0;
 bool ok=psa_hash_compute(PSA_ALG_SHA_256,data,n+16,c.password_hash,32,&written)==PSA_SUCCESS&&written==32;std::memset(data,0,sizeof(data));return ok;
}
uint8_t device_display_brightness(bool dim){return dim?dim_brightness.load():brightness.load();}
uint32_t device_display_dim_seconds(){return dim_seconds.load();}
void device_config_restart(){restart_at=esp_timer_get_time()+1500000LL;}
void device_update_active(bool active){updating=active;}
bool device_update_is_active(){return updating;}
void device_web_ready(){ready=true;}
